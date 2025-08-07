/* SPDX-License-Identifier: MIT */
/*
 * Details of the "wire" protocol between crux Store Daemon and client
 * library or guest kernel.
 *
 * Copyright (C) 2005 Rusty Russell IBM Corporation
 */

#ifndef _XS_WIRE_H
#define _XS_WIRE_H

enum xsd_sockmsg_type
{
    XS_CONTROL,
#define XS_DEBUG XS_CONTROL
    XS_DIRECTORY,
    XS_READ,
    XS_GET_PERMS,
    XS_WATCH,
    XS_UNWATCH,
    XS_TRANSACTION_START,
    XS_TRANSACTION_END,
    XS_INTRODUCE,
    XS_RELEASE,
    XS_GET_DOMAIN_PATH,
    XS_WRITE,
    XS_MKDIR,
    XS_RM,
    XS_SET_PERMS,
    XS_WATCH_EVENT,
    XS_ERROR,
    XS_IS_DOMAIN_INTRODUCED,
    XS_RESUME,
    XS_SET_TARGET,
    /* XS_RESTRICT has been removed */
    XS_RESET_WATCHES = XS_SET_TARGET + 2,
    XS_DIRECTORY_PART,
    XS_GET_FEATURE,
    XS_SET_FEATURE,
    XS_GET_QUOTA,
    XS_SET_QUOTA,

    XS_TYPE_COUNT,      /* Number of valid types. */

    XS_INVALID = 0xffff /* Guaranteed to remain an invalid type */
};

/* We hand errors as strings, for portability. */
struct xsd_errors
{
    int errnum;
    const char *errstring;
};
#ifdef EINVAL
#define XSD_ERROR(x) { x, #x }
/* LINTED: static unused */
static const struct xsd_errors xsd_errors[]
#if defined(__GNUC__)
__attribute__((unused))
#endif
    = {
    /* /!\ New errors should be added at the end of the array. */
    XSD_ERROR(EINVAL),
    XSD_ERROR(EACCES),
    XSD_ERROR(EEXIST),
    XSD_ERROR(EISDIR),
    XSD_ERROR(ENOENT),
    XSD_ERROR(ENOMEM),
    XSD_ERROR(ENOSPC),
    XSD_ERROR(EIO),
    XSD_ERROR(ENOTEMPTY),
    XSD_ERROR(ENOSYS),
    XSD_ERROR(EROFS),
    XSD_ERROR(EBUSY),
    XSD_ERROR(EAGAIN),
    XSD_ERROR(EISCONN),
    XSD_ERROR(E2BIG),
    XSD_ERROR(EPERM),
};
#endif

struct xsd_sockmsg
{
    uint32_t type;  /* XS_??? */
    uint32_t req_id;/* Request identifier, echoed in daemon's response.  */
    uint32_t tx_id; /* Transaction id (0 if not related to a transaction). */
    uint32_t len;   /* Length of data following this. */

    /* Generally followed by nul-terminated string(s). */
};

enum xs_watch_type
{
    XS_WATCH_PATH = 0,
    XS_WATCH_TOKEN
};

/*
 * `incontents 150 cruxstore_struct cruxStore wire protocol.
 *
 * Inter-domain shared memory communications. */
#define CRUXSTORE_RING_SIZE 1024
typedef uint32_t CRUXSTORE_RING_IDX;
#define MASK_CRUXSTORE_IDX(idx) ((idx) & (CRUXSTORE_RING_SIZE-1))
struct cruxstore_domain_interface {
    char req[CRUXSTORE_RING_SIZE]; /* Requests to cruxstore daemon. */
    char rsp[CRUXSTORE_RING_SIZE]; /* Replies and async watch events. */
    CRUXSTORE_RING_IDX req_cons, req_prod;
    CRUXSTORE_RING_IDX rsp_cons, rsp_prod;
    uint32_t server_features; /* Bitmap of features supported by the server */
    uint32_t connection;
    uint32_t error;
    uint32_t evtchn_port;
};

/* Violating this is very bad.  See docs/misc/cruxstore.txt. */
#define CRUXSTORE_PAYLOAD_MAX 4096

/* Violating these just gets you an error back */
#define CRUXSTORE_ABS_PATH_MAX 3072
#define CRUXSTORE_REL_PATH_MAX 2048

/* The ability to reconnect a ring */
#define CRUXSTORE_SERVER_FEATURE_RECONNECTION 1
/* The presence of the "error" field in the ring page */
#define CRUXSTORE_SERVER_FEATURE_ERROR        2

/* Valid values for the connection field */
#define CRUXSTORE_CONNECTED 0 /* the steady-state */
#define CRUXSTORE_RECONNECT 1 /* reconnect in progress */

/* Valid values for the error field */
#define CRUXSTORE_ERROR_NONE    0 /* No error */
#define CRUXSTORE_ERROR_COMM    1 /* Communication problem */
#define CRUXSTORE_ERROR_RINGIDX 2 /* Invalid ring index */
#define CRUXSTORE_ERROR_PROTO   3 /* Protocol violation (payload too long) */

/*
 * The evtchn_port field is the domain's event channel for cruxstored to signal.
 * It is filled in by crux for dom0less/Hyperlaunch domains.  It is only used
 * when non-zero.  Otherwise the event channel from XS_INTRODUCE is used.
 */

#endif /* _XS_WIRE_H */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
