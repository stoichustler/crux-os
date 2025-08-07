/* SPDX-License-Identifier: MIT */
/******************************************************************************
 * arch-x86/guest-acpi.h
 *
 * Guest ACPI interface to x86 crux.
 *
 */

#ifndef __CRUX_PUBLIC_ARCH_X86_GUEST_ACPI_H__
#define __CRUX_PUBLIC_ARCH_X86_GUEST_ACPI_H__

#ifdef __CRUX_TOOLS__

/* Location of online VCPU bitmap. */
#define CRUX_ACPI_CPU_MAP             0xaf00
#define CRUX_ACPI_CPU_MAP_LEN         ((HVM_MAX_VCPUS + 7) / 8)

/* GPE0 bit set during CPU hotplug */
#define CRUX_ACPI_GPE0_CPUHP_BIT      2

#endif /* __CRUX_TOOLS__ */

#endif /* __CRUX_PUBLIC_ARCH_X86_GUEST_ACPI_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
