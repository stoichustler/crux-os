// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2016 The Android Open Source Project
 */

#include <android_bootloader.h>
#include <common.h>
#include <command.h>
#include <part.h>

static int boot_android(struct cmd_tbl *cmdtp, int flag, int argc,
			char * const argv[], bool verify)
{
	unsigned long load_address;
	int ret = CMD_RET_SUCCESS;
	char *addr_arg_endp, *addr_str;
	struct blk_desc *boot_dev_desc;
	struct blk_desc *persistant_dev_desc = NULL;
	struct disk_partition misc_part_info, *misc_part_info_p = NULL;
	const char *boot_iface;
	const char *boot_dev_or_misc_part;
	const char *slot = NULL;
	if (argc < 3)
		return CMD_RET_USAGE;
	if (argc > 5)
		return CMD_RET_USAGE;

	if (argc >= 5) {
		load_address = simple_strtoul(argv[4], &addr_arg_endp, 16);
		if (addr_arg_endp == argv[4] || *addr_arg_endp != '\0')
			return CMD_RET_USAGE;
	} else {
		addr_str = env_get("loadaddr");
		if (addr_str)
			load_address = simple_strtoul(addr_str, NULL, 16);
		else
			load_address = CONFIG_SYS_LOAD_ADDR;
	}
	if (argc >= 4)
		slot = argv[3];

	boot_iface = argv[1];
	boot_dev_or_misc_part = argv[2];

	/* Was it passed as "devicenum.hwpartnum#partition_name"? */
	if (part_get_info_by_dev_and_name(boot_iface,
					  boot_dev_or_misc_part,
					  &boot_dev_desc,
					  &misc_part_info) < 0) {
		/* It wasn't, or the specified partition couldn't be found.
		 * Fall back to the non-misc partition flow.
		 */
		char *hash_char = strchr(boot_dev_or_misc_part, '#');
		if (hash_char)
			*hash_char = '\0';
		if (blk_get_device_by_str(boot_iface,
					  boot_dev_or_misc_part,
					  &boot_dev_desc) < 0) {
			/* Invalid devicenum */
			return CMD_RET_FAILURE;
		}
	} else {
		misc_part_info_p = &misc_part_info;
	}

#ifdef CONFIG_ANDROID_PERSISTENT_RAW_DISK_DEVICE
	/* Get the persistent disk that contains the bootconfig partition */
	persistant_dev_desc = blk_get_dev(boot_iface, CONFIG_ANDROID_PERSISTENT_RAW_DISK_DEVICE);
	if (!persistant_dev_desc) {
		printf("Failed to get blk device with dev_num: %d\n", CONFIG_ANDROID_PERSISTENT_RAW_DISK_DEVICE);
		return CMD_RET_FAILURE;
	}
#endif /* CONFIG_ANDROID_PERSISTENT_RAW_DISK_DEVICE */

	ret = android_bootloader_boot_flow(boot_iface, boot_dev_or_misc_part,
					   boot_dev_desc, misc_part_info_p,
					   slot, verify, load_address,
					   persistant_dev_desc);
	if (ret < 0) {
		printf("Android boot failed, error %d.\n", ret);
		return CMD_RET_FAILURE;
	}
	return CMD_RET_SUCCESS;
}

static int do_boot_android(struct cmd_tbl *cmdtp, int flag, int argc,
			   char * const argv[]) {
	bool verify = false;
	return boot_android(cmdtp, flag, argc, argv, verify);
}

static int do_verified_boot_android(struct cmd_tbl *cmdtp, int flag, int argc,
				    char * const argv[]) {
	bool verify = true;
	return boot_android(cmdtp, flag, argc, argv, verify);
}

U_BOOT_CMD(
	boot_android, 5, 0, do_boot_android,
	"Execute the Android Bootloader flow.",
	"<interface> <dev[#part_name]> [<slot>] [<kernel_addr>]\n"
	"    - If 'dev#part_name' is specified, load the Boot Control Block\n"
	"      from the device type 'interface', instance 'dev', partition\n"
	"      'part_name' to determine the boot mode, then load and execute\n"
	"      the appropriate kernel. If 'slot' is not specified, this also\n"
	"      sets the slot up as specified in the BCB.\n"
	"    - If only 'dev' is specified, the boot mode is assumed to be\n"
	"      normal mode. The 'slot' must be specified, or it will be read\n"
	"      from 'android_boot_slot' in the environment, or it will fall\n"
	"      back to assuming the first boot slot (0, or '_a')\n"
	"    - In normal and recovery mode, the kernel will be loaded from\n"
	"      the corresponding \"boot\" partition. In bootloader mode, the\n"
	"      command defined in the \"fastbootcmd\" variable will be\n"
	"      executed.\n"
);
U_BOOT_CMD(
	verified_boot_android, 5, 0, do_verified_boot_android,
	"Execute the Android Verified Boot flow. The arguments are the same \n",
	"as \"boot_android\"."
);
