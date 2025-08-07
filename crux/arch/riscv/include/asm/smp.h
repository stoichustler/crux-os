/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ASM__RISCV__SMP_H
#define ASM__RISCV__SMP_H

#include <crux/cpumask.h>
#include <crux/macros.h>
#include <crux/percpu.h>

#include <asm/current.h>

DECLARE_PER_CPU(cpumask_var_t, cpu_sibling_mask);
DECLARE_PER_CPU(cpumask_var_t, cpu_core_mask);

/*
 * Mapping between Xen logical cpu index and hartid.
 */
static inline unsigned long cpuid_to_hartid(unsigned long cpuid)
{
    return pcpu_info[cpuid].hart_id;
}

static inline unsigned int hartid_to_cpuid(unsigned long hartid)
{
    for ( unsigned int cpu = 0; cpu < ARRAY_SIZE(pcpu_info); cpu++ )
    {
        if ( hartid == cpuid_to_hartid(cpu) )
            return cpu;
    }

    /* hartid isn't valid for some reason */
    return NR_CPUS;
}

static inline void set_cpuid_to_hartid(unsigned long cpuid,
                                       unsigned long hartid)
{
    pcpu_info[cpuid].hart_id = hartid;
}

void setup_tp(unsigned int cpuid);

struct dt_device_node;
int dt_processor_hartid(const struct dt_device_node *node,
                        unsigned long *hartid);

#endif

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
