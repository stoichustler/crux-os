// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2024 The Android Open Source Project
 */

#include <blk.h>
#include <u-boot/crc.h>
#include <efi_api.h>
#include <efi.h>
#include <android_bootloader_message.h>
#include <efi_gbl_ab.h>
#include <efi_selftest.h>
#include <part.h>
#include <stdlib.h>
#include <string.h>

static struct efi_boot_services *boot_services;
static struct efi_gbl_slot_protocol *protocol;

static int setup(const efi_handle_t handle,
		 const struct efi_system_table *systable)
{
	boot_services = systable->boottime;

	efi_status_t res = boot_services->locate_protocol(&efi_gbl_ab_boot_guid,
							  NULL, (void **)&protocol);
	if (res != EFI_SUCCESS) {
		protocol = NULL;
		efi_st_error("Failed to locate GBL AB boot protocol\n");
		return EFI_ST_FAILURE;
	}

	return EFI_ST_SUCCESS;
}

static int execute(void)
{
	struct efi_gbl_slot_metadata_block meta;
	efi_status_t res = protocol->load_boot_data(protocol, &meta);

	if (res == EFI_CRC_ERROR) {
		efi_st_printf("On-disk metadata corrupted, reinitializing\n");
		res = protocol->reinitialize(protocol);
		if (res != EFI_SUCCESS) {
			efi_st_error("Failed to reinitialize boot data: %lu\n", res);
			return EFI_ST_FAILURE;
		}
		res = protocol->load_boot_data(protocol, &meta);
		if (res != EFI_SUCCESS) {
			efi_st_error("Failed to load boot data after reinitialization: %lu\n", res);
		}
	} else if (res != EFI_SUCCESS) {
		efi_st_error("Failed to load boot data: %lu\n", res);
		return EFI_ST_FAILURE;
	}

	if (meta.max_retries != 7 || meta.slot_count != 2 ||
	    meta.unbootable_metadata != 0) {
		efi_st_error(
			     "metadata: retries = %u, slot_count = %u, unbootable_metadata = %u\n",
			     meta.max_retries, meta.slot_count,
			     meta.unbootable_metadata);
	}

	struct efi_gbl_slot_info slot;

	res = protocol->get_current_slot(protocol, &slot);
	if (res != EFI_SUCCESS) {
		efi_st_error("Failed to get current slot: %lu\n", res);
		return EFI_ST_FAILURE;
	}

	/* Quick checks on the current slot */
	struct efi_gbl_slot_info expected = {
		.suffix = 'a',
		.priority = 15,
		.successful = 0,
		.tries = 7,
		.unbootable_reason = 0,
		.merge_status = 0,
	};

	if (memcmp(&expected, &slot, sizeof(slot)) != 0) {
		efi_st_error("Unexpected active slot:\n");
		efi_st_error("suffix = %u\n", slot.suffix);
		efi_st_error("priority = %u\n", slot.priority);
		efi_st_error("successful = %u\n", slot.successful);
		efi_st_error("tries = %u\n", slot.tries);
		efi_st_error("unbootable_reason = %u\n", slot.unbootable_reason);
		efi_st_error("merge_status = %u\n", slot.merge_status);
	}

	for (int i = 0; i < meta.slot_count; i++) {
		res = protocol->get_slot_info(protocol, i, &slot);
		if (res != EFI_SUCCESS) {
			efi_st_error("Could not get slot at index: %d\n, res = %lu",
				     i, res);
		}
		expected.suffix = 'a' + i;
		if (memcmp(&expected, &slot, sizeof(slot)) != 0) {
			efi_st_error("Unexpected slot value at index: %d\n", i);
		}
	}

	res = protocol->set_active_slot(protocol, 1);
	if (res != EFI_SUCCESS) {
		efi_st_error("Failed to set active slot: %lu\n", res);
	}

	res = protocol->get_current_slot(protocol, &slot);
	if (res != EFI_SUCCESS) {
		efi_st_error("Failed to get current slot after setting active: %lu\n",
			     res);
	}

	if (slot.suffix != 'b') {
		efi_st_error("set_active_slot did not change current_slot\n");
	}

	res = protocol->set_slot_unbootable(protocol, 1, USER_REQUESTED);
	if (res != EFI_SUCCESS) {
		efi_st_error("Failed to set slot unbootable: %lu\n", res);
	}

	res = protocol->get_current_slot(protocol, &slot);
	if (res != EFI_SUCCESS) {
		efi_st_error("Cannot get active slot after making active unbootable: %lu\n",
			     res);
	}

	if (slot.suffix != 'a' || slot.tries != 7) {
		efi_st_error("Incorrect active slot after setting active unbootable\n");
	}

	res = protocol->get_slot_info(protocol, 1, &slot);
	if (res != EFI_SUCCESS) {
		efi_st_error("Failed to get info for slot marked unbootable: %lu\n",
			     res);
	}

	if (slot.suffix != 'b' || slot.tries != 0 || slot.priority != 0) {
		efi_st_error("Failed to mark slot unbootable\n");
	}

	/* Deliberately stop on i = -1 */
	for (int i = meta.max_retries - 1; i > -1; i--) {
		res = protocol->mark_boot_attempt(protocol);
		if (res != EFI_SUCCESS) {
			efi_st_error("Failed to mark boot attempt: %lu\n", res);
			return EFI_ST_FAILURE;
		}

		res = protocol->get_current_slot(protocol, &slot);
		if (res != EFI_SUCCESS) {
			efi_st_error("Failed to get current slot in boot attempt loop: %lu\n",
				     res);
			return EFI_ST_FAILURE;
		}

		if (slot.tries != i) {
			efi_st_error("Unexpected number of tries remaining: %d\n",
				     slot.tries);
			return EFI_ST_FAILURE;
		}
	}

	res = protocol->mark_boot_attempt(protocol);
	if (res != EFI_UNSUPPORTED) {
		efi_st_error("Failed to fail to mark boot attempt on slot with no more tries: %u\n",
			     res);
		return EFI_ST_FAILURE;
	}

	u32 reason;
	size_t size = 0;
	u8 subreason;

	res = protocol->get_boot_reason(protocol, &reason, &size, &subreason);
	if (res != EFI_SUCCESS) {
		efi_st_error("Failed to get boot reason: %lu\n", res);
	}
	if (reason != EMPTY_EFI_BOOT_REASON) {
		efi_st_error("Unexpected boot reason: %u\n", reason);
	}

	res = protocol->set_boot_reason(protocol, RECOVERY, size,
					&subreason);
	if (res != EFI_SUCCESS) {
		efi_st_error("Failed to set boot reason: %lu\n", res);
	}

	res = protocol->get_boot_reason(protocol, &reason, &size, &subreason);
	if (res != EFI_SUCCESS) {
		efi_st_error("Failed to get boot reason: %lu\n", res);
	}
	if (reason != RECOVERY) {
		efi_st_error("Unexpected boot reason: %u\n", reason);
	}

	res = protocol->reinitialize(protocol);
	if (res != EFI_SUCCESS) {
		efi_st_error("Failed to reinitialize AB metadata: %lu\n", res);
		return EFI_ST_FAILURE;
	}

	res = protocol->flush(protocol);
	if (res != EFI_SUCCESS) {
		efi_st_error("Failed to flush slot changes: %lu\n", res);
		return EFI_ST_FAILURE;
	}

	/* Instead of rebooting to make sure changes persist,
	 * just cheat and read them straight off the disk
	 */
	const char *ab_partition_name = "misc";
	struct disk_partition ab_partition;
	struct blk_desc *block_device = blk_get_dev("virtio", 0);

	if (!block_device) {
		efi_st_error("Failed to get backing block device\n");
		return EFI_ST_FAILURE;
	}

	u8 *buffer = calloc(1, block_device->blksz);

	if (!buffer) {
		efi_st_error("Out of resources\n");
		return EFI_ST_FAILURE;
	}

	if (part_get_info_by_name(block_device, ab_partition_name,
				  &ab_partition) < 1) {
		efi_st_error("Couldn't find partition: %s\n",
			     ab_partition_name);
		free(buffer);
		return EFI_ST_FAILURE;
	}

	if (blk_dread(block_device,
		      ab_partition.start + (2048 / ab_partition.blksz),
		      1,
		      buffer) != 1) {
		efi_st_error("Couldn't read from disk\n");
		free(buffer);
		return EFI_ST_FAILURE;
	}

	int cmp = EFI_ST_SUCCESS;
	struct bootloader_control expected_ctrl = {
		.magic = BOOT_CTRL_MAGIC,
		.version = BOOT_CTRL_VERSION,
		.nb_slot = 2,
		.slot_suffix = { 'a', 'b', '\0', '\0' },
		.slot_info = {
			{
				.priority = 15,
				.tries_remaining = 7,
				.successful_boot = 0,
			},
			{
				.priority = 15,
				.tries_remaining = 7,
				.successful_boot = 0,
			},
		{},
		{},
		},
		.crc32_le = 0,
	};
	expected_ctrl.crc32_le = crc32(0,
				       (const u8 *)&expected_ctrl,
				       sizeof(expected_ctrl) - sizeof(expected_ctrl.crc32_le));

	if (memcmp(&expected_ctrl,
		   buffer + (2048 % ab_partition.blksz),
		   sizeof(expected_ctrl)) != 0) {
		efi_st_error("Slot metadata block differs from disk\n");
		cmp = EFI_ST_FAILURE;
	}

	free(buffer);
	return cmp;
}

static int teardown(void)
{
	if (protocol) {
		protocol->reinitialize(protocol);
		protocol->flush(protocol);
	}

	return EFI_ST_SUCCESS;
}

EFI_UNIT_TEST(gbl_ab) = {
	.name = "GBL AB Boot Slot Protocol",
	.phase = EFI_EXECUTE_BEFORE_BOOTTIME_EXIT,
	.setup = setup,
	.execute = execute,
	.teardown = teardown,
};
