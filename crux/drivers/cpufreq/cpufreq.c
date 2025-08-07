/*
 *  Copyright (C) 2001, 2002 Andy Grover <andrew.grover@intel.com>
 *  Copyright (C) 2001, 2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *  Copyright (C) 2002 - 2004 Dominik Brodowski <linux@brodo.de>
 *  Copyright (C) 2006        Denis Sadykov <denis.m.sadykov@intel.com>
 *
 *  Feb 2008 - Liu Jinsong <jinsong.liu@intel.com>
 *      Add cpufreq limit change handle and per-cpu cpufreq add/del
 *      to cope with cpu hotplug
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; If not, see <http://www.gnu.org/licenses/>.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <crux/types.h>
#include <crux/errno.h>
#include <crux/delay.h>
#include <crux/cpumask.h>
#include <crux/list.h>
#include <crux/param.h>
#include <crux/sched.h>
#include <crux/string.h>
#include <crux/timer.h>
#include <crux/xmalloc.h>
#include <crux/guest_access.h>
#include <crux/domain.h>
#include <crux/cpu.h>
#include <crux/pmstat.h>
#include <asm/io.h>
#include <asm/processor.h>

#include <acpi/acpi.h>
#include <acpi/cpufreq/cpufreq.h>

static unsigned int __read_mostly usr_min_freq;
static unsigned int __read_mostly usr_max_freq;
static void cpufreq_cmdline_common_para(struct cpufreq_policy *new_policy);

struct cpufreq_dom {
    unsigned int	dom;
    cpumask_var_t	map;
    struct list_head	node;
};
static LIST_HEAD_READ_MOSTLY(cpufreq_dom_list_head);

bool __initdata cpufreq_governor_internal;
struct cpufreq_governor *__read_mostly cpufreq_opt_governor;
LIST_HEAD_READ_MOSTLY(cpufreq_governor_list);

/* set crux as default cpufreq */
enum cpufreq_controller cpufreq_controller = FREQCTL_crux;

enum cpufreq_crux_opt __initdata cpufreq_crux_opts[2] = { CPUFREQ_crux,
                                                        CPUFREQ_none };
unsigned int __initdata cpufreq_crux_cnt = 1;

static int __init cpufreq_cmdline_parse(const char *s, const char *e);

static int __init cf_check setup_cpufreq_option(const char *str)
{
    const char *arg = strpbrk(str, ",:;");
    int choice;
    int ret = -EINVAL;

    cpufreq_crux_cnt = 0;

    if ( !arg )
        arg = strchr(str, '\0');
    choice = parse_bool(str, arg);

    if ( choice < 0 && !cmdline_strcmp(str, "dom0-kernel") )
    {
        crux_processor_pmbits &= ~CRUX_PROCESSOR_PM_PX;
        cpufreq_controller = FREQCTL_dom0_kernel;
        opt_dom0_vcpus_pin = 1;
        return 0;
    }

    if ( choice == 0 || !cmdline_strcmp(str, "none") )
    {
        crux_processor_pmbits &= ~CRUX_PROCESSOR_PM_PX;
        cpufreq_controller = FREQCTL_none;
        return 0;
    }

    do
    {
        const char *end = strchr(str, ';');

        if ( end == NULL )
            end = strchr(str, '\0');

        arg = strpbrk(str, ",:");
        if ( !arg || arg > end )
            arg = strchr(str, '\0');

        if ( cpufreq_crux_cnt == ARRAY_SIZE(cpufreq_crux_opts) )
            return -E2BIG;

        if ( choice > 0 || !cmdline_strcmp(str, "crux") )
        {
            crux_processor_pmbits |= CRUX_PROCESSOR_PM_PX;
            cpufreq_controller = FREQCTL_crux;
            cpufreq_crux_opts[cpufreq_crux_cnt++] = CPUFREQ_crux;
            ret = 0;
            if ( arg[0] && arg[1] )
                ret = cpufreq_cmdline_parse(arg + 1, end);
        }
        else if ( IS_ENABLED(CONFIG_INTEL) && choice < 0 &&
                  !cmdline_strcmp(str, "hwp") )
        {
            crux_processor_pmbits |= CRUX_PROCESSOR_PM_PX;
            cpufreq_controller = FREQCTL_crux;
            cpufreq_crux_opts[cpufreq_crux_cnt++] = CPUFREQ_hwp;
            ret = 0;
            if ( arg[0] && arg[1] )
                ret = hwp_cmdline_parse(arg + 1, end);
        }
        else
            ret = -EINVAL;

        str = *end ? ++end : end;
    } while ( choice < 0 && ret == 0 && *str );

    return (choice < 0) ? ret : 0;
}
custom_param("cpufreq", setup_cpufreq_option);

bool __read_mostly cpufreq_verbose;

struct cpufreq_governor *__find_governor(const char *governor)
{
    struct cpufreq_governor *t;

    if (!governor)
        return NULL;

    list_for_each_entry(t, &cpufreq_governor_list, governor_list)
        if (!strncasecmp(governor, t->name, CPUFREQ_NAME_LEN))
            return t;

    return NULL;
}

int __init cpufreq_register_governor(struct cpufreq_governor *governor)
{
    if (!governor)
        return -EINVAL;

    if (__find_governor(governor->name) != NULL)
        return -EEXIST;

    list_add(&governor->governor_list, &cpufreq_governor_list);
    return 0;
}

int cpufreq_limit_change(unsigned int cpu)
{
    struct processor_performance *perf;
    struct cpufreq_policy *data;
    struct cpufreq_policy policy;

    if (!cpu_online(cpu) || !(data = per_cpu(cpufreq_cpu_policy, cpu)) ||
        !processor_pminfo[cpu])
        return -ENODEV;

    perf = &processor_pminfo[cpu]->perf;

    if (perf->platform_limit >= perf->state_count)
        return -EINVAL;

    memcpy(&policy, data, sizeof(struct cpufreq_policy)); 

    policy.max =
        perf->states[perf->platform_limit].core_frequency * 1000;

    return __cpufreq_set_policy(data, &policy);
}

static int get_psd_info(unsigned int cpu, unsigned int *shared_type,
                        const struct crux_psd_package **domain_info)
{
    int ret = 0;

    switch ( processor_pminfo[cpu]->init )
    {
    case CRUX_PX_INIT:
        *shared_type = processor_pminfo[cpu]->perf.shared_type;
        *domain_info = &processor_pminfo[cpu]->perf.domain_info;
        break;

    default:
        ret = -EINVAL;
        break;
    }

    return ret;
}

int cpufreq_add_cpu(unsigned int cpu)
{
    int ret;
    unsigned int firstcpu;
    unsigned int dom, domexist = 0;
    unsigned int hw_all = 0;
    struct list_head *pos;
    struct cpufreq_dom *cpufreq_dom = NULL;
    struct cpufreq_policy new_policy;
    struct cpufreq_policy *policy;
    const struct crux_psd_package *domain_info;
    unsigned int shared_type;

    /* to protect the case when Px was not controlled by crux */
    if ( !processor_pminfo[cpu] || !cpu_online(cpu) )
        return -EINVAL;

    if ( !(processor_pminfo[cpu]->init & CRUX_PX_INIT) )
        return -EINVAL;

    if (!cpufreq_driver.init)
        return 0;

    if (per_cpu(cpufreq_cpu_policy, cpu))
        return 0;

    ret = get_psd_info(cpu, &shared_type, &domain_info);
    if ( ret )
        return ret;

    if ( shared_type == CPUFREQ_SHARED_TYPE_HW )
        hw_all = 1;

    dom = domain_info->domain;

    list_for_each(pos, &cpufreq_dom_list_head) {
        cpufreq_dom = list_entry(pos, struct cpufreq_dom, node);
        if (dom == cpufreq_dom->dom) {
            domexist = 1;
            break;
        }
    }

    if (!domexist) {
        cpufreq_dom = xzalloc(struct cpufreq_dom);
        if (!cpufreq_dom)
            return -ENOMEM;

        if (!zalloc_cpumask_var(&cpufreq_dom->map)) {
            xfree(cpufreq_dom);
            return -ENOMEM;
        }

        cpufreq_dom->dom = dom;
        list_add(&cpufreq_dom->node, &cpufreq_dom_list_head);
    } else {
        unsigned int firstcpu_shared_type;
        const struct crux_psd_package *firstcpu_domain_info;

        /* domain sanity check under whatever coordination type */
        firstcpu = cpumask_first(cpufreq_dom->map);
        ret = get_psd_info(firstcpu, &firstcpu_shared_type,
                           &firstcpu_domain_info);
        if ( ret )
            return ret;

        if ( domain_info->coord_type != firstcpu_domain_info->coord_type ||
             domain_info->num_processors !=
             firstcpu_domain_info->num_processors )
        {
            printk(KERN_WARNING "cpufreq fail to add CPU%d:"
                   "incorrect _PSD(%"PRIu64":%"PRIu64"), "
                   "expect(%"PRIu64"/%"PRIu64")\n",
                   cpu, domain_info->coord_type,
                   domain_info->num_processors,
                   firstcpu_domain_info->coord_type,
                   firstcpu_domain_info->num_processors);
            return -EINVAL;
        }
    }

    if (!domexist || hw_all) {
        policy = xzalloc(struct cpufreq_policy);
        if (!policy) {
            ret = -ENOMEM;
            goto err0;
        }

        if (!zalloc_cpumask_var(&policy->cpus)) {
            xfree(policy);
            ret = -ENOMEM;
            goto err0;
        }

        policy->cpu = cpu;
        per_cpu(cpufreq_cpu_policy, cpu) = policy;

        ret = alternative_call(cpufreq_driver.init, policy);
        if (ret) {
            free_cpumask_var(policy->cpus);
            xfree(policy);
            per_cpu(cpufreq_cpu_policy, cpu) = NULL;
            goto err0;
        }
        if (cpufreq_verbose)
            printk("CPU %u initialization completed\n", cpu);
    } else {
        firstcpu = cpumask_first(cpufreq_dom->map);
        policy = per_cpu(cpufreq_cpu_policy, firstcpu);

        per_cpu(cpufreq_cpu_policy, cpu) = policy;
        if (cpufreq_verbose)
            printk("adding CPU %u\n", cpu);
    }

    cpumask_set_cpu(cpu, policy->cpus);
    cpumask_set_cpu(cpu, cpufreq_dom->map);

    ret = cpufreq_statistic_init(cpu);
    if (ret)
        goto err1;

    if ( hw_all || cpumask_weight(cpufreq_dom->map) ==
                   domain_info->num_processors )
    {
        memcpy(&new_policy, policy, sizeof(struct cpufreq_policy));

        /*
         * Only when cpufreq_driver.setpolicy == NULL, we need to deliberately
         * set old gov as NULL to trigger the according gov starting.
         */
        if ( cpufreq_driver.setpolicy == NULL )
            policy->governor = NULL;

        cpufreq_cmdline_common_para(&new_policy);

        ret = __cpufreq_set_policy(policy, &new_policy);
        if (ret) {
            if (new_policy.governor == CPUFREQ_DEFAULT_GOVERNOR)
                /* if default governor fail, cpufreq really meet troubles */
                goto err2;
            else {
                /* grub option governor fail */
                /* give one more chance to default gov */
                memcpy(&new_policy, policy, sizeof(struct cpufreq_policy));
                new_policy.governor = CPUFREQ_DEFAULT_GOVERNOR;
                ret = __cpufreq_set_policy(policy, &new_policy);
                if (ret)
                    goto err2;
            }
        }
    }

    return 0;

err2:
    cpufreq_statistic_exit(cpu);
err1:
    per_cpu(cpufreq_cpu_policy, cpu) = NULL;
    cpumask_clear_cpu(cpu, policy->cpus);
    cpumask_clear_cpu(cpu, cpufreq_dom->map);

    if (cpumask_empty(policy->cpus)) {
        alternative_call(cpufreq_driver.exit, policy);
        free_cpumask_var(policy->cpus);
        xfree(policy);
    }
err0:
    if (cpumask_empty(cpufreq_dom->map)) {
        list_del(&cpufreq_dom->node);
        free_cpumask_var(cpufreq_dom->map);
        xfree(cpufreq_dom);
    }

    return ret;
}

int cpufreq_del_cpu(unsigned int cpu)
{
    int ret;
    unsigned int dom, domexist = 0;
    unsigned int hw_all = 0;
    struct list_head *pos;
    struct cpufreq_dom *cpufreq_dom = NULL;
    struct cpufreq_policy *policy;
    unsigned int shared_type;
    const struct crux_psd_package *domain_info;

    /* to protect the case when Px was not controlled by crux */
    if ( !processor_pminfo[cpu] || !cpu_online(cpu) )
        return -EINVAL;

    if ( !(processor_pminfo[cpu]->init & CRUX_PX_INIT) )
        return -EINVAL;

    if (!per_cpu(cpufreq_cpu_policy, cpu))
        return 0;

    ret = get_psd_info(cpu, &shared_type, &domain_info);
    if ( ret )
        return ret;

    if ( shared_type == CPUFREQ_SHARED_TYPE_HW )
        hw_all = 1;

    dom = domain_info->domain;
    policy = per_cpu(cpufreq_cpu_policy, cpu);

    list_for_each(pos, &cpufreq_dom_list_head) {
        cpufreq_dom = list_entry(pos, struct cpufreq_dom, node);
        if (dom == cpufreq_dom->dom) {
            domexist = 1;
            break;
        }
    }

    if (!domexist)
        return -EINVAL;

    /* for HW_ALL, stop gov for each core of the _PSD domain */
    /* for SW_ALL & SW_ANY, stop gov for the 1st core of the _PSD domain */
    if ( hw_all || cpumask_weight(cpufreq_dom->map) ==
                   domain_info->num_processors )
        __cpufreq_governor(policy, CPUFREQ_GOV_STOP);

    cpufreq_statistic_exit(cpu);
    per_cpu(cpufreq_cpu_policy, cpu) = NULL;
    cpumask_clear_cpu(cpu, policy->cpus);
    cpumask_clear_cpu(cpu, cpufreq_dom->map);

    if (cpumask_empty(policy->cpus)) {
        alternative_call(cpufreq_driver.exit, policy);
        free_cpumask_var(policy->cpus);
        xfree(policy);
    }

    /* for the last cpu of the domain, clean room */
    /* It's safe here to free freq_table, drv_data and policy */
    if (cpumask_empty(cpufreq_dom->map)) {
        list_del(&cpufreq_dom->node);
        free_cpumask_var(cpufreq_dom->map);
        xfree(cpufreq_dom);
    }

    if (cpufreq_verbose)
        printk("deleting CPU %u\n", cpu);
    return 0;
}

static void print_PCT(struct crux_pct_register *ptr)
{
    printk("\t_PCT: descriptor=%d, length=%d, space_id=%d, "
           "bit_width=%d, bit_offset=%d, reserved=%d, address=%"PRId64"\n",
           ptr->descriptor, ptr->length, ptr->space_id, ptr->bit_width,
           ptr->bit_offset, ptr->reserved, ptr->address);
}

static void print_PSS(struct crux_processor_px *ptr, int count)
{
    int i;
    printk("\t_PSS: state_count=%d\n", count);
    for (i=0; i<count; i++){
        printk("\tState%d: %"PRId64"MHz %"PRId64"mW %"PRId64"us "
               "%"PRId64"us %#"PRIx64" %#"PRIx64"\n",
               i,
               ptr[i].core_frequency,
               ptr[i].power,
               ptr[i].transition_latency,
               ptr[i].bus_master_latency,
               ptr[i].control,
               ptr[i].status);
    }
}

static void print_PSD( struct crux_psd_package *ptr)
{
    printk("\t_PSD: num_entries=%"PRId64" rev=%"PRId64
           " domain=%"PRId64" coord_type=%"PRId64" num_processors=%"PRId64"\n",
           ptr->num_entries, ptr->revision, ptr->domain, ptr->coord_type,
           ptr->num_processors);
}

static void print_PPC(unsigned int platform_limit)
{
    printk("\t_PPC: %d\n", platform_limit);
}

static bool check_psd_pminfo(unsigned int shared_type)
{
    /* Check domain coordination */
    if ( shared_type != CPUFREQ_SHARED_TYPE_ALL &&
         shared_type != CPUFREQ_SHARED_TYPE_ANY &&
         shared_type != CPUFREQ_SHARED_TYPE_HW )
        return false;

    return true;
}

int set_px_pminfo(uint32_t acpi_id, struct crux_processor_performance *perf)
{
    int ret = 0, cpu;
    struct processor_pminfo *pmpt;
    struct processor_performance *pxpt;

    cpu = get_cpu_id(acpi_id);
    if ( cpu < 0 || !perf )
    {
        ret = -EINVAL;
        goto out;
    }
    if ( cpufreq_verbose )
        printk("Set CPU acpi_id(%d) cpu(%d) Px State info:\n",
               acpi_id, cpu);

    pmpt = processor_pminfo[cpu];
    if ( !pmpt )
    {
        pmpt = xzalloc(struct processor_pminfo);
        if ( !pmpt )
        {
            ret = -ENOMEM;
            goto out;
        }
        processor_pminfo[cpu] = pmpt;
    }
    pxpt = &pmpt->perf;
    pmpt->acpi_id = acpi_id;
    pmpt->id = cpu;

    if ( perf->flags & CRUX_PX_PCT )
    {
        /* space_id check */
        if ( perf->control_register.space_id !=
             perf->status_register.space_id )
        {
            ret = -EINVAL;
            goto out;
        }

        memcpy(&pxpt->control_register, &perf->control_register,
               sizeof(struct crux_pct_register));
        memcpy(&pxpt->status_register, &perf->status_register,
               sizeof(struct crux_pct_register));

        if ( cpufreq_verbose )
        {
            print_PCT(&pxpt->control_register);
            print_PCT(&pxpt->status_register);
        }
    }

    if ( perf->flags & CRUX_PX_PSS && !pxpt->states )
    {
        /* capability check */
        if ( perf->state_count <= 1 )
        {
            ret = -EINVAL;
            goto out;
        }

        if ( !(pxpt->states = xmalloc_array(struct crux_processor_px,
                                            perf->state_count)) )
        {
            ret = -ENOMEM;
            goto out;
        }
        if ( copy_from_guest(pxpt->states, perf->states, perf->state_count) )
        {
            XFREE(pxpt->states);
            ret = -EFAULT;
            goto out;
        }
        pxpt->state_count = perf->state_count;

        if ( cpufreq_verbose )
            print_PSS(pxpt->states,pxpt->state_count);
    }

    if ( perf->flags & CRUX_PX_PSD )
    {
        if ( !check_psd_pminfo(perf->shared_type) )
        {
            ret = -EINVAL;
            goto out;
        }

        pxpt->shared_type = perf->shared_type;
        memcpy(&pxpt->domain_info, &perf->domain_info,
               sizeof(struct crux_psd_package));

        if ( cpufreq_verbose )
            print_PSD(&pxpt->domain_info);
    }

    if ( perf->flags & CRUX_PX_PPC )
    {
        pxpt->platform_limit = perf->platform_limit;

        if ( cpufreq_verbose )
            print_PPC(pxpt->platform_limit);

        if ( pmpt->init == CRUX_PX_INIT )
        {
            ret = cpufreq_limit_change(cpu);
            goto out;
        }
    }

    if ( perf->flags == ( CRUX_PX_PCT | CRUX_PX_PSS | CRUX_PX_PSD | CRUX_PX_PPC ) )
    {
        pmpt->init = CRUX_PX_INIT;

        ret = cpufreq_cpu_init(cpu);
        goto out;
    }

out:
    return ret;
}

int acpi_set_pdc_bits(unsigned int acpi_id, CRUX_GUEST_HANDLE(uint32) pdc)
{
    uint32_t bits[3];
    int ret;

    if ( copy_from_guest(bits, pdc, 2) )
        ret = -EFAULT;
    else if ( bits[0] != ACPI_PDC_REVISION_ID || !bits[1] )
        ret = -EINVAL;
    else if ( copy_from_guest_offset(bits + 2, pdc, 2, 1) )
        ret = -EFAULT;
    else
    {
        uint32_t mask = 0;

        if ( crux_processor_pmbits & CRUX_PROCESSOR_PM_CX )
            mask |= ACPI_PDC_C_MASK | ACPI_PDC_SMP_C1PT;
        if ( crux_processor_pmbits & CRUX_PROCESSOR_PM_PX )
            mask |= ACPI_PDC_P_MASK | ACPI_PDC_SMP_C1PT;
        if ( crux_processor_pmbits & CRUX_PROCESSOR_PM_TX )
            mask |= ACPI_PDC_T_MASK | ACPI_PDC_SMP_C1PT;
        bits[2] &= (ACPI_PDC_C_MASK | ACPI_PDC_P_MASK | ACPI_PDC_T_MASK |
                    ACPI_PDC_SMP_C1PT) & ~mask;
        ret = arch_acpi_set_pdc_bits(acpi_id, bits, mask);
    }
    if ( !ret && __copy_to_guest_offset(pdc, 2, bits + 2, 1) )
        ret = -EFAULT;

    return ret;
}

static void cpufreq_cmdline_common_para(struct cpufreq_policy *new_policy)
{
    if (usr_max_freq)
        new_policy->max = usr_max_freq;
    if (usr_min_freq)
        new_policy->min = usr_min_freq;
}

static int __init cpufreq_handle_common_option(const char *name, const char *val)
{
    if (!strcmp(name, "maxfreq") && val) {
        usr_max_freq = simple_strtoul(val, NULL, 0);
        return 1;
    }

    if (!strcmp(name, "minfreq") && val) {
        usr_min_freq = simple_strtoul(val, NULL, 0);
        return 1;
    }

    if (!strcmp(name, "verbose")) {
        cpufreq_verbose = !val || !!simple_strtoul(val, NULL, 0);
        return 1;
    }

    return 0;
}

static int __init cpufreq_cmdline_parse(const char *s, const char *e)
{
    static struct cpufreq_governor *__initdata cpufreq_governors[] =
    {
        CPUFREQ_DEFAULT_GOVERNOR,
        &cpufreq_gov_userspace,
        &cpufreq_gov_dbs,
        &cpufreq_gov_performance,
        &cpufreq_gov_powersave
    };
    static char __initdata buf[128];
    char *str = buf;
    unsigned int gov_index = 0;
    int rc = 0;

    strlcpy(buf, s, sizeof(buf));
    if (e - s < sizeof(buf))
        buf[e - s] = '\0';
    do {
        char *val, *end = strchr(str, ',');
        unsigned int i;

        if (end)
            *end++ = '\0';
        val = strchr(str, '=');
        if (val)
            *val++ = '\0';

        if (!cpufreq_opt_governor) {
            if (!val) {
                for (i = 0; i < ARRAY_SIZE(cpufreq_governors); ++i) {
                    if (!strcmp(str, cpufreq_governors[i]->name)) {
                        cpufreq_opt_governor = cpufreq_governors[i];
                        gov_index = i;
                        str = NULL;
                        break;
                    }
                }
            } else {
                cpufreq_opt_governor = CPUFREQ_DEFAULT_GOVERNOR;
            }
        }

        if (str && !cpufreq_handle_common_option(str, val) &&
            (!cpufreq_governors[gov_index]->handle_option ||
             !cpufreq_governors[gov_index]->handle_option(str, val)))
        {
            printk(CRUXLOG_WARNING "cpufreq/%s: option '%s' not recognized\n",
                   cpufreq_governors[gov_index]->name, str);
            rc = -EINVAL;
        }

        str = end;
    } while (str);

    return rc;
}

static int cf_check cpu_callback(
    struct notifier_block *nfb, unsigned long action, void *hcpu)
{
    unsigned int cpu = (unsigned long)hcpu;

    switch ( action )
    {
    case CPU_DOWN_FAILED:
    case CPU_ONLINE:
        (void)cpufreq_add_cpu(cpu);
        break;
    case CPU_DOWN_PREPARE:
        (void)cpufreq_del_cpu(cpu);
        break;
    default:
        break;
    }

    return NOTIFY_DONE;
}

static struct notifier_block cpu_nfb = {
    .notifier_call = cpu_callback
};

static int __init cf_check cpufreq_presmp_init(void)
{
    register_cpu_notifier(&cpu_nfb);
    return 0;
}
presmp_initcall(cpufreq_presmp_init);

int __init cpufreq_register_driver(const struct cpufreq_driver *driver_data)
{
   if ( !driver_data || !driver_data->init ||
        !driver_data->verify || !driver_data->exit ||
        (!driver_data->target == !driver_data->setpolicy) )
        return -EINVAL;

    if ( cpufreq_driver.init )
        return -EBUSY;

    cpufreq_driver = *driver_data;

    return 0;
}
