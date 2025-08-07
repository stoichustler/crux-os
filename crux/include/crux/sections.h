/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __CRUX_SECTIONS_H__
#define __CRUX_SECTIONS_H__

#include <crux/compiler.h>

/* SAF-0-safe */
extern const char __ro_after_init_start[], __ro_after_init_end[];

/* SAF-0-safe */
extern char __init_begin[], __init_end[];
#define is_init_section(p) ({                           \
    const char *p_ = (const char *)(unsigned long)(p);  \
    (p_ >= __init_begin) && (p_ < __init_end);          \
})

/*
 * Some data is expected to be written rarely (if at all).
 *
 * For performance reasons is it helpful to group such data in the build, to
 * avoid the linker placing it adjacent to often-written data.
 */
#define __read_mostly __section(".data.read_mostly")

/*
 * Some data should be chosen during boot and be immutable thereafter.
 *
 * Variables annotated with __ro_after_init will become read-only after boot
 * and suffer a runtime access fault if modified.
 *
 * For architectures/platforms which haven't implemented support, these
 * variables will be treated as regular mutable data.
 */
#define __ro_after_init __section(".data.ro_after_init")

#endif /* !__CRUX_SECTIONS_H__ */
/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
