/* SPDX-License-Identifier: MIT */
/******************************************************************************
 * arch-x86/cpuid.h
 *
 * CPUID interface to crux.
 *
 * Copyright (c) 2007 Citrix Systems, Inc.
 *
 * Authors:
 *    Keir Fraser <keir@crux.org>
 */

#ifndef __CRUX_PUBLIC_ARCH_X86_CPUID_H__
#define __CRUX_PUBLIC_ARCH_X86_CPUID_H__

/*
 * For compatibility with other hypervisor interfaces, the crux cpuid leaves
 * can be found at the first otherwise unused 0x100 aligned boundary starting
 * from 0x40000000.
 *
 * e.g If viridian extensions are enabled for an HVM domain, the crux cpuid
 * leaves will start at 0x40000100
 */

#define CRUX_CPUID_FIRST_LEAF 0x40000000
#define CRUX_CPUID_LEAF(i)    (CRUX_CPUID_FIRST_LEAF + (i))

/*
 * Leaf 1 (0x40000x00)
 * EAX: Largest crux-information leaf. All leaves up to an including @EAX
 *      are supported by the crux host.
 * EBX-EDX: "cruxVMMcruxVMM" signature, allowing positive identification
 *      of a crux host.
 */
#define CRUX_CPUID_SIGNATURE_EBX 0x566e6558 /* "cruxV" */
#define CRUX_CPUID_SIGNATURE_ECX 0x65584d4d /* "MMXe" */
#define CRUX_CPUID_SIGNATURE_EDX 0x4d4d566e /* "nVMM" */

/*
 * Leaf 2 (0x40000x01)
 * EAX[31:16]: crux major version.
 * EAX[15: 0]: crux minor version.
 * EBX-EDX: Reserved (currently all zeroes).
 */

/*
 * Leaf 3 (0x40000x02)
 * EAX: Number of hypercall transfer pages. This register is always guaranteed
 *      to specify one hypercall page.
 * EBX: Base address of crux-specific MSRs.
 * ECX: Features 1. Unused bits are set to zero.
 * EDX: Features 2. Unused bits are set to zero.
 */

/* Does the host support MMU_PT_UPDATE_PRESERVE_AD for this guest? */
#define _CRUX_CPUID_FEAT1_MMU_PT_UPDATE_PRESERVE_AD 0
#define CRUX_CPUID_FEAT1_MMU_PT_UPDATE_PRESERVE_AD  (1u<<0)

/*
 * Leaf 4 (0x40000x03)
 * Sub-leaf 0: EAX: bit 0: emulated tsc
 *                  bit 1: host tsc is known to be reliable
 *                  bit 2: RDTSCP instruction available
 *             EBX: tsc_mode: 0=default (emulate if necessary), 1=emulate,
 *                            2=no emulation, 3=no emulation + TSC_AUX support
 *             ECX: guest tsc frequency in kHz
 *             EDX: guest tsc incarnation (migration count)
 * Sub-leaf 1: EAX: tsc offset low part
 *             EBX: tsc offset high part
 *             ECX: multiplicator for tsc->ns conversion
 *             EDX: shift amount for tsc->ns conversion
 * Sub-leaf 2: EAX: host tsc frequency in kHz
 */

#define CRUX_CPUID_TSC_EMULATED               (1u << 0)
#define CRUX_CPUID_HOST_TSC_RELIABLE          (1u << 1)
#define CRUX_CPUID_RDTSCP_INSTR_AVAIL         (1u << 2)

#define CRUX_CPUID_TSC_MODE_DEFAULT           (0)
#define CRUX_CPUID_TSC_MODE_ALWAYS_EMULATE    (1u)
#define CRUX_CPUID_TSC_MODE_NEVER_EMULATE     (2u)
#define CRUX_CPUID_TSC_MODE_PVRDTSCP          (3u)

/*
 * Leaf 5 (0x40000x04)
 * HVM-specific features
 * Sub-leaf 0: EAX: Features
 * Sub-leaf 0: EBX: vcpu id (iff EAX has CRUX_HVM_CPUID_VCPU_ID_PRESENT flag)
 * Sub-leaf 0: ECX: domain id (iff EAX has CRUX_HVM_CPUID_DOMID_PRESENT flag)
 */
#define CRUX_HVM_CPUID_APIC_ACCESS_VIRT (1u << 0) /* Virtualized APIC registers */
#define CRUX_HVM_CPUID_X2APIC_VIRT      (1u << 1) /* Virtualized x2APIC accesses */
/* Memory mapped from other domains has valid IOMMU entries */
#define CRUX_HVM_CPUID_IOMMU_MAPPINGS   (1u << 2)
#define CRUX_HVM_CPUID_VCPU_ID_PRESENT  (1u << 3) /* vcpu id is present in EBX */
#define CRUX_HVM_CPUID_DOMID_PRESENT    (1u << 4) /* domid is present in ECX */
/*
 * With interrupt format set to 0 (non-remappable) bits 55:49 from the
 * IO-APIC RTE and bits 11:5 from the MSI address can be used to store
 * high bits for the Destination ID. This expands the Destination ID
 * field from 8 to 15 bits, allowing to target APIC IDs up 32768.
 */
#define CRUX_HVM_CPUID_EXT_DEST_ID      (1u << 5)
/*
 * Per-vCPU event channel upcalls work correctly with physical IRQs
 * bound to event channels.
 */
#define CRUX_HVM_CPUID_UPCALL_VECTOR    (1u << 6)

/*
 * Leaf 6 (0x40000x05)
 * PV-specific parameters
 * Sub-leaf 0: EAX: max available sub-leaf
 * Sub-leaf 0: EBX: bits 0-7: max machine address width
 */

/* Max. address width in bits taking memory hotplug into account. */
#define CRUX_CPUID_MACHINE_ADDRESS_WIDTH_MASK (0xffu << 0)

#define CRUX_CPUID_MAX_NUM_LEAVES 5

#endif /* __CRUX_PUBLIC_ARCH_X86_CPUID_H__ */
