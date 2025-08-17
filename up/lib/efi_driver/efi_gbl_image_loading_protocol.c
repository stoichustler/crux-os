// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2024 The Android Open Source Project
 */

#include <efi.h>
#include <efi_gbl_image_loading.h>
#include <efi_loader.h>
#include <inttypes.h>
#include <linux/sizes.h>
#include <stdint.h>
#include <stdlib.h>

const efi_guid_t efi_gbl_image_loading_protocol_guid =
	EFI_GBL_IMAGE_LOADING_PROTOCOL_GUID;

typedef struct image_buffer {
	size_t buffer_size;
	uintptr_t buffer;
	size_t alignment;
	size_t name_len_bytes;
	efi_char16_t name[PARTITION_NAME_LEN_U16];
} image_buffer;

static image_buffer image_buffers[] = {
	{
		.buffer_size = 0,
		.buffer = 0,
		.alignment = 2 * 1024 * 1024,
		.name_len_bytes = 5 * sizeof(efi_char16_t),
		.name = u"boot",
	},
	{
		// ramdisk contains 'init_boot' and 'vendor_boot'
		.buffer_size = 0,
		.buffer = 0,
		.alignment = 0,
		.name_len_bytes = 8 * sizeof(efi_char16_t),
		.name = u"ramdisk",
	},
};

static efi_status_t EFIAPI get_buffer(struct efi_image_loading_protocol *this,
				      const gbl_image_info *gbl_info,
				      gbl_image_buffer *buffer)
{
	EFI_ENTRY("%p %p %p", this, gbl_info, buffer);

	for (size_t i = 0; i < ARRAY_SIZE(image_buffers); i++) {
		image_buffer *pbuf = &image_buffers[i];

		if (memcmp(pbuf->name, gbl_info->image_type,
			   pbuf->name_len_bytes) == 0) {
			if (pbuf->buffer == 0) {
				u64 address;
				size_t alloc_size = gbl_info->size_bytes + pbuf->alignment;

				efi_status_t ret = efi_allocate_pages(
					EFI_ALLOCATE_ANY_PAGES,
					EFI_RUNTIME_SERVICES_CODE,
					efi_size_in_pages(alloc_size),
					&address);

				if (ret != EFI_SUCCESS) {
					log_err("Failed to allocate UEFI buffer: %lu\n",
						ret);
					return ret;
				}

				size_t offset = pbuf->alignment - address % pbuf->alignment;
				pbuf->buffer = address + offset;
				pbuf->buffer_size = alloc_size - offset;
			}

			if (gbl_info->size_bytes > pbuf->buffer_size)
				return EFI_EXIT(EFI_OUT_OF_RESOURCES);

			buffer->memory = (void *)pbuf->buffer;
			buffer->size_bytes = pbuf->buffer_size;
			return EFI_EXIT(EFI_SUCCESS);
		}
	}

	buffer->memory = NULL;
	return EFI_EXIT(EFI_SUCCESS);
}

static efi_status_t EFIAPI
get_verify_partitions(struct efi_image_loading_protocol *this,
		      size_t *partitions_count, gbl_partition_name *partitions)
{
	EFI_ENTRY("%p %p %p", this, partitions_count, partitions);

	// No additions partitions to verify
	*partitions_count = 0;

	return EFI_EXIT(EFI_SUCCESS);
}

static efi_image_loading_protocol efi_gbl_image_loading_proto = {
	.revision = EFI_GBL_IMAGE_LOADING_PROTOCOL_REVISION,
	.get_buffer = get_buffer,
	.get_verify_partitions = get_verify_partitions,
};

efi_status_t efi_gbl_image_loading_register(void)
{
	efi_status_t ret =
		efi_add_protocol(efi_root, &efi_gbl_image_loading_protocol_guid,
				 &efi_gbl_image_loading_proto);
	if (ret != EFI_SUCCESS) {
		log_err("Failed to install EFI_GBL_IMAGE_LOADING_PROTOCOL: 0x%lx\n",
			ret);
	}

	return ret;
}
