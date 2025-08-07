/* SPDX-License-Identifier: MIT */
/*
 * fbif.h -- Xen virtual frame buffer device
 *
 * Copyright (C) 2005 Anthony Liguori <aliguori@us.ibm.com>
 * Copyright (C) 2006 Red Hat, Inc., Markus Armbruster <armbru@redhat.com>
 */

#ifndef __CRUX_PUBLIC_IO_FBIF_H__
#define __CRUX_PUBLIC_IO_FBIF_H__

/* Out events (frontend -> backend) */

/*
 * Out events may be sent only when requested by backend, and receipt
 * of an unknown out event is an error.
 */

/* Event type 1 currently not used */
/*
 * Framebuffer update notification event
 * Capable frontend sets feature-update in cruxstore.
 * Backend requests it by setting request-update in cruxstore.
 */
#define CRUXFB_TYPE_UPDATE 2

struct cruxfb_update
{
    uint8_t type;    /* CRUXFB_TYPE_UPDATE */
    int32_t x;      /* source x */
    int32_t y;      /* source y */
    int32_t width;  /* rect width */
    int32_t height; /* rect height */
};

/*
 * Framebuffer resize notification event
 * Capable backend sets feature-resize in cruxstore.
 */
#define CRUXFB_TYPE_RESIZE 3

struct cruxfb_resize
{
    uint8_t type;    /* CRUXFB_TYPE_RESIZE */
    int32_t width;   /* width in pixels */
    int32_t height;  /* height in pixels */
    int32_t stride;  /* stride in bytes */
    int32_t depth;   /* depth in bits */
    int32_t offset;  /* offset of the framebuffer in bytes */
};

#define CRUXFB_OUT_EVENT_SIZE 40

union cruxfb_out_event
{
    uint8_t type;
    struct cruxfb_update update;
    struct cruxfb_resize resize;
    char pad[CRUXFB_OUT_EVENT_SIZE];
};

/* In events (backend -> frontend) */

/*
 * Frontends should ignore unknown in events.
 */

/*
 * Framebuffer refresh period advice
 * Backend sends it to advise the frontend their preferred period of
 * refresh.  Frontends that keep the framebuffer constantly up-to-date
 * just ignore it.  Frontends that use the advice should immediately
 * refresh the framebuffer (and send an update notification event if
 * those have been requested), then use the update frequency to guide
 * their periodical refreshs.
 */
#define CRUXFB_TYPE_REFRESH_PERIOD 1
#define CRUXFB_NO_REFRESH 0

struct cruxfb_refresh_period
{
    uint8_t type;    /* CRUXFB_TYPE_UPDATE_PERIOD */
    uint32_t period; /* period of refresh, in ms,
                      * CRUXFB_NO_REFRESH if no refresh is needed */
};

#define CRUXFB_IN_EVENT_SIZE 40

union cruxfb_in_event
{
    uint8_t type;
    struct cruxfb_refresh_period refresh_period;
    char pad[CRUXFB_IN_EVENT_SIZE];
};

/* shared page */

#define CRUXFB_IN_RING_SIZE 1024
#define CRUXFB_IN_RING_LEN (CRUXFB_IN_RING_SIZE / CRUXFB_IN_EVENT_SIZE)
#define CRUXFB_IN_RING_OFFS 1024
#define CRUXFB_IN_RING(page) \
    ((union cruxfb_in_event *)((char *)(page) + CRUXFB_IN_RING_OFFS))
#define CRUXFB_IN_RING_REF(page, idx) \
    (CRUXFB_IN_RING((page))[(idx) % CRUXFB_IN_RING_LEN])

#define CRUXFB_OUT_RING_SIZE 2048
#define CRUXFB_OUT_RING_LEN (CRUXFB_OUT_RING_SIZE / CRUXFB_OUT_EVENT_SIZE)
#define CRUXFB_OUT_RING_OFFS (CRUXFB_IN_RING_OFFS + CRUXFB_IN_RING_SIZE)
#define CRUXFB_OUT_RING(page) \
    ((union cruxfb_out_event *)((char *)(page) + CRUXFB_OUT_RING_OFFS))
#define CRUXFB_OUT_RING_REF(page, idx) \
    (CRUXFB_OUT_RING((page))[(idx) % CRUXFB_OUT_RING_LEN])

struct cruxfb_page
{
    uint32_t in_cons, in_prod;
    uint32_t out_cons, out_prod;

    int32_t width;          /* the width of the framebuffer (in pixels) */
    int32_t height;         /* the height of the framebuffer (in pixels) */
    uint32_t line_length;   /* the length of a row of pixels (in bytes) */
    uint32_t mem_length;    /* the length of the framebuffer (in bytes) */
    uint8_t depth;          /* the depth of a pixel (in bits) */

    /*
     * Framebuffer page directory
     *
     * Each directory page holds PAGE_SIZE / sizeof(*pd)
     * framebuffer pages, and can thus map up to PAGE_SIZE *
     * PAGE_SIZE / sizeof(*pd) bytes.  With PAGE_SIZE == 4096 and
     * sizeof(unsigned long) == 4/8, that's 4 Megs 32 bit and 2 Megs
     * 64 bit.  256 directories give enough room for a 512 Meg
     * framebuffer with a max resolution of 12,800x10,240.  Should
     * be enough for a while with room leftover for expansion.
     */
    unsigned long pd[256];
};

/*
 * Wart: cruxkbd needs to know default resolution.  Put it here until a
 * better solution is found, but don't leak it to the backend.
 */
#ifdef __KERNEL__
#define CRUXFB_WIDTH 800
#define CRUXFB_HEIGHT 600
#define CRUXFB_DEPTH 32
#endif

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
