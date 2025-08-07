/*
 *  utility.c - misc functions for cpufreq driver and Px statistic
 *
 *  Copyright (C) 2001 Russell King
 *            (C) 2002 - 2003 Dominik Brodowski <linux@brodo.de>
 *
 *  Oct 2005 - Ashok Raj <ashok.raj@intel.com>
 *    Added handling for CPU hotplug
 *  Feb 2006 - Jacob Shin <jacob.shin@amd.com>
 *    Fix handling for CPU hotplug -- affected CPUs
 *  Feb 2008 - Liu Jinsong <jinsong.liu@intel.com>
 *    1. Merge cpufreq.c and freq_table.c of linux 2.6.23
 *    And poring to Xen hypervisor
 *    2. some Px statistic interface funcdtions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <crux/errno.h>
#include <crux/cpumask.h>
#include <crux/types.h>
#include <crux/spinlock.h>
#include <crux/percpu.h>
#include <crux/types.h>
#include <crux/sched.h>
#include <crux/timer.h>
#include <crux/trace.h>
#include <acpi/cpufreq/cpufreq.h>
#include <public/sysctl.h>

struct cpufreq_driver __read_mostly cpufreq_driver;
struct processor_pminfo *__read_mostly processor_pminfo[NR_CPUS];
DEFINE_PER_CPU_READ_MOSTLY(struct cpufreq_policy *, cpufreq_cpu_policy);

/*********************************************************************
 *                   FREQUENCY TABLE HELPERS                         *
 *********************************************************************/

int cpufreq_frequency_table_cpuinfo(struct cpufreq_policy *policy,
                                    struct cpufreq_frequency_table *table)
{
    unsigned int min_freq = ~0;
    unsigned int max_freq = 0;
    unsigned int second_max_freq = 0;
    unsigned int i;

    for (i=0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
        unsigned int freq = table[i].frequency;
        if (freq == CPUFREQ_ENTRY_INVALID)
            continue;
        if (freq < min_freq)
            min_freq = freq;
        if (freq > max_freq)
            max_freq = freq;
    }
    for (i=0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
        unsigned int freq = table[i].frequency;
        if (freq == CPUFREQ_ENTRY_INVALID || freq == max_freq)
            continue;
        if (freq > second_max_freq)
            second_max_freq = freq;
    }
    if (second_max_freq == 0)
        second_max_freq = max_freq;
    if (cpufreq_verbose)
        printk("max_freq: %u    second_max_freq: %u\n",
               max_freq, second_max_freq);

    policy->min = policy->cpuinfo.min_freq = min_freq;
    policy->max = policy->cpuinfo.max_freq = max_freq;
    policy->cpuinfo.perf_freq = max_freq;
    policy->cpuinfo.second_max_freq = second_max_freq;

    if (policy->min == ~0)
        return -EINVAL;
    else
        return 0;
}

int cpufreq_frequency_table_verify(struct cpufreq_policy *policy,
                                   struct cpufreq_frequency_table *table)
{
    unsigned int next_larger = ~0;
    unsigned int i;
    unsigned int count = 0;

    if (!cpu_online(policy->cpu))
        return -EINVAL;

    cpufreq_verify_within_limits(policy, policy->cpuinfo.min_freq,
                                 policy->cpuinfo.max_freq);

    for (i=0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
        unsigned int freq = table[i].frequency;
        if (freq == CPUFREQ_ENTRY_INVALID)
            continue;
        if ((freq >= policy->min) && (freq <= policy->max))
            count++;
        else if ((next_larger > freq) && (freq > policy->max))
            next_larger = freq;
    }

    if (!count)
        policy->max = next_larger;

    cpufreq_verify_within_limits(policy, policy->cpuinfo.min_freq,
                                 policy->cpuinfo.max_freq);

    return 0;
}

int cpufreq_frequency_table_target(struct cpufreq_policy *policy,
                                   struct cpufreq_frequency_table *table,
                                   unsigned int target_freq,
                                   unsigned int relation,
                                   unsigned int *index)
{
    struct cpufreq_frequency_table optimal = {
        .index = ~0,
        .frequency = 0,
    };
    struct cpufreq_frequency_table suboptimal = {
        .index = ~0,
        .frequency = 0,
    };
    unsigned int i;

    switch (relation) {
    case CPUFREQ_RELATION_H:
        suboptimal.frequency = ~0;
        break;
    case CPUFREQ_RELATION_L:
        optimal.frequency = ~0;
        break;
    }

    if (!cpu_online(policy->cpu))
        return -EINVAL;

    for (i=0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
        unsigned int freq = table[i].frequency;
        if (freq == CPUFREQ_ENTRY_INVALID)
            continue;
        if ((freq < policy->min) || (freq > policy->max))
            continue;
        switch(relation) {
        case CPUFREQ_RELATION_H:
            if (freq <= target_freq) {
                if (freq >= optimal.frequency) {
                    optimal.frequency = freq;
                    optimal.index = i;
                }
            } else {
                if (freq <= suboptimal.frequency) {
                    suboptimal.frequency = freq;
                    suboptimal.index = i;
                }
            }
            break;
        case CPUFREQ_RELATION_L:
            if (freq >= target_freq) {
                if (freq <= optimal.frequency) {
                    optimal.frequency = freq;
                    optimal.index = i;
                }
            } else {
                if (freq >= suboptimal.frequency) {
                    suboptimal.frequency = freq;
                    suboptimal.index = i;
                }
            }
            break;
        }
    }
    if (optimal.index > i) {
        if (suboptimal.index > i)
            return -EINVAL;
        *index = suboptimal.index;
    } else
        *index = optimal.index;

    return 0;
}


/*********************************************************************
 *               GOVERNORS                                           *
 *********************************************************************/

int __cpufreq_driver_target(struct cpufreq_policy *policy,
                            unsigned int target_freq,
                            unsigned int relation)
{
    int retval = -EINVAL;

    if (cpu_online(policy->cpu) && cpufreq_driver.target)
    {
        unsigned int prev_freq = policy->cur;

        retval = alternative_call(cpufreq_driver.target,
                                  policy, target_freq, relation);
        if ( retval == 0 )
            TRACE_TIME(TRC_PM_FREQ_CHANGE, prev_freq / 1000, policy->cur / 1000);
    }

    return retval;
}

int cpufreq_driver_getavg(unsigned int cpu, unsigned int flag)
{
    struct cpufreq_policy *policy;
    int freq_avg;

    if (!cpu_online(cpu) || !(policy = per_cpu(cpufreq_cpu_policy, cpu)))
        return 0;

    freq_avg = get_measured_perf(cpu, flag);
    if ( freq_avg > 0 )
        return freq_avg;

    return policy->cur;
}

/*********************************************************************
 *                 POLICY                                            *
 *********************************************************************/

/*
 * data   : current policy.
 * policy : policy to be set.
 */
int __cpufreq_set_policy(struct cpufreq_policy *data,
                                struct cpufreq_policy *policy)
{
    int ret = 0;

    memcpy(&policy->cpuinfo, &data->cpuinfo, sizeof(struct cpufreq_cpuinfo));

    if (policy->min > data->min && policy->min > policy->max)
        return -EINVAL;

    /* verify the cpu speed can be set within this limit */
    ret = alternative_call(cpufreq_driver.verify, policy);
    if (ret)
        return ret;

    data->min = policy->min;
    data->max = policy->max;
    data->limits = policy->limits;
    if (cpufreq_driver.setpolicy)
        return alternative_call(cpufreq_driver.setpolicy, data);

    if (policy->governor != data->governor) {
        /* save old, working values */
        struct cpufreq_governor *old_gov = data->governor;

        /* end old governor */
        if (data->governor)
            __cpufreq_governor(data, CPUFREQ_GOV_STOP);

        /* start new governor */
        data->governor = policy->governor;
        if (__cpufreq_governor(data, CPUFREQ_GOV_START)) {
            printk(KERN_WARNING "Fail change to %s governor\n",
                                 data->governor->name);

            /* new governor failed, so re-start old one */
            data->governor = old_gov;
            if (old_gov) {
                __cpufreq_governor(data, CPUFREQ_GOV_START);
                printk(KERN_WARNING "Still stay at %s governor\n",
                                     data->governor->name);
            }
            return -EINVAL;
        }
        /* might be a policy change, too, so fall through */
    }

    return __cpufreq_governor(data, CPUFREQ_GOV_LIMITS);
}
