// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2024 The Android Open Source Project
 */

#include <avb_verify.h>
#include <efi_api.h>
#include <efi_gbl_os_configuration.h>
#include <efi_loader.h>
#include <efi.h>

#define ANDROID_PARTITION_BOOTCONFIG "bootconfig"

const efi_guid_t efi_gbl_os_config_guid =
	EFI_GBL_OS_CONFIGURATION_PROTOCOL_GUID;

static efi_status_t EFIAPI fixup_kernel_commandline(
	struct efi_gbl_os_configuration_protocol *self,
	const char *command_line, char *fixup, size_t *fixup_buffer_size)
{
	EFI_ENTRY("%p, %p, %p, %p", self, command_line, fixup,
		  fixup_buffer_size);

	if (!self || !command_line || !fixup || !fixup_buffer_size)
		return EFI_EXIT(EFI_INVALID_PARAMETER);

	// No fixup needed, set fixup_buffer_size to 0
	*fixup_buffer_size = 0;

	return EFI_EXIT(EFI_SUCCESS);
}

static efi_status_t
bootconfig_load_from_persistent_disk_device(char *fixup,
					    size_t *fixup_buffer_size)
{
	AvbSlotVerifyData *avb_verify_data = NULL;
	AvbPartitionData *avb_bootconfig_data = NULL;
	struct AvbOps *ops = NULL;
	int ret = 0;
	char devnum_str[3];
	const char *slot_suffix = "";
	const char *requested_partitions[] = { ANDROID_PARTITION_BOOTCONFIG,
					       NULL };

	sprintf(devnum_str, "%d", CONFIG_ANDROID_PERSISTENT_RAW_DISK_DEVICE);
	ops = avb_ops_alloc("virtio", devnum_str);

	ret = avb_verify_partitions(ops, slot_suffix, requested_partitions,
				    &avb_verify_data, NULL);
	if (ret != CMD_RET_SUCCESS) {
		printf("Failed to verify bootconfig partition from persistent disk\n");
		return EFI_LOAD_ERROR;
	}

	for (int i = 0; i < avb_verify_data->num_loaded_partitions; i++) {
		AvbPartitionData *p = &avb_verify_data->loaded_partitions[i];
		if (!strcmp(ANDROID_PARTITION_BOOTCONFIG, p->partition_name))
			avb_bootconfig_data = p;
	}
	if (avb_bootconfig_data == NULL) {
		printf("Failed to verify bootconfig partition from persistent disk\n");
		return EFI_LOAD_ERROR;
	}

	if (avb_bootconfig_data->data_size > *fixup_buffer_size) {
		printf("Buffer too small for bootconfig\n");
		*fixup_buffer_size = avb_bootconfig_data->data_size;
		return EFI_BUFFER_TOO_SMALL;
	}

	*fixup_buffer_size = strlen(avb_bootconfig_data->data);
	memcpy(fixup, avb_bootconfig_data->data, *fixup_buffer_size);

	return EFI_SUCCESS;
}

static efi_status_t EFIAPI fixup_bootconfig(
	struct efi_gbl_os_configuration_protocol *self, const char *bootconfig,
	size_t size, char *fixup, size_t *fixup_buffer_size)
{
	EFI_ENTRY("%p, %p, %zu, %p, %p", self, bootconfig, size, fixup,
		  fixup_buffer_size);

	if (!self || !bootconfig || !fixup || !fixup_buffer_size)
		return EFI_EXIT(EFI_INVALID_PARAMETER);

	if (IS_ENABLED(CONFIG_ANDROID_PERSISTENT_RAW_DISK_DEVICE))
		return EFI_EXIT(bootconfig_load_from_persistent_disk_device(
			fixup, fixup_buffer_size));

	// No fixup needed, set fixup_buffer_size to 0
	*fixup_buffer_size = 0;

	return EFI_EXIT(EFI_SUCCESS);
}

static efi_status_t EFIAPI
select_device_trees(struct efi_gbl_os_configuration_protocol *self,
		    struct efi_gbl_verified_device_tree *device_trees,
		    size_t num_device_trees)
{
	EFI_ENTRY("%p, %p, %zu", self, device_trees, num_device_trees);

	if (!self)
		return EFI_EXIT(EFI_INVALID_PARAMETER);

	return EFI_EXIT(EFI_SUCCESS);
}

static struct efi_gbl_os_configuration_protocol efi_gbl_os_config_proto = {
	.revision = EFI_GBL_OS_CONFIGURATION_PROTOCOL_REVISION,
	.fixup_kernel_commandline = fixup_kernel_commandline,
	.fixup_bootconfig = fixup_bootconfig,
	.select_device_trees = select_device_trees,
};

efi_status_t efi_gbl_os_config_register(void)
{
	efi_status_t ret = efi_add_protocol(efi_root, &efi_gbl_os_config_guid,
					    &efi_gbl_os_config_proto);
	if (ret != EFI_SUCCESS)
		log_err("Failed to install EFI_GBL_OS_CONFIGURATION_PROTOCOL: 0x%lx\n",
			ret);

	return ret;
}
