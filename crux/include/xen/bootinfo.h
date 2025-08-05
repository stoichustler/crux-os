/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef XEN_BOOTINFO_H
#define XEN_BOOTINFO_H

#include <xen/bootfdt.h>
#include <xen/kernel.h>
#include <xen/macros.h>
#include <xen/xmalloc.h>

#define NR_MEM_BANKS 256
#define NR_SHMEM_BANKS 32

#define MAX_MODULES 32 /* Current maximum useful modules */

enum membank_type {
    /*
     * The MEMBANK_DEFAULT type refers to either reserved memory for the
     * device/firmware (when the bank is in 'reserved_mem') or any RAM (when
     * the bank is in 'mem').
     */
    MEMBANK_DEFAULT,
    /*
     * The MEMBANK_STATIC_DOMAIN type is used to indicate whether the memory
     * bank is bound to a static xen domain. It is only valid when the bank
     * is in reserved_mem.
     */
    MEMBANK_STATIC_DOMAIN,
    /*
     * The MEMBANK_STATIC_HEAP type is used to indicate whether the memory
     * bank is reserved as static heap. It is only valid when the bank is
     * in reserved_mem.
     */
    MEMBANK_STATIC_HEAP,
    /*
     * The MEMBANK_FDT_RESVMEM type is used to indicate whether the memory
     * bank is from the FDT reserve map.
     */
    MEMBANK_FDT_RESVMEM,
};

enum region_type {
    MEMORY,
    RESERVED_MEMORY,
    STATIC_SHARED_MEMORY
};

/* Indicates the maximum number of characters(\0 included) for shm_id */
#define MAX_SHM_ID_LENGTH 16

struct shmem_membank_extra {
    char shm_id[MAX_SHM_ID_LENGTH];
    unsigned int nr_shm_borrowers;
};

struct membank {
    paddr_t start;
    paddr_t size;
    union {
        enum membank_type type;
#ifdef CONFIG_STATIC_SHM
        struct shmem_membank_extra *shmem_extra;
#endif
    };
};

struct membanks {
    __struct_group(membanks_hdr, common, ,
        unsigned int nr_banks;
        unsigned int max_banks;
        enum region_type type;
    );
    struct membank bank[];
};

struct meminfo {
    struct membanks_hdr common;
    struct membank bank[NR_MEM_BANKS];
};

struct shared_meminfo {
    struct membanks_hdr common;
    struct membank bank[NR_SHMEM_BANKS];
    struct shmem_membank_extra extra[NR_SHMEM_BANKS];
};

/* DT_MAX_NAME is the node name max length according the DT spec */
#define DT_MAX_NAME 41
struct bootcmdline {
    boot_module_kind kind;
    bool domU;
    paddr_t start;
    char dt_name[DT_MAX_NAME];
    char cmdline[BOOTMOD_MAX_CMDLINE];
};

struct boot_modules {
    int nr_mods;
    struct boot_module module[MAX_MODULES];
};

struct bootcmdlines {
    unsigned int nr_mods;
    struct bootcmdline cmdline[MAX_MODULES];
};

struct bootinfo {
    struct meminfo mem;
    /* The reserved regions are only used when booting using Device-Tree */
    struct meminfo reserved_mem;
    struct boot_modules modules;
    struct bootcmdlines cmdlines;
#ifdef CONFIG_ACPI
    struct meminfo acpi;
#endif
#ifdef CONFIG_STATIC_SHM
    struct shared_meminfo shmem;
#endif
};

#ifdef CONFIG_ACPI
#define BOOTINFO_ACPI_INIT                          \
    .acpi.common.max_banks = NR_MEM_BANKS,          \
    .acpi.common.type = MEMORY,
#else
#define BOOTINFO_ACPI_INIT
#endif

#ifdef CONFIG_STATIC_SHM
#define BOOTINFO_SHMEM_INIT                         \
    .shmem.common.max_banks = NR_SHMEM_BANKS,       \
    .shmem.common.type = STATIC_SHARED_MEMORY,
#else
#define BOOTINFO_SHMEM_INIT
#endif

#define BOOTINFO_INIT                               \
{                                                   \
    .mem.common.max_banks = NR_MEM_BANKS,           \
    .mem.common.type = MEMORY,                      \
    .reserved_mem.common.max_banks = NR_MEM_BANKS,  \
    .reserved_mem.common.type = RESERVED_MEMORY,    \
    BOOTINFO_ACPI_INIT                              \
    BOOTINFO_SHMEM_INIT                             \
}

extern struct bootinfo bootinfo;

bool check_reserved_regions_overlap(paddr_t region_start,
                                    paddr_t region_size,
                                    bool allow_memreserve_overlap);

struct boot_module *add_boot_module(boot_module_kind kind,
                                    paddr_t start, paddr_t size, bool domU);
struct boot_module *boot_module_find_by_kind(boot_module_kind kind);
struct boot_module * boot_module_find_by_addr_and_kind(boot_module_kind kind,
                                                             paddr_t start);
void add_boot_cmdline(const char *name, const char *cmdline,
                      boot_module_kind kind, paddr_t start, bool domU);
struct bootcmdline *boot_cmdline_find_by_kind(boot_module_kind kind);
struct bootcmdline * boot_cmdline_find_by_name(const char *name);
const char *boot_module_kind_as_string(boot_module_kind kind);

void populate_boot_allocator(void);

size_t boot_fdt_info(const void *fdt, paddr_t paddr);
const char *boot_fdt_cmdline(const void *fdt);
int domain_fdt_begin_node(void *fdt, const char *name, uint64_t unit);

static inline struct membanks *bootinfo_get_reserved_mem(void)
{
    return container_of(&bootinfo.reserved_mem.common, struct membanks, common);
}

static inline struct membanks *bootinfo_get_mem(void)
{
    return container_of(&bootinfo.mem.common, struct membanks, common);
}

#ifdef CONFIG_ACPI
static inline struct membanks *bootinfo_get_acpi(void)
{
    return container_of(&bootinfo.acpi.common, struct membanks, common);
}
#endif

#ifdef CONFIG_STATIC_SHM
static inline struct membanks *bootinfo_get_shmem(void)
{
    return container_of(&bootinfo.shmem.common, struct membanks, common);
}

static inline struct shmem_membank_extra *bootinfo_get_shmem_extra(void)
{
    return bootinfo.shmem.extra;
}
#endif

static inline struct membanks *membanks_xzalloc(unsigned int nr,
                                                enum region_type type)
{
    struct membanks *banks = xzalloc_flex_struct(struct membanks, bank, nr);

    if ( !banks )
        goto out;

    banks->max_banks = nr;
    banks->type = type;

 out:
    return banks;
}

#endif /* XEN_BOOTINFO_H */
