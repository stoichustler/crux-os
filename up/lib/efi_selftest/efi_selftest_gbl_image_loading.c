// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2024 The Android Open Source Project
 */

#include <blk.h>
#include <efi_api.h>
#include <efi.h>
#include <efi_gbl_image_loading.h>
#include <efi_selftest.h>
#include <part.h>
#include <stdlib.h>
#include <string.h>

static struct efi_boot_services *boot_services;
static efi_image_loading_protocol *protocol;

static int setup(const efi_handle_t handle,
		 const struct efi_system_table *systable)
{
	boot_services = systable->boottime;

	efi_status_t res = boot_services->locate_protocol(
		&efi_gbl_image_loading_protocol_guid, NULL, (void **)&protocol);
	if (res != EFI_SUCCESS) {
		protocol = NULL;
		efi_st_error("Failed to locate GBL image loading protocol\n");
		return EFI_ST_FAILURE;
	}

	return EFI_ST_SUCCESS;
}

int execute_get_verify_partitions(void);
int execute_get_buffer_known(void);
int execute_get_buffer_unknown(void);

static int execute(void)
{
	efi_status_t res;

	res = execute_get_verify_partitions();
	if (res != EFI_SUCCESS) {
		return res;
	}

	res = execute_get_buffer_known();
	if (res != EFI_SUCCESS) {
		return res;
	}

	res = execute_get_buffer_unknown();
	if (res != EFI_SUCCESS) {
		return res;
	}

	return EFI_SUCCESS;
}

int execute_get_verify_partitions(void)
{
	size_t partitions_count = 1;
	gbl_partition_name partitions;
	efi_status_t res = protocol->get_verify_partitions(
		protocol, &partitions_count, &partitions);
	if (res != EFI_SUCCESS) {
		efi_st_error("Failed to get verify partitions: %lu\n", res);
		return EFI_ST_FAILURE;
	}
	if (partitions_count != 0) {
		efi_st_error("Incorrect partitions count received: %u\n",
			     partitions_count);
		return EFI_ST_FAILURE;
	}
	return EFI_SUCCESS;
}

int execute_get_buffer_known(void)
{
	const gbl_image_info image_info[2] = { {
						       .image_type = u"boot",
						       .size_bytes = 10,
					       },
					       {
						       .image_type = u"ramdisk",
						       .size_bytes = 10,
					       } };

	for (size_t i = 0; i < ARRAY_SIZE(image_info); i++) {
		gbl_image_buffer image_buffer = {
			.memory = NULL,
			.size_bytes = 0,
		};
		efi_status_t res = protocol->get_buffer(
			protocol, &image_info[i], &image_buffer);
		if (res != EFI_SUCCESS) {
			efi_st_error("Failed to get buffer: (%lu)\n", res);
			return EFI_ST_FAILURE;
		}
		if (image_buffer.memory == NULL) {
			efi_st_error("Failed to get buffer memory: (%lu)\n",
				     res);
			return EFI_ST_FAILURE;
		}
		if (image_buffer.size_bytes < image_info[i].size_bytes) {
			efi_st_error(
				"Failed to get big enough buffer: (%lu) (%u < %u)\n",
				res, image_buffer.size_bytes,
				image_info[i].size_bytes);
			return EFI_ST_FAILURE;
		}
	}

	return EFI_SUCCESS;
}

int execute_get_buffer_unknown(void)
{
	const gbl_image_info image_info = {
		.image_type = u"unknown",
		.size_bytes = 10,
	};

	gbl_image_buffer image_buffer = {
		.memory = NULL,
		.size_bytes = 0,
	};
	efi_status_t res =
		protocol->get_buffer(protocol, &image_info, &image_buffer);
	if (res != EFI_SUCCESS) {
		efi_st_error("Failed to get buffer: (%lu)\n", res);
		return EFI_ST_FAILURE;
	}
	if (image_buffer.memory != NULL) {
		efi_st_error("Failed to get unknow buffer null result: (%lu)\n",
			     res);
		return EFI_ST_FAILURE;
	}

	return EFI_SUCCESS;
}

static int teardown(void)
{
	return EFI_ST_SUCCESS;
}

EFI_UNIT_TEST(gbl_image_loading) = {
	.name = "GBL image loading protocol",
	.phase = EFI_EXECUTE_BEFORE_BOOTTIME_EXIT,
	.setup = setup,
	.execute = execute,
	.teardown = teardown,
};
