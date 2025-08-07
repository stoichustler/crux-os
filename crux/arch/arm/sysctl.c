/* SPDX-License-Identifier: GPL-2.0-only */
/******************************************************************************
 * Arch-specific sysctl.c
 *
 * System management operations. For use by node control stack.
 *
 * Copyright (c) 2012, Citrix Systems
 */

#include <crux/types.h>
#include <crux/lib.h>
#include <crux/dt-overlay.h>
#include <crux/errno.h>
#include <crux/hypercall.h>
#include <asm/arm64/sve.h>
#include <public/sysctl.h>

void arch_do_physinfo(struct crux_sysctl_physinfo *pi)
{
    pi->capabilities |= CRUX_SYSCTL_PHYSCAP_hvm | CRUX_SYSCTL_PHYSCAP_hap;

    pi->arch_capabilities |= MASK_INSR(sve_encode_vl(get_sys_vl_len()),
                                       CRUX_SYSCTL_PHYSCAP_ARM_SVE_MASK);
}

long arch_do_sysctl(struct crux_sysctl *sysctl,
                    CRUX_GUEST_HANDLE_PARAM(crux_sysctl_t) u_sysctl)
{
    long ret;

    switch ( sysctl->cmd )
    {
    case CRUX_SYSCTL_dt_overlay:
        ret = dt_overlay_sysctl(&sysctl->u.dt_overlay);
        break;

    default:
        ret = -ENOSYS;
        break;
    }

    return ret;
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
