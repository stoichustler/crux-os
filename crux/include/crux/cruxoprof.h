/******************************************************************************
 * cruxoprof.h
 * 
 * cruxoprof: cruxoprof enables performance profiling in crux
 * 
 * Copyright (C) 2005 Hewlett-Packard Co.
 * written by Aravind Menon & Jose Renato Santos
 */

#ifndef __CRUX_CRUXOPROF_H__
#define __CRUX_CRUXOPROF_H__

#define PMU_OWNER_NONE          0
#define PMU_OWNER_CRUXOPROF      1
#define PMU_OWNER_HVM           2

#ifdef CONFIG_CRUXOPROF

#include <crux/stdint.h>
#include <asm/cruxoprof.h>

struct domain;
struct vcpu;
struct cpu_user_regs;

int acquire_pmu_ownership(int pmu_ownership);
void release_pmu_ownership(int pmu_ownership);

int is_active(struct domain *d);
int is_passive(struct domain *d);
void free_cruxoprof_pages(struct domain *d);

int cruxoprof_add_trace(struct vcpu *, uint64_t pc, int mode);

void cruxoprof_log_event(struct vcpu *, const struct cpu_user_regs *,
                        uint64_t pc, int mode, int event);

#else
static inline int acquire_pmu_ownership(int pmu_ownership)
{
    return 1;
}

static inline void release_pmu_ownership(int pmu_ownership)
{
}
#endif /* CONFIG_CRUXOPROF */

#endif  /* __CRUX__CRUXOPROF_H__ */
