/* SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2024 The Android Open Source Project
 */

#ifndef __EFI_GBL_IMAGE_LOADING_H__
#define __EFI_GBL_IMAGE_LOADING_H__

#include <efi_api.h>
#include <part_efi.h>
#include <stddef.h>

#define EFI_GBL_IMAGE_LOADING_PROTOCOL_REVISION 0x00010000

#define PARTITION_NAME_LEN_U16 36

extern const efi_guid_t efi_gbl_image_loading_protocol_guid;

typedef struct gbl_image_info {
	efi_char16_t image_type[PARTITION_NAME_LEN_U16];
	size_t size_bytes;
} gbl_image_info;

typedef struct gbl_image_buffer {
	void *memory;
	size_t size_bytes;
} gbl_image_buffer;

typedef struct gbl_partition_name {
	efi_char16_t str_utf16[PARTITION_NAME_LEN_U16];
} gbl_partition_name;

typedef struct efi_image_loading_protocol {
	// Currently must contain 0x00010000
	u64 revision;

	efi_status_t(EFIAPI *get_buffer)(struct efi_image_loading_protocol *,
					 const gbl_image_info * /* in param */,
					 gbl_image_buffer * /* in-out param */);

	efi_status_t(EFIAPI *get_verify_partitions)(
		struct efi_image_loading_protocol *,
		size_t * /* in-out param */,
		gbl_partition_name * /* in-out param */);
} efi_image_loading_protocol;

efi_status_t efi_gbl_image_loading_register(void);

#endif /* __EFI_GBL_IMAGE_LOADING_H__ */
