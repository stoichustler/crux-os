/* SPDX-License-Identifier: MIT */
/*****************************************************************************
 * xenbus.h
 *
 * xenbus protocol details.
 *
 * Copyright (C) 2005 xenSource Ltd.
 */

#ifndef _XEN_PUBLIC_IO_XENBUS_H
#define _XEN_PUBLIC_IO_XENBUS_H

/*
 * The state of either end of the xenbus, i.e. the current communication
 * status of initialisation across the bus.  States here imply nothing about
 * the state of the connection between the driver and the kernel's device
 * layers.
 */
enum xenbus_state {
    xenbusStateUnknown       = 0,

    xenbusStateInitialising  = 1,

    /*
     * InitWait: Finished early initialisation but waiting for information
     * from the peer or hotplug scripts.
     */
    xenbusStateInitWait      = 2,

    /*
     * Initialised: Waiting for a connection from the peer.
     */
    xenbusStateInitialised   = 3,

    xenbusStateConnected     = 4,

    /*
     * Closing: The device is being closed due to an error or an unplug event.
     */
    xenbusStateClosing       = 5,

    xenbusStateClosed        = 6,

    /*
     * Reconfiguring: The device is being reconfigured.
     */
    xenbusStateReconfiguring = 7,

    xenbusStateReconfigured  = 8
};
typedef enum xenbus_state xenbusState;

#endif /* _XEN_PUBLIC_IO_XENBUS_H */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
