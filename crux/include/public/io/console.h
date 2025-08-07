/* SPDX-License-Identifier: MIT */
/******************************************************************************
 * console.h
 *
 * Console I/O interface for crux guest OSes.
 *
 * Copyright (c) 2005, Keir Fraser
 */

#ifndef __CRUX_PUBLIC_IO_CONSOLE_H__
#define __CRUX_PUBLIC_IO_CONSOLE_H__

typedef uint32_t CRUXCONS_RING_IDX;

#define MASK_CRUXCONS_IDX(idx, ring) ((idx) & (sizeof(ring)-1))

struct cruxcons_interface {
    char in[1024];
    char out[2048];
    CRUXCONS_RING_IDX in_cons, in_prod;
    CRUXCONS_RING_IDX out_cons, out_prod;
};

#ifdef CRUX_WANT_FLEX_CONSOLE_RING
#include "ring.h"
DEFINE_CRUX_FLEX_RING(cruxcons);
#endif

#endif /* __CRUX_PUBLIC_IO_CONSOLE_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
