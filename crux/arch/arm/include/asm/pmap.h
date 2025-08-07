#ifndef __ASM_PMAP_H__
#define __ASM_PMAP_H__

#include <crux/mm.h>

#include <asm/fixmap.h>

static inline void arch_pmap_map(unsigned int slot, mfn_t mfn)
{
    lpae_t *entry = &crux_fixmap[slot];
    lpae_t pte;

    ASSERT(!lpae_is_valid(*entry));

    pte = mfn_to_crux_entry(mfn, PAGE_HYPERVISOR_RW);
    pte.pt.table = 1;
    write_pte(entry, pte);
    /*
     * The new entry will be used very soon after arch_pmap_map() returns.
     * So ensure the DSB in write_pte() has completed before continuing.
     */
    isb();
}

static inline void arch_pmap_unmap(unsigned int slot)
{
    lpae_t pte = {};

    write_pte(&crux_fixmap[slot], pte);

    flush_crux_tlb_range_va_local(FIXMAP_ADDR(slot), PAGE_SIZE);
}

#endif /* __ASM_PMAP_H__ */
