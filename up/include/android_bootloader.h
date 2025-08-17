/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef __ANDROID_BOOTLOADER_H
#define __ANDROID_BOOTLOADER_H

#include <common.h>

enum android_boot_mode {
	ANDROID_BOOT_MODE_NORMAL = 0,

	/* "recovery" mode is triggered by the "reboot recovery" command or
	 * equivalent adb/fastboot command. It can also be triggered by writing
	 * "boot-recovery" in the BCB message. This mode should boot the
	 * recovery kernel.
	 */
	ANDROID_BOOT_MODE_RECOVERY,

	/* "bootloader" mode is triggered by the "reboot bootloader" command or
	 * equivalent adb/fastboot command. It can also be triggered by writing
	 * "bootonce-bootloader" in the BCB message. This mode should boot into
	 * fastboot.
	 */
	ANDROID_BOOT_MODE_BOOTLOADER,
};

struct blk_desc;
struct disk_partition;

/** android_bootloader_boot_flow - Execute the Android Bootloader Flow.
 * Performs the Android Bootloader boot flow, loading the appropriate Android
 * image (normal kernel, recovery kernel or "bootloader" mode) and booting it.
 * The boot mode is determined by the contents of the Android Bootloader
 * Message. On success it doesn't return.
 *
 * @dev_desc:			device to load the kernel and system from.
 * @misc_part_info:		the "misc" partition descriptor in 'dev_desc'.
 * @slot:			the boot slot to boot from.
 * @verify:			whether to boot using verified boot protocol
 * @kernel_address:		address where to load the kernel if needed.
 * @persistant_dev_desc:	device to load all persistent data from.
 *
 * @return a negative number in case of error, otherwise it doesn't return.
 */
int android_bootloader_boot_flow(const char *iface_str,
				 const char *dev_str,
				 struct blk_desc *dev_desc,
				 struct disk_partition *misc_part_info,
				 const char *slot,
				 bool verify,
				 unsigned long kernel_address,
				 struct blk_desc *persistant_dev_desc);

#endif  /* __ANDROID_BOOTLOADER_H */
