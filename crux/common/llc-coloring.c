/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Last Level Cache (LLC) coloring common code
 *
 * Copyright (C) 2024, Advanced Micro Devices, Inc.
 * Copyright (C) 2024, Minerva Systems SRL
 */
#include <crux/guest_access.h>
#include <crux/keyhandler.h>
#include <crux/llc-coloring.h>
#include <crux/param.h>
#include <crux/sched.h>
#include <crux/types.h>

#define NR_LLC_COLORS          (1U << CONFIG_LLC_COLORS_ORDER)
#define CRUX_DEFAULT_NUM_COLORS 1

/*
 * -1: not specified (disabled unless llc-size and llc-nr-ways present)
 *  0: explicitly disabled through cmdline
 *  1: explicitly enabled through cmdline
 */
static int8_t __initdata opt_llc_coloring = -1;
boolean_param("llc-coloring", opt_llc_coloring);

bool __ro_after_init llc_coloring_enabled;

static unsigned int __initdata llc_size;
size_param("llc-size", llc_size);
static unsigned int __initdata llc_nr_ways;
integer_param("llc-nr-ways", llc_nr_ways);
/* Number of colors available in the LLC */
static unsigned int __ro_after_init max_nr_colors;
/* Default coloring configuration */
static unsigned int __ro_after_init default_colors[NR_LLC_COLORS];

static unsigned int __initdata dom0_colors[NR_LLC_COLORS];
static unsigned int __initdata dom0_num_colors;

static unsigned int __ro_after_init crux_colors[NR_LLC_COLORS];
static unsigned int __ro_after_init crux_num_colors;

#define mfn_color_mask              (max_nr_colors - 1)
#define mfn_to_color(mfn)           (mfn_x(mfn) & mfn_color_mask)
#define get_mfn_with_color(mfn, color) \
    (_mfn((mfn_x(mfn) & ~mfn_color_mask) | (color)))

/*
 * Parse the coloring configuration given in the buf string, following the
 * syntax below.
 *
 * COLOR_CONFIGURATION ::= COLOR | RANGE,...,COLOR | RANGE
 * RANGE               ::= COLOR-COLOR
 *
 * Example: "0,2-6,15-16" represents the set of colors: 0,2,3,4,5,6,15,16.
 */
static int __init parse_color_config(const char *buf, unsigned int colors[],
                                     unsigned int max_num_colors,
                                     unsigned int *num_colors)
{
    const char *s = buf;

    *num_colors = 0;

    while ( *s != '\0' )
    {
        unsigned int color, start, end;

        start = simple_strtoul(s, &s, 0);

        if ( *s == '-' )    /* Range */
        {
            s++;
            end = simple_strtoul(s, &s, 0);
        }
        else                /* Single value */
            end = start;

        if ( start > end || (end - start) > (UINT_MAX - *num_colors) ||
             (*num_colors + (end - start)) >= max_num_colors )
            return -EINVAL;

        /* Colors are range checked in check_colors() */
        for ( color = start; color <= end; color++ )
            colors[(*num_colors)++] = color;

        if ( *s == ',' )
            s++;
        else if ( *s != '\0' )
            break;
    }

    return *s ? -EINVAL : 0;
}

static int __init parse_dom0_colors(const char *s)
{
    return parse_color_config(s, dom0_colors, ARRAY_SIZE(dom0_colors),
                              &dom0_num_colors);
}
custom_param("dom0-llc-colors", parse_dom0_colors);

static int __init parse_crux_colors(const char *s)
{
    return parse_color_config(s, crux_colors, ARRAY_SIZE(crux_colors),
                              &crux_num_colors);
}
custom_param("crux-llc-colors", parse_crux_colors);

static void print_colors(const unsigned int colors[], unsigned int num_colors)
{
    unsigned int i;

    printk("{ ");
    for ( i = 0; i < num_colors; i++ )
    {
        unsigned int start = colors[i], end = start;

        printk("%u", start);

        for ( ; i < num_colors - 1 && end + 1 == colors[i + 1]; i++, end++ )
            ;

        if ( start != end )
            printk("-%u", end);

        if ( i < num_colors - 1 )
            printk(", ");
    }
    printk(" }\n");
}

static bool check_colors(const unsigned int colors[], unsigned int num_colors)
{
    unsigned int i;

    for ( i = 0; i < num_colors; i++ )
    {
        if ( colors[i] >= max_nr_colors )
        {
            printk(CRUXLOG_ERR "LLC color %u >= %u (max allowed)\n", colors[i],
                   max_nr_colors);
            return false;
        }
    }

    return true;
}

void __init llc_coloring_init(void)
{
    unsigned int way_size, i;

    llc_coloring_enabled = (opt_llc_coloring >= 1);
    if ( (opt_llc_coloring != 0) && llc_size && llc_nr_ways )
    {
        llc_coloring_enabled = true;
        way_size = llc_size / llc_nr_ways;
    }
    else if ( !llc_coloring_enabled )
        return;
    else
    {
        way_size = get_llc_way_size();
        if ( !way_size )
            panic("LLC probing failed and 'llc-size' or 'llc-nr-ways' missing\n");
    }

    if ( way_size & ~PAGE_MASK )
        panic("LLC way size must be a multiple of PAGE_SIZE\n");

    /*
     * The maximum number of colors must be a power of 2 in order to correctly
     * map them to bits of an address.
     */
    max_nr_colors = way_size >> PAGE_SHIFT;

    if ( max_nr_colors & (max_nr_colors - 1) )
        panic("Number of LLC colors (%u) isn't a power of 2\n", max_nr_colors);

    if ( max_nr_colors > NR_LLC_COLORS )
    {
        printk(CRUXLOG_WARNING
               "Number of LLC colors (%u) too big. Using configured max %u\n",
               max_nr_colors, NR_LLC_COLORS);
        max_nr_colors = NR_LLC_COLORS;
    }
    else if ( max_nr_colors < 2 )
        panic("Number of LLC colors %u < 2\n", max_nr_colors);

    for ( i = 0; i < max_nr_colors; i++ )
        default_colors[i] = i;

    if ( !crux_num_colors )
    {
        unsigned int i;

        crux_num_colors = MIN(CRUX_DEFAULT_NUM_COLORS, max_nr_colors);

        printk(CRUXLOG_WARNING
               "Xen LLC color config not found. Using first %u colors\n",
               crux_num_colors);
        for ( i = 0; i < crux_num_colors; i++ )
            crux_colors[i] = i;
    }
    else if ( crux_num_colors > max_nr_colors ||
              !check_colors(crux_colors, crux_num_colors) )
        panic("Bad LLC color config for Xen\n");

    arch_llc_coloring_init();
}

void dump_llc_coloring_info(void)
{
    if ( !llc_coloring_enabled )
        return;

    printk("LLC coloring info:\n");
    printk("    Number of LLC colors supported: %u\n", max_nr_colors);
    printk("    Xen LLC colors (%u): ", crux_num_colors);
    print_colors(crux_colors, crux_num_colors);
}

void domain_dump_llc_colors(const struct domain *d)
{
    if ( !llc_coloring_enabled )
        return;

    printk("%u LLC colors: ", d->num_llc_colors);
    print_colors(d->llc_colors, d->num_llc_colors);
}

int __init dom0_set_llc_colors(struct domain *d)
{
    typeof(*dom0_colors) *colors;

    if ( !dom0_num_colors )
        return 0;

    if ( (dom0_num_colors > max_nr_colors) ||
         !check_colors(dom0_colors, dom0_num_colors) )
    {
        printk(CRUXLOG_ERR "%pd:  bad LLC color config\n", d);
        return -EINVAL;
    }

    colors = xmalloc_array(typeof(*dom0_colors), dom0_num_colors);
    if ( !colors )
        return -ENOMEM;

    memcpy(colors, dom0_colors, sizeof(*colors) * dom0_num_colors);
    d->llc_colors = colors;
    d->num_llc_colors = dom0_num_colors;

    return 0;
}

int domain_set_llc_colors(struct domain *d,
                          const struct crux_domctl_set_llc_colors *config)
{
    unsigned int *colors;

    if ( d->llc_colors != default_colors )
        return -EEXIST;

    if ( !config->num_llc_colors )
        return 0;

    if ( config->num_llc_colors > max_nr_colors )
        return -EINVAL;

    colors = xmalloc_array(unsigned int, config->num_llc_colors);
    if ( !colors )
        return -ENOMEM;

    if ( copy_from_guest(colors, config->llc_colors, config->num_llc_colors) )
    {
        xfree(colors);
        return -EFAULT;
    }

    if ( !check_colors(colors, config->num_llc_colors) )
    {
        printk(CRUXLOG_ERR "%pd: bad LLC color config\n", d);
        xfree(colors);
        return -EINVAL;
    }

    d->llc_colors = colors;
    d->num_llc_colors = config->num_llc_colors;

    return 0;
}

void domain_llc_coloring_init(struct domain *d)
{
    if ( !llc_coloring_enabled )
        return;

    /*
     * Any change to this logic needs to consider the position of our call in
     * domain_create().
     */
    d->llc_colors = default_colors;
    d->num_llc_colors = max_nr_colors;
}

void domain_llc_coloring_free(struct domain *d)
{
    d->num_llc_colors = 0;

    if ( d->llc_colors != default_colors )
        XFREE(d->llc_colors);
}

int __init domain_set_llc_colors_from_str(struct domain *d, const char *str)
{
    int err;
    unsigned int *colors, num_colors;

    if ( !str )
        return 0;

    colors = xmalloc_array(unsigned int, max_nr_colors);
    if ( !colors )
        return -ENOMEM;

    err = parse_color_config(str, colors, max_nr_colors, &num_colors);
    if ( err )
    {
        printk(CRUXLOG_ERR "Error parsing LLC color configuration");
        xfree(colors);
        return err;
    }

    if ( !check_colors(colors, num_colors) )
    {
        printk(CRUXLOG_ERR "%pd: bad LLC color config\n", d);
        xfree(colors);
        return -EINVAL;
    }

    /* Adjust the size cause it was initially set to max_nr_colors */
    d->llc_colors = xrealloc_array(colors, num_colors);
    if ( !d->llc_colors )
        d->llc_colors = colors;

    d->num_llc_colors = num_colors;

    return 0;
}

unsigned int page_to_llc_color(const struct page_info *pg)
{
    return mfn_to_color(page_to_mfn(pg));
}

unsigned int get_max_nr_llc_colors(void)
{
    return max_nr_colors;
}

mfn_t __init crux_colored_mfn(mfn_t mfn)
{
    unsigned int i, color = mfn_to_color(mfn);

    for ( i = 0; i < crux_num_colors; i++ )
    {
        if ( color == crux_colors[i] )
            return mfn;
        if ( color < crux_colors[i] )
            return get_mfn_with_color(mfn, crux_colors[i]);
    }

    /* Jump to next color space (max_nr_colors mfns) and use the first color */
    return get_mfn_with_color(mfn_add(mfn, max_nr_colors), crux_colors[0]);
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
