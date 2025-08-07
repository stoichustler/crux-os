/* SAF-10-safe direct inclusion guard before */
#ifndef __CRUX_HYPERCALL_H__
#error "asm/hypercall.h should not be included directly - include crux/hypercall.h instead"
#endif

#ifndef __ASM_ARM_HYPERCALL_H__
#define __ASM_ARM_HYPERCALL_H__

#include <public/domctl.h> /* for arch_do_domctl */

long subarch_do_domctl(struct crux_domctl *domctl, struct domain *d,
                       CRUX_GUEST_HANDLE_PARAM(crux_domctl_t) u_domctl);

#endif /* __ASM_ARM_HYPERCALL_H__ */
/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
