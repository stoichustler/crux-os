/******************************************************************************
 * Subarch-specific domctl.c
 *
 * Copyright (c) 2013, Citrix Systems
 */

#include <crux/types.h>
#include <crux/lib.h>
#include <crux/errno.h>
#include <crux/sched.h>
#include <crux/hypercall.h>
#include <public/domctl.h>

long subarch_do_domctl(struct crux_domctl *domctl, struct domain *d,
               CRUX_GUEST_HANDLE_PARAM(crux_domctl_t) u_domctl)
{
    switch ( domctl->cmd )
    {
    case CRUX_DOMCTL_set_address_size:
        return domctl->u.address_size.size == 32 ? 0 : -EINVAL;
    default:
        return -ENOSYS;
    }
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
