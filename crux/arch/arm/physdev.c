/* SPDX-License-Identifier: GPL-2.0-only */
/******************************************************************************
 * Arch-specific physdev.c
 *
 * Copyright (c) 2012, Citrix Systems
 */

#include <crux/types.h>
#include <crux/lib.h>
#include <crux/errno.h>
#include <crux/sched.h>
#include <crux/hypercall.h>


int do_arm_physdev_op(int cmd, CRUX_GUEST_HANDLE_PARAM(void) arg)
{
#ifdef CONFIG_HAS_PCI
    return pci_physdev_op(cmd, arg);
#else
    gdprintk(CRUXLOG_DEBUG, "PHYSDEVOP cmd=%d: not implemented\n", cmd);
    return -ENOSYS;
#endif
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
