/* SPDX-License-Identifier: MIT */
/******************************************************************************
 * dom0_ops.h
 *
 * Process command requests from domain-0 guest OS.
 *
 * Copyright (c) 2002-2003, B Dragovic
 * Copyright (c) 2002-2006, K Fraser
 */

#ifndef __CRUX_PUBLIC_DOM0_OPS_H__
#define __CRUX_PUBLIC_DOM0_OPS_H__

#include "crux.h"
#include "platform.h"

#if __CRUX_INTERFACE_VERSION__ >= 0x00030204
#error "dom0_ops.h is a compatibility interface only"
#endif

#define DOM0_INTERFACE_VERSION CRUXPF_INTERFACE_VERSION

#define DOM0_SETTIME          CRUXPF_settime
#define dom0_settime          cruxpf_settime
#define dom0_settime_t        cruxpf_settime_t

#define DOM0_ADD_MEMTYPE      CRUXPF_add_memtype
#define dom0_add_memtype      cruxpf_add_memtype
#define dom0_add_memtype_t    cruxpf_add_memtype_t

#define DOM0_DEL_MEMTYPE      CRUXPF_del_memtype
#define dom0_del_memtype      cruxpf_del_memtype
#define dom0_del_memtype_t    cruxpf_del_memtype_t

#define DOM0_READ_MEMTYPE     CRUXPF_read_memtype
#define dom0_read_memtype     cruxpf_read_memtype
#define dom0_read_memtype_t   cruxpf_read_memtype_t

#define DOM0_MICROCODE        CRUXPF_microcode_update
#define dom0_microcode        cruxpf_microcode_update
#define dom0_microcode_t      cruxpf_microcode_update_t

#define DOM0_PLATFORM_QUIRK   CRUXPF_platform_quirk
#define dom0_platform_quirk   cruxpf_platform_quirk
#define dom0_platform_quirk_t cruxpf_platform_quirk_t

typedef uint64_t cpumap_t;

/* Unsupported legacy operation -- defined for API compatibility. */
#define DOM0_MSR                 15
struct dom0_msr {
    /* IN variables. */
    uint32_t write;
    cpumap_t cpu_mask;
    uint32_t msr;
    uint32_t in1;
    uint32_t in2;
    /* OUT variables. */
    uint32_t out1;
    uint32_t out2;
};
typedef struct dom0_msr dom0_msr_t;
DEFINE_CRUX_GUEST_HANDLE(dom0_msr_t);

/* Unsupported legacy operation -- defined for API compatibility. */
#define DOM0_PHYSICAL_MEMORY_MAP 40
struct dom0_memory_map_entry {
    uint64_t start, end;
    uint32_t flags; /* reserved */
    uint8_t  is_ram;
};
typedef struct dom0_memory_map_entry dom0_memory_map_entry_t;
DEFINE_CRUX_GUEST_HANDLE(dom0_memory_map_entry_t);

struct dom0_op {
    uint32_t cmd;
    uint32_t interface_version; /* DOM0_INTERFACE_VERSION */
    union {
        struct dom0_msr               msr;
        struct dom0_settime           settime;
        struct dom0_add_memtype       add_memtype;
        struct dom0_del_memtype       del_memtype;
        struct dom0_read_memtype      read_memtype;
        struct dom0_microcode         microcode;
        struct dom0_platform_quirk    platform_quirk;
        struct dom0_memory_map_entry  physical_memory_map;
        uint8_t                       pad[128];
    } u;
};
typedef struct dom0_op dom0_op_t;
DEFINE_CRUX_GUEST_HANDLE(dom0_op_t);

#endif /* __CRUX_PUBLIC_DOM0_OPS_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
