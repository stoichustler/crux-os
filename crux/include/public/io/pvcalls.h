/* SPDX-License-Identifier: MIT */
/*
 * pvcalls.h -- crux PV Calls Protocol
 *
 * Refer to docs/misc/pvcalls.markdown for the specification
 *
 * The header is provided as a C reference for the specification. In
 * case of conflict, the specification is authoritative.
 *
 * Copyright (C) 2017 Stefano Stabellini <stefano@aporeto.com>
 */

#ifndef __CRUX_PUBLIC_IO_PVCALLS_H__
#define __CRUX_PUBLIC_IO_PVCALLS_H__

#include "../grant_table.h"
#include "ring.h"

/*
 * See docs/misc/pvcalls.markdown in crux.git for the full specification:
 * https://cruxbits.crux.org/docs/unstable/misc/pvcalls.html
 */
struct pvcalls_data_intf {
    RING_IDX in_cons, in_prod, in_error;

    uint8_t pad1[52];

    RING_IDX out_cons, out_prod, out_error;

    uint8_t pad2[52];

    RING_IDX ring_order;
    grant_ref_t ref[CRUX_FLEX_ARRAY_DIM];
};
DEFINE_CRUX_FLEX_RING(pvcalls);

#define PVCALLS_SOCKET         0
#define PVCALLS_CONNECT        1
#define PVCALLS_RELEASE        2
#define PVCALLS_BIND           3
#define PVCALLS_LISTEN         4
#define PVCALLS_ACCEPT         5
#define PVCALLS_POLL           6

struct crux_pvcalls_request {
    uint32_t req_id; /* private to guest, echoed in response */
    uint32_t cmd;    /* command to execute */
    union {
        struct crux_pvcalls_socket {
            uint64_t id;
            uint32_t domain;
            uint32_t type;
            uint32_t protocol;
            uint8_t pad[4];
        } socket;
        struct crux_pvcalls_connect {
            uint64_t id;
            uint8_t addr[28];
            uint32_t len;
            uint32_t flags;
            grant_ref_t ref;
            uint32_t evtchn;
            uint8_t pad[4];
        } connect;
        struct crux_pvcalls_release {
            uint64_t id;
            uint8_t reuse;
            uint8_t pad[7];
        } release;
        struct crux_pvcalls_bind {
            uint64_t id;
            uint8_t addr[28];
            uint32_t len;
        } bind;
        struct crux_pvcalls_listen {
            uint64_t id;
            uint32_t backlog;
            uint8_t pad[4];
        } listen;
        struct crux_pvcalls_accept {
            uint64_t id;
            uint64_t id_new;
            grant_ref_t ref;
            uint32_t evtchn;
        } accept;
        struct crux_pvcalls_poll {
            uint64_t id;
        } poll;
        /* dummy member to force sizeof(struct crux_pvcalls_request)
         * to match across archs */
        struct crux_pvcalls_dummy {
            uint8_t dummy[56];
        } dummy;
    } u;
};

struct crux_pvcalls_response {
    uint32_t req_id;
    uint32_t cmd;
    int32_t ret;
    uint32_t pad;
    union {
        struct _crux_pvcalls_socket {
            uint64_t id;
        } socket;
        struct _crux_pvcalls_connect {
            uint64_t id;
        } connect;
        struct _crux_pvcalls_release {
            uint64_t id;
        } release;
        struct _crux_pvcalls_bind {
            uint64_t id;
        } bind;
        struct _crux_pvcalls_listen {
            uint64_t id;
        } listen;
        struct _crux_pvcalls_accept {
            uint64_t id;
        } accept;
        struct _crux_pvcalls_poll {
            uint64_t id;
        } poll;
        struct _crux_pvcalls_dummy {
            uint8_t dummy[8];
        } dummy;
    } u;
};

DEFINE_RING_TYPES(crux_pvcalls, struct crux_pvcalls_request,
                  struct crux_pvcalls_response);

#endif

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
