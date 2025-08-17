/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (C) 2024 The Android Open Source Project
 */

/*
 * Original definition:
 * https://cs.android.com/android/platform/superproject/main/+/124321be6391247d71ee8737c3a49bd36213b3de:bootable/libbootloader/gbl/libefi_types/defs/protocols/gbl_efi_os_configuration_protocol.h
 *
 * Documentation:
 * https://cs.android.com/android/platform/superproject/main/+/124321be6391247d71ee8737c3a49bd36213b3de:bootable/libbootloader/gbl/docs/gbl_os_configuration_protocol.md
 *
 * Warning: API is UNSTABLE
 */

#ifndef __EFI_GBL_OS_CONFIG_H__
#define __EFI_GBL_OS_CONFIG_H__

#include <efi_api.h>

#define EFI_GBL_OS_CONFIGURATION_PROTOCOL_REVISION 0x00010000

enum GBL_EFI_DEVICE_TREE_SOURCE {
	BOOT = 0,
	VENDOR_BOOT,
	DTBO,
	DTB,
};

struct efi_gbl_device_tree_metadata {
	// GblDeviceTreeSource
	u32 source;
	// Values are zeroed and must not be used in case of BOOT / VENDOR_BOOT source
	u32 id;
	u32 rev;
	u32 custom[4];
	// Make sure GblDeviceTreeMetadata size is 8-bytes aligned. Also reserved for
	// the future cases
	u32 reserved;
};

struct efi_gbl_verified_device_tree {
	struct efi_gbl_device_tree_metadata metadata;
	// Base device tree / overlay buffer (guaranteed to be 8-bytes aligned),
	// cannot be NULL. Device tree size can be identified by the header totalsize
	// field
	const void *device_tree;
	// Indicates whether this device tree (or overlay) must be included in the
	// final device tree. Set to true by a FW if this component must be used
	u8 selected;
};

struct efi_gbl_os_configuration_protocol {
	u64 revision;

	// Generates fixups for the kernel command line built by GBL.
	efi_status_t(EFIAPI *fixup_kernel_commandline)(
		struct efi_gbl_os_configuration_protocol *self,
		const char *command_line, /* in */
		char *fixup, /* out */
		size_t *fixup_buffer_size /* in-out */
	);

	// Generates fixups for the bootconfig built by GBL.
	efi_status_t(EFIAPI *fixup_bootconfig)(
		struct efi_gbl_os_configuration_protocol *self,
		const char *bootconfig, /* in */
		size_t size, /* in */
		char *fixup, /* out */
		size_t *fixup_buffer_size /* in-out */
	);

	// Selects which device trees and overlays to use from those loaded by GBL.
	efi_status_t(EFIAPI *select_device_trees)(
		struct efi_gbl_os_configuration_protocol *self,
		struct efi_gbl_verified_device_tree *device_trees, /* in-out */
		size_t num_device_trees /* in */
	);
};

extern const efi_guid_t efi_gbl_os_config_guid;

efi_status_t efi_gbl_os_config_register(void);

#endif /* __EFI_GBL_OS_CONFIG_H__ */
