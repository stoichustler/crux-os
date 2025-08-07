/******************************************************************************
 * config.h
 *
 * A Linux-style configuration list.
 */

#ifndef __ARM_CONFIG_H__
#define __ARM_CONFIG_H__

#if defined(CONFIG_ARM_64)
# define ELFSIZE 64
#else
# define ELFSIZE 32
#endif

/* crux_ulong_t is always 64 bits */
#define BITS_PER_CRUX_ULONG 64

#define CONFIG_PAGING_LEVELS 3

#define CONFIG_ARM 1

#define CONFIG_ARM_L1_CACHE_SHIFT 7 /* XXX */

#define CONFIG_SMP 1

#define CONFIG_IRQ_HAS_MULTIPLE_ACTION 1

#define CONFIG_PAGEALLOC_MAX_ORDER 18
#define CONFIG_DOMU_MAX_ORDER      9
#define CONFIG_HWDOM_MAX_ORDER     10

#define OPT_CONSOLE_STR "dtuart"

#ifdef CONFIG_ARM_64
#define MAX_VIRT_CPUS 128u
#else
#define MAX_VIRT_CPUS 8u
#endif

#define INVALID_VCPU_ID MAX_VIRT_CPUS

#define __LINUX_ARM_ARCH__ 7
#define CONFIG_AEABI

/* Linkage for ARM */
#ifdef __ASSEMBLY__
#define GLOBAL(name)                            \
  .globl name;                                  \
  name:
#endif

#include <crux/const.h>
#include <crux/page-size.h>

#if defined(CONFIG_MMU)
#include <asm/mmu/layout.h>
#elif defined(CONFIG_MPU)
#include <asm/mpu/layout.h>
#else
# error "Unknown memory management layout"
#endif

#define NR_hypercalls 64

#define STACK_ORDER 3
#define STACK_SIZE  (PAGE_SIZE << STACK_ORDER)

#define watchdog_disable() ((void)0)
#define watchdog_enable()  ((void)0)

#if defined(__ASSEMBLY__) && !defined(LINKER_SCRIPT)
#include <asm/asm_defns.h>
#include <asm/macros.h>
#endif

#endif /* __ARM_CONFIG_H__ */
/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
