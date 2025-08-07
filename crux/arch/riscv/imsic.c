/* SPDX-License-Identifier: MIT */

/*
 * crux/arch/riscv/imsic.c
 *
 * RISC-V Incoming MSI Controller support
 *
 * (c) Microchip Technology Inc.
 * (c) Vates
 */

#include <crux/bitops.h>
#include <crux/const.h>
#include <crux/cpumask.h>
#include <crux/device_tree.h>
#include <crux/errno.h>
#include <crux/init.h>
#include <crux/macros.h>
#include <crux/smp.h>
#include <crux/spinlock.h>
#include <crux/xvmalloc.h>

#include <asm/imsic.h>

#define IMSIC_HART_SIZE(guest_bits) (BIT(guest_bits, U) * IMSIC_MMIO_PAGE_SZ)

struct imsic_mmios {
    paddr_t base_addr;
    unsigned long size;
};

static struct imsic_config imsic_cfg = {
    .lock = SPIN_LOCK_UNLOCKED,
};

#define IMSIC_DISABLE_EIDELIVERY    0
#define IMSIC_ENABLE_EIDELIVERY     1
#define IMSIC_DISABLE_EITHRESHOLD   1
#define IMSIC_ENABLE_EITHRESHOLD    0

#define imsic_csr_write(c, v)   \
do {                            \
    csr_write(CSR_SISELECT, c); \
    csr_write(CSR_SIREG, v);    \
} while (0)

#define imsic_csr_set(c, v)     \
do {                            \
    csr_write(CSR_SISELECT, c); \
    csr_set(CSR_SIREG, v);      \
} while (0)

#define imsic_csr_clear(c, v)   \
do {                            \
    csr_write(CSR_SISELECT, c); \
    csr_clear(CSR_SIREG, v);    \
} while (0)

void __init imsic_ids_local_delivery(bool enable)
{
    if ( enable )
    {
        imsic_csr_write(IMSIC_EITHRESHOLD, IMSIC_ENABLE_EITHRESHOLD);
        imsic_csr_write(IMSIC_EIDELIVERY, IMSIC_ENABLE_EIDELIVERY);
    }
    else
    {
        imsic_csr_write(IMSIC_EITHRESHOLD, IMSIC_DISABLE_EITHRESHOLD);
        imsic_csr_write(IMSIC_EIDELIVERY, IMSIC_DISABLE_EIDELIVERY);
    }
}

static void imsic_local_eix_update(unsigned long base_id, unsigned long num_id,
                                   bool pend, bool val)
{
    unsigned long id = base_id, last_id = base_id + num_id;

    while ( id < last_id )
    {
        unsigned long isel, ireg;
        unsigned long start_id = id & (__riscv_xlen - 1);
        unsigned long chunk = __riscv_xlen - start_id;
        unsigned long count = min(last_id - id, chunk);

        isel = id / __riscv_xlen;
        isel *= __riscv_xlen / IMSIC_EIPx_BITS;
        isel += pend ? IMSIC_EIP0 : IMSIC_EIE0;

        ireg = GENMASK(start_id + count - 1, start_id);

        id += count;

        if ( val )
            imsic_csr_set(isel, ireg);
        else
            imsic_csr_clear(isel, ireg);
    }
}

void imsic_irq_enable(unsigned int irq)
{
    /*
     * The only caller of imsic_irq_enable() is aplic_irq_enable(), which
     * already runs with IRQs disabled. Therefore, there's no need to use
     * spin_lock_irqsave() in this function.
     *
     * This ASSERT is added as a safeguard: if imsic_irq_enable() is ever
     * called from a context where IRQs are not disabled,
     * spin_lock_irqsave() should be used instead of spin_lock().
     */
    ASSERT(!local_irq_is_enabled());

    spin_lock(&imsic_cfg.lock);
    /*
     * There is no irq - 1 here (look at aplic_set_irq_type()) because:
     * From the spec:
     *   When an interrupt file supports distinct interrupt identities,
     *   valid identity numbers are between 1 and inclusive. The identity
     *   numbers within this range are said to be implemented by the interrupt
     *   file; numbers outside this range are not implemented. The number zero
     *   is never a valid interrupt identity.
     *   ...
     *   Bit positions in a valid eiek register that don’t correspond to a
     *   supported interrupt identity (such as bit 0 of eie0) are read-only zeros.
     *
     * So in EIx registers interrupt i corresponds to bit i in comparison wiht
     * APLIC's sourcecfg which starts from 0.
     */
    imsic_local_eix_update(irq, 1, false, true);
    spin_unlock(&imsic_cfg.lock);
}

void imsic_irq_disable(unsigned int irq)
{
   /*
    * The only caller of imsic_irq_disable() is aplic_irq_disable(), which
    * already runs with IRQs disabled. Therefore, there's no need to use
    * spin_lock_irqsave() in this function.
    *
    * This ASSERT is added as a safeguard: if imsic_irq_disable() is ever
    * called from a context where IRQs are not disabled,
    * spin_lock_irqsave() should be used instead of spin_lock().
    */
    ASSERT(!local_irq_is_enabled());

    spin_lock(&imsic_cfg.lock);
    imsic_local_eix_update(irq, 1, false, false);
    spin_unlock(&imsic_cfg.lock);
}

/* Callers aren't intended to changed imsic_cfg so return const. */
const struct imsic_config *imsic_get_config(void)
{
    return &imsic_cfg;
}

static int __init imsic_get_parent_hartid(const struct dt_device_node *node,
                                          unsigned int index,
                                          unsigned long *hartid)
{
    int res;
    struct dt_phandle_args args;

    res = dt_parse_phandle_with_args(node, "interrupts-extended",
                                     "#interrupt-cells", index, &args);
    if ( !res )
        res = dt_processor_hartid(args.np->parent, hartid);

    return res;
}

/*
 * Parses IMSIC DT node.
 *
 * Returns 0 if initialization is successful, a negative value on failure,
 * or IRQ_M_EXT if the IMSIC node corresponds to a machine-mode IMSIC,
 * which should be ignored by the hypervisor.
 */
static int imsic_parse_node(const struct dt_device_node *node,
                            unsigned int *nr_parent_irqs,
                            unsigned int *nr_mmios)
{
    int rc;
    unsigned int tmp;
    paddr_t base_addr;
    uint32_t *irq_range;

    *nr_parent_irqs = dt_number_of_irq(node);
    if ( !*nr_parent_irqs )
        panic("%s: irq_num can't be 0. Check %s node\n", __func__,
              dt_node_full_name(node));

    irq_range = xvzalloc_array(uint32_t, *nr_parent_irqs * 2);
    if ( !irq_range )
        panic("%s: irq_range[] allocation failed\n", __func__);

    if ( (rc = dt_property_read_u32_array(node, "interrupts-extended",
                                          irq_range, *nr_parent_irqs * 2)) )
        panic("%s: unable to find interrupt-extended in %s node: %d\n",
              __func__, dt_node_full_name(node), rc);

    /* Check that interrupts-extended property is well-formed. */
    for ( unsigned int i = 2; i < (*nr_parent_irqs * 2); i += 2 )
    {
        if ( irq_range[i + 1] != irq_range[1] )
            panic("%s: mode[%u] != %u\n", __func__, i + 1, irq_range[1]);
    }

    if ( irq_range[1] == IRQ_M_EXT )
    {
        /* Machine mode imsic node, ignore it. */
        xvfree(irq_range);

        return IRQ_M_EXT;
    }

    xvfree(irq_range);

    if ( !dt_property_read_u32(node, "riscv,guest-index-bits",
                               &imsic_cfg.guest_index_bits) )
        imsic_cfg.guest_index_bits = 0;
    tmp = BITS_PER_LONG - IMSIC_MMIO_PAGE_SHIFT;
    if ( tmp < imsic_cfg.guest_index_bits )
    {
        printk(CRUXLOG_ERR "%s: guest index bits too big\n",
               dt_node_name(node));
        return -ENOENT;
    }

    /* Find number of HART index bits */
    if ( !dt_property_read_u32(node, "riscv,hart-index-bits",
                               &imsic_cfg.hart_index_bits) )
        /* Assume default value */
        imsic_cfg.hart_index_bits = fls(*nr_parent_irqs - 1);
    tmp -= imsic_cfg.guest_index_bits;
    if ( tmp < imsic_cfg.hart_index_bits )
    {
        printk(CRUXLOG_ERR "%s: HART index bits too big\n",
               dt_node_name(node));
        return -ENOENT;
    }

    /* Find number of group index bits */
    if ( !dt_property_read_u32(node, "riscv,group-index-bits",
                               &imsic_cfg.group_index_bits) )
        imsic_cfg.group_index_bits = 0;
    tmp -= imsic_cfg.hart_index_bits;
    if ( tmp < imsic_cfg.group_index_bits )
    {
        printk(CRUXLOG_ERR "%s: group index bits too big\n",
               dt_node_name(node));
        return -ENOENT;
    }

    /* Find first bit position of group index */
    tmp = IMSIC_MMIO_PAGE_SHIFT * 2;
    if ( !dt_property_read_u32(node, "riscv,group-index-shift",
                               &imsic_cfg.group_index_shift) )
        imsic_cfg.group_index_shift = tmp;
    if ( imsic_cfg.group_index_shift < tmp )
    {
        printk(CRUXLOG_ERR "%s: group index shift too small\n",
               dt_node_name(node));
        return -ENOENT;
    }
    tmp = imsic_cfg.group_index_bits + imsic_cfg.group_index_shift - 1;
    if ( tmp >= BITS_PER_LONG )
    {
        printk(CRUXLOG_ERR "%s: group index shift too big\n",
               dt_node_name(node));
        return -ENOENT;
    }

    /* Find number of interrupt identities */
    if ( !dt_property_read_u32(node, "riscv,num-ids", &imsic_cfg.nr_ids) )
    {
        printk(CRUXLOG_ERR "%s: number of interrupt identities not found\n",
               node->name);
        return -ENOENT;
    }

    if ( (imsic_cfg.nr_ids < IMSIC_MIN_ID) ||
         (imsic_cfg.nr_ids > IMSIC_MAX_ID) )
    {
        printk(CRUXLOG_ERR "%s: invalid number of interrupt identities\n",
               node->name);
        return -ENOENT;
    }

    /* Compute base address */
    *nr_mmios = 0;
    rc = dt_device_get_address(node, *nr_mmios, &base_addr, NULL);
    if ( rc )
    {
        printk(CRUXLOG_ERR "%s: first MMIO resource not found: %d\n",
               dt_node_name(node), rc);
        return rc;
    }

    imsic_cfg.base_addr = base_addr;
    imsic_cfg.base_addr &= ~(BIT(imsic_cfg.guest_index_bits +
                                 imsic_cfg.hart_index_bits +
                                 IMSIC_MMIO_PAGE_SHIFT, UL) - 1);
    imsic_cfg.base_addr &= ~((BIT(imsic_cfg.group_index_bits, UL) - 1) <<
                             imsic_cfg.group_index_shift);

    /* Find number of MMIO register sets */
    do {
        ++*nr_mmios;
    } while ( !dt_device_get_address(node, *nr_mmios, &base_addr, NULL) );

    return 0;
}

/*
 * Initialize the imsic_cfg structure based on the IMSIC DT node.
 *
 * Returns 0 if initialization is successful, a negative value on failure,
 * or IRQ_M_EXT if the IMSIC node corresponds to a machine-mode IMSIC,
 * which should be ignored by the hypervisor.
 */
int __init imsic_init(const struct dt_device_node *node)
{
    int rc;
    unsigned long reloff, hartid;
    unsigned int nr_parent_irqs, index, nr_handlers = 0;
    paddr_t base_addr;
    unsigned int nr_mmios;
    struct imsic_mmios *mmios;
    struct imsic_msi *msi = NULL;

    /* Parse IMSIC node */
    rc = imsic_parse_node(node, &nr_parent_irqs, &nr_mmios);
    /*
     * If machine mode imsic node => ignore it.
     * If rc < 0 => parsing of IMSIC DT node failed.
     */
    if ( (rc == IRQ_M_EXT) || (rc < 0) )
        return rc;

    /* Allocate MMIO resource array */
    mmios = xvzalloc_array(struct imsic_mmios, nr_mmios);
    if ( !mmios )
    {
        rc = -ENOMEM;
        goto imsic_init_err;
    }

    msi = xvzalloc_array(struct imsic_msi, nr_parent_irqs);
    if ( !msi )
    {
        rc = -ENOMEM;
        goto imsic_init_err;
    }

    /* Check MMIO register sets */
    for ( unsigned int i = 0; i < nr_mmios; i++ )
    {
        unsigned int guest_bits = imsic_cfg.guest_index_bits;
        unsigned long expected_mmio_size =
            IMSIC_HART_SIZE(guest_bits) * nr_parent_irqs;

        rc = dt_device_get_address(node, i, &mmios[i].base_addr,
                                   &mmios[i].size);
        if ( rc )
        {
            printk(CRUXLOG_ERR "%s: unable to parse MMIO regset %u\n",
                   node->name, i);
            goto imsic_init_err;
        }

        base_addr = mmios[i].base_addr;
        base_addr &= ~(BIT(guest_bits +
                           imsic_cfg.hart_index_bits +
                           IMSIC_MMIO_PAGE_SHIFT, UL) - 1);
        base_addr &= ~((BIT(imsic_cfg.group_index_bits, UL) - 1) <<
                       imsic_cfg.group_index_shift);
        if ( base_addr != imsic_cfg.base_addr )
        {
            rc = -EINVAL;
            printk(CRUXLOG_ERR "%s: address mismatch for regset %u\n",
                   node->name, i);
            goto imsic_init_err;
        }

        if ( mmios[i].size != expected_mmio_size )
        {
            rc = -EINVAL;
            printk(CRUXLOG_ERR "%s: IMSIC MMIO size is incorrect %ld, expected MMIO size: %ld\n",
                   node->name, mmios[i].size, expected_mmio_size);
            goto imsic_init_err;
        }
    }

    /* Configure handlers for target CPUs */
    for ( unsigned int i = 0; i < nr_parent_irqs; i++ )
    {
        unsigned int cpu;

        rc = imsic_get_parent_hartid(node, i, &hartid);
        if ( rc )
        {
            printk(CRUXLOG_WARNING "%s: cpu ID for parent irq%u not found\n",
                   node->name, i);
            continue;
        }

        cpu = hartid_to_cpuid(hartid);

        /*
         * If .base_addr is not 0, it indicates that the CPU has already been
         * found.
         * In this case, skip re-initialization to avoid duplicate setup.
         * Also, print a warning message to signal that the DTS should be
         * reviewed for possible duplication.
         */
        if ( msi[cpu].base_addr )
        {
            printk("%s: cpu%u is found twice in interrupts-extended prop\n",
                   node->name, cpu);
            continue;
        }

        if ( cpu >= num_possible_cpus() )
        {
            printk(CRUXLOG_WARNING "%s: unsupported hart ID=%#lx for parent irq%u\n",
                   node->name, hartid, i);
            continue;
        }

        /* Find MMIO location of MSI page */
        reloff = i * IMSIC_HART_SIZE(imsic_cfg.guest_index_bits);
        for ( index = 0; index < nr_mmios; index++ )
        {
            if ( reloff < mmios[index].size )
                break;

            /*
             * MMIO region size may not be aligned to
             * IMSIC_HART_SIZE(guest_index_bits) if
             * holes are present.
             */
            reloff -= ROUNDUP(mmios[index].size,
                              IMSIC_HART_SIZE(imsic_cfg.guest_index_bits));
        }

        if ( index == nr_mmios )
        {
            printk(CRUXLOG_WARNING "%s: MMIO not found for parent irq%u\n",
                   node->name, i);
            continue;
        }

        if ( !IS_ALIGNED(mmios[cpu].base_addr + reloff,
                         IMSIC_MMIO_PAGE_SZ) )
        {
            printk(CRUXLOG_WARNING "%s: MMIO address %#lx is not aligned on a page\n",
                   node->name, msi[cpu].base_addr + reloff);
            continue;
        }

        msi[cpu].base_addr = mmios[index].base_addr;
        msi[cpu].offset = reloff;

        nr_handlers++;
    }

    if ( !nr_handlers )
    {
        printk(CRUXLOG_ERR "%s: No CPU handlers found\n", node->name);
        rc = -ENODEV;
        goto imsic_init_err;
    }

    /* Enable local interrupt delivery */
    imsic_ids_local_delivery(true);

    imsic_cfg.msi = msi;

    xvfree(mmios);

    return 0;

 imsic_init_err:
    xvfree(mmios);
    xvfree(msi);

    return rc;
}
