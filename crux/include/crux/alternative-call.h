/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef CRUX_ALTERNATIVE_CALL
#define CRUX_ALTERNATIVE_CALL

/*
 * Some subsystems in Xen may have multiple implementations, which can be
 * resolved to a single implementation at boot time.  By default, this will
 * result in the use of function pointers.
 *
 * Some architectures may have mechanisms for dynamically modifying .text.
 * Using this mechanism, function pointers can be converted to direct calls
 * which are typically more efficient at runtime.
 *
 * For architectures to support:
 *
 * - Implement alternative_{,v}call() in asm/alternative-call.h.  Code
 *   generation requirements are to emit a function pointer call at build
 *   time, and stash enough metadata to simplify the call at boot once the
 *   implementation has been resolved.
 * - Implement {boot,livepatch}_apply_alt_calls() to convert the function
 *   pointer calls into direct calls on boot/livepatch.
 * - Select ALTERNATIVE_CALL in Kconfig.
 *
 * To use:
 *
 * Consider the following simplified example.
 *
 *  1) struct foo_ops __alt_call_maybe_initdata ops;
 *
 *  2) const struct foo_ops __initconstrel foo_a_ops = { ... };
 *     const struct foo_ops __initconstrel foo_b_ops = { ... };
 *
 *     void __init foo_init(void)
 *     {
 *         ...
 *         if ( use_impl_a )
 *             ops = *foo_a_ops;
 *         else if ( use_impl_b )
 *             ops = *foo_b_ops;
 *         ...
 *     }
 *
 *  3) alternative_call(ops.bar, ...);
 *
 * There needs to a single ops object (1) which will eventually contain the
 * function pointers.  This should be populated in foo's init() function (2)
 * by one of the available implementations.  To call functions, use
 * alternative_{,v}call() referencing the main ops object (3).
 */

#ifdef CONFIG_ALTERNATIVE_CALL

#include <asm/alternative-call.h>

#ifdef CONFIG_LIVEPATCH
/* Must keep for livepatches to resolve alternative calls. */
# define __alt_call_maybe_initdata __ro_after_init
#else
# define __alt_call_maybe_initdata __initdata
#endif

/*
 * Devirtualise the alternative_{,v}call()'s on boot.  Convert still-NULL
 * function pointers into traps.
 */
void boot_apply_alt_calls(void);

/* As per boot_apply_alt_calls() but for a livepatch. */
int livepatch_apply_alt_calls(const struct alt_call *start,
                              const struct alt_call *end);

#else /* CONFIG_ALTERNATIVE_CALL */

#define alternative_call(func, args...)  (func)(args)
#define alternative_vcall(func, args...) (func)(args)

#define __alt_call_maybe_initdata __ro_after_init

#endif /* !CONFIG_ALTERNATIVE_CALL */
#endif /* CRUX_ALTERNATIVE_CALL */
