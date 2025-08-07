/*
 * Copyright (c) 2008, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) Allen Kay <allen.m.kay@intel.com>
 * Copyright (C) Weidong Han <weidong.han@intel.com>
 */

#include <crux/param.h>
#include <crux/sched.h>
#include <crux/softirq.h>
#include <crux/domain_page.h>
#include <asm/paging.h>
#include <crux/iommu.h>
#include <crux/irq.h>
#include <crux/numa.h>
#include <asm/fixmap.h>
#include "../iommu.h"
#include "../dmar.h"
#include "../vtd.h"
#include "../extern.h"

/*
 * iommu_inclusive_mapping: when set, all memory below 4GB is included in dom0
 * 1:1 iommu mappings except crux and unusable regions.
 */
boolean_param("iommu_inclusive_mapping", iommu_hwdom_inclusive);

void *map_vtd_domain_page(u64 maddr)
{
    return map_domain_page(_mfn(paddr_to_pfn(maddr)));
}

void unmap_vtd_domain_page(const void *va)
{
    unmap_domain_page(va);
}
