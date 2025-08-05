/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ASM__RISCV__IRQ_H
#define ASM__RISCV__IRQ_H

#include <xen/bug.h>
#include <xen/device_tree.h>

#include <asm/irq-dt.h>

/*
 * According to the AIA spec:
 *   The maximum number of interrupt sources an APLIC may support is 1023.
 *
 * The same is true for PLIC.
 *
 * Interrupt Source 0 is reserved and shall never generate an interrupt.
 */
#define NR_IRQS 1024

#define IRQ_NO_PRIORITY 0

/* TODO */
#define nr_irqs 0U
#define nr_static_irqs 0
#define arch_hwdom_irqs(domid) 0U

#define domain_pirq_to_irq(d, pirq) (pirq)

#define arch_evtchn_bind_pirq(d, pirq) ((void)((d) + (pirq)))

struct arch_pirq {
};

struct arch_irq_desc {
    unsigned int type;
};

struct cpu_user_regs;
struct dt_device_node;

static inline void arch_move_irqs(struct vcpu *v)
{
    BUG_ON("unimplemented");
}

int platform_get_irq(const struct dt_device_node *device, int index);

void init_IRQ(void);

void do_IRQ(struct cpu_user_regs *regs, unsigned int irq);

#endif /* ASM__RISCV__IRQ_H */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
