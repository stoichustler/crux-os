/* SPDX-License-Identifier: GPL-2.0-only */
/******************************************************************************
 * platform_hypercall.c
 *
 * Hardware platform operations. Intended for use by domain-0 kernel.
 *
 * Copyright (c) 2015, Citrix
 */

#include <crux/types.h>
#include <crux/sched.h>
#include <crux/guest_access.h>
#include <crux/hypercall.h>
#include <crux/spinlock.h>
#include <public/platform.h>
#include <xsm/xsm.h>
#include <asm/current.h>
#include <asm/event.h>

static DEFINE_SPINLOCK(cruxpf_lock);

long do_platform_op(CRUX_GUEST_HANDLE_PARAM(crux_platform_op_t) u_cruxpf_op)
{
    long ret;
    struct crux_platform_op curop, *op = &curop;
    struct domain *d;

    if ( copy_from_guest(op, u_cruxpf_op, 1) )
        return -EFAULT;

    if ( op->interface_version != CRUXPF_INTERFACE_VERSION )
        return -EACCES;

    d = rcu_lock_current_domain();
    if ( d == NULL )
        return -ESRCH;

    ret = xsm_platform_op(XSM_PRIV, op->cmd);
    if ( ret )
        return ret;

    /*
     * Trylock here avoids deadlock with an existing platform critical section
     * which might (for some current or future reason) want to synchronise
     * with this vcpu.
     */
    while ( !spin_trylock(&cruxpf_lock) )
        if ( hypercall_preempt_check() )
            return hypercall_create_continuation(
                __HYPERVISOR_platform_op, "h", u_cruxpf_op);

    switch ( op->cmd )
    {
    case CRUXPF_settime64:
        if ( likely(!op->u.settime64.mbz) )
            do_settime(op->u.settime64.secs,
                       op->u.settime64.nsecs,
                       op->u.settime64.system_time + SECONDS(d->time_offset.seconds));
        else
            ret = -EINVAL;
        break;

    default:
        ret = -ENOSYS;
        break;
    }

    spin_unlock(&cruxpf_lock);
    rcu_unlock_domain(d);
    return ret;
}
