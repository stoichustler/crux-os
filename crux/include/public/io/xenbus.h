/* SPDX-License-Identifier: MIT */
/*****************************************************************************
 * cruxbus.h
 *
 * cruxbus protocol details.
 *
 * Copyright (C) 2005 cruxSource Ltd.
 */

#ifndef _CRUX_PUBLIC_IO_CRUXBUS_H
#define _CRUX_PUBLIC_IO_CRUXBUS_H

/*
 * The state of either end of the cruxbus, i.e. the current communication
 * status of initialisation across the bus.  States here imply nothing about
 * the state of the connection between the driver and the kernel's device
 * layers.
 */
enum cruxbus_state {
    cruxbusStateUnknown       = 0,

    cruxbusStateInitialising  = 1,

    /*
     * InitWait: Finished early initialisation but waiting for information
     * from the peer or hotplug scripts.
     */
    cruxbusStateInitWait      = 2,

    /*
     * Initialised: Waiting for a connection from the peer.
     */
    cruxbusStateInitialised   = 3,

    cruxbusStateConnected     = 4,

    /*
     * Closing: The device is being closed due to an error or an unplug event.
     */
    cruxbusStateClosing       = 5,

    cruxbusStateClosed        = 6,

    /*
     * Reconfiguring: The device is being reconfigured.
     */
    cruxbusStateReconfiguring = 7,

    cruxbusStateReconfigured  = 8
};
typedef enum cruxbus_state cruxbusState;

#endif /* _CRUX_PUBLIC_IO_CRUXBUS_H */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
