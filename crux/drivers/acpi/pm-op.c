/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <crux/acpi.h>
#include <crux/domain.h>
#include <crux/errno.h>
#include <crux/guest_access.h>
#include <crux/lib.h>
#include <crux/pmstat.h>
#include <crux/sched.h>

#include <acpi/cpufreq/cpufreq.h>
#include <public/platform.h>
#include <public/sysctl.h>

/*
 * 1. Get PM parameter
 * 2. Provide user PM control
 */
static int cpufreq_update_turbo(unsigned int cpu, int new_state)
{
    struct cpufreq_policy *policy;
    int curr_state;
    int ret = 0;

    if ( new_state != CPUFREQ_TURBO_ENABLED &&
         new_state != CPUFREQ_TURBO_DISABLED )
        return -EINVAL;

    policy = per_cpu(cpufreq_cpu_policy, cpu);
    if ( !policy )
        return -EACCES;

    if ( policy->turbo == CPUFREQ_TURBO_UNSUPPORTED )
        return -EOPNOTSUPP;

    curr_state = policy->turbo;
    if ( curr_state == new_state )
        return 0;

    policy->turbo = new_state;
    if ( cpufreq_driver.update )
    {
        ret = alternative_call(cpufreq_driver.update, cpu, policy);
        if ( ret )
            policy->turbo = curr_state;
    }

    return ret;
}

static int cpufreq_get_turbo_status(unsigned int cpu)
{
    struct cpufreq_policy *policy;

    policy = per_cpu(cpufreq_cpu_policy, cpu);
    return policy && policy->turbo == CPUFREQ_TURBO_ENABLED;
}

static int read_scaling_available_governors(char *scaling_available_governors,
                                            unsigned int size)
{
    unsigned int i = 0;
    struct cpufreq_governor *t;

    if ( !scaling_available_governors )
        return -EINVAL;

    list_for_each_entry(t, &cpufreq_governor_list, governor_list)
    {
        i += scnprintf(&scaling_available_governors[i],
                       CPUFREQ_NAME_LEN, "%s ", t->name);
        if ( i > size )
            return -EINVAL;
    }
    scaling_available_governors[i-1] = '\0';

    return 0;
}

static int get_cpufreq_para(struct crux_sysctl_pm_op *op)
{
    uint32_t ret = 0;
    const struct processor_pminfo *pmpt;
    struct cpufreq_policy *policy;
    uint32_t gov_num = 0;
    uint32_t *data;
    char     *scaling_available_governors;
    struct list_head *pos;
    unsigned int cpu, i = 0;

    pmpt = processor_pminfo[op->cpuid];
    policy = per_cpu(cpufreq_cpu_policy, op->cpuid);

    if ( !pmpt || !pmpt->perf.states ||
         !policy || !policy->governor )
        return -EINVAL;

    list_for_each(pos, &cpufreq_governor_list)
        gov_num++;

    if ( (op->u.get_para.cpu_num  != cpumask_weight(policy->cpus)) ||
         (op->u.get_para.freq_num != pmpt->perf.state_count)    ||
         (op->u.get_para.gov_num  != gov_num) )
    {
        op->u.get_para.cpu_num =  cpumask_weight(policy->cpus);
        op->u.get_para.freq_num = pmpt->perf.state_count;
        op->u.get_para.gov_num  = gov_num;
        return -EAGAIN;
    }

    if ( !(data = xzalloc_array(uint32_t,
                                max(op->u.get_para.cpu_num,
                                    op->u.get_para.freq_num))) )
        return -ENOMEM;

    for_each_cpu(cpu, policy->cpus)
        data[i++] = cpu;
    ret = copy_to_guest(op->u.get_para.affected_cpus,
                        data, op->u.get_para.cpu_num);

    for ( i = 0; i < op->u.get_para.freq_num; i++ )
        data[i] = pmpt->perf.states[i].core_frequency * 1000;
    ret += copy_to_guest(op->u.get_para.scaling_available_frequencies,
                         data, op->u.get_para.freq_num);

    xfree(data);
    if ( ret )
        return -EFAULT;

    op->u.get_para.cpuinfo_cur_freq =
        cpufreq_driver.get ? alternative_call(cpufreq_driver.get, op->cpuid)
                           : policy->cur;
    op->u.get_para.cpuinfo_max_freq = policy->cpuinfo.max_freq;
    op->u.get_para.cpuinfo_min_freq = policy->cpuinfo.min_freq;
    op->u.get_para.turbo_enabled = cpufreq_get_turbo_status(op->cpuid);

    if ( cpufreq_driver.name[0] )
        strlcpy(op->u.get_para.scaling_driver,
                cpufreq_driver.name, CPUFREQ_NAME_LEN);
    else
        strlcpy(op->u.get_para.scaling_driver, "Unknown", CPUFREQ_NAME_LEN);

    if ( hwp_active() )
        ret = get_hwp_para(policy->cpu, &op->u.get_para.u.cppc_para);
    else
    {
        if ( !(scaling_available_governors =
               xzalloc_array(char, gov_num * CPUFREQ_NAME_LEN)) )
            return -ENOMEM;
        if ( (ret = read_scaling_available_governors(
                        scaling_available_governors,
                        (gov_num * CPUFREQ_NAME_LEN *
                         sizeof(*scaling_available_governors)))) )
        {
            xfree(scaling_available_governors);
            return ret;
        }
        ret = copy_to_guest(op->u.get_para.scaling_available_governors,
                            scaling_available_governors,
                            gov_num * CPUFREQ_NAME_LEN);
        xfree(scaling_available_governors);
        if ( ret )
            return -EFAULT;

        op->u.get_para.u.s.scaling_cur_freq = policy->cur;
        op->u.get_para.u.s.scaling_max_freq = policy->max;
        op->u.get_para.u.s.scaling_min_freq = policy->min;

        if ( policy->governor->name[0] )
            strlcpy(op->u.get_para.u.s.scaling_governor,
                    policy->governor->name, CPUFREQ_NAME_LEN);
        else
            strlcpy(op->u.get_para.u.s.scaling_governor, "Unknown",
                    CPUFREQ_NAME_LEN);

        /* governor specific para */
        if ( !strncasecmp(op->u.get_para.u.s.scaling_governor,
                          "userspace", CPUFREQ_NAME_LEN) )
            op->u.get_para.u.s.u.userspace.scaling_setspeed = policy->cur;

        if ( !strncasecmp(op->u.get_para.u.s.scaling_governor,
                          "ondemand", CPUFREQ_NAME_LEN) )
            ret = get_cpufreq_ondemand_para(
                &op->u.get_para.u.s.u.ondemand.sampling_rate_max,
                &op->u.get_para.u.s.u.ondemand.sampling_rate_min,
                &op->u.get_para.u.s.u.ondemand.sampling_rate,
                &op->u.get_para.u.s.u.ondemand.up_threshold);
    }

    return ret;
}

static int set_cpufreq_gov(struct crux_sysctl_pm_op *op)
{
    struct cpufreq_policy new_policy, *old_policy;

    old_policy = per_cpu(cpufreq_cpu_policy, op->cpuid);
    if ( !old_policy )
        return -EINVAL;

    memcpy(&new_policy, old_policy, sizeof(struct cpufreq_policy));

    new_policy.governor = __find_governor(op->u.set_gov.scaling_governor);
    if ( new_policy.governor == NULL )
        return -EINVAL;

    return __cpufreq_set_policy(old_policy, &new_policy);
}

static int set_cpufreq_para(struct crux_sysctl_pm_op *op)
{
    int ret = 0;
    struct cpufreq_policy *policy;

    policy = per_cpu(cpufreq_cpu_policy, op->cpuid);

    if ( !policy || !policy->governor )
        return -EINVAL;

    if ( hwp_active() )
        return -EOPNOTSUPP;

    switch( op->u.set_para.ctrl_type )
    {
    case SCALING_MAX_FREQ:
    {
        struct cpufreq_policy new_policy;

        memcpy(&new_policy, policy, sizeof(struct cpufreq_policy));
        new_policy.max = op->u.set_para.ctrl_value;
        ret = __cpufreq_set_policy(policy, &new_policy);

        break;
    }

    case SCALING_MIN_FREQ:
    {
        struct cpufreq_policy new_policy;

        memcpy(&new_policy, policy, sizeof(struct cpufreq_policy));
        new_policy.min = op->u.set_para.ctrl_value;
        ret = __cpufreq_set_policy(policy, &new_policy);

        break;
    }

    case SCALING_SETSPEED:
    {
        unsigned int freq =op->u.set_para.ctrl_value;

        if ( !strncasecmp(policy->governor->name,
                          "userspace", CPUFREQ_NAME_LEN) )
            ret = write_userspace_scaling_setspeed(op->cpuid, freq);
        else
            ret = -EINVAL;

        break;
    }

    case SAMPLING_RATE:
    {
        unsigned int sampling_rate = op->u.set_para.ctrl_value;

        if ( !strncasecmp(policy->governor->name,
                          "ondemand", CPUFREQ_NAME_LEN) )
            ret = write_ondemand_sampling_rate(sampling_rate);
        else
            ret = -EINVAL;

        break;
    }

    case UP_THRESHOLD:
    {
        unsigned int up_threshold = op->u.set_para.ctrl_value;

        if ( !strncasecmp(policy->governor->name,
                          "ondemand", CPUFREQ_NAME_LEN) )
            ret = write_ondemand_up_threshold(up_threshold);
        else
            ret = -EINVAL;

        break;
    }

    default:
        ret = -EINVAL;
        break;
    }

    return ret;
}

static int set_cpufreq_cppc(struct crux_sysctl_pm_op *op)
{
    struct cpufreq_policy *policy = per_cpu(cpufreq_cpu_policy, op->cpuid);

    if ( !policy || !policy->governor )
        return -ENOENT;

    if ( !hwp_active() )
        return -EOPNOTSUPP;

    return set_hwp_para(policy, &op->u.set_cppc);
}

int do_pm_op(struct crux_sysctl_pm_op *op)
{
    int ret = 0;
    const struct processor_pminfo *pmpt;

    switch ( op->cmd )
    {
    case CRUX_SYSCTL_pm_op_set_sched_opt_smt:
    {
        uint32_t saved_value = sched_smt_power_savings;

        if ( op->cpuid != 0 )
            return -EINVAL;
        sched_smt_power_savings = !!op->u.set_sched_opt_smt;
        op->u.set_sched_opt_smt = saved_value;
        return 0;
    }

    case CRUX_SYSCTL_pm_op_get_max_cstate:
        BUILD_BUG_ON(CRUX_SYSCTL_CX_UNLIMITED != UINT_MAX);
        if ( op->cpuid == 0 )
            op->u.get_max_cstate = acpi_get_cstate_limit();
        else if ( op->cpuid == 1 )
            op->u.get_max_cstate = acpi_get_csubstate_limit();
        else
            ret = -EINVAL;
        return ret;

    case CRUX_SYSCTL_pm_op_set_max_cstate:
        if ( op->cpuid == 0 )
            acpi_set_cstate_limit(op->u.set_max_cstate);
        else if ( op->cpuid == 1 )
            acpi_set_csubstate_limit(op->u.set_max_cstate);
        else
            ret = -EINVAL;
        return ret;
    }

    if ( op->cpuid >= nr_cpu_ids || !cpu_online(op->cpuid) )
        return -EINVAL;
    pmpt = processor_pminfo[op->cpuid];

    switch ( op->cmd & PM_PARA_CATEGORY_MASK )
    {
    case CPUFREQ_PARA:
        if ( !(crux_processor_pmbits & CRUX_PROCESSOR_PM_PX) )
            return -ENODEV;
        if ( !pmpt || !(pmpt->init & CRUX_PX_INIT) )
            return -EINVAL;
        break;
    }

    switch ( op->cmd )
    {
    case GET_CPUFREQ_PARA:
        ret = get_cpufreq_para(op);
        break;

    case SET_CPUFREQ_GOV:
        ret = set_cpufreq_gov(op);
        break;

    case SET_CPUFREQ_PARA:
        ret = set_cpufreq_para(op);
        break;

    case SET_CPUFREQ_CPPC:
        ret = set_cpufreq_cppc(op);
        break;

    case GET_CPUFREQ_AVGFREQ:
        op->u.get_avgfreq = cpufreq_driver_getavg(op->cpuid, USR_GETAVG);
        break;

    case CRUX_SYSCTL_pm_op_enable_turbo:
        ret = cpufreq_update_turbo(op->cpuid, CPUFREQ_TURBO_ENABLED);
        break;

    case CRUX_SYSCTL_pm_op_disable_turbo:
        ret = cpufreq_update_turbo(op->cpuid, CPUFREQ_TURBO_DISABLED);
        break;

    default:
        printk("not defined sub-hypercall @ do_pm_op\n");
        ret = -ENOSYS;
        break;
    }

    return ret;
}
