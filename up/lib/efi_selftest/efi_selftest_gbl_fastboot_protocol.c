/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (C) 2025 The Android Open Source Project
 */

#include <efi.h>
#include <efi_api.h>
#include <efi_gbl_fastboot.h>
#include <efi_selftest.h>
#include <string.h>

static struct efi_boot_services *boot_services;
static struct gbl_efi_fastboot_protocol *protocol;

static int test_local_session(void)
{
	void *ctx;
	efi_status_t res;

	res = protocol->start_local_session(protocol, NULL);
	if (res != EFI_INVALID_PARAMETER) {
		efi_st_error(
			"Call to start_local_session should have failed with NULL ctx\n");
		return EFI_ST_FAILURE;
	}

	res = protocol->start_local_session(protocol, &ctx);
	if (res != EFI_SUCCESS) {
		efi_st_error(
			"Call to start_local_session failed unexpectedly\n");
		return EFI_ST_FAILURE;
	}

	char buf[32];
	size_t bufsize = sizeof(buf);
	res = protocol->update_local_session(protocol, NULL, buf, &bufsize);
	if (res != EFI_INVALID_PARAMETER) {
		efi_st_error(
			"Call to update_local_session should have failed with NULL ctx\n");
		return EFI_ST_FAILURE;
	}

	res = protocol->update_local_session(protocol, ctx, NULL, &bufsize);
	if (res != EFI_INVALID_PARAMETER) {
		efi_st_error(
			"Call to update_local_session should have failed with NULL buffer\n");
		return EFI_ST_FAILURE;
	}

	res = protocol->update_local_session(protocol, ctx, buf, NULL);
	if (res != EFI_INVALID_PARAMETER) {
		efi_st_error(
			"Call to update_local_session should have failed with NULL bufsize\n");
		return EFI_ST_FAILURE;
	}

	res = protocol->update_local_session(protocol, ctx, buf, &bufsize);
	if (res != EFI_SUCCESS) {
		efi_st_error(
			"Call to update_local_session failed unexpectedly\n");
		return EFI_ST_FAILURE;
	}

	res = protocol->close_local_session(protocol, NULL);
	if (res != EFI_INVALID_PARAMETER) {
		efi_st_error(
			"Call to close_local_session should have failed with NULL ctx\n");
		return EFI_ST_FAILURE;
	}

	res = protocol->close_local_session(protocol, ctx);
	if (res != EFI_SUCCESS) {
		efi_st_error(
			"Call to close_local_session failed unexpectedly\n");
		return EFI_ST_FAILURE;
	}

	return EFI_ST_SUCCESS;
}

static int setup(const efi_handle_t handle,
		 const struct efi_system_table *systable)
{
	boot_services = systable->boottime;
	efi_status_t res = boot_services->locate_protocol(
		&efi_gbl_fastboot_guid, NULL, (void **)&protocol);
	if (res != EFI_SUCCESS) {
		protocol = NULL;
		efi_st_error("Failed to locate GBL Fastboot protocol\n");
		return EFI_ST_FAILURE;
	}

	return EFI_ST_SUCCESS;
}

static int execute(void)
{
	int res;
	res = test_local_session();
	if (res != EFI_ST_SUCCESS) {
		return res;
	}
	return EFI_ST_SUCCESS;
}

static int teardown(void)
{
	return EFI_ST_SUCCESS;
}

EFI_UNIT_TEST(gbl_fastboot) = {
	.name = "GBL Fastboot Protocol",
	.phase = EFI_EXECUTE_BEFORE_BOOTTIME_EXIT,
	.setup = setup,
	.execute = execute,
	.teardown = teardown,
};
