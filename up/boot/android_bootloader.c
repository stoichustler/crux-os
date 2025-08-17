/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <android_bootloader.h>

#include <android_bootloader_keymint.h>
#include <android_ab.h>
#include <bcb.h>
#include <cli.h>
#include <common.h>
#include <dm/device.h>
#include <dm/uclass.h>
#include <image.h>
#include <env.h>
#include <log.h>
#include <malloc.h>
#include <part.h>
#include <serial.h>
#include <avb_verify.h>
#include <linux/sizes.h>

#define ANDROID_PARTITION_BOOT "boot"
#define ANDROID_PARTITION_VENDOR_BOOT "vendor_boot"
#define ANDROID_PARTITION_RECOVERY "recovery"
#define ANDROID_PARTITION_SYSTEM "system"
#define ANDROID_PARTITION_BOOTCONFIG "bootconfig"
#define ANDROID_PARTITION_INIT_BOOT "init_boot"

#define ANDROID_ARG_SLOT_SUFFIX "androidboot.slot_suffix="
#define ANDROID_ARG_ROOT "root="

#define ANDROID_NORMAL_BOOT "androidboot.force_normal_boot=1"

static enum android_boot_mode android_bootloader_load_and_clear_mode(
	struct blk_desc *dev_desc,
	struct disk_partition *misc_part_info)
{
	enum android_boot_mode ret = ANDROID_BOOT_MODE_NORMAL;
	char bcb_command[32];

	if (bcb_load(dev_desc, misc_part_info)) {
		printf("WARNING: Unable to load the BCB.\n");
		goto out;
	}
	if (bcb_get(BCB_FIELD_COMMAND, bcb_command, sizeof(bcb_command))) {
		printf("WARNING: Unable to load the BCB command field.\n");
		goto out;
	}

	if (!strcmp("bootonce-bootloader", bcb_command)) {
		/* Erase the message in the BCB since this value should be used
		 * only once.
		 */
		ret = ANDROID_BOOT_MODE_BOOTLOADER;

		if (bcb_set(BCB_FIELD_COMMAND, "")) {
			printf("WARNING: Unable to clear BCB field for bootonce-bootloader.\n");
			goto out;
		}
		if (bcb_store())
			printf("WARNING: Unable to clear BCB state for bootonce-bootloader.\n");

		goto out;
	}

	if (!strcmp("boot-recovery", bcb_command) || !strcmp("boot-fastboot", bcb_command))
		ret = ANDROID_BOOT_MODE_RECOVERY;

out:
	bcb_reset();
	return ret;
}

/**
 * Return the reboot reason string for the passed boot mode.
 *
 * @param mode	The Android Boot mode.
 * @return a pointer to the reboot reason string for mode.
 */
static const char *android_boot_mode_str(enum android_boot_mode mode)
{
	switch (mode) {
	case ANDROID_BOOT_MODE_NORMAL:
		return "(none)";
	case ANDROID_BOOT_MODE_RECOVERY:
		return "recovery";
	case ANDROID_BOOT_MODE_BOOTLOADER:
		return "bootloader";
	}
	return NULL;
}

static int android_part_get_info_by_name_suffix(struct blk_desc *dev_desc,
						const char *base_name,
						const char *slot_suffix,
						struct disk_partition *part_info)
{
	char *part_name;
	int part_num;
	size_t part_name_len;

	part_name_len = strlen(base_name) + 1;
	if (slot_suffix)
		part_name_len += strlen(slot_suffix);
	part_name = malloc(part_name_len);
	if (!part_name)
		return -1;
	strcpy(part_name, base_name);
	if (slot_suffix)
		strcat(part_name, slot_suffix);

	part_num = part_get_info_by_name(dev_desc, part_name, part_info);
	if (part_num < 0) {
		debug("ANDROID: Could not find partition \"%s\"\n", part_name);
		part_num = -1;
	}

	free(part_name);
	return part_num;
}

static int android_bootloader_boot_bootloader(void)
{
	const char *fastboot_cmd = env_get("fastbootcmd");

	if (fastboot_cmd)
		return run_command(fastboot_cmd, CMD_FLAG_ENV);
	return -1;
}

static char hex_to_char(uint8_t nibble) {
	if (nibble < 10) {
		return '0' + nibble;
	} else {
		return 'a' + nibble - 10;
	}
}
// Helper function to convert 32bit int to a hex string
static void hex_to_str(char* str, ulong input) {
	str[0] = '0'; str[1] = 'x';
	size_t str_idx = 2;
	uint32_t byte_extracted;
	uint8_t nibble;
	// Assume that this is on a little endian system.
	for(int byte_idx = 3; byte_idx >= 0; byte_idx--) {
		byte_extracted = ((0xFF << (byte_idx*8)) & input) >> (byte_idx*8);
		nibble = byte_extracted & 0xF0;
		nibble = nibble >> 4;
		nibble = nibble & 0xF;
		str[str_idx] = hex_to_char(nibble);
		str_idx++;
		nibble = byte_extracted & 0xF;
		str[str_idx] = hex_to_char(nibble);
		str_idx++;
	}
	str[str_idx] = 0;
}
__weak int android_bootloader_boot_kernel(const struct andr_boot_info* boot_info)
{
	ulong kernel_addr, kernel_size, ramdisk_addr, ramdisk_size;
	char *ramdisk_size_str, *fdt_addr = env_get("fdtaddr");
	char kernel_addr_str[12], ramdisk_addr_size_str[22];
	char *boot_args[] = {
		NULL, kernel_addr_str, ramdisk_addr_size_str, fdt_addr, NULL };

	if (android_image_get_kernel(boot_info, images.verify, NULL, &kernel_size))
		return -1;
	if (android_image_get_ramdisk(boot_info, &ramdisk_addr, &ramdisk_size))
		return -1;

	kernel_addr = android_image_get_kload(boot_info);
	hex_to_str(kernel_addr_str, kernel_addr);
	hex_to_str(ramdisk_addr_size_str, ramdisk_addr);
	ramdisk_size_str = &ramdisk_addr_size_str[strlen(ramdisk_addr_size_str)];
	*ramdisk_size_str = ':';
	hex_to_str(ramdisk_size_str + 1, ramdisk_size);

	printf("Booting kernel at %s with fdt at %s ramdisk %s...\n\n\n",
	       kernel_addr_str, fdt_addr, ramdisk_addr_size_str);
#if defined(CONFIG_ARM) && !defined(CONFIG_ARM64)
	do_bootz(NULL, 0, 4, boot_args);
#else
	do_booti(NULL, 0, 4, boot_args);
#endif

	return -1;
}

/** android_assemble_bootconfig - Assemble the extra bootconfig parameters
 * @return a newly allocated string or NULL if no extra bootconfig
 */
static char *android_assemble_bootconfig(const char *avb_cmdline)
{
	char *bootconfig;
	size_t len = 0;
	size_t avb_len = 0;

	/* +1 for the trailing '\n' in the bootconfig. */
	if (avb_cmdline)
		avb_len = strlen(avb_cmdline) + 1;

	len = avb_len;
	if (len == 0)
		return NULL;
	bootconfig = malloc(len + 1);
	bootconfig[len] = '\0';

	/* Copy cmdline, including null terminators. */
	if (avb_cmdline)
		strncpy(bootconfig, avb_cmdline, avb_len);

	/* Replace the cmdline spaces and null terminators with '\n'. */
	while (len--) {
		char c = bootconfig[len];
		if (c == ' ' || c == '\0')
			bootconfig[len] = '\n';
	}

	return bootconfig;
}

static char *strjoin(const char **chunks, char separator)
{
	int len, joined_len = 0;
	char *ret, *current;
	const char **p;

	for (p = chunks; *p; p++)
		joined_len += strlen(*p) + 1;

	if (!joined_len) {
		ret = malloc(1);
		if (ret)
			ret[0] = '\0';
		return ret;
	}

	ret = malloc(joined_len);
	current = ret;
	if (!ret)
		return ret;

	for (p = chunks; *p; p++) {
		len = strlen(*p);
		memcpy(current, *p, len);
		current += len;
		*current = separator;
		current++;
	}
	/* Replace the last separator by a \0. */
	current[-1] = '\0';
	return ret;
}

/** android_assemble_cmdline - Assemble the command line to pass to the kernel
 * @return a newly allocated string
 */
static char *android_assemble_cmdline(const char *slot_suffix,
				      const char *extra_args,
				      const bool normal_boot,
				      const char *android_kernel_cmdline,
				      const bool bootconfig_used,
				      const char *avb_cmdline)
{
	const char *cmdline_chunks[16];
	const char **current_chunk = cmdline_chunks;
	char *env_cmdline, *cmdline, *rootdev_input;
	char *allocated_suffix = NULL;
	char *allocated_rootdev = NULL;
	unsigned long rootdev_len;

	if (android_kernel_cmdline)
		*(current_chunk++) = android_kernel_cmdline;

	env_cmdline = env_get("bootargs");
	if (env_cmdline)
		*(current_chunk++) = env_cmdline;

	/* The |slot_suffix| needs to be passed to Android init to know what
	 * slot to boot from. This is done through bootconfig when supported.
	 */
	if (slot_suffix && !bootconfig_used) {
		allocated_suffix = malloc(strlen(ANDROID_ARG_SLOT_SUFFIX) +
					  strlen(slot_suffix));
		strcpy(allocated_suffix, ANDROID_ARG_SLOT_SUFFIX);
		strcat(allocated_suffix, slot_suffix);
		*(current_chunk++) = allocated_suffix;
	}

	rootdev_input = env_get("android_rootdev");
	if (rootdev_input) {
		rootdev_len = strlen(ANDROID_ARG_ROOT) + CONFIG_SYS_CBSIZE + 1;
		allocated_rootdev = malloc(rootdev_len);
		strcpy(allocated_rootdev, ANDROID_ARG_ROOT);
		cli_simple_process_macros(rootdev_input,
					  allocated_rootdev +
					  strlen(ANDROID_ARG_ROOT),
					  rootdev_len);
		/* Make sure that the string is null-terminated since the
		 * previous could not copy to the end of the input string if it
		 * is too big.
		 */
		allocated_rootdev[rootdev_len - 1] = '\0';
		*(current_chunk++) = allocated_rootdev;
	}

	if (extra_args) {
		*(current_chunk++) = extra_args;
	}

	if (avb_cmdline && !bootconfig_used) {
		*(current_chunk++) = avb_cmdline;
	}

#ifdef CONFIG_ANDROID_USES_RECOVERY_AS_BOOT
	/* The force_normal_boot param must be passed to android's init sequence
	 * to avoid booting into recovery mode when using recovery as boot.
	 * This is done through bootconfig when supported.
	 * Refer to link below under "Early Init Boot Sequence"
	 * https://source.android.com/devices/architecture/kernel/mounting-partitions-early
	 */
	if (normal_boot && !bootconfig_used) {
		*(current_chunk++) = ANDROID_NORMAL_BOOT;
	}
#endif

	*(current_chunk++) = NULL;
	cmdline = strjoin(cmdline_chunks, ' ');
	free(allocated_suffix);
	free(allocated_rootdev);
	return cmdline;
}

static char *join_str(const char *a, const char *b) {
	size_t len = strlen(a) + strlen(b) + 1 /* null term */;
	char *ret = (char *)malloc(len);
	if (ret == NULL) {
		debug("failed to alloc %zu\n", len);
		return NULL;
	}
	strcpy(ret, a);
	strcat(ret, b);
	return ret;
}

static size_t get_partition_size(AvbOps *ops, char *name, const char *slot_suffix) {
	uint64_t size = 0;
	char *partition_name = join_str(name, slot_suffix);
	if (partition_name == NULL) {
		goto bail;
	}
	AvbIOResult res = ops->get_size_of_partition(ops, partition_name, &size);
	if (res != AVB_IO_RESULT_OK && res != AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION) {
		goto bail;
	}
	free(partition_name);
	return size;

bail:
	debug("failed to determine size for partition %s (slot %s)\n", name, slot_suffix);
	free(partition_name);
	return 0;
}

/**
 * Calls avb_verify() with ops allocated for iface and devnum.
 *
 * Returns AvbSlotVerifyData and kernel command line parameters as out arguments and either
 * CMD_RET_SUCCESS or CMD_RET_FAILURE as the return value.
 */
static int do_avb_verify(const char *iface,
		         const char *devstr,
		         const char *slot_suffix,
		         const char *requested_partitions[],
		         uint8_t *kernel_address,
		         AvbSlotVerifyData **out_data,
		         char **out_cmdline)
{
	int ret = CMD_RET_FAILURE;
	struct AvbOps *ops;
	char *devnum = strdup(devstr);
	char *hash_char = NULL;

	if (devnum == NULL) {
		 printf("OOM when copying devstr\n");
		 return CMD_RET_FAILURE;
	}

	hash_char = strchr(devnum, '#');
	if (hash_char != NULL) {
		*hash_char = '\0';
	}

	ops = avb_ops_alloc(iface, devnum);
	if (ops == NULL) {
		 printf("Failed to initialize avb2\n");
		 goto out;
	}

	/* Android-specific extension. */
	ops->get_preloaded_partition = android_get_preloaded_partition;
	struct AvbOpsData *data = (struct AvbOpsData *)(ops->user_data);
	/* Determine where to preload boot, vendor_boot, and init_boot partitions. Specifically,
	 * the partitions are preloaded to the place where kernel is expected to be loaded.
	 *
	 * When the sum of their sizes are less than 64MB - which is the maximum size of the boot
	 * partition, then the three partitions are loaded next to each other within the 64MB
	 * region. This is to save RAM requirements and is safe because vendor_boot and init_boot
	 * will be relocated to "after" the 64MB boundary and the kernel (which is in the boot
	 * partition) will always be shifted forward (i.e. to the beginning of the partition), and
	 * never backward.
	 *
	 * When the sum of their sizes exceed 64MB, each partition is loaded into a dedicated 64MB
	 * region for safe distancing during the relocation. */
	if (requested_partitions == NULL) {
		size_t boot_size = get_partition_size(ops, "boot", slot_suffix);
		size_t vendor_boot_size = get_partition_size(ops, "vendor_boot", slot_suffix);
		size_t init_boot_size = get_partition_size(ops, "init_boot", slot_suffix);
		bool packed = (boot_size + vendor_boot_size + init_boot_size) <= SZ_64M;
		data->slot_suffix = slot_suffix;
		data->boot.addr = kernel_address;
		data->boot.size = 0; // 0 indicates that it hasn't yet been preloaded.
		data->vendor_boot.addr = data->boot.addr + (packed ? boot_size : ALIGN(boot_size, SZ_64M));
		data->vendor_boot.size = 0;
		if (init_boot_size != 0) {
			data->init_boot.addr = data->vendor_boot.addr + (packed ? vendor_boot_size : ALIGN(vendor_boot_size, SZ_64M));
			data->init_boot.size = 0;
			ret = avb_verify(ops, slot_suffix, out_data, out_cmdline);
		} else {
			const char *min_partition_set[] =
				{ANDROID_PARTITION_BOOT, ANDROID_PARTITION_VENDOR_BOOT, NULL};
			ret = avb_verify_partitions(ops, slot_suffix, min_partition_set, out_data,
						    out_cmdline);
		}
	} else {
		ret = avb_verify_partitions(ops, slot_suffix, requested_partitions, out_data,
					    out_cmdline);
	}

	if (ops != NULL) {
		avb_ops_free(ops);
	}

out:
	free(devnum);
	return ret;
}

int android_bootloader_boot_flow(const char* iface_str,
				 const char* dev_str,
				 struct blk_desc *dev_desc,
				 struct disk_partition *misc_part_info,
				 const char *slot,
				 bool verify,
				 unsigned long kernel_address,
				 struct blk_desc *persistant_dev_desc)
{
	enum android_boot_mode mode = ANDROID_BOOT_MODE_NORMAL;
	struct disk_partition boot_part_info;
	struct disk_partition vendor_boot_part_info;
	struct disk_partition init_boot_part_info;
	int boot_part_num, vendor_boot_part_num, init_boot_part_num;
	char *command_line;
	char slot_suffix[3];
	const char *mode_cmdline = NULL;
	char *avb_cmdline = NULL;
	char *extra_bootconfig = NULL;
	const char *boot_partition = ANDROID_PARTITION_BOOT;
	const char *vendor_boot_partition = ANDROID_PARTITION_VENDOR_BOOT;
	const char *init_boot_partition = ANDROID_PARTITION_INIT_BOOT;
#ifdef CONFIG_ANDROID_SYSTEM_AS_ROOT
	int system_part_num
	struct disk_partition system_part_info;
#endif

	/* Determine the boot mode and clear its value for the next boot if
	 * needed. This is only done is a misc partition is specified; if
	 * there is no misc partition, assume we want the normal boot flow.
	 */
	if (misc_part_info) {
		mode = android_bootloader_load_and_clear_mode(dev_desc, misc_part_info);
		printf("ANDROID: reboot reason: \"%s\"\n", android_boot_mode_str(mode));
	}

	if (!verify) {
		printf("ANDROID: Booting Unverified!!\n");
	}

	if (verify && CONFIG_IS_ENABLED(AVB_IS_UNLOCKED)) {
		printf("ANDROID: Booting Unlocked!!\n");
	}

	bool normal_boot = (mode == ANDROID_BOOT_MODE_NORMAL);
	switch (mode) {
	case ANDROID_BOOT_MODE_NORMAL:
#ifdef CONFIG_ANDROID_SYSTEM_AS_ROOT
		/* In normal mode, we load the kernel from "boot" but append
		 * "skip_initramfs" to the cmdline to make it ignore the
		 * recovery initramfs in the boot partition.
		 */
		mode_cmdline = "skip_initramfs";
#endif
		break;
	case ANDROID_BOOT_MODE_RECOVERY:
#if defined(CONFIG_ANDROID_SYSTEM_AS_ROOT) || defined(CONFIG_ANDROID_USES_RECOVERY_AS_BOOT)
		/* In recovery mode we still boot the kernel from "boot" but
		 * don't skip the initramfs so it boots to recovery.
		 * If on Android device using Recovery As Boot, there is no
		 * recovery  partition.
		 */
#else
		boot_partition = ANDROID_PARTITION_RECOVERY;
#endif
		break;
	case ANDROID_BOOT_MODE_BOOTLOADER:
		/* Bootloader mode enters fastboot. If this operation fails we
		 * simply return since we can't recover from this situation by
		 * switching to another slot.
		 */
		return android_bootloader_boot_bootloader();
	}

	slot_suffix[2] = '\0';

	/* Slot wasn't specified on the command line. Check the environment */
	if (!slot || !slot[0])
		slot = env_get("android_slot_suffix");

	/* Slot wasn't specified on the command line, or in the environment */
	if (!slot || !slot[0]) {
		int slot_num = 0;
#ifdef CONFIG_ANDROID_AB
		/* If U-Boot was built with Android A/B API support, use it to
		 * check the misc partition for the slot to boot, if specified.
		 */
		if (misc_part_info) {
			slot_num = ab_select_slot(dev_desc, misc_part_info,
						  true, normal_boot);
			if (slot_num < 0) {
				log_err("Could not determine Android boot slot.\n");
				slot_num = 0;
				/* Fall through */
			}
		}
#endif
		slot_suffix[0] = '_';
		slot_suffix[1] = BOOT_SLOT_NAME(slot_num);
	} else {
		strncpy(slot_suffix, slot, 2);
	}

	/* Run AVB if requested. During the verification, the bits from the
	 * partitions are loaded by libAVB and are stored in avb_out_data.
	 * We need to use the verified data and shouldn't read data from the
	 * disk again.*/
	AvbSlotVerifyData *avb_out_data = NULL;
	AvbPartitionData *verified_boot_img = NULL;
	AvbPartitionData *verified_init_boot_img = NULL;
	AvbPartitionData *verified_vendor_boot_img = NULL;
	AvbSlotVerifyData *avb_out_bootconfig_data = NULL;
	AvbPartitionData *verified_bootconfig_img = NULL;

	if (verify) {
		if (do_avb_verify(iface_str, dev_str, slot_suffix, NULL, (uint8_t *)kernel_address,
				  &avb_out_data, &avb_cmdline) == CMD_RET_FAILURE) {
			goto bail;
		}
		for (int i = 0; i < avb_out_data->num_loaded_partitions; i++) {
			AvbPartitionData *p =
			    &avb_out_data->loaded_partitions[i];
			if (strcmp(ANDROID_PARTITION_BOOT, p->partition_name) == 0) {
				verified_boot_img = p;
			}
			if (strcmp(ANDROID_PARTITION_INIT_BOOT, p->partition_name) == 0) {
				verified_init_boot_img = p;
			}
			if (strcmp(ANDROID_PARTITION_VENDOR_BOOT, p->partition_name) == 0) {
				verified_vendor_boot_img = p;
			}
		}
		if (verified_boot_img == NULL || verified_vendor_boot_img == NULL) {
			debug("verified partition not found\n");
			goto bail;
		}
		if (verified_init_boot_img == NULL) {
			debug("init_boot not found. Could be a pre-TM device\n");
		}
	}

#ifdef CONFIG_ANDROID_BOOTLOADER_KEYMINT_CONSOLE
	// env_get_yesno returns -1 when the env var is not defined. android_keymint_needed should
	// default to "yes" if CONFIG_ANDROID_BOOTLOADER_KEYMINT_CONSOLE is set. So we demand
	// keymint unless it is explicitly turned off (returns 0).
	const bool keymint_needed = env_get_yesno("android_keymint_needed") != 0;
	if (keymint_needed) {
		struct udevice* km_console = NULL;
		static const char km_name[] = "virtio-console#3";
		if (uclass_get_device_by_name(UCLASS_SERIAL, km_name, &km_console)) {
			log_err("Failed to find keymint console\n");
			goto bail;
		}
		if (avb_out_data != NULL) {
			int r = write_avb_to_keymint_console(avb_out_data, km_console);
			if (r) {
				log_err("Failed to write to KM console: %d\n", r);
				goto bail;
			}
		}
	} else {
		debug("keymint not needed. skipping.\n");
	}
#endif

	// Load device-specific bootconfig if there is any.
	// CONFIG_ANDROID_PERSISTENT_RAW_DISK_DEVICE and ANDROID_PARTITION_BOOTCONFIG specify the
	// disk number and the partition name where the bootconfig is. If the bootloader is locked,
	// the bootconfig is verified using AVB and the verification failure stops the booting.
	struct disk_partition *bootconfig_part_info_ptr = NULL;
#ifdef CONFIG_ANDROID_PERSISTENT_RAW_DISK_DEVICE
	struct disk_partition bootconfig_part_info;
	const char *bootconfig_partition = ANDROID_PARTITION_BOOTCONFIG;
	int bootconfig_part_num = android_part_get_info_by_name_suffix(persistant_dev_desc,
						 bootconfig_partition,
						 NULL,
						 &bootconfig_part_info);
	if (bootconfig_part_num < 0) {
		log_err("Failed to find device specific bootconfig.\n");
	} else {
		bootconfig_part_info_ptr = &bootconfig_part_info;
	}

	if (bootconfig_part_info_ptr != NULL && verify) {
		char devnum_str[3];
		sprintf(devnum_str, "%d", persistant_dev_desc->devnum);
		const char *slot_suffix = ""; // No slots in this disk. Shouldn't be NULL.
		const char *requested_partitions[] = {ANDROID_PARTITION_BOOTCONFIG, NULL};
		if (do_avb_verify(iface_str, devnum_str, slot_suffix, requested_partitions, 0,
				  &avb_out_bootconfig_data, NULL) == CMD_RET_FAILURE) {
			log_err("Failed to verify bootconfig.\n");
			goto bail;
		}
		for (int i = 0; i < avb_out_bootconfig_data->num_loaded_partitions; i++) {
			AvbPartitionData *p =
			    &avb_out_bootconfig_data->loaded_partitions[i];
			if (strcmp(ANDROID_PARTITION_BOOTCONFIG, p->partition_name) == 0) {
				verified_bootconfig_img = p;
			}
		}
		if (verified_bootconfig_img == NULL) {
			log_err("Failed to load bootconfig.\n");
			goto bail;
		}
	}
#endif /* CONFIG_ANDROID_PERSISTENT_RAW_DISK_DEVICE */

	/* Load the kernel from the desired "boot" partition. */
	boot_part_num =
	    android_part_get_info_by_name_suffix(dev_desc, boot_partition,
						 slot_suffix, &boot_part_info);
	init_boot_part_num =
	    android_part_get_info_by_name_suffix(dev_desc, init_boot_partition,
						 slot_suffix, &init_boot_part_info);
	/* Load the vendor boot partition if there is one. */
	vendor_boot_part_num =
	    android_part_get_info_by_name_suffix(dev_desc, vendor_boot_partition,
						 slot_suffix,
						 &vendor_boot_part_info);
	if (init_boot_part_num < 0) {
		debug("Failed to find init_boot partition\n");
	} else {
		printf("ANDROID: Loading ramdisk from \"%s\", partition %d.\n",
			init_boot_part_info.name, init_boot_part_num);
	}
	if (boot_part_num < 0)
		goto bail;
	printf("ANDROID: Loading kernel from \"%s\", partition %d.\n",
		boot_part_info.name, boot_part_num);

#ifdef CONFIG_ANDROID_SYSTEM_AS_ROOT
	system_part_num =
	    android_part_get_info_by_name_suffix(dev_desc,
						 ANDROID_PARTITION_SYSTEM,
						 slot_suffix,
						 &system_part_info);
	if (system_part_num < 0)
		goto bail;
	debug("ANDROID: Using system image from \"%s\", partition %d.\n",
	      system_part_info.name, system_part_num);
#endif

	struct disk_partition *vendor_boot_part_info_ptr = &vendor_boot_part_info;
	if (vendor_boot_part_num < 0) {
		vendor_boot_part_info_ptr = NULL;
	} else {
		printf("ANDROID: Loading vendor ramdisk from \"%s\", partition"
		       " %d.\n", vendor_boot_part_info.name,
		       vendor_boot_part_num);
	}

	extra_bootconfig = android_assemble_bootconfig(avb_cmdline);

	struct andr_boot_info* boot_info = android_image_load(dev_desc, &boot_part_info,
				vendor_boot_part_info_ptr,
				&init_boot_part_info,
				kernel_address, slot_suffix, normal_boot, extra_bootconfig,
				persistant_dev_desc, bootconfig_part_info_ptr,
				verified_boot_img, verified_vendor_boot_img,
				verified_bootconfig_img, verified_init_boot_img);

	if (!boot_info)
		goto bail;


#ifdef CONFIG_ANDROID_SYSTEM_AS_ROOT
	/* Set Android root variables. */
	env_set_ulong("android_root_devnum", dev_desc->devnum);
	env_set_ulong("android_root_partnum", system_part_num);
#endif
	env_set("android_slotsufix", slot_suffix);

	/* Assemble the command line */
	command_line = android_assemble_cmdline(slot_suffix, mode_cmdline, normal_boot,
							android_image_get_kernel_cmdline(boot_info),
							android_image_is_bootconfig_used(boot_info),
							avb_cmdline);
	env_set("bootargs", command_line);

	debug("ANDROID: bootargs: \"%s\"\n", command_line);
	android_bootloader_boot_kernel(boot_info);

	/* TODO: If the kernel doesn't boot mark the selected slot as bad. */
	goto bail;

bail:
	if (avb_out_data != NULL) {
		avb_slot_verify_data_free(avb_out_data);
	}
	if (avb_cmdline != NULL) {
		free(avb_cmdline);
	}
	if (extra_bootconfig != NULL) {
		free(extra_bootconfig);
	}
	if (avb_out_bootconfig_data != NULL) {
		avb_slot_verify_data_free(avb_out_bootconfig_data);
	}
	return -1;
}
