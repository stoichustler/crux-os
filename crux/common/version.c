#include <crux/bug.h>
#include <crux/compile.h>
#include <crux/init.h>
#include <crux/errno.h>
#include <crux/lib.h>
#include <crux/sections.h>
#include <crux/string.h>
#include <crux/types.h>
#include <crux/efi.h>
#include <crux/elf.h>
#include <crux/version.h>

const char *crux_compile_date(void)
{
    return CRUX_COMPILE_DATE;
}

const char *crux_compile_time(void)
{
    return CRUX_COMPILE_TIME;
}

const char *crux_compile_by(void)
{
    return CRUX_COMPILE_BY;
}

const char *crux_compile_domain(void)
{
    return CRUX_COMPILE_DOMAIN;
}

const char *crux_compile_host(void)
{
    return CRUX_COMPILE_HOST;
}

const char *crux_compiler(void)
{
    return CRUX_COMPILER;
}

unsigned int crux_major_version(void)
{
    return CRUX_VERSION;
}

unsigned int crux_minor_version(void)
{
    return CRUX_SUBVERSION;
}

const char *crux_extra_version(void)
{
    return CRUX_EXTRAVERSION;
}

const char *crux_changeset(void)
{
    return CRUX_CHANGESET;
}

const char *crux_banner(void)
{
    return CRUX_BANNER;
}

const char *crux_deny(void)
{
    return "<denied>";
}

static const char build_info[] =
    "debug="
#ifdef CONFIG_DEBUG
    "y"
#else
    "n"
#endif
#ifdef CONFIG_COVERAGE
# ifdef __clang__
    " llvmcov=y"
# else
    " gcov=y"
# endif
#endif
#ifdef CONFIG_UBSAN
    " ubsan=y"
#endif
    "";

const char *crux_build_info(void)
{
    return build_info;
}

const void *__ro_after_init crux_build_id;
unsigned int __ro_after_init crux_build_id_len;

void print_version(void)
{
    printk("crux version %d.%d%s (%s@%s) (%s) %s %s\n",
           crux_major_version(), crux_minor_version(), crux_extra_version(),
           crux_compile_by(), crux_compile_domain(), crux_compiler(),
           crux_build_info(), crux_compile_date());

    if ( crux_build_id_len )
        printk("build-id: %*phN\n", crux_build_id_len, crux_build_id);
}

#ifdef BUILD_ID
/* Defined in linker script. */
extern const Elf_Note __note_gnu_build_id_start[], __note_gnu_build_id_end[];

int crux_build_id_check(const Elf_Note *n, unsigned int n_sz,
                       const void **p, unsigned int *len)
{
    /* Check if we really have a build-id. */
    ASSERT(n_sz > sizeof(*n));

    if ( NT_GNU_BUILD_ID != n->type )
        return -ENODATA;

    if ( n->namesz + n->descsz < n->namesz )
        return -EINVAL;

    if ( n->namesz < 4 /* GNU\0 */)
        return -EINVAL;

    if ( n->namesz + n->descsz > n_sz - sizeof(*n) )
        return -EINVAL;

    /* Sanity check, name should be "GNU" for ld-generated build-id. */
    if ( strncmp(ELFNOTE_NAME(n), "GNU", n->namesz) != 0 )
        return -ENODATA;

    if ( len )
        *len = n->descsz;
    if ( p )
        *p = ELFNOTE_DESC(n);

    return 0;
}

struct pe_external_debug_directory
{
    uint32_t characteristics;
    uint32_t time_stamp;
    uint16_t major_version;
    uint16_t minor_version;
#define PE_IMAGE_DEBUG_TYPE_CODEVIEW 2
    uint32_t type;
    uint32_t size;
    uint32_t rva_of_data;
    uint32_t filepos_of_data;
};

struct cv_info_pdb70
{
#define CVINFO_PDB70_CVSIGNATURE 0x53445352 /* "RSDS" */
    uint32_t cv_signature;
    unsigned char signature[16];
    uint32_t age;
    char pdb_filename[];
};

void __init crux_build_init(void)
{
    const Elf_Note *n = __note_gnu_build_id_start;
    unsigned int sz;
    int rc;

    /* --build-id invoked with wrong parameters. */
    if ( __note_gnu_build_id_end <= &n[0] )
        return;

    /* Check for full Note header. */
    if ( &n[1] >= __note_gnu_build_id_end )
        return;

    sz = (uintptr_t)__note_gnu_build_id_end - (uintptr_t)n;

    rc = crux_build_id_check(n, sz, &crux_build_id, &crux_build_id_len);
}
#endif /* BUILD_ID */
/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
