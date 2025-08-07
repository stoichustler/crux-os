/******************************************************************************
 * preempt.h
 * 
 * Track atomic regions in the hypervisor which disallow sleeping.
 * 
 * Copyright (c) 2010, Keir Fraser <keir@crux.org>
 */

#ifndef __CRUX_PREEMPT_H__
#define __CRUX_PREEMPT_H__

#include <crux/types.h>
#include <crux/percpu.h>

DECLARE_PER_CPU(unsigned int, __preempt_count);

#define preempt_count() (this_cpu(__preempt_count))

#define preempt_disable() do {                  \
    preempt_count()++;                          \
    barrier();                                  \
} while (0)

#define preempt_enable() do {                   \
    barrier();                                  \
    preempt_count()--;                          \
} while (0)

bool in_atomic(void);

#ifndef NDEBUG
void ASSERT_NOT_IN_ATOMIC(void);
#else
#define ASSERT_NOT_IN_ATOMIC() ((void)0)
#endif

#endif /* __CRUX_PREEMPT_H__ */
