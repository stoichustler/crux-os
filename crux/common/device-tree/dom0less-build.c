/* SPDX-License-Identifier: GPL-2.0-only */

#include <crux/bootinfo.h>
#include <crux/device_tree.h>
#include <crux/dom0less-build.h>
#include <crux/domain.h>
#include <crux/domain_page.h>
#include <crux/err.h>
#include <crux/event.h>
#include <crux/fdt-domain-build.h>
#include <crux/fdt-kernel.h>
#include <crux/grant_table.h>
#include <crux/init.h>
#include <crux/iocap.h>
#include <crux/iommu.h>
#include <crux/libfdt/libfdt.h>
#include <crux/llc-coloring.h>
#include <crux/sizes.h>
#include <crux/sched.h>
#include <crux/stdbool.h>
#include <crux/types.h>
#include <crux/vmap.h>

#include <public/bootfdt.h>
#include <public/domctl.h>
#include <public/event_channel.h>
#include <public/io/xs_wire.h>

#include <asm/setup.h>

#include <crux/static-memory.h>
#include <crux/static-shmem.h>

#define CRUXSTORE_PFN_LATE_ALLOC UINT64_MAX

static domid_t __initdata xs_domid = DOMID_INVALID;
static bool __initdata need_cruxstore;

void __init set_xs_domain(struct domain *d)
{
    if ( xs_domid != DOMID_INVALID )
        panic("Only 1 cruxstore domain can be specified! (%u)", xs_domid);

    xs_domid = d->domain_id;
    set_global_virq_handler(d, VIRQ_DOM_EXC);
}

bool __init is_dom0less_mode(void)
{
    struct boot_modules *mods = &bootinfo.modules;
    struct boot_module *mod;
    unsigned int i;
    bool dom0found = false;
    bool domUfound = false;

    /* Look into the boot_modules */
    for ( i = 0 ; i < mods->nr_mods ; i++ )
    {
        mod = &mods->module[i];
        /* Find if dom0 and domU kernels are present */
        if ( mod->kind == BOOTMOD_KERNEL )
        {
            if ( mod->domU == false )
            {
                dom0found = true;
                break;
            }
            else
                domUfound = true;
        }
    }

    /*
     * If there is no dom0 kernel but at least one domU, then we are in
     * dom0less mode
     */
    return ( !dom0found && domUfound );
}

static int __init alloc_cruxstore_evtchn(struct domain *d)
{
    evtchn_alloc_unbound_t alloc;
    int rc;

    alloc.dom = d->domain_id;
    alloc.remote_dom = xs_domid;
    rc = evtchn_alloc_unbound(&alloc, 0);
    if ( rc )
    {
        printk("Failed allocating event channel for domain\n");
        return rc;
    }

    d->arch.hvm.params[HVM_PARAM_STORE_EVTCHN] = alloc.port;

    return 0;
}

static void __init initialize_domU_cruxstore(void)
{
    struct domain *d;

    if ( xs_domid == DOMID_INVALID )
        return;

    for_each_domain( d )
    {
        uint64_t gfn = d->arch.hvm.params[HVM_PARAM_STORE_PFN];
        int rc;

        if ( gfn == 0 )
            continue;

        if ( is_cruxstore_domain(d) )
            continue;

        rc = alloc_cruxstore_evtchn(d);
        if ( rc < 0 )
            panic("%pd: Failed to allocate cruxstore_evtchn\n", d);

        if ( gfn != CRUXSTORE_PFN_LATE_ALLOC && IS_ENABLED(CONFIG_GRANT_TABLE) )
        {
            ASSERT(gfn < UINT32_MAX);
            gnttab_seed_entry(d, GNTTAB_RESERVED_CRUXSTORE, xs_domid, gfn);
        }
    }
}

/*
 * Scan device tree properties for passthrough specific information.
 * Returns < 0 on error
 *         0 on success
 */
static int __init handle_passthrough_prop(struct kernel_info *kinfo,
                                          const struct fdt_property *crux_reg,
                                          const struct fdt_property *crux_path,
                                          bool crux_force,
                                          uint32_t address_cells,
                                          uint32_t size_cells)
{
    const __be32 *cell;
    unsigned int i, len;
    struct dt_device_node *node;
    int res;
    paddr_t mstart, size, gstart;

    if ( !kinfo->crux_reg_assigned )
    {
        kinfo->crux_reg_assigned = rangeset_new(NULL, NULL, 0);

        if ( !kinfo->crux_reg_assigned )
            return -ENOMEM;
    }

    /* crux,reg specifies where to map the MMIO region */
    cell = (const __be32 *)crux_reg->data;
    len = fdt32_to_cpu(crux_reg->len) / ((address_cells * 2 + size_cells) *
                                        sizeof(uint32_t));

    for ( i = 0; i < len; i++ )
    {
        device_tree_get_reg(&cell, address_cells, size_cells,
                            &mstart, &size);
        gstart = dt_next_cell(address_cells, &cell);

        if ( gstart & ~PAGE_MASK || mstart & ~PAGE_MASK || size & ~PAGE_MASK )
        {
            printk(CRUXLOG_ERR
                   "domU passthrough config has not page aligned addresses/sizes\n");
            return -EINVAL;
        }

        res = iomem_permit_access(kinfo->bd.d, paddr_to_pfn(mstart),
                                  paddr_to_pfn(PAGE_ALIGN(mstart + size - 1)));
        if ( res )
        {
            printk(CRUXLOG_ERR "Unable to permit to dom%d access to"
                   " 0x%"PRIpaddr" - 0x%"PRIpaddr"\n",
                   kinfo->bd.d->domain_id,
                   mstart & PAGE_MASK, PAGE_ALIGN(mstart + size) - 1);
            return res;
        }

        res = map_regions_p2mt(kinfo->bd.d,
                               gaddr_to_gfn(gstart),
                               PFN_DOWN(size),
                               maddr_to_mfn(mstart),
                               p2m_mmio_direct_dev);
        if ( res < 0 )
        {
            printk(CRUXLOG_ERR
                   "Failed to map %"PRIpaddr" to the guest at%"PRIpaddr"\n",
                   mstart, gstart);
            return -EFAULT;
        }

        res = rangeset_add_range(kinfo->crux_reg_assigned, PFN_DOWN(gstart),
                                 PFN_DOWN(gstart + size - 1));
        if ( res )
            return res;
    }

    /*
     * If crux_force, we let the user assign a MMIO region with no
     * associated path.
     */
    if ( crux_path == NULL )
        return crux_force ? 0 : -EINVAL;

    /*
     * crux,path specifies the corresponding node in the host DT.
     * Both interrupt mappings and IOMMU settings are based on it,
     * as they are done based on the corresponding host DT node.
     */
    node = dt_find_node_by_path(crux_path->data);
    if ( node == NULL )
    {
        printk(CRUXLOG_ERR "Couldn't find node %s in host_dt!\n",
               crux_path->data);
        return -EINVAL;
    }

    res = map_device_irqs_to_domain(kinfo->bd.d, node, true, NULL);
    if ( res < 0 )
        return res;

    res = iommu_add_dt_device(node);
    if ( res < 0 )
        return res;

    /* If crux_force, we allow assignment of devices without IOMMU protection. */
    if ( crux_force && !dt_device_is_protected(node) )
        return 0;

    return iommu_assign_dt_device(kinfo->bd.d, node);
}

static int __init handle_prop_pfdt(struct kernel_info *kinfo,
                                   const void *pfdt, int nodeoff,
                                   uint32_t address_cells, uint32_t size_cells,
                                   bool scan_passthrough_prop)
{
    void *fdt = kinfo->fdt;
    int propoff, nameoff, res;
    const struct fdt_property *prop, *crux_reg = NULL, *crux_path = NULL;
    const char *name;
    bool found, crux_force = false;

    for ( propoff = fdt_first_property_offset(pfdt, nodeoff);
          propoff >= 0;
          propoff = fdt_next_property_offset(pfdt, propoff) )
    {
        if ( !(prop = fdt_get_property_by_offset(pfdt, propoff, NULL)) )
            return -FDT_ERR_INTERNAL;

        found = false;
        nameoff = fdt32_to_cpu(prop->nameoff);
        name = fdt_string(pfdt, nameoff);

        if ( scan_passthrough_prop )
        {
            if ( dt_prop_cmp("crux,reg", name) == 0 )
            {
                crux_reg = prop;
                found = true;
            }
            else if ( dt_prop_cmp("crux,path", name) == 0 )
            {
                crux_path = prop;
                found = true;
            }
            else if ( dt_prop_cmp("crux,force-assign-without-iommu",
                                  name) == 0 )
            {
                crux_force = true;
                found = true;
            }
        }

        /*
         * Copy properties other than the ones above: crux,reg, crux,path,
         * and crux,force-assign-without-iommu.
         */
        if ( !found )
        {
            res = fdt_property(fdt, name, prop->data, fdt32_to_cpu(prop->len));
            if ( res )
                return res;
        }
    }

    /*
     * Only handle passthrough properties if both crux,reg and crux,path
     * are present, or if crux,force-assign-without-iommu is specified.
     */
    if ( crux_reg != NULL && (crux_path != NULL || crux_force) )
    {
        res = handle_passthrough_prop(kinfo, crux_reg, crux_path, crux_force,
                                      address_cells, size_cells);
        if ( res < 0 )
        {
            printk(CRUXLOG_ERR "Failed to assign device to %pd\n", kinfo->bd.d);
            return res;
        }
    }
    else if ( (crux_path && !crux_reg) || (crux_reg && !crux_path && !crux_force) )
    {
        printk(CRUXLOG_ERR "crux,reg or crux,path missing for %pd\n",
               kinfo->bd.d);
        return -EINVAL;
    }

    /* FDT_ERR_NOTFOUND => There is no more properties for this node */
    return ( propoff != -FDT_ERR_NOTFOUND ) ? propoff : 0;
}

static int __init scan_pfdt_node(struct kernel_info *kinfo, const void *pfdt,
                                 int nodeoff,
                                 uint32_t address_cells, uint32_t size_cells,
                                 bool scan_passthrough_prop)
{
    int rc = 0;
    void *fdt = kinfo->fdt;
    int node_next;

    rc = fdt_begin_node(fdt, fdt_get_name(pfdt, nodeoff, NULL));
    if ( rc )
        return rc;

    rc = handle_prop_pfdt(kinfo, pfdt, nodeoff, address_cells, size_cells,
                          scan_passthrough_prop);
    if ( rc )
        return rc;

    address_cells = device_tree_get_u32(pfdt, nodeoff, "#address-cells",
                                        DT_ROOT_NODE_ADDR_CELLS_DEFAULT);
    size_cells = device_tree_get_u32(pfdt, nodeoff, "#size-cells",
                                     DT_ROOT_NODE_SIZE_CELLS_DEFAULT);

    node_next = fdt_first_subnode(pfdt, nodeoff);
    while ( node_next > 0 )
    {
        rc = scan_pfdt_node(kinfo, pfdt, node_next, address_cells, size_cells,
                            scan_passthrough_prop);
        if ( rc )
            return rc;

        node_next = fdt_next_subnode(pfdt, node_next);
    }

    return fdt_end_node(fdt);
}

static int __init check_partial_fdt(void *pfdt, size_t size)
{
    int res;

    if ( fdt_magic(pfdt) != FDT_MAGIC )
    {
        dprintk(CRUXLOG_ERR, "Partial FDT is not a valid Flat Device Tree");
        return -EINVAL;
    }

    res = fdt_check_header(pfdt);
    if ( res )
    {
        dprintk(CRUXLOG_ERR, "Failed to check the partial FDT (%d)", res);
        return -EINVAL;
    }

    if ( fdt_totalsize(pfdt) > size )
    {
        dprintk(CRUXLOG_ERR, "Partial FDT totalsize is too big");
        return -EINVAL;
    }

    return 0;
}

static int __init domain_handle_dtb_boot_module(struct domain *d,
                                                struct kernel_info *kinfo)
{
    void *pfdt;
    int res, node_next;

    pfdt = ioremap_cache(kinfo->dtb->start, kinfo->dtb->size);
    if ( pfdt == NULL )
        return -EFAULT;

    res = check_partial_fdt(pfdt, kinfo->dtb->size);
    if ( res < 0 )
        goto out;

    for ( node_next = fdt_first_subnode(pfdt, 0);
          node_next > 0;
          node_next = fdt_next_subnode(pfdt, node_next) )
    {
        const char *name = fdt_get_name(pfdt, node_next, NULL);

        if ( name == NULL )
            continue;

        /*
         * Only scan /$(interrupt_controller) /aliases /passthrough,
         * ignore the rest.
         * They don't have to be parsed in order.
         *
         * Take the interrupt controller phandle value from the special
         * interrupt controller node in the DTB fragment.
         */
        if ( init_intc_phandle(kinfo, name, node_next, pfdt) == 0 )
            continue;

        if ( dt_node_cmp(name, "aliases") == 0 )
        {
            res = scan_pfdt_node(kinfo, pfdt, node_next,
                                 DT_ROOT_NODE_ADDR_CELLS_DEFAULT,
                                 DT_ROOT_NODE_SIZE_CELLS_DEFAULT,
                                 false);
            if ( res )
                goto out;
            continue;
        }
        if ( dt_node_cmp(name, "passthrough") == 0 )
        {
            res = scan_pfdt_node(kinfo, pfdt, node_next,
                                 DT_ROOT_NODE_ADDR_CELLS_DEFAULT,
                                 DT_ROOT_NODE_SIZE_CELLS_DEFAULT,
                                 true);
            if ( res )
                goto out;
            continue;
        }
    }

 out:
    iounmap(pfdt);

    return res;
}

/*
 * The max size for DT is 2MB. However, the generated DT is small (not including
 * domU passthrough DT nodes whose size we account separately), 4KB are enough
 * for now, but we might have to increase it in the future.
 */
#define DOMU_DTB_SIZE 4096
static int __init prepare_dtb_domU(struct domain *d, struct kernel_info *kinfo)
{
    int addrcells, sizecells;
    int ret, fdt_size = DOMU_DTB_SIZE;

    kinfo->phandle_intc = GUEST_PHANDLE_GIC;

#ifdef CONFIG_GRANT_TABLE
    kinfo->gnttab_start = GUEST_GNTTAB_BASE;
    kinfo->gnttab_size = GUEST_GNTTAB_SIZE;
#endif

    addrcells = GUEST_ROOT_ADDRESS_CELLS;
    sizecells = GUEST_ROOT_SIZE_CELLS;

    /* Account for domU passthrough DT size */
    if ( kinfo->dtb )
        fdt_size += kinfo->dtb->size;

    /* Cap to max DT size if needed */
    fdt_size = min(fdt_size, SZ_2M);

    kinfo->fdt = xmalloc_bytes(fdt_size);
    if ( kinfo->fdt == NULL )
        return -ENOMEM;

    ret = fdt_create(kinfo->fdt, fdt_size);
    if ( ret < 0 )
        goto err;

    ret = fdt_finish_reservemap(kinfo->fdt);
    if ( ret < 0 )
        goto err;

    ret = fdt_begin_node(kinfo->fdt, "");
    if ( ret < 0 )
        goto err;

    ret = fdt_property_cell(kinfo->fdt, "#address-cells", addrcells);
    if ( ret )
        goto err;

    ret = fdt_property_cell(kinfo->fdt, "#size-cells", sizecells);
    if ( ret )
        goto err;

    ret = make_chosen_node(kinfo);
    if ( ret )
        goto err;

    ret = make_cpus_node(d, kinfo->fdt);
    if ( ret )
        goto err;

    ret = make_memory_node(kinfo, addrcells, sizecells,
                           kernel_info_get_mem(kinfo));
    if ( ret )
        goto err;

    ret = make_resv_memory_node(kinfo, addrcells, sizecells);
    if ( ret )
        goto err;

    /*
     * domain_handle_dtb_boot_module has to be called before the rest of
     * the device tree is generated because it depends on the value of
     * the field phandle_intc.
     */
    if ( kinfo->dtb )
    {
        ret = domain_handle_dtb_boot_module(d, kinfo);
        if ( ret )
            goto err;
    }

    ret = make_intc_domU_node(kinfo);
    if ( ret )
        goto err;

    ret = make_timer_node(kinfo);
    if ( ret )
        goto err;

    if ( kinfo->dom0less_feature & DOM0LESS_ENHANCED_NO_XS )
    {
        ret = make_hypervisor_node(d, kinfo, addrcells, sizecells);
        if ( ret )
            goto err;
    }

    ret = make_arch_nodes(kinfo);
    if ( ret )
        goto err;

    ret = fdt_end_node(kinfo->fdt);
    if ( ret < 0 )
        goto err;

    ret = fdt_finish(kinfo->fdt);
    if ( ret < 0 )
        goto err;

    return 0;

  err:
    printk("Device tree generation failed (%d).\n", ret);
    xfree(kinfo->fdt);

    return -EINVAL;
}

#define CRUXSTORE_PFN_OFFSET 1
static int __init alloc_cruxstore_page(struct domain *d)
{
    struct page_info *cruxstore_pg;
    struct cruxstore_domain_interface *interface;
    mfn_t mfn;
    gfn_t gfn;
    int rc;

    if ( (UINT_MAX - d->max_pages) < 1 )
    {
        printk(CRUXLOG_ERR "%pd: Over-allocation for d->max_pages by 1 page.\n",
               d);
        return -EINVAL;
    }

    d->max_pages += 1;
    cruxstore_pg = alloc_domheap_page(d, MEMF_bits(32));
    if ( cruxstore_pg == NULL && is_64bit_domain(d) )
        cruxstore_pg = alloc_domheap_page(d, 0);
    if ( cruxstore_pg == NULL )
        return -ENOMEM;

    mfn = page_to_mfn(cruxstore_pg);
    if ( !mfn_x(mfn) )
        return -ENOMEM;

    if ( !is_domain_direct_mapped(d) )
        gfn = gaddr_to_gfn(GUEST_MAGIC_BASE +
                           (CRUXSTORE_PFN_OFFSET << PAGE_SHIFT));
    else
        gfn = gaddr_to_gfn(mfn_to_maddr(mfn));

    rc = guest_physmap_add_page(d, gfn, mfn, 0);
    if ( rc )
    {
        free_domheap_page(cruxstore_pg);
        return rc;
    }

#ifdef CONFIG_HVM
    d->arch.hvm.params[HVM_PARAM_STORE_PFN] = gfn_x(gfn);
#endif
    interface = map_domain_page(mfn);
    interface->connection = CRUXSTORE_RECONNECT;
    unmap_domain_page(interface);

    return 0;
}

static int __init alloc_cruxstore_params(struct kernel_info *kinfo)
{
    struct domain *d = kinfo->bd.d;
    int rc = 0;

#ifdef CONFIG_HVM
    if ( (kinfo->dom0less_feature & (DOM0LESS_CRUXSTORE | DOM0LESS_XS_LEGACY))
                                 == (DOM0LESS_CRUXSTORE | DOM0LESS_XS_LEGACY) )
        d->arch.hvm.params[HVM_PARAM_STORE_PFN] = CRUXSTORE_PFN_LATE_ALLOC;
    else
#endif
    if ( kinfo->dom0less_feature & DOM0LESS_CRUXSTORE )
    {
        rc = alloc_cruxstore_page(d);
        if ( rc < 0 )
            return rc;
    }

    return rc;
}

static void __init domain_vcpu_affinity(struct domain *d,
                                        const struct dt_device_node *node)
{
    struct dt_device_node *np;

    dt_for_each_child_node(node, np)
    {
        const char *hard_affinity_str = NULL;
        uint32_t val;
        int rc;
        struct vcpu *v;
        cpumask_t affinity;

        if ( !dt_device_is_compatible(np, "crux,vcpu") )
            continue;

        if ( !dt_property_read_u32(np, "id", &val) )
            panic("Invalid crux,vcpu node for domain %s\n", dt_node_name(node));

        if ( val >= d->max_vcpus )
            panic("Invalid vcpu_id %u for domain %s, max_vcpus=%u\n", val,
                  dt_node_name(node), d->max_vcpus);

        v = d->vcpu[val];
        rc = dt_property_read_string(np, "hard-affinity", &hard_affinity_str);
        if ( rc < 0 )
            continue;

        cpumask_clear(&affinity);
        while ( *hard_affinity_str != '\0' )
        {
            unsigned int start, end;

            start = simple_strtoul(hard_affinity_str, &hard_affinity_str, 0);

            if ( *hard_affinity_str == '-' )    /* Range */
            {
                hard_affinity_str++;
                end = simple_strtoul(hard_affinity_str, &hard_affinity_str, 0);
            }
            else                /* Single value */
                end = start;

            if ( end >= nr_cpu_ids )
                panic("Invalid pCPU %u for domain %s\n", end, dt_node_name(node));

            for ( ; start <= end; start++ )
                cpumask_set_cpu(start, &affinity);

            if ( *hard_affinity_str == ',' )
                hard_affinity_str++;
            else if ( *hard_affinity_str != '\0' )
                break;
        }

        rc = vcpu_set_hard_affinity(v, &affinity);
        if ( rc )
            panic("vcpu%d: failed (rc=%d) to set hard affinity for domain %s\n",
                  v->vcpu_id, rc, dt_node_name(node));
    }
}

#ifdef CONFIG_ARCH_PAGING_MEMPOOL
static unsigned long __init domain_p2m_pages(unsigned long maxmem_kb,
                                             unsigned int smp_cpus)
{
    /*
     * Keep in sync with libxl__get_required_paging_memory().
     * 256 pages (1MB) per vcpu, plus 1 page per MiB of RAM for the P2M map,
     * plus 128 pages to cover extended regions.
     */
    unsigned long memkb = 4 * (256 * smp_cpus + (maxmem_kb / 1024) + 128);

    BUILD_BUG_ON(PAGE_SIZE != SZ_4K);

    return DIV_ROUND_UP(memkb, 1024) << (20 - PAGE_SHIFT);
}

static int __init domain_p2m_set_allocation(struct domain *d, uint64_t mem,
                                            const struct dt_device_node *node)
{
    unsigned long p2m_pages;
    uint32_t p2m_mem_mb;
    int rc;

    rc = dt_property_read_u32(node, "crux,domain-p2m-mem-mb", &p2m_mem_mb);
    /* If crux,domain-p2m-mem-mb is not specified, use the default value. */
    p2m_pages = rc ?
                p2m_mem_mb << (20 - PAGE_SHIFT) :
                domain_p2m_pages(mem, d->max_vcpus);

    spin_lock(&d->arch.paging.lock);
    rc = p2m_set_allocation(d, p2m_pages, NULL);
    spin_unlock(&d->arch.paging.lock);

    return rc;
}
#else /* !CONFIG_ARCH_PAGING_MEMPOOL */
static inline int __init domain_p2m_set_allocation(
    struct domain *d, uint64_t mem, const struct dt_device_node *node)
{
    return 0;
}
#endif /* CONFIG_ARCH_PAGING_MEMPOOL */

static int __init construct_domU(struct kernel_info *kinfo,
                          const struct dt_device_node *node)
{
    struct domain *d = kinfo->bd.d;
    const char *dom0less_enhanced;
    int rc;
    u64 mem;

    rc = dt_property_read_u64(node, "memory", &mem);
    if ( !rc )
    {
        printk("Error building domU: cannot read \"memory\" property\n");
        return -EINVAL;
    }
    kinfo->unassigned_mem = (paddr_t)mem * SZ_1K;

    rc = domain_p2m_set_allocation(d, mem, node);
    if ( rc != 0 )
        return rc;

    printk("### LOADING DOMU cpus=%u memory=%#"PRIx64"KB\n",
           d->max_vcpus, mem);

    rc = dt_property_read_string(node, "crux,enhanced", &dom0less_enhanced);
    if ( rc == -EILSEQ ||
         rc == -ENODATA ||
         (rc == 0 && !strcmp(dom0less_enhanced, "enabled")) )
    {
        need_cruxstore = true;
        kinfo->dom0less_feature = DOM0LESS_ENHANCED;
    }
    else if ( rc == 0 && !strcmp(dom0less_enhanced, "legacy") )
    {
        need_cruxstore = true;
        kinfo->dom0less_feature = DOM0LESS_ENHANCED_LEGACY;
    }
    else if ( rc == 0 && !strcmp(dom0less_enhanced, "no-cruxstore") )
        kinfo->dom0less_feature = DOM0LESS_ENHANCED_NO_XS;

    if ( vcpu_create(d, 0) == NULL )
        return -ENOMEM;

    d->max_pages = ((paddr_t)mem * SZ_1K) >> PAGE_SHIFT;

    rc = kernel_probe(kinfo, node);
    if ( rc < 0 )
        return rc;

    set_domain_type(d, kinfo);

    if ( is_hardware_domain(d) )
    {
        rc = construct_hwdom(kinfo, node);
        if ( rc < 0 )
            return rc;
    }
    else
    {
        if ( !dt_find_property(node, "crux,static-mem", NULL) )
            allocate_memory(d, kinfo);
        else if ( !is_domain_direct_mapped(d) )
            allocate_static_memory(d, kinfo, node);
        else
            assign_static_memory_11(d, kinfo, node);

        rc = process_shm(d, kinfo, node);
        if ( rc < 0 )
            return rc;

        rc = init_vuart(d, kinfo, node);
        if ( rc < 0 )
            return rc;

        rc = prepare_dtb_domU(d, kinfo);
        if ( rc < 0 )
            return rc;

        rc = construct_domain(d, kinfo);
        if ( rc < 0 )
            return rc;
    }

    domain_vcpu_affinity(d, node);

    rc = alloc_cruxstore_params(kinfo);

    rangeset_destroy(kinfo->crux_reg_assigned);

    return rc;
}

void __init create_domUs(void)
{
    struct dt_device_node *node;
    const struct dt_device_node *chosen = dt_find_node_by_path("/chosen");

    BUG_ON(chosen == NULL);
    dt_for_each_child_node(chosen, node)
    {
        struct kernel_info ki = KERNEL_INFO_INIT;
        int rc = parse_dom0less_node(node, &ki.bd);

        if ( rc == -ENOENT )
            continue;
        if ( rc )
            panic("Malformed DTB: Invalid domain %s\n", dt_node_name(node));

        if ( (max_init_domid + 1) >= DOMID_FIRST_RESERVED )
            panic("No more domain IDs available\n");

        /*
         * The variable max_init_domid is initialized with zero, so here it's
         * very important to use the pre-increment operator to call
         * domain_create() with a domid > 0. (domid == 0 is reserved for dom0)
         */
        ki.bd.d = domain_create(++max_init_domid,
                                &ki.bd.create_cfg, ki.bd.create_flags);
        if ( IS_ERR(ki.bd.d) )
            panic("Error creating domain %s (rc = %ld)\n",
                  dt_node_name(node), PTR_ERR(ki.bd.d));

#ifdef CONFIG_HAS_LLC_COLORING
        if ( llc_coloring_enabled &&
             (rc = domain_set_llc_colors_from_str(ki.bd.d,
                                                  ki.bd.llc_colors_str)) )
            panic("Error initializing LLC coloring for domain %s (rc = %d)\n",
                  dt_node_name(node), rc);
#endif /* CONFIG_HAS_LLC_COLORING */

        ki.bd.d->is_console = true;
        dt_device_set_used_by(node, ki.bd.d->domain_id);

        rc = construct_domU(&ki, node);
        if ( rc )
            panic("Could not set up domain %s (rc = %d)\n",
                  dt_node_name(node), rc);

        if ( ki.bd.create_cfg.flags & CRUX_DOMCTL_CDF_xs_domain )
            set_xs_domain(ki.bd.d);
    }

    if ( need_cruxstore && xs_domid == DOMID_INVALID )
        panic("cruxstore requested, but cruxstore domain not present\n");

    initialize_domU_cruxstore();
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
