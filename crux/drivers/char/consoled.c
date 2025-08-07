/******************************************************************************
 * drivers/char/consoled.c
 *
 * A backend driver for Xen's PV console.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (c) 2017 Citrix Systems Ltd.
 */

#include <crux/lib.h>
#include <crux/event.h>
#include <crux/pv_console.h>
#include <crux/consoled.h>

#include <asm/guest.h>

static struct cruxcons_interface *cons_ring;
static DEFINE_SPINLOCK(rx_lock);

void consoled_set_ring_addr(struct cruxcons_interface *ring)
{
    cons_ring = ring;
}

struct cruxcons_interface *consoled_get_ring_addr(void)
{
    return cons_ring;
}

#define BUF_SZ 255
static char buf[BUF_SZ + 1];

/* Receives characters from a domain's PV console */
int consoled_guest_rx(void)
{
    size_t idx = 0;
    CRUXCONS_RING_IDX cons, prod;

    if ( !cons_ring )
        return -ENODEV;

    spin_lock(&rx_lock);

    cons = cons_ring->out_cons;
    prod = ACCESS_ONCE(cons_ring->out_prod);

    /*
     * Latch pointers before accessing the ring. Included compiler barrier also
     * ensures that pointers are really read only once into local variables.
     */
    smp_rmb();

    ASSERT((prod - cons) <= sizeof(cons_ring->out));

    /* Is the ring empty? */
    if ( cons == prod )
        goto out;

    while ( cons != prod )
    {
        char c = cons_ring->out[MASK_CRUXCONS_IDX(cons++, cons_ring->out)];

        buf[idx++] = c;

        if ( idx >= BUF_SZ )
        {
            pv_console_puts(buf, BUF_SZ);
            idx = 0;
        }
    }

    if ( idx )
        pv_console_puts(buf, idx);

    /* No need for a mem barrier because every character was already consumed */
    barrier();
    ACCESS_ONCE(cons_ring->out_cons) = cons;
    pv_shim_inject_evtchn(pv_console_evtchn());

 out:
    spin_unlock(&rx_lock);

    return 0;
}

/* Sends a character into a domain's PV console */
int consoled_guest_tx(char c)
{
    CRUXCONS_RING_IDX cons, prod;

    if ( !cons_ring )
        return -ENODEV;

    cons = ACCESS_ONCE(cons_ring->in_cons);
    prod = cons_ring->in_prod;

    /*
     * Latch pointers before accessing the ring. Included compiler barrier also
     * ensures that pointers are really read only once into local variables.
     */
    smp_rmb();

    ASSERT((prod - cons) <= sizeof(cons_ring->in));

    /* Is the ring out of space? */
    if ( sizeof(cons_ring->in) - (prod - cons) == 0 )
        goto notify;

    cons_ring->in[MASK_CRUXCONS_IDX(prod++, cons_ring->in)] = c;

    /* Write to the ring before updating the pointer */
    smp_wmb();
    ACCESS_ONCE(cons_ring->in_prod) = prod;

 notify:
    /* Always notify the guest: prevents receive path from getting stuck. */
    pv_shim_inject_evtchn(pv_console_evtchn());

    return 0;
}

bool consoled_is_enabled(void)
{
    return pv_shim && pv_console;
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
