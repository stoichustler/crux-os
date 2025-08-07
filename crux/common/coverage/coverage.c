/*
 * Generic functionality for coverage analysis.
 *
 * Copyright (C) 2017 Citrix Systems R&D
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms and conditions of the GNU General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; If not, see <http://www.gnu.org/licenses/>.
 */

#include <crux/errno.h>
#include <crux/guest_access.h>
#include <crux/types.h>
#include <crux/coverage.h>

#include <public/sysctl.h>

#include "coverage.h"

int sysctl_cov_op(struct crux_sysctl_coverage_op *op)
{
    int ret;

    switch ( op->cmd )
    {
    case CRUX_SYSCTL_COVERAGE_get_size:
        op->size = cov_ops.get_size();
        ret = 0;
        break;

    case CRUX_SYSCTL_COVERAGE_read:
    {
        CRUX_GUEST_HANDLE_PARAM(char) buf;
        uint32_t size = op->size;

        buf = guest_handle_cast(op->buffer, char);

        ret = cov_ops.dump(buf, &size);
        op->size = size;

        break;
    }

    case CRUX_SYSCTL_COVERAGE_reset:
        cov_ops.reset_counters();
        ret = 0;
        break;

    default:
        ret = -EOPNOTSUPP;
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
