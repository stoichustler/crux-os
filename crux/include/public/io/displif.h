/* SPDX-License-Identifier: MIT */
/******************************************************************************
 * displif.h
 *
 * Unified display device I/O interface for crux guest OSes.
 *
 * Copyright (C) 2016-2017 EPAM Systems Inc.
 *
 * Authors: Oleksandr Andrushchenko <oleksandr_andrushchenko@epam.com>
 *          Oleksandr Grytsov <oleksandr_grytsov@epam.com>
 */

#ifndef __CRUX_PUBLIC_IO_DISPLIF_H__
#define __CRUX_PUBLIC_IO_DISPLIF_H__

#include "ring.h"
#include "../grant_table.h"

/*
 ******************************************************************************
 *                           Protocol version
 ******************************************************************************
 */
#define CRUXDISPL_PROTOCOL_VERSION     "2"
#define CRUXDISPL_PROTOCOL_VERSION_INT  2

/*
 ******************************************************************************
 *                  Main features provided by the protocol
 ******************************************************************************
 * This protocol aims to provide a unified protocol which fits more
 * sophisticated use-cases than a framebuffer device can handle. At the
 * moment basic functionality is supported with the intention to be extended:
 *  o multiple dynamically allocated/destroyed framebuffers
 *  o buffers of arbitrary sizes
 *  o buffer allocation at either back or front end
 *  o better configuration options including multiple display support
 *
 * Note: existing fbif can be used together with displif running at the
 * same time, e.g. on Linux one provides framebuffer and another DRM/KMS
 *
 * Note: display resolution (cruxStore's "resolution" property) defines
 * visible area of the virtual display. At the same time resolution of
 * the display and frame buffers may differ: buffers can be smaller, equal
 * or bigger than the visible area. This is to enable use-cases, where backend
 * may do some post-processing of the display and frame buffers supplied,
 * e.g. those buffers can be just a part of the final composition.
 *
 ******************************************************************************
 *                        Direction of improvements
 ******************************************************************************
 * Future extensions to the existing protocol may include:
 *  o display/connector cloning
 *  o allocation of objects other than display buffers
 *  o plane/overlay support
 *  o scaling support
 *  o rotation support
 *
 ******************************************************************************
 *                  Feature and Parameter Negotiation
 ******************************************************************************
 *
 * Front->back notifications: when enqueuing a new request, sending a
 * notification can be made conditional on cruxdispl_req (i.e., the generic
 * hold-off mechanism provided by the ring macros). Backends must set
 * cruxdispl_req appropriately (e.g., using RING_FINAL_CHECK_FOR_REQUESTS()).
 *
 * Back->front notifications: when enqueuing a new response, sending a
 * notification can be made conditional on cruxdispl_resp (i.e., the generic
 * hold-off mechanism provided by the ring macros). Frontends must set
 * cruxdispl_resp appropriately (e.g., using RING_FINAL_CHECK_FOR_RESPONSES()).
 *
 * The two halves of a para-virtual display driver utilize nodes within
 * cruxStore to communicate capabilities and to negotiate operating parameters.
 * This section enumerates these nodes which reside in the respective front and
 * backend portions of cruxStore, following the cruxBus convention.
 *
 * All data in cruxStore is stored as strings. Nodes specifying numeric
 * values are encoded in decimal. Integer value ranges listed below are
 * expressed as fixed sized integer types capable of storing the conversion
 * of a properly formated node string, without loss of information.
 *
 ******************************************************************************
 *                        Example configuration
 ******************************************************************************
 *
 * Note: depending on the use-case backend can expose more display connectors
 * than the underlying HW physically has by employing SW graphics compositors
 *
 * This is an example of backend and frontend configuration:
 *
 *--------------------------------- Backend -----------------------------------
 *
 * /local/domain/0/backend/vdispl/1/0/frontend-id = "1"
 * /local/domain/0/backend/vdispl/1/0/frontend = "/local/domain/1/device/vdispl/0"
 * /local/domain/0/backend/vdispl/1/0/state = "4"
 * /local/domain/0/backend/vdispl/1/0/versions = "1,2"
 *
 *--------------------------------- Frontend ----------------------------------
 *
 * /local/domain/1/device/vdispl/0/backend-id = "0"
 * /local/domain/1/device/vdispl/0/backend = "/local/domain/0/backend/vdispl/1/0"
 * /local/domain/1/device/vdispl/0/state = "4"
 * /local/domain/1/device/vdispl/0/version = "1"
 * /local/domain/1/device/vdispl/0/be-alloc = "1"
 *
 *-------------------------- Connector 0 configuration ------------------------
 *
 * /local/domain/1/device/vdispl/0/0/resolution = "1920x1080"
 * /local/domain/1/device/vdispl/0/0/req-ring-ref = "2832"
 * /local/domain/1/device/vdispl/0/0/req-event-channel = "15"
 * /local/domain/1/device/vdispl/0/0/evt-ring-ref = "387"
 * /local/domain/1/device/vdispl/0/0/evt-event-channel = "16"
 *
 *-------------------------- Connector 1 configuration ------------------------
 *
 * /local/domain/1/device/vdispl/0/1/resolution = "800x600"
 * /local/domain/1/device/vdispl/0/1/req-ring-ref = "2833"
 * /local/domain/1/device/vdispl/0/1/req-event-channel = "17"
 * /local/domain/1/device/vdispl/0/1/evt-ring-ref = "388"
 * /local/domain/1/device/vdispl/0/1/evt-event-channel = "18"
 *
 ******************************************************************************
 *                            Backend cruxBus Nodes
 ******************************************************************************
 *
 *----------------------------- Protocol version ------------------------------
 *
 * versions
 *      Values:         <string>
 *
 *      List of CRUXDISPL_LIST_SEPARATOR separated protocol versions supported
 *      by the backend. For example "1,2,3".
 *
 ******************************************************************************
 *                            Frontend cruxBus Nodes
 ******************************************************************************
 *
 *-------------------------------- Addressing ---------------------------------
 *
 * dom-id
 *      Values:         <uint16_t>
 *
 *      domain identifier.
 *
 * dev-id
 *      Values:         <uint16_t>
 *
 *      Device identifier.
 *
 * conn-idx
 *      Values:         <uint8_t>
 *
 *      Zero based contigous index of the connector.
 *      /local/domain/<dom-id>/device/vdispl/<dev-id>/<conn-idx>/...
 *
 *----------------------------- Protocol version ------------------------------
 *
 * version
 *      Values:         <string>
 *
 *      Protocol version, chosen among the ones supported by the backend.
 *
 *------------------------- Backend buffer allocation -------------------------
 *
 * be-alloc
 *      Values:         "0", "1"
 *
 *      If value is set to "1", then backend can be a buffer provider/allocator
 *      for this domain during CRUXDISPL_OP_DBUF_CREATE operation (see below
 *      for negotiation).
 *      If value is not "1" or omitted frontend must allocate buffers itself.
 *
 *----------------------------- Connector settings ----------------------------
 *
 * unique-id
 *      Values:         <string>
 *
 *      After device instance initialization each connector is assigned a
 *      unique ID, so it can be identified by the backend by this ID.
 *      This can be UUID or such.
 *
 * resolution
 *      Values:         <width, uint32_t>x<height, uint32_t>
 *
 *      Width and height of the connector in pixels separated by
 *      CRUXDISPL_RESOLUTION_SEPARATOR. This defines visible area of the
 *      display.
 *      If backend provides extended display identification data (EDID) with
 *      CRUXDISPL_OP_GET_EDID request then EDID values must take precedence
 *      over the resolutions defined here.
 *
 *------------------ Connector Request Transport Parameters -------------------
 *
 * This communication path is used to deliver requests from frontend to backend
 * and get the corresponding responses from backend to frontend,
 * set up per connector.
 *
 * req-event-channel
 *      Values:         <uint32_t>
 *
 *      The identifier of the crux connector's control event channel
 *      used to signal activity in the ring buffer.
 *
 * req-ring-ref
 *      Values:         <uint32_t>
 *
 *      The crux grant reference granting permission for the backend to map
 *      a sole page of connector's control ring buffer.
 *
 *------------------- Connector Event Transport Parameters --------------------
 *
 * This communication path is used to deliver asynchronous events from backend
 * to frontend, set up per connector.
 *
 * evt-event-channel
 *      Values:         <uint32_t>
 *
 *      The identifier of the crux connector's event channel
 *      used to signal activity in the ring buffer.
 *
 * evt-ring-ref
 *      Values:         <uint32_t>
 *
 *      The crux grant reference granting permission for the backend to map
 *      a sole page of connector's event ring buffer.
 */

/*
 ******************************************************************************
 *                               STATE DIAGRAMS
 ******************************************************************************
 *
 * Tool stack creates front and back state nodes with initial state
 * cruxbusStateInitialising.
 * Tool stack creates and sets up frontend display configuration
 * nodes per domain.
 *
 *-------------------------------- Normal flow --------------------------------
 *
 * Front                                Back
 * =================================    =====================================
 * cruxbusStateInitialising              cruxbusStateInitialising
 *                                       o Query backend device identification
 *                                         data.
 *                                       o Open and validate backend device.
 *                                                |
 *                                                |
 *                                                V
 *                                      cruxbusStateInitWait
 *
 * o Query frontend configuration
 * o Allocate and initialize
 *   event channels per configured
 *   connector.
 * o Publish transport parameters
 *   that will be in effect during
 *   this connection.
 *              |
 *              |
 *              V
 * cruxbusStateInitialised
 *
 *                                       o Query frontend transport parameters.
 *                                       o Connect to the event channels.
 *                                                |
 *                                                |
 *                                                V
 *                                      cruxbusStateConnected
 *
 *  o Create and initialize OS
 *    virtual display connectors
 *    as per configuration.
 *              |
 *              |
 *              V
 * cruxbusStateConnected
 *
 *                                      cruxbusStateUnknown
 *                                      cruxbusStateClosed
 *                                      cruxbusStateClosing
 * o Remove virtual display device
 * o Remove event channels
 *              |
 *              |
 *              V
 * cruxbusStateClosed
 *
 *------------------------------- Recovery flow -------------------------------
 *
 * In case of frontend unrecoverable errors backend handles that as
 * if frontend goes into the cruxbusStateClosed state.
 *
 * In case of backend unrecoverable errors frontend tries removing
 * the virtualized device. If this is possible at the moment of error,
 * then frontend goes into the cruxbusStateInitialising state and is ready for
 * new connection with backend. If the virtualized device is still in use and
 * cannot be removed, then frontend goes into the cruxbusStateReconfiguring state
 * until either the virtualized device is removed or backend initiates a new
 * connection. On the virtualized device removal frontend goes into the
 * cruxbusStateInitialising state.
 *
 * Note on cruxbusStateReconfiguring state of the frontend: if backend has
 * unrecoverable errors then frontend cannot send requests to the backend
 * and thus cannot provide functionality of the virtualized device anymore.
 * After backend is back to normal the virtualized device may still hold some
 * state: configuration in use, allocated buffers, client application state etc.
 * In most cases, this will require frontend to implement complex recovery
 * reconnect logic. Instead, by going into cruxbusStateReconfiguring state,
 * frontend will make sure no new clients of the virtualized device are
 * accepted, allow existing client(s) to exit gracefully by signaling error
 * state etc.
 * Once all the clients are gone frontend can reinitialize the virtualized
 * device and get into cruxbusStateInitialising state again signaling the
 * backend that a new connection can be made.
 *
 * There are multiple conditions possible under which frontend will go from
 * cruxbusStateReconfiguring into cruxbusStateInitialising, some of them are OS
 * specific. For example:
 * 1. The underlying OS framework may provide callbacks to signal that the last
 *    client of the virtualized device has gone and the device can be removed
 * 2. Frontend can schedule a deferred work (timer/tasklet/workqueue)
 *    to periodically check if this is the right time to re-try removal of
 *    the virtualized device.
 * 3. By any other means.
 *
 ******************************************************************************
 *                             REQUEST CODES
 ******************************************************************************
 * Request codes [0; 15] are reserved and must not be used
 */

#define CRUXDISPL_OP_DBUF_CREATE       0x10
#define CRUXDISPL_OP_DBUF_DESTROY      0x11
#define CRUXDISPL_OP_FB_ATTACH         0x12
#define CRUXDISPL_OP_FB_DETACH         0x13
#define CRUXDISPL_OP_SET_CONFIG        0x14
#define CRUXDISPL_OP_PG_FLIP           0x15
/* The below command is available in protocol version 2 and above. */
#define CRUXDISPL_OP_GET_EDID          0x16

/*
 ******************************************************************************
 *                                 EVENT CODES
 ******************************************************************************
 */
#define CRUXDISPL_EVT_PG_FLIP          0x00

/*
 ******************************************************************************
 *               CRUXSTORE FIELD AND PATH NAME STRINGS, HELPERS
 ******************************************************************************
 */
#define CRUXDISPL_DRIVER_NAME          "vdispl"

#define CRUXDISPL_LIST_SEPARATOR       ","
#define CRUXDISPL_RESOLUTION_SEPARATOR "x"

#define CRUXDISPL_FIELD_BE_VERSIONS    "versions"
#define CRUXDISPL_FIELD_FE_VERSION     "version"
#define CRUXDISPL_FIELD_REQ_RING_REF   "req-ring-ref"
#define CRUXDISPL_FIELD_REQ_CHANNEL    "req-event-channel"
#define CRUXDISPL_FIELD_EVT_RING_REF   "evt-ring-ref"
#define CRUXDISPL_FIELD_EVT_CHANNEL    "evt-event-channel"
#define CRUXDISPL_FIELD_RESOLUTION     "resolution"
#define CRUXDISPL_FIELD_BE_ALLOC       "be-alloc"
#define CRUXDISPL_FIELD_UNIQUE_ID      "unique-id"

#define CRUXDISPL_EDID_BLOCK_SIZE      128
#define CRUXDISPL_EDID_BLOCK_COUNT     256
#define CRUXDISPL_EDID_MAX_SIZE        (CRUXDISPL_EDID_BLOCK_SIZE * CRUXDISPL_EDID_BLOCK_COUNT)

/*
 ******************************************************************************
 *                          STATUS RETURN CODES
 ******************************************************************************
 *
 * Status return code is zero on success and -CRUX_EXX on failure.
 *
 ******************************************************************************
 *                              Assumptions
 ******************************************************************************
 * o usage of grant reference 0 as invalid grant reference:
 *   grant reference 0 is valid, but never exposed to a PV driver,
 *   because of the fact it is already in use/reserved by the PV console.
 * o all references in this document to page sizes must be treated
 *   as pages of size CRUX_PAGE_SIZE unless otherwise noted.
 *
 ******************************************************************************
 *       Description of the protocol between frontend and backend driver
 ******************************************************************************
 *
 * The two halves of a Para-virtual display driver communicate with
 * each other using shared pages and event channels.
 * Shared page contains a ring with request/response packets.
 *
 * All reserved fields in the structures below must be 0.
 * Display buffers's cookie of value 0 is treated as invalid.
 * Framebuffer's cookie of value 0 is treated as invalid.
 *
 * For all request/response/event packets that use cookies:
 *   dbuf_cookie - uint64_t, unique to guest domain value used by the backend
 *     to map remote display buffer to its local one
 *   fb_cookie - uint64_t, unique to guest domain value used by the backend
 *     to map remote framebuffer to its local one
 *
 *---------------------------------- Requests ---------------------------------
 *
 * All requests/responses, which are not connector specific, must be sent over
 * control ring of the connector which has the index value of 0:
 *   /local/domain/<dom-id>/device/vdispl/<dev-id>/0/req-ring-ref
 *
 * All request packets have the same length (64 octets)
 * All request packets have common header:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                |    operation   |   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 *   id - uint16_t, private guest value, echoed in response
 *   operation - uint8_t, operation code, CRUXDISPL_OP_???
 *
 * Request dbuf creation - request creation of a display buffer.
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                |_OP_DBUF_CREATE |   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 * |                       dbuf_cookie low 32-bit                      | 12
 * +----------------+----------------+----------------+----------------+
 * |                       dbuf_cookie high 32-bit                     | 16
 * +----------------+----------------+----------------+----------------+
 * |                               width                               | 20
 * +----------------+----------------+----------------+----------------+
 * |                               height                              | 24
 * +----------------+----------------+----------------+----------------+
 * |                                bpp                                | 28
 * +----------------+----------------+----------------+----------------+
 * |                             buffer_sz                             | 32
 * +----------------+----------------+----------------+----------------+
 * |                               flags                               | 36
 * +----------------+----------------+----------------+----------------+
 * |                           gref_directory                          | 40
 * +----------------+----------------+----------------+----------------+
 * |                             data_ofs                              | 44
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 48
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * Must be sent over control ring of the connector which has the index
 * value of 0:
 *   /local/domain/<dom-id>/device/vdispl/<dev-id>/0/req-ring-ref
 * All unused bits in flags field must be set to 0.
 *
 * An attempt to create multiple display buffers with the same dbuf_cookie is
 * an error. dbuf_cookie can be re-used after destroying the corresponding
 * display buffer.
 *
 * Width and height of the display buffers can be smaller, equal or bigger
 * than the connector's resolution. Depth/pixel format of the individual
 * buffers can differ as well.
 *
 * width - uint32_t, width in pixels
 * height - uint32_t, height in pixels
 * bpp - uint32_t, bits per pixel
 * buffer_sz - uint32_t, buffer size to be allocated, octets
 * flags - uint32_t, flags of the operation
 *   o CRUXDISPL_DBUF_FLG_REQ_ALLOC - if set, then backend is requested
 *     to allocate the buffer with the parameters provided in this request.
 *     Page directory is handled as follows:
 *       Frontend on request:
 *         o allocates pages for the directory (gref_directory,
 *           gref_dir_next_page(s)
 *         o grants permissions for the pages of the directory to the backend
 *         o sets gref_dir_next_page fields
 *       Backend on response:
 *         o grants permissions for the pages of the buffer allocated to
 *           the frontend
 *         o fills in page directory with grant references
 *           (gref[] in struct cruxdispl_page_directory)
 * gref_directory - grant_ref_t, a reference to the first shared page
 *   describing shared buffer references. At least one page exists. If shared
 *   buffer size (buffer_sz) exceeds what can be addressed by this single page,
 *   then reference to the next page must be supplied (see gref_dir_next_page
 *   below)
 * data_ofs - uint32_t, offset of the data in the buffer, octets
 */

#define CRUXDISPL_DBUF_FLG_REQ_ALLOC       (1 << 0)

struct cruxdispl_dbuf_create_req {
    uint64_t dbuf_cookie;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t buffer_sz;
    uint32_t flags;
    grant_ref_t gref_directory;
    uint32_t data_ofs;
};

/*
 * Shared page for CRUXDISPL_OP_DBUF_CREATE buffer descriptor (gref_directory in
 * the request) employs a list of pages, describing all pages of the shared
 * data buffer:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |                        gref_dir_next_page                         | 4
 * +----------------+----------------+----------------+----------------+
 * |                              gref[0]                              | 8
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                              gref[i]                              | i*4+8
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             gref[N - 1]                           | N*4+8
 * +----------------+----------------+----------------+----------------+
 *
 * gref_dir_next_page - grant_ref_t, reference to the next page describing
 *   page directory. Must be 0 if there are no more pages in the list.
 * gref[i] - grant_ref_t, reference to a shared page of the buffer
 *   allocated at CRUXDISPL_OP_DBUF_CREATE
 *
 * Number of grant_ref_t entries in the whole page directory is not
 * passed, but instead can be calculated as:
 *   num_grefs_total = (CRUXDISPL_OP_DBUF_CREATE.buffer_sz + CRUX_PAGE_SIZE - 1) /
 *       CRUX_PAGE_SIZE
 */

struct cruxdispl_page_directory {
    grant_ref_t gref_dir_next_page;
    grant_ref_t gref[CRUXPV_FLEX_ARRAY_DIM];
};

/*
 * Request dbuf destruction - destroy a previously allocated display buffer:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                |_OP_DBUF_DESTROY|   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 * |                       dbuf_cookie low 32-bit                      | 12
 * +----------------+----------------+----------------+----------------+
 * |                       dbuf_cookie high 32-bit                     | 16
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 20
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * Must be sent over control ring of the connector which has the index
 * value of 0:
 *   /local/domain/<dom-id>/device/vdispl/<dev-id>/0/req-ring-ref
 */

struct cruxdispl_dbuf_destroy_req {
    uint64_t dbuf_cookie;
};

/*
 * Request framebuffer attachment - request attachment of a framebuffer to
 * previously created display buffer.
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                | _OP_FB_ATTACH  |   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 * |                       dbuf_cookie low 32-bit                      | 12
 * +----------------+----------------+----------------+----------------+
 * |                       dbuf_cookie high 32-bit                     | 16
 * +----------------+----------------+----------------+----------------+
 * |                        fb_cookie low 32-bit                       | 20
 * +----------------+----------------+----------------+----------------+
 * |                        fb_cookie high 32-bit                      | 24
 * +----------------+----------------+----------------+----------------+
 * |                               width                               | 28
 * +----------------+----------------+----------------+----------------+
 * |                               height                              | 32
 * +----------------+----------------+----------------+----------------+
 * |                            pixel_format                           | 36
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 40
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * Must be sent over control ring of the connector which has the index
 * value of 0:
 *   /local/domain/<dom-id>/device/vdispl/<dev-id>/0/req-ring-ref
 * Width and height can be smaller, equal or bigger than the connector's
 * resolution.
 *
 * An attempt to create multiple frame buffers with the same fb_cookie is
 * an error. fb_cookie can be re-used after destroying the corresponding
 * frame buffer.
 *
 * width - uint32_t, width in pixels
 * height - uint32_t, height in pixels
 * pixel_format - uint32_t, pixel format of the framebuffer, FOURCC code
 */

struct cruxdispl_fb_attach_req {
    uint64_t dbuf_cookie;
    uint64_t fb_cookie;
    uint32_t width;
    uint32_t height;
    uint32_t pixel_format;
};

/*
 * Request framebuffer detach - detach a previously
 * attached framebuffer from the display buffer in request:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                |  _OP_FB_DETACH |   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 * |                        fb_cookie low 32-bit                       | 12
 * +----------------+----------------+----------------+----------------+
 * |                        fb_cookie high 32-bit                      | 16
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 20
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * Must be sent over control ring of the connector which has the index
 * value of 0:
 *   /local/domain/<dom-id>/device/vdispl/<dev-id>/0/req-ring-ref
 */

struct cruxdispl_fb_detach_req {
    uint64_t fb_cookie;
};

/*
 * Request configuration set/reset - request to set or reset
 * the configuration/mode of the display:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                | _OP_SET_CONFIG |   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 * |                        fb_cookie low 32-bit                       | 12
 * +----------------+----------------+----------------+----------------+
 * |                        fb_cookie high 32-bit                      | 16
 * +----------------+----------------+----------------+----------------+
 * |                                 x                                 | 20
 * +----------------+----------------+----------------+----------------+
 * |                                 y                                 | 24
 * +----------------+----------------+----------------+----------------+
 * |                               width                               | 28
 * +----------------+----------------+----------------+----------------+
 * |                               height                              | 32
 * +----------------+----------------+----------------+----------------+
 * |                                bpp                                | 40
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 44
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * Pass all zeros to reset, otherwise command is treated as
 * configuration set.
 * Framebuffer's cookie defines which framebuffer/dbuf must be
 * displayed while enabling display (applying configuration).
 * x, y, width and height are bound by the connector's resolution and must not
 * exceed it.
 *
 * x - uint32_t, starting position in pixels by X axis
 * y - uint32_t, starting position in pixels by Y axis
 * width - uint32_t, width in pixels
 * height - uint32_t, height in pixels
 * bpp - uint32_t, bits per pixel
 */

struct cruxdispl_set_config_req {
    uint64_t fb_cookie;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
};

/*
 * Request page flip - request to flip a page identified by the framebuffer
 * cookie:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                | _OP_PG_FLIP    |   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 * |                        fb_cookie low 32-bit                       | 12
 * +----------------+----------------+----------------+----------------+
 * |                        fb_cookie high 32-bit                      | 16
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 20
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 */

struct cruxdispl_page_flip_req {
    uint64_t fb_cookie;
};

/*
 * Request EDID - request EDID describing current connector:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                | _OP_GET_EDID   |   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             buffer_sz                             | 8
 * +----------------+----------------+----------------+----------------+
 * |                          gref_directory                           | 12
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 16
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * Notes:
 *   - This command is not available in protocol version 1 and should be
 *     ignored.
 *   - This request is optional and if not supported then visible area
 *     is defined by the relevant cruxStore's "resolution" property.
 *   - Shared buffer, allocated for EDID storage, must not be less then
 *     CRUXDISPL_EDID_MAX_SIZE octets.
 *
 * buffer_sz - uint32_t, buffer size to be allocated, octets
 * gref_directory - grant_ref_t, a reference to the first shared page
 *   describing EDID buffer references. See CRUXDISPL_OP_DBUF_CREATE for
 *   grant page directory structure (struct cruxdispl_page_directory).
 *
 * See response format for this request.
 */

struct cruxdispl_get_edid_req {
    uint32_t buffer_sz;
    grant_ref_t gref_directory;
};

/*
 *---------------------------------- Responses --------------------------------
 *
 * All response packets have the same length (64 octets)
 *
 * All response packets have common header:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                |            reserved             | 4
 * +----------------+----------------+----------------+----------------+
 * |                              status                               | 8
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 12
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * id - uint16_t, private guest value, echoed from request
 * status - int32_t, response status, zero on success and -CRUX_EXX on failure
 *
 *
 * Get EDID response - response for CRUXDISPL_OP_GET_EDID:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                |    operation   |    reserved    | 4
 * +----------------+----------------+----------------+----------------+
 * |                              status                               | 8
 * +----------------+----------------+----------------+----------------+
 * |                             edid_sz                               | 12
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 16
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 *
 * Notes:
 *   - This response is not available in protocol version 1 and should be
 *     ignored.
 *
 * edid_sz - uint32_t, size of the EDID, octets
 */

struct cruxdispl_get_edid_resp {
    uint32_t edid_sz;
};

/*
 *----------------------------------- Events ----------------------------------
 *
 * Events are sent via a shared page allocated by the front and propagated by
 *   evt-event-channel/evt-ring-ref cruxStore entries
 * All event packets have the same length (64 octets)
 * All event packets have common header:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                |      type      |   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 *
 * id - uint16_t, event id, may be used by front
 * type - uint8_t, type of the event
 *
 *
 * Page flip complete event - event from back to front on page flip completed:
 *         0                1                 2               3        octet
 * +----------------+----------------+----------------+----------------+
 * |               id                |   _EVT_PG_FLIP |   reserved     | 4
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 8
 * +----------------+----------------+----------------+----------------+
 * |                        fb_cookie low 32-bit                       | 12
 * +----------------+----------------+----------------+----------------+
 * |                        fb_cookie high 32-bit                      | 16
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 20
 * +----------------+----------------+----------------+----------------+
 * |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
 * +----------------+----------------+----------------+----------------+
 * |                             reserved                              | 64
 * +----------------+----------------+----------------+----------------+
 */

struct cruxdispl_pg_flip_evt {
    uint64_t fb_cookie;
};

struct cruxdispl_req {
    uint16_t id;
    uint8_t operation;
    uint8_t reserved[5];
    union {
        struct cruxdispl_dbuf_create_req dbuf_create;
        struct cruxdispl_dbuf_destroy_req dbuf_destroy;
        struct cruxdispl_fb_attach_req fb_attach;
        struct cruxdispl_fb_detach_req fb_detach;
        struct cruxdispl_set_config_req set_config;
        struct cruxdispl_page_flip_req pg_flip;
        struct cruxdispl_get_edid_req get_edid;
        uint8_t reserved[56];
    } op;
};

struct cruxdispl_resp {
    uint16_t id;
    uint8_t operation;
    uint8_t reserved;
    int32_t status;
    union {
        struct cruxdispl_get_edid_resp get_edid;
        uint8_t reserved1[56];
    } op;
};

struct cruxdispl_evt {
    uint16_t id;
    uint8_t type;
    uint8_t reserved[5];
    union {
        struct cruxdispl_pg_flip_evt pg_flip;
        uint8_t reserved[56];
    } op;
};

DEFINE_RING_TYPES(crux_displif, struct cruxdispl_req, struct cruxdispl_resp);

/*
 ******************************************************************************
 *                        Back to front events delivery
 ******************************************************************************
 * In order to deliver asynchronous events from back to front a shared page is
 * allocated by front and its granted reference propagated to back via
 * cruxStore entries (evt-ring-ref/evt-event-channel).
 * This page has a common header used by both front and back to synchronize
 * access and control event's ring buffer, while back being a producer of the
 * events and front being a consumer. The rest of the page after the header
 * is used for event packets.
 *
 * Upon reception of an event(s) front may confirm its reception
 * for either each event, group of events or none.
 */

struct cruxdispl_event_page {
    uint32_t in_cons;
    uint32_t in_prod;
    uint8_t reserved[56];
};

#define CRUXDISPL_EVENT_PAGE_SIZE 4096
#define CRUXDISPL_IN_RING_OFFS (sizeof(struct cruxdispl_event_page))
#define CRUXDISPL_IN_RING_SIZE (CRUXDISPL_EVENT_PAGE_SIZE - CRUXDISPL_IN_RING_OFFS)
#define CRUXDISPL_IN_RING_LEN (CRUXDISPL_IN_RING_SIZE / sizeof(struct cruxdispl_evt))
#define CRUXDISPL_IN_RING(page) \
	((struct cruxdispl_evt *)((char *)(page) + CRUXDISPL_IN_RING_OFFS))
#define CRUXDISPL_IN_RING_REF(page, idx) \
	(CRUXDISPL_IN_RING((page))[(idx) % CRUXDISPL_IN_RING_LEN])

#endif /* __CRUX_PUBLIC_IO_DISPLIF_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
