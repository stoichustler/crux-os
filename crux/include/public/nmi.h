/* SPDX-License-Identifier: MIT */
/******************************************************************************
 * nmi.h
 *
 * NMI callback registration and reason codes.
 *
 * Copyright (c) 2005, Keir Fraser <keir@cruxsource.com>
 */

#ifndef __CRUX_PUBLIC_NMI_H__
#define __CRUX_PUBLIC_NMI_H__

#include "crux.h"

/*
 * NMI reason codes:
 * Currently these are x86-specific, stored in arch_shared_info.nmi_reason.
 */
 /* I/O-check error reported via ISA port 0x61, bit 6. */
#define _CRUX_NMIREASON_io_error     0
#define CRUX_NMIREASON_io_error      (1UL << _CRUX_NMIREASON_io_error)
 /* PCI SERR reported via ISA port 0x61, bit 7. */
#define _CRUX_NMIREASON_pci_serr     1
#define CRUX_NMIREASON_pci_serr      (1UL << _CRUX_NMIREASON_pci_serr)
#if __CRUX_INTERFACE_VERSION__ < 0x00040300 /* legacy alias of the above */
 /* Parity error reported via ISA port 0x61, bit 7. */
#define _CRUX_NMIREASON_parity_error 1
#define CRUX_NMIREASON_parity_error  (1UL << _CRUX_NMIREASON_parity_error)
#endif
 /* Unknown hardware-generated NMI. */
#define _CRUX_NMIREASON_unknown      2
#define CRUX_NMIREASON_unknown       (1UL << _CRUX_NMIREASON_unknown)

/*
 * long nmi_op(unsigned int cmd, void *arg)
 * NB. All ops return zero on success, else a negative error code.
 */

/*
 * Register NMI callback for this (calling) VCPU. Currently this only makes
 * sense for domain 0, vcpu 0. All other callers will be returned EINVAL.
 * arg == pointer to cruxnmi_callback structure.
 */
#define CRUXNMI_register_callback   0
struct cruxnmi_callback {
    unsigned long handler_address;
    unsigned long pad;
};
typedef struct cruxnmi_callback cruxnmi_callback_t;
DEFINE_CRUX_GUEST_HANDLE(cruxnmi_callback_t);

/*
 * Deregister NMI callback for this (calling) VCPU.
 * arg == NULL.
 */
#define CRUXNMI_unregister_callback 1

#endif /* __CRUX_PUBLIC_NMI_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
