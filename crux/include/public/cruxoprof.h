/* SPDX-License-Identifier: MIT */
/******************************************************************************
 * cruxoprof.h
 *
 * Interface for enabling system wide profiling based on hardware performance
 * counters
 *
 * Copyright (C) 2005 Hewlett-Packard Co.
 * Written by Aravind Menon & Jose Renato Santos
 */

#ifndef __CRUX_PUBLIC_CRUXOPROF_H__
#define __CRUX_PUBLIC_CRUXOPROF_H__

#include "crux.h"

/*
 * Commands to HYPERVISOR_cruxoprof_op().
 */
#define CRUXOPROF_init                0
#define CRUXOPROF_reset_active_list   1
#define CRUXOPROF_reset_passive_list  2
#define CRUXOPROF_set_active          3
#define CRUXOPROF_set_passive         4
#define CRUXOPROF_reserve_counters    5
#define CRUXOPROF_counter             6
#define CRUXOPROF_setup_events        7
#define CRUXOPROF_enable_virq         8
#define CRUXOPROF_start               9
#define CRUXOPROF_stop               10
#define CRUXOPROF_disable_virq       11
#define CRUXOPROF_release_counters   12
#define CRUXOPROF_shutdown           13
#define CRUXOPROF_get_buffer         14
#define CRUXOPROF_set_backtrace      15

/* AMD IBS support */
#define CRUXOPROF_get_ibs_caps       16
#define CRUXOPROF_ibs_counter        17
#define CRUXOPROF_last_op            17

#define MAX_OPROF_EVENTS    32
#define MAX_OPROF_DOMAINS   25
#define CRUXOPROF_CPU_TYPE_SIZE 64

/* cruxoprof performance events (not crux events) */
struct event_log {
    uint64_t eip;
    uint8_t mode;
    uint8_t event;
};

/* PC value that indicates a special code */
#define CRUXOPROF_ESCAPE_CODE (~crux_mk_ullong(0))
/* Transient events for the cruxoprof->oprofile cpu buf */
#define CRUXOPROF_TRACE_BEGIN 1

/* cruxoprof buffer shared between crux and domain - 1 per VCPU */
struct cruxoprof_buf {
    uint32_t event_head;
    uint32_t event_tail;
    uint32_t event_size;
    uint32_t vcpu_id;
    uint64_t crux_samples;
    uint64_t kernel_samples;
    uint64_t user_samples;
    uint64_t lost_samples;
    struct event_log event_log[1];
};
#ifndef __CRUX__
typedef struct cruxoprof_buf cruxoprof_buf_t;
DEFINE_CRUX_GUEST_HANDLE(cruxoprof_buf_t);
#endif

struct cruxoprof_init {
    int32_t  num_events;
    int32_t  is_primary;
    char cpu_type[CRUXOPROF_CPU_TYPE_SIZE];
};
typedef struct cruxoprof_init cruxoprof_init_t;
DEFINE_CRUX_GUEST_HANDLE(cruxoprof_init_t);

struct cruxoprof_get_buffer {
    int32_t  max_samples;
    int32_t  nbuf;
    int32_t  bufsize;
    uint64_t buf_gmaddr;
};
typedef struct cruxoprof_get_buffer cruxoprof_get_buffer_t;
DEFINE_CRUX_GUEST_HANDLE(cruxoprof_get_buffer_t);

struct cruxoprof_counter {
    uint32_t ind;
    uint64_t count;
    uint32_t enabled;
    uint32_t event;
    uint32_t hypervisor;
    uint32_t kernel;
    uint32_t user;
    uint64_t unit_mask;
};
typedef struct cruxoprof_counter cruxoprof_counter_t;
DEFINE_CRUX_GUEST_HANDLE(cruxoprof_counter_t);

typedef struct cruxoprof_passive {
    uint16_t domain_id;
    int32_t  max_samples;
    int32_t  nbuf;
    int32_t  bufsize;
    uint64_t buf_gmaddr;
} cruxoprof_passive_t;
DEFINE_CRUX_GUEST_HANDLE(cruxoprof_passive_t);

struct cruxoprof_ibs_counter {
    uint64_t op_enabled;
    uint64_t fetch_enabled;
    uint64_t max_cnt_fetch;
    uint64_t max_cnt_op;
    uint64_t rand_en;
    uint64_t dispatched_ops;
};
typedef struct cruxoprof_ibs_counter cruxoprof_ibs_counter_t;
DEFINE_CRUX_GUEST_HANDLE(cruxoprof_ibs_counter_t);

#endif /* __CRUX_PUBLIC_CRUXOPROF_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
