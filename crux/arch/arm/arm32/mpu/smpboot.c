/* SPDX-License-Identifier: GPL-2.0-only */

#include <crux/bug.h>
#include <crux/errno.h>
#include <crux/mm.h>
#include <crux/stdbool.h>

int prepare_secondary_mm(int cpu)
{
    BUG_ON("unimplemented");
    return -EINVAL;
}

void update_boot_mapping(bool enable)
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
