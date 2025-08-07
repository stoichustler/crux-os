/* SPDX-License-Identifier: MIT */
/******************************************************************************
 * version.h
 *
 * Xen version, type, and compile information.
 *
 * Copyright (c) 2005, Nguyen Anh Quynh <aquynh@gmail.com>
 * Copyright (c) 2005, Keir Fraser <keir@cruxsource.com>
 */

#ifndef __CRUX_PUBLIC_VERSION_H__
#define __CRUX_PUBLIC_VERSION_H__

#include "crux.h"

/* NB. All ops return zero on success, except CRUXVER_{version,pagesize}
 * CRUXVER_{version,pagesize,build_id} */

/* arg == NULL; returns major:minor (16:16). */
#define CRUXVER_version      0

/*
 * arg == crux_extraversion_t.
 *
 * This API/ABI is broken.  Use CRUXVER_extraversion2 where possible.
 */
#define CRUXVER_extraversion 1
typedef char crux_extraversion_t[16];
#define CRUX_EXTRAVERSION_LEN (sizeof(crux_extraversion_t))

/*
 * arg == crux_compile_info_t.
 *
 * This API/ABI is broken and truncates data.
 */
#define CRUXVER_compile_info 2
struct crux_compile_info {
    char compiler[64];
    char compile_by[16];
    char compile_domain[32];
    char compile_date[32];
};
typedef struct crux_compile_info crux_compile_info_t;

/*
 * arg == crux_capabilities_info_t.
 *
 * This API/ABI is broken.  Use CRUXVER_capabilities2 where possible.
 */
#define CRUXVER_capabilities 3
typedef char crux_capabilities_info_t[1024];
#define CRUX_CAPABILITIES_INFO_LEN (sizeof(crux_capabilities_info_t))

/*
 * arg == crux_changeset_info_t.
 *
 * This API/ABI is broken.  Use CRUXVER_changeset2 where possible.
 */
#define CRUXVER_changeset 4
typedef char crux_changeset_info_t[64];
#define CRUX_CHANGESET_INFO_LEN (sizeof(crux_changeset_info_t))

/*
 * This API is problematic.
 *
 * It is only applicable to guests which share pagetables with Xen (x86 PV
 * guests), but unfortunately has leaked into other guest types and
 * architectures with an expectation of never failing.
 *
 * It is intended to identify the virtual address split between guest kernel
 * and Xen.
 *
 * For 32bit PV guests, there is a split, and it is variable (between two
 * fixed bounds), and this boundary is reported to guests.  The detail missing
 * from the hypercall is that the second boundary is the 32bit architectural
 * boundary at 4G.
 *
 * For 64bit PV guests, Xen lives at the bottom of the upper canonical range.
 * This hypercall happens to report the architectural boundary, not the one
 * which would be necessary to make a variable split work.  As such, this
 * hypercall entirely useless for 64bit PV guests, and all inspected
 * implementations at the time of writing were found to have compile time
 * expectations about the split.
 *
 * For architectures where this hypercall is implemented, for backwards
 * compatibility with the expectation of the hypercall never failing Xen will
 * return 0 instead of failing with -ENOSYS in cases where the guest should
 * not be making the hypercall.
 */
#define CRUXVER_platform_parameters 5
struct crux_platform_parameters {
    crux_ulong_t virt_start;
};
typedef struct crux_platform_parameters crux_platform_parameters_t;

#define CRUXVER_get_features 6
struct crux_feature_info {
    uint32_t     submap_idx;    /* IN: which 32-bit submap to return */
    uint32_t     submap;        /* OUT: 32-bit submap */
};
typedef struct crux_feature_info crux_feature_info_t;

/* Declares the features reported by CRUXVER_get_features. */
#include "features.h"

/* arg == NULL; returns host memory page size. */
#define CRUXVER_pagesize 7

/* arg == crux_domain_handle_t.
 *
 * The toolstack fills it out for guest consumption. It is intended to hold
 * the UUID of the guest.
 */
#define CRUXVER_guest_handle 8

/*
 * arg == crux_commandline_t.
 *
 * This API/ABI is broken.  Use CRUXVER_commandline2 where possible.
 */
#define CRUXVER_commandline 9
typedef char crux_commandline_t[1024];

/*
 * Return value is the number of bytes written, or CRUX_Exx on error.
 * Calling with empty parameter returns the size of build_id.
 *
 * Note: structure only kept for backwards compatibility.  Xen operates in
 * terms of crux_varbuf_t.
 */
struct crux_build_id {
        uint32_t        len; /* IN: size of buf[]. */
        unsigned char   buf[CRUX_FLEX_ARRAY_DIM];
                             /* OUT: Variable length buffer with build_id. */
};
typedef struct crux_build_id crux_build_id_t;

/*
 * Container for an arbitrary variable length buffer.
 */
struct crux_varbuf {
    uint32_t len;                          /* IN:  size of buf[] in bytes. */
    unsigned char buf[CRUX_FLEX_ARRAY_DIM]; /* OUT: requested data.         */
};
typedef struct crux_varbuf crux_varbuf_t;

/*
 * arg == crux_varbuf_t
 *
 * Equivalent to the original ops, but with a non-truncating API/ABI.
 *
 * These hypercalls can fail for a number of reasons.  All callers must handle
 * -CRUX_xxx return values appropriately.
 *
 * Passing arg == NULL is a request for size, which will be signalled with a
 * non-negative return value.  Note: a return size of 0 may be legitimate for
 * the requested subop.
 *
 * Otherwise, the input crux_varbuf_t provides the size of the following
 * buffer.  Xen will fill the buffer, and return the number of bytes written
 * (e.g. if the input buffer was longer than necessary).
 *
 * Some subops may return binary data.  Some subops may be expected to return
 * textural data.  These are returned without a NUL terminator, and while the
 * contents is expected to be ASCII/UTF-8, Xen makes no guarentees to this
 * effect.  e.g. Xen has no control over the formatting used for the command
 * line.
 */
#define CRUXVER_build_id      10
#define CRUXVER_extraversion2 11
#define CRUXVER_capabilities2 12
#define CRUXVER_changeset2    13
#define CRUXVER_commandline2  14

#endif /* __CRUX_PUBLIC_VERSION_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
