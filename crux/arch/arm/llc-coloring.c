/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Last Level Cache (LLC) coloring support for ARM
 *
 * Copyright (C) 2024, Advanced Micro Devices, Inc.
 * Copyright (C) 2024, Minerva Systems SRL
 */
#include <crux/init.h>
#include <crux/llc-coloring.h>

#include <asm/processor.h>
#include <asm/setup.h>
#include <asm/sysregs.h>
#include <asm/system.h>

/* Return the LLC way size by probing the hardware */
unsigned int __init get_llc_way_size(void)
{
    register_t ccsidr_el1;
    register_t clidr_el1 = READ_SYSREG(CLIDR_EL1);
    register_t csselr_el1 = READ_SYSREG(CSSELR_EL1);
    register_t id_aa64mmfr2_el1 = READ_SYSREG(ID_AA64MMFR2_EL1);
    uint32_t ccsidr_numsets_shift = CCSIDR_NUMSETS_SHIFT;
    uint32_t ccsidr_numsets_mask = CCSIDR_NUMSETS_MASK;
    unsigned int n, line_size, num_sets;

    for ( n = CLIDR_CTYPEn_LEVELS; n != 0; n-- )
    {
        uint8_t ctype_n = (clidr_el1 >> CLIDR_CTYPEn_SHIFT(n)) &
                           CLIDR_CTYPEn_MASK;

        /* Unified cache (see Arm ARM DDI 0487J.a D19.2.27) */
        if ( ctype_n == 0b100 )
            break;
    }

    if ( n == 0 )
        return 0;

    WRITE_SYSREG((n - 1) << CSSELR_LEVEL_SHIFT, CSSELR_EL1);
    isb();

    ccsidr_el1 = READ_SYSREG(CCSIDR_EL1);

    /* Arm ARM: (Log2(Number of bytes in cache line)) - 4 */
    line_size = 1U << ((ccsidr_el1 & CCSIDR_LINESIZE_MASK) + 4);

    /* If FEAT_CCIDX is enabled, CCSIDR_EL1 has a different bit layout */
    if ( (id_aa64mmfr2_el1 >> ID_AA64MMFR2_CCIDX_SHIFT) & 0x7 )
    {
        ccsidr_numsets_shift = CCSIDR_NUMSETS_SHIFT_FEAT_CCIDX;
        ccsidr_numsets_mask = CCSIDR_NUMSETS_MASK_FEAT_CCIDX;
    }

    /* Arm ARM: (Number of sets in cache) - 1 */
    num_sets = ((ccsidr_el1 >> ccsidr_numsets_shift) & ccsidr_numsets_mask) + 1;

    printk(CRUXLOG_INFO "LLC found: L%u (line size: %u bytes, sets num: %u)\n",
           n, line_size, num_sets);

    /* Restore value in CSSELR_EL1 */
    WRITE_SYSREG(csselr_el1, CSSELR_EL1);
    isb();

    return line_size * num_sets;
}

/*
 * get_crux_paddr - get physical address to relocate crux to
 *
 * crux is relocated to as near to the top of RAM as possible and
 * aligned to a CRUX_PADDR_ALIGN boundary.
 */
static paddr_t __init get_crux_paddr(paddr_t crux_size)
{
    const struct membanks *mem = bootinfo_get_mem();
    paddr_t min_size, paddr = 0;
    unsigned int i;

    min_size = ROUNDUP(crux_size, CRUX_PADDR_ALIGN);

    /* Find the highest bank with enough space. */
    for ( i = 0; i < mem->nr_banks; i++ )
    {
        const struct membank *bank = &mem->bank[i];
        paddr_t s, e;

        if ( bank->size >= min_size )
        {
            e = consider_modules(bank->start, bank->start + bank->size,
                                 min_size, CRUX_PADDR_ALIGN, 0);
            if ( !e )
                continue;

#ifdef CONFIG_ARM_32
            /* crux must be under 4GB */
            if ( e > GB(4) )
                e = GB(4);
            if ( e < bank->start )
                continue;
#endif

            s = e - min_size;

            if ( s > paddr )
                paddr = s;
        }
    }

    if ( !paddr )
        panic("Not enough memory to relocate crux\n");

    printk("Placing crux at 0x%"PRIpaddr"-0x%"PRIpaddr"\n",
           paddr, paddr + min_size);

    return paddr;
}

static paddr_t __init crux_colored_map_size(void)
{
    return ROUNDUP((_end - _start) * get_max_nr_llc_colors(), CRUX_PADDR_ALIGN);
}

void __init arch_llc_coloring_init(void)
{
    struct boot_module *crux_boot_module = boot_module_find_by_kind(BOOTMOD_CRUX);

    BUG_ON(!crux_boot_module);

    crux_boot_module->size = crux_colored_map_size();
    crux_boot_module->start = get_crux_paddr(crux_boot_module->size);
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
