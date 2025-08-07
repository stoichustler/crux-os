/******************************************************************************
 * config.h
 * 
 * A Linux-style configuration list.
 */

#ifndef __CRUX_CONFIG_H__
#define __CRUX_CONFIG_H__

#ifdef CPPCHECK
#include <generated/compiler-def.h>
#endif

#include <crux/kconfig.h>

#ifndef __ASSEMBLY__
#include <crux/compiler.h>

#if defined(CONFIG_ENFORCE_UNIQUE_SYMBOLS) || defined(__clang__)
# define EMIT_FILE asm ( "" )
#else
# define EMIT_FILE asm ( ".file \"" __FILE__ "\"" )
#endif

#endif

#include <asm/config.h>

#define EXPORT_SYMBOL(var)

/*
 * The following log levels are as follows:
 *
 *   CRUXLOG_ERR: Fatal errors, either Xen, Guest or Dom0
 *               is about to crash.
 *
 *   CRUXLOG_WARNING: Something bad happened, but we can recover.
 *
 *   CRUXLOG_INFO: Interesting stuff, but not too noisy.
 *
 *   CRUXLOG_DEBUG: Use where ever you like. Lots of noise.
 *
 *
 * Since we don't trust the guest operating system, we don't want
 * it to allow for DoS by causing the HV to print out a lot of
 * info, so where ever the guest has control of what is printed
 * we use the CRUXLOG_GUEST to distinguish that the output is
 * controlled by the guest.
 *
 * To make it easier on the typing, the above log levels all
 * have a corresponding _G_ equivalent that appends the
 * CRUXLOG_GUEST. (see the defines below).
 *
 */
#define CRUXLOG_ERR     "<0>"
#define CRUXLOG_WARNING "<1>"
#define CRUXLOG_INFO    "<2>"
#define CRUXLOG_DEBUG   "<3>"

#define CRUXLOG_GUEST   "<G>"

#define CRUXLOG_G_ERR     CRUXLOG_GUEST CRUXLOG_ERR
#define CRUXLOG_G_WARNING CRUXLOG_GUEST CRUXLOG_WARNING
#define CRUXLOG_G_INFO    CRUXLOG_GUEST CRUXLOG_INFO
#define CRUXLOG_G_DEBUG   CRUXLOG_GUEST CRUXLOG_DEBUG

/*
 * Some code is copied directly from Linux.
 * Match some of the Linux log levels to Xen.
 */
#define KERN_ERR       CRUXLOG_ERR
#define KERN_CRIT      CRUXLOG_ERR
#define KERN_EMERG     CRUXLOG_ERR
#define KERN_WARNING   CRUXLOG_WARNING
#define KERN_NOTICE    CRUXLOG_INFO
#define KERN_INFO      CRUXLOG_INFO
#define KERN_DEBUG     CRUXLOG_DEBUG

/* Linux 'checker' project. */
#define __iomem
#define __user
#define __force
#define __bitwise

#define KB(_kb)     (_AC(_kb, ULL) << 10)
#define MB(_mb)     (_AC(_mb, ULL) << 20)
#define GB(_gb)     (_AC(_gb, ULL) << 30)

/* allow existing code to work with Kconfig variable */
#define NR_CPUS CONFIG_NR_CPUS

#ifndef CONFIG_DEBUG
#define NDEBUG
#endif

#ifndef ZERO_BLOCK_PTR
/* Return value for zero-size allocation, distinguished from NULL. */
#define ZERO_BLOCK_PTR ((void *)-1L)
#endif

#define BYTES_PER_LONG  __SIZEOF_LONG__

#define BITS_PER_BYTE   __CHAR_BIT__
#define BITS_PER_INT    (BITS_PER_BYTE * __SIZEOF_INT__)
#define BITS_PER_LONG   (BITS_PER_BYTE * BYTES_PER_LONG)
#define BITS_PER_LLONG  (BITS_PER_BYTE * __SIZEOF_LONG_LONG__)

/* It is assumed that sizeof(void *) == __alignof(void *) */
#define POINTER_ALIGN   __SIZEOF_POINTER__

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
# define __LITTLE_ENDIAN
# define __LITTLE_ENDIAN_BITFIELD
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
# define __BIG_ENDIAN
# define __BIG_ENDIAN_BITFIELD
#endif

#endif /* __CRUX_CONFIG_H__ */
