/* SPDX-License-Identifier: GPL-2.0-only */

#include <crux/bug.h>
#include <crux/mm-frame.h>
#include <crux/types.h>
#include <crux/vmap.h>

void *vmap_contig(mfn_t mfn, unsigned int nr)
{
    BUG_ON("unimplemented");
    return NULL;
}

void vunmap(const void *va)
{
    BUG_ON("unimplemented");
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
