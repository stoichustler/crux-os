/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef ASM__RISCV__MM_H
#define ASM__RISCV__MM_H

#include <public/crux.h>
#include <crux/bug.h>
#include <crux/const.h>
#include <crux/mm-frame.h>
#include <crux/pdx.h>
#include <crux/pfn.h>
#include <crux/sections.h>
#include <crux/types.h>

#include <asm/page.h>
#include <asm/page-bits.h>

extern vaddr_t directmap_virt_start;

#define pfn_to_paddr(pfn) ((paddr_t)(pfn) << PAGE_SHIFT)
#define paddr_to_pfn(pa)  ((unsigned long)((pa) >> PAGE_SHIFT))

static inline pte_t paddr_to_pte(paddr_t paddr,
                                 unsigned int permissions)
{
    return (pte_t) { .pte = (paddr_to_pfn(paddr) << PTE_PPN_SHIFT) | permissions };
}

static inline paddr_t pte_to_paddr(pte_t pte)
{
    return pfn_to_paddr(pte.pte >> PTE_PPN_SHIFT);
}

#define gfn_to_gaddr(gfn)   pfn_to_paddr(gfn_x(gfn))
#define gaddr_to_gfn(ga)    _gfn(paddr_to_pfn(ga))
#define mfn_to_maddr(mfn)   pfn_to_paddr(mfn_x(mfn))
#define maddr_to_mfn(ma)    _mfn(paddr_to_pfn(ma))
#define vmap_to_page(va)    mfn_to_page(vmap_to_mfn(va))

static inline void *maddr_to_virt(paddr_t ma)
{
    unsigned long va = directmap_virt_start + maddr_to_directmapoff(ma);

    ASSERT((va >= DIRECTMAP_VIRT_START) && (va <= DIRECTMAP_VIRT_END));

    return (void *)va;
}

#define mfn_from_pte(pte) maddr_to_mfn(pte_to_paddr(pte))

#define vmap_to_mfn(va)                             \
({                                                  \
    pte_t __entry = pt_walk((vaddr_t)(va), NULL);   \
    BUG_ON(!pte_is_mapping(__entry));               \
    maddr_to_mfn(pte_to_paddr(__entry));            \
})

/*
 * virt_to_maddr() is expected to work with virtual addresses from either
 * the directmap region or Xen's linkage (CRUX_VIRT_START) region.
 * Therefore, it is sufficient to check only these regions and assert if `va`
 * is not within the directmap or Xen's linkage region.
 */
static inline unsigned long virt_to_maddr(unsigned long va)
{
    const unsigned long crux_size = (unsigned long)(_end - _start);
    const unsigned long crux_virt_start = _AC(CRUX_VIRT_START, UL);
    const unsigned long crux_virt_end = crux_virt_start + crux_size - 1;

    if ((va >= DIRECTMAP_VIRT_START) &&
        (va <= DIRECTMAP_VIRT_END))
        return directmapoff_to_maddr(va - directmap_virt_start);

    ASSERT((va >= crux_virt_start) && (va <= crux_virt_end));

    /*
    * The .init* sections will be freed when Xen completes booting,
    * so the [__init_begin, __init_end) range must be excluded.
    */
    ASSERT((system_state < SYS_STATE_active) || !is_init_section(va));

    /* phys_offset = load_start - CRUX_VIRT_START */
    return phys_offset + va;
}
#define virt_to_maddr(va) virt_to_maddr((unsigned long)(va))

/* Convert between Xen-heap virtual addresses and machine frame numbers. */
#define __virt_to_mfn(va)  mfn_x(maddr_to_mfn(virt_to_maddr(va)))
#define __mfn_to_virt(mfn) maddr_to_virt(mfn_to_maddr(_mfn(mfn)))

/*
 * We define non-underscored wrappers for above conversion functions.
 * These are overriden in various source files while underscored version
 * remain intact.
 */
#define virt_to_mfn(va)     __virt_to_mfn(va)
#define mfn_to_virt(mfn)    __mfn_to_virt(mfn)

struct page_info
{
    /* Each frame can be threaded onto a doubly-linked list. */
    struct page_list_entry list;

    /* Reference count and various PGC_xxx flags and fields. */
    unsigned long count_info;

    /* Context-dependent fields follow... */
    union {
        /* Page is in use: ((count_info & PGC_count_mask) != 0). */
        struct {
            /* Type reference count and various PGT_xxx flags and fields. */
            unsigned long type_info;
        } inuse;

        /* Page is on a free list: ((count_info & PGC_count_mask) == 0). */
        union {
            struct {
                /*
                 * Index of the first *possibly* unscrubbed page in the buddy.
                 * One more bit than maximum possible order to accommodate
                 * INVALID_DIRTY_IDX.
                 */
#define INVALID_DIRTY_IDX ((1UL << (MAX_ORDER + 1)) - 1)
                unsigned long first_dirty:MAX_ORDER + 1;

                /* Do TLBs need flushing for safety before next page use? */
                bool need_tlbflush:1;

#define BUDDY_NOT_SCRUBBING    0
#define BUDDY_SCRUBBING        1
#define BUDDY_SCRUB_ABORT      2
                unsigned long scrub_state:2;
            };

            unsigned long val;
        } free;
    } u;

    union {
        /* Page is in use */
        struct {
            /* Owner of this page (NULL if page is anonymous). */
            struct domain *domain;
        } inuse;

        /* Page is on a free list. */
        struct {
            /* Order-size of the free chunk this page is the head of. */
            unsigned int order;
        } free;
    } v;

    union {
        /*
         * Timestamp from 'TLB clock', used to avoid extra safety flushes.
         * Only valid for: a) free pages, and b) pages with zero type count
         */
        uint32_t tlbflush_timestamp;
    };
};

extern struct page_info *frametable_virt_start;

#define frame_table ((struct page_info *)FRAMETABLE_VIRT_START)

/* Convert between machine frame numbers and page-info structures. */
#define mfn_to_page(mfn)    (frametable_virt_start + mfn_x(mfn))
#define page_to_mfn(pg)     _mfn((pg) - frametable_virt_start)

static inline void *page_to_virt(const struct page_info *pg)
{
    return mfn_to_virt(mfn_x(page_to_mfn(pg)));
}

/* Convert between Xen-heap virtual addresses and page-info structures. */
static inline struct page_info *virt_to_page(const void *v)
{
    unsigned long va = (unsigned long)v;

    ASSERT((va >= DIRECTMAP_VIRT_START) && (va <= DIRECTMAP_VIRT_END));

    return frametable_virt_start + PFN_DOWN(va - directmap_virt_start);
}

/*
 * Common code requires get_page_type and put_page_type.
 * We don't care about typecounts so we just do the minimum to make it
 * happy.
 */
static inline int get_page_type(struct page_info *page, unsigned long type)
{
    return 1;
}

static inline void put_page_type(struct page_info *page)
{
}

static inline void put_page_and_type(struct page_info *page)
{
    put_page_type(page);
    put_page(page);
}

/*
 * RISC-V does not have an M2P, but common code expects a handful of
 * M2P-related defines and functions. Provide dummy versions of these.
 */
#define INVALID_M2P_ENTRY        (~0UL)
#define SHARED_M2P_ENTRY         (~0UL - 1UL)
#define SHARED_M2P(_e)           ((_e) == SHARED_M2P_ENTRY)

#define set_gpfn_from_mfn(mfn, pfn) do { (void)(mfn), (void)(pfn); } while (0)
#define mfn_to_gfn(d, mfn) ((void)(d), _gfn(mfn_x(mfn)))

#define PDX_GROUP_SHIFT (PAGE_SHIFT + VPN_BITS)

static inline unsigned long domain_get_maximum_gpfn(struct domain *d)
{
    BUG_ON("unimplemented");
    return 0;
}

static inline long arch_memory_op(int op, CRUX_GUEST_HANDLE_PARAM(void) arg)
{
    BUG_ON("unimplemented");
    return 0;
}

/*
 * On RISCV, all the RAM is currently direct mapped in Xen.
 * Hence return always true.
 */
static inline bool arch_mfns_in_directmap(unsigned long mfn, unsigned long nr)
{
    return true;
}

#define PG_shift(idx)   (BITS_PER_LONG - (idx))
#define PG_mask(x, idx) (x ## UL << PG_shift(idx))

#define PGT_none          PG_mask(0, 1)  /* no special uses of this page   */
#define PGT_writable_page PG_mask(1, 1)  /* has writable mappings?         */
#define PGT_type_mask     PG_mask(1, 1)  /* Bits 31 or 63.                 */

/* Count of uses of this frame as its current type. */
#define PGT_count_width   PG_shift(2)
#define PGT_count_mask    ((1UL << PGT_count_width) - 1)

/*
 * Page needs to be scrubbed. Since this bit can only be set on a page that is
 * free (i.e. in PGC_state_free) we can reuse PGC_allocated bit.
 */
#define _PGC_need_scrub   _PGC_allocated
#define PGC_need_scrub    PGC_allocated

/* Cleared when the owning guest 'frees' this page. */
#define _PGC_allocated    PG_shift(1)
#define PGC_allocated     PG_mask(1, 1)
/* Page is Xen heap? */
#define _PGC_crux_heap     PG_shift(2)
#define PGC_crux_heap      PG_mask(1, 2)
/* Page is broken? */
#define _PGC_broken       PG_shift(7)
#define PGC_broken        PG_mask(1, 7)
/* Mutually-exclusive page states: { inuse, offlining, offlined, free }. */
#define PGC_state         PG_mask(3, 9)
#define PGC_state_inuse   PG_mask(0, 9)
#define PGC_state_offlining PG_mask(1, 9)
#define PGC_state_offlined PG_mask(2, 9)
#define PGC_state_free    PG_mask(3, 9)
#define page_state_is(pg, st) (((pg)->count_info&PGC_state) == PGC_state_##st)

/* Count of references to this frame. */
#define PGC_count_width   PG_shift(9)
#define PGC_count_mask    ((1UL << PGC_count_width) - 1)

#define _PGC_extra        PG_shift(10)
#define PGC_extra         PG_mask(1, 10)

#define is_crux_heap_page(page) ((page)->count_info & PGC_crux_heap)
#define is_crux_heap_mfn(mfn) \
    (mfn_valid(mfn) && is_crux_heap_page(mfn_to_page(mfn)))

#define is_crux_fixed_mfn(mfn)                                   \
    ((mfn_to_maddr(mfn) >= virt_to_maddr((vaddr_t)_start)) &&   \
     (mfn_to_maddr(mfn) <= virt_to_maddr((vaddr_t)_end - 1)))

#define page_get_owner(p)    (p)->v.inuse.domain
#define page_set_owner(p, d) ((p)->v.inuse.domain = (d))

/* TODO: implement */
#define mfn_valid(mfn) ({ (void)(mfn); 0; })

#define domain_set_alloc_bitsize(d) ((void)(d))
#define domain_clamp_alloc_bitsize(d, b) ((void)(d), (b))

#define PFN_ORDER(pg) ((pg)->v.free.order)

extern unsigned char cpu0_boot_stack[];

void setup_initial_pagetables(void);

void enable_mmu(void);

void remove_identity_mapping(void);

unsigned long calc_phys_offset(void);

void turn_on_mmu(unsigned long ra);

static inline unsigned int arch_get_dma_bitsize(void)
{
    return 32; /* TODO */
}

void setup_fixmap_mappings(void);

void *early_fdt_map(paddr_t fdt_paddr);

#endif /* ASM__RISCV__MM_H */
