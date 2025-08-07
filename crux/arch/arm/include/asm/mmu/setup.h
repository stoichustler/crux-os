/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef __ARM_MMU_SETUP_H__
#define __ARM_MMU_SETUP_H__

#include <asm/lpae.h>
#include <asm/mmu/layout.h>

extern lpae_t boot_pgtable[CRUX_PT_LPAE_ENTRIES];

#ifdef CONFIG_ARM_64
extern lpae_t boot_first[CRUX_PT_LPAE_ENTRIES];
extern lpae_t boot_first_id[CRUX_PT_LPAE_ENTRIES];
#endif
extern lpae_t boot_second[CRUX_PT_LPAE_ENTRIES];
extern lpae_t boot_second_id[CRUX_PT_LPAE_ENTRIES];
extern lpae_t boot_third[CRUX_PT_LPAE_ENTRIES * CRUX_NR_ENTRIES(2)];
extern lpae_t boot_third_id[CRUX_PT_LPAE_ENTRIES];

/* Find where crux will be residing at runtime and return a PT entry */
lpae_t pte_of_cruxaddr(vaddr_t va);

#endif /* __ARM_MMU_SETUP_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
