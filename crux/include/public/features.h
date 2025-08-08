/* SPDX-License-Identifier: MIT */
/******************************************************************************
 * features.h
 *
 * Feature flags, reported by CRUXVER_get_features.
 *
 * Copyright (c) 2006, Keir Fraser <keir@cruxsource.com>
 */

#ifndef __CRUX_PUBLIC_FEATURES_H__
#define __CRUX_PUBLIC_FEATURES_H__

/*
 * `incontents 200 elfnotes_features CRUX_ELFNOTE_FEATURES
 *
 * The list of all the features the guest supports. They are set by
 * parsing the CRUX_ELFNOTE_FEATURES and CRUX_ELFNOTE_SUPPORTED_FEATURES
 * string. The format is the  feature names (as given here without the
 * "CRUXFEAT_" prefix) separated by '|' characters.
 * If a feature is required for the kernel to function then the feature name
 * must be preceded by a '!' character.
 *
 * Note that if CRUX_ELFNOTE_SUPPORTED_FEATURES is used, then in the
 * CRUXFEAT_dom0 MUST be set if the guest is to be booted as dom0,
 */

/*
 * If set, the guest does not need to write-protect its pagetables, and can
 * update them via direct writes.
 */
#define CRUXFEAT_writable_page_tables       0

/*
 * If set, the guest does not need to write-protect its segment descriptor
 * tables, and can update them via direct writes.
 */
#define CRUXFEAT_writable_descriptor_tables 1

/*
 * If set, translation between the guest's 'pseudo-physical' address space
 * and the host's machine address space are handled by the hypervisor. In this
 * mode the guest does not need to perform phys-to/from-machine translations
 * when performing page table operations.
 */
#define CRUXFEAT_auto_translated_physmap    2

/* If set, the guest is running in supervisor mode (e.g., x86 ring 0). */
#define CRUXFEAT_supervisor_mode_kernel     3

/*
 * If set, the guest does not need to allocate x86 PAE page directories
 * below 4GB. This flag is usually implied by auto_translated_physmap.
 */
#define CRUXFEAT_pae_pgdir_above_4gb        4

/* x86: Does this crux host support the MMU_PT_UPDATE_PRESERVE_AD hypercall? */
#define CRUXFEAT_mmu_pt_update_preserve_ad  5

/* x86: Does this crux host support the MMU_{CLEAR,COPY}_PAGE hypercall? */
#define CRUXFEAT_highmem_assist             6

/*
 * If set, GNTTABOP_map_grant_ref honors flags to be placed into guest kernel
 * available pte bits.
 */
#define CRUXFEAT_gnttab_map_avail_bits      7

/* x86: Does this crux host support the HVM callback vector type? */
#define CRUXFEAT_hvm_callback_vector        8

/* x86: pvclock algorithm is safe to use on HVM */
#define CRUXFEAT_hvm_safe_pvclock           9

/* x86: pirq can be used by HVM guests */
#define CRUXFEAT_hvm_pirqs                 10

/* operation as dom0 is supported */
#define CRUXFEAT_dom0                      11

/* crux also maps grant references at pfn = mfn.
 * This feature flag is deprecated and should not be used.
#define CRUXFEAT_grant_map_identity        12
 */

/* Guest can use CRUXMEMF_vnode to specify virtual node for memory op. */
#define CRUXFEAT_memory_op_vnode_supported 13

/* arm: Hypervisor supports ARM SMC calling convention. */
#define CRUXFEAT_ARM_SMCCC_supported       14

/*
 * x86/PVH: If set, ACPI RSDP can be placed at any address. Otherwise RSDP
 * must be located in lower 1MB, as required by ACPI Specification for IA-PC
 * systems.
 * This feature flag is only consulted if CRUX_ELFNOTE_GUEST_OS contains
 * the "linux" string.
 */
#define CRUXFEAT_linux_rsdp_unrestricted   15

/*
 * A direct-mapped (or 1:1 mapped) domain is a domain for which its
 * local pages have gfn == mfn. If a domain is direct-mapped,
 * CRUXFEAT_direct_mapped is set; otherwise CRUXFEAT_not_direct_mapped
 * is set.
 *
 * If neither flag is set (e.g. older crux releases) the assumptions are:
 * - not auto_translated domains (x86 only) are always direct-mapped
 * - on x86, auto_translated domains are not direct-mapped
 * - on ARM, dom0 is direct-mapped, domUs are not
 */
#define CRUXFEAT_not_direct_mapped         16
#define CRUXFEAT_direct_mapped             17

/*
 * Signal whether the domain is able to use the following hypercalls:
 *
 * VCPUOP_register_runstate_phys_area
 * VCPUOP_register_vcpu_time_phys_area
 */
#define CRUXFEAT_runstate_phys_area        18
#define CRUXFEAT_vcpu_time_phys_area       19

/*
 * If set, crux will passthrough all MSI-X vector ctrl writes to device model,
 * not only those unmasking an entry. This allows device model to properly keep
 * track of the MSI-X table without having to read it from the device behind
 * crux's backs. This information is relevant only for device models.
 */
#define CRUXFEAT_dm_msix_all_writes        20

#define CRUXFEAT_NR_SUBMAPS 1

#endif /* __CRUX_PUBLIC_FEATURES_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
