#include <crux/bug.h>
#include <crux/domain_page.h>
#include <crux/errno.h>
#include <crux/init.h>
#include <crux/lib.h>
#include <crux/mm.h>
#include <crux/pfn.h>
#include <crux/pmap.h>
#include <crux/spinlock.h>

#include <asm/fixmap.h>
#include <asm/flushtlb.h>
#include <asm/page.h>

static inline mfn_t get_root_page(void)
{
    paddr_t root_maddr = pfn_to_paddr(csr_read(CSR_SATP) & SATP_PPN_MASK);

    return maddr_to_mfn(root_maddr);
}

/*
 * Sanity check a page table entry about to be updated as per an (MFN,flags)
 * tuple.
 * See the comment about the possible combination of (mfn, flags) in
 * the comment above pt_update().
 */
static bool pt_check_entry(pte_t entry, mfn_t mfn, pte_attr_t flags)
{
    /* Sanity check when modifying an entry. */
    if ( (flags & PTE_VALID) && mfn_eq(mfn, INVALID_MFN) )
    {
        /* We don't allow modifying an invalid entry. */
        if ( !pte_is_valid(entry) )
        {
            dprintk(CRUXLOG_ERR, "Modifying invalid entry is not allowed\n");
            return false;
        }

        /* We don't allow modifying a table entry */
        if ( pte_is_table(entry) )
        {
            dprintk(CRUXLOG_ERR, "Modifying a table entry is not allowed\n");
            return false;
        }
    }
    /* Sanity check when inserting a mapping */
    else if ( flags & PTE_VALID )
    {
        /*
         * We don't allow replacing any valid entry.
         *
         * Note that the function pt_update() relies on this
         * assumption and will skip the TLB flush (when Svvptc
         * extension will be ratified). The function will need
         * to be updated if the check is relaxed.
         */
        if ( pte_is_valid(entry) )
        {
            if ( pte_is_mapping(entry) )
                dprintk(CRUXLOG_ERR, "Changing MFN for valid PTE is not allowed (%#"PRI_mfn" -> %#"PRI_mfn")\n",
                        mfn_x(mfn_from_pte(entry)), mfn_x(mfn));
            else
                dprintk(CRUXLOG_ERR, "Trying to replace table with mapping\n");
            return false;
        }
    }
    /* Sanity check when removing a mapping. */
    else if ( !(flags & PTE_POPULATE) )
    {
        /* We should be here with an invalid MFN. */
        ASSERT(mfn_eq(mfn, INVALID_MFN));

        /* We don't allow removing a table */
        if ( pte_is_table(entry) )
        {
            dprintk(CRUXLOG_ERR, "Removing a table is not allowed\n");
            return false;
        }
    }
    /* Sanity check when populating the page-table. No check so far. */
    else
    {
        /* We should be here with an invalid MFN */
        ASSERT(mfn_eq(mfn, INVALID_MFN));
    }

    return true;
}

static pte_t *map_table(mfn_t mfn)
{
    /*
     * During early boot, map_domain_page() may be unusable. Use the
     * PMAP to map temporarily a page-table.
     */
    if ( system_state == SYS_STATE_early_boot )
        return pmap_map(mfn);

    return map_domain_page(mfn);
}

static void unmap_table(const pte_t *table)
{
    if ( !table )
        return;

    /*
     * During early boot, map_table() will not use map_domain_page()
     * but the PMAP.
     */
    if ( system_state == SYS_STATE_early_boot )
        pmap_unmap(table);
    else
        unmap_domain_page(table);
}

static bool create_table(pte_t *entry)
{
    mfn_t mfn;
    void *p;
    pte_t pte;

    if ( system_state != SYS_STATE_early_boot )
    {
        struct page_info *pg = alloc_domheap_page(NULL, 0);

        if ( pg == NULL )
            return false;

        mfn = page_to_mfn(pg);
    }
    else
        mfn = alloc_boot_pages(1, 1);

    p = map_table(mfn);
    clear_page(p);
    unmap_table(p);

    pte = pte_from_mfn(mfn, PTE_TABLE);
    write_pte(entry, pte);

    return true;
}

#define CRUX_TABLE_MAP_NONE 0
#define CRUX_TABLE_MAP_NOMEM 1
#define CRUX_TABLE_SUPER_PAGE 2
#define CRUX_TABLE_NORMAL 3

/*
 * Take the currently mapped table, find the corresponding entry,
 * and map the next table, if available.
 *
 * The alloc_tbl parameters indicates whether intermediate tables should
 * be allocated when not present.
 *
 * Return values:
 *  CRUX_TABLE_MAP_NONE: a table allocation isn't permitted.
 *  CRUX_TABLE_MAP_NOMEM: allocating a new page failed.
 *  CRUX_TABLE_NORMAL: next level or leaf mapped normally.
 *  CRUX_TABLE_SUPER_PAGE: The next entry points to a superpage.
 */
static int pt_next_level(bool alloc_tbl, pte_t **table, unsigned int offset)
{
    pte_t *entry;
    mfn_t mfn;

    entry = *table + offset;

    if ( !pte_is_valid(*entry) )
    {
        if ( !alloc_tbl )
            return CRUX_TABLE_MAP_NONE;

        if ( !create_table(entry) )
            return CRUX_TABLE_MAP_NOMEM;
    }

    if ( pte_is_mapping(*entry) )
        return CRUX_TABLE_SUPER_PAGE;

    mfn = mfn_from_pte(*entry);

    unmap_table(*table);
    *table = map_table(mfn);

    return CRUX_TABLE_NORMAL;
}

/*
 * _pt_walk() performs software page table walking and returns the pte_t of
 * a leaf node or the leaf-most not-present pte_t if no leaf node is found
 * for further analysis.
 *
 * _pt_walk() can optionally return the level of the found pte. Pass NULL
 * for `pte_level` if this information isn't needed.
 *
 * Note: unmapping of final `table` should be done by a caller.
 */
static pte_t *_pt_walk(vaddr_t va, unsigned int *pte_level)
{
    const mfn_t root = get_root_page();
    unsigned int level;
    pte_t *table;

    DECLARE_OFFSETS(offsets, va);

    table = map_table(root);

    /*
     * Find `table` of an entry which corresponds to `va` by iterating for each
     * page level and checking if the entry points to a next page table or
     * to a page.
     *
     * Two cases are possible:
     * - ret == CRUX_TABLE_SUPER_PAGE means that the entry was found;
     *   (Despite the name) CRUX_TABLE_SUPER_PAGE also covers 4K mappings. If
     *   pt_next_level() is called for page table level 0, it results in the
     *   entry being a pointer to a leaf node, thereby returning
     *   CRUX_TABLE_SUPER_PAGE, despite of the fact this leaf covers 4k mapping.
     * - ret == CRUX_TABLE_MAP_NONE means that requested `va` wasn't actually
     *   mapped.
     */
    for ( level = HYP_PT_ROOT_LEVEL; ; --level )
    {
        int ret = pt_next_level(false, &table, offsets[level]);

        if ( ret == CRUX_TABLE_MAP_NONE || ret == CRUX_TABLE_SUPER_PAGE )
            break;

        ASSERT(level);
    }

    if ( pte_level )
        *pte_level = level;

    return table + offsets[level];
}

pte_t pt_walk(vaddr_t va, unsigned int *pte_level)
{
    pte_t *ptep = _pt_walk(va, pte_level);
    pte_t pte = *ptep;

    unmap_table(ptep);

    return pte;
}

/*
 * Update an entry at the level @target.
 *
 * If `target` == CONFIG_PAGING_LEVELS, the search will continue until
 * a leaf node is found.
 * Otherwise, the page table entry will be searched at the requested
 * `target` level.
 * For an example of why this might be needed, see the comment in
 * pt_update() before pt_update_entry() is called.
 */
static int pt_update_entry(mfn_t root, vaddr_t virt,
                           mfn_t mfn, unsigned int *target,
                           pte_attr_t flags)
{
    int rc;
    /*
     * The intermediate page table shouldn't be allocated when MFN isn't
     * valid and we are not populating page table.
     * This means we either modify permissions or remove an entry, or
     * inserting brand new entry.
     *
     * See the comment above pt_update() for an additional explanation about
     * combinations of (mfn, flags).
    */
    bool alloc_tbl = !mfn_eq(mfn, INVALID_MFN) || (flags & PTE_POPULATE);
    pte_t pte, *ptep = NULL;

    if ( *target == CONFIG_PAGING_LEVELS )
        ptep = _pt_walk(virt, target);
    else
    {
        pte_t *table;
        unsigned int level = HYP_PT_ROOT_LEVEL;
        /* Convenience aliases */
        DECLARE_OFFSETS(offsets, virt);

        table = map_table(root);
        for ( ; level > *target; level-- )
        {
            rc = pt_next_level(alloc_tbl, &table, offsets[level]);
            if ( rc == CRUX_TABLE_MAP_NOMEM )
            {
                rc = -ENOMEM;
                goto out;
            }

            if ( rc == CRUX_TABLE_MAP_NONE )
            {
                rc = 0;
                goto out;
            }

            if ( rc != CRUX_TABLE_NORMAL )
                break;
        }

        if ( level != *target )
        {
            dprintk(CRUXLOG_ERR,
                    "%s: Shattering superpage is not supported\n", __func__);
            rc = -EOPNOTSUPP;
            goto out;
        }

        ptep = table + offsets[level];
    }

    rc = -EINVAL;
    if ( !pt_check_entry(*ptep, mfn, flags) )
        goto out;

    /* We are removing the page */
    if ( !(flags & PTE_VALID) )
        /*
         * There is also a check in pt_check_entry() which check that
         * mfn=INVALID_MFN
         */
        pte.pte = 0;
    else
    {
        const pte_attr_t attrs = PTE_ACCESS_MASK | PTE_PBMT_MASK;

        /* We are inserting a mapping => Create new pte. */
        if ( !mfn_eq(mfn, INVALID_MFN) )
            pte = pte_from_mfn(mfn, PTE_VALID);
        else /* We are updating the attributes => Copy the current pte. */
        {
            pte = *ptep;
            pte.pte &= ~attrs;
        }

        /* Update attributes of PTE according to the flags. */
        pte.pte |= (flags & attrs) | PTE_ACCESSED | PTE_DIRTY;
    }

    write_pte(ptep, pte);

    rc = 0;

 out:
    unmap_table(ptep);

    return rc;
}

/* Return the level where mapping should be done */
static int pt_mapping_level(unsigned long vfn, mfn_t mfn, unsigned long nr,
                            pte_attr_t flags)
{
    unsigned int level = 0;
    unsigned long mask;
    unsigned int i;

    /*
     * Use a larger mapping than 4K unless the caller specifically requests
     * 4K mapping
     */
    if ( unlikely(flags & PTE_SMALL) )
        return level;

    /*
     * Don't take into account the MFN when removing mapping (i.e
     * MFN_INVALID) to calculate the correct target order.
     *
     * `vfn` and `mfn` must be both superpage aligned.
     * They are or-ed together and then checked against the size of
     * each level.
     *
     * `left` ( variable declared in pt_update() ) is not included
     * and checked separately to allow superpage mapping even if it
     * is not properly aligned (the user may have asked to map 2MB + 4k).
     */
    mask = !mfn_eq(mfn, INVALID_MFN) ? mfn_x(mfn) : 0;
    mask |= vfn;

    for ( i = HYP_PT_ROOT_LEVEL; i != 0; i-- )
    {
        if ( !(mask & (BIT(CRUX_PT_LEVEL_ORDER(i), UL) - 1)) &&
             (nr >= BIT(CRUX_PT_LEVEL_ORDER(i), UL)) )
        {
            level = i;
            break;
        }
    }

    return level;
}

static DEFINE_SPINLOCK(pt_lock);

/*
 * If `mfn` equals `INVALID_MFN`, it indicates that the following page table
 * update operation might be related to either:
 *   - populating the table (PTE_POPULATE will be set additionaly),
 *   - destroying a mapping (PTE_VALID=0),
 *   - modifying an existing mapping (PTE_VALID=1).
 *
 * If `mfn` != INVALID_MFN and flags has PTE_VALID bit set then it means that
 * inserting will be done.
 */
static int pt_update(vaddr_t virt, mfn_t mfn,
                     unsigned long nr_mfns, pte_attr_t flags)
{
    int rc = 0;
    unsigned long vfn = PFN_DOWN(virt);
    unsigned long left = nr_mfns;
    const mfn_t root = get_root_page();

    /*
     * It is bad idea to have mapping both writeable and
     * executable.
     * When modifying/creating mapping (i.e PTE_VALID is set),
     * prevent any update if this happen.
     */
    if ( (flags & PTE_VALID) && (flags & PTE_WRITABLE) &&
         (flags & PTE_EXECUTABLE) )
    {
        dprintk(CRUXLOG_ERR,
                "Mappings should not be both Writeable and Executable\n");
        return -EINVAL;
    }

    if ( !IS_ALIGNED(virt, PAGE_SIZE) )
    {
        dprintk(CRUXLOG_ERR,
                "The virtual address is not aligned to the page-size\n");
        return -EINVAL;
    }

    spin_lock(&pt_lock);

    while ( left )
    {
        unsigned int order, level = CONFIG_PAGING_LEVELS;

        /*
         * In the case when modifying or destroying a mapping, it is necessary
         * to search until a leaf node is found, instead of searching for
         * a page table entry based on the precalculated `level` and `order`
         * (look at pt_update()).
         * This is because when `mfn` == INVALID_MFN, the `mask`(in
         * pt_mapping_level()) will take into account only `vfn`, which could
         * accidentally return an incorrect level, leading to the discovery of
         * an incorrect page table entry.
         *
         * For example, if `vfn` is page table level 1 aligned, but it was
         * mapped as page table level 0, then pt_mapping_level() will return
         * `level` = 1, since only `vfn` (which is page table level 1 aligned)
         * is taken into account when `mfn` == INVALID_MFN
         * (look at pt_mapping_level()).
         *
         * To force searching until a leaf node is found is necessary to have
         * `level` == CONFIG_PAGING_LEVELS.
         *
         * For other cases (when a mapping is not being modified or destroyed),
         * pt_mapping_level() should be used.
         */
        if ( !mfn_eq(mfn, INVALID_MFN) || (flags & PTE_POPULATE) )
            level = pt_mapping_level(vfn, mfn, left, flags);

        rc = pt_update_entry(root, vfn << PAGE_SHIFT, mfn, &level, flags);
        if ( rc )
            break;

        order = CRUX_PT_LEVEL_ORDER(level);

        vfn += 1UL << order;
        if ( !mfn_eq(mfn, INVALID_MFN) )
            mfn = mfn_add(mfn, 1UL << order);

        left -= (1UL << order);
    }

    /* Ensure that PTEs are all updated before flushing */
    RISCV_FENCE(rw, rw);

    spin_unlock(&pt_lock);

    /*
     * Always flush TLB at the end of the function as non-present entries
     * can be put in the TLB.
     *
     * The remote fence operation applies to the entire address space if
     * either:
     *  - start and size are both 0, or
     *  - size is equal to 2^XLEN-1.
     *
     * TODO: come up with something which will allow not to flash the entire
     *       address space.
     */
    flush_tlb_range_va(0, 0);

    return rc;
}

int map_pages_to_crux(unsigned long virt,
                     mfn_t mfn,
                     unsigned long nr_mfns,
                     pte_attr_t flags)
{
    /*
     * Ensure that flags has PTE_VALID bit as map_pages_to_crux() is supposed
     * to create a mapping.
     *
     * Ensure that we have a valid MFN before proceeding.
     *
     * If the MFN is invalid, pt_update() might misinterpret the operation,
     * treating it as either a population, a mapping destruction,
     * or a mapping modification.
     */
    ASSERT(!mfn_eq(mfn, INVALID_MFN) && (flags & PTE_VALID));

    return pt_update(virt, mfn, nr_mfns, flags);
}

int destroy_crux_mappings(unsigned long s, unsigned long e)
{
    ASSERT(IS_ALIGNED(s, PAGE_SIZE));
    ASSERT(IS_ALIGNED(e, PAGE_SIZE));

    return s < e ? pt_update(s, INVALID_MFN, PFN_DOWN(e - s), 0) : -EINVAL;
}

int __init populate_pt_range(unsigned long virt, unsigned long nr_mfns)
{
    return pt_update(virt, INVALID_MFN, nr_mfns, PTE_POPULATE);
}

/* Map a 4k page in a fixmap entry */
void set_fixmap(unsigned int map, mfn_t mfn, pte_attr_t flags)
{
    if ( map_pages_to_crux(FIXMAP_ADDR(map), mfn, 1, flags | PTE_SMALL) != 0 )
        BUG();
}

/* Remove a mapping from a fixmap entry */
void clear_fixmap(unsigned int map)
{
    if ( destroy_crux_mappings(FIXMAP_ADDR(map),
                              FIXMAP_ADDR(map) + PAGE_SIZE) != 0 )
        BUG();
}
