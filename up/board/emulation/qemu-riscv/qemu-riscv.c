// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018, Bin Meng <bmeng.cn@gmail.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/ofnode.h>
#include <env.h>
#include <fdtdec.h>
#include <image.h>
#include <log.h>
#include <spl.h>
#include <init.h>
#include <usb.h>
#include <virtio_types.h>
#include <virtio.h>

DECLARE_GLOBAL_DATA_PTR;

#if IS_ENABLED(CONFIG_MTD_NOR_FLASH)
int is_flash_available(void)
{
	if (!ofnode_equal(ofnode_by_compatible(ofnode_null(), "cfi-flash"),
			  ofnode_null()))
		return 1;

	return 0;
}
#endif

int board_init(void)
{
	/*
	 * Make sure virtio bus is enumerated so that peripherals
	 * on the virtio bus can be discovered by their drivers
	 */
	virtio_init();

	return 0;
}

int board_late_init(void)
{
	ulong fdtaddr, kernel_start;
	const char *bootargs;
	ofnode chosen_node;
	int ret;

	/* start usb so that usb keyboard can be used as input device */
	if (CONFIG_IS_ENABLED(USB_KEYBOARD))
		usb_init();

	fdtaddr = (ulong)board_fdt_blob_setup(&ret);
	env_set_hex("fdtaddr", fdtaddr);
	#ifdef CONFIG_SYS_LOAD_ADDR
		env_set_hex("loadaddr", (ulong)CONFIG_SYS_LOAD_ADDR);
	#endif

	chosen_node = ofnode_path("/chosen");
	if (!ofnode_valid(chosen_node)) {
		debug("No chosen node found, can't get kernel start address\n");
		return 0;
	}

	bootargs = ofnode_read_prop(chosen_node, "bootargs", NULL);
	if (bootargs) {
		env_set("cbootargs", bootargs);
	}

#ifdef CONFIG_ARCH_RV64I
	ret = ofnode_read_u64(chosen_node, "riscv,kernel-start",
			      (u64 *)&kernel_start);
#else
	ret = ofnode_read_u32(chosen_node, "riscv,kernel-start",
			      (u32 *)&kernel_start);
#endif
	if (ret) {
		debug("Can't find kernel start address in device tree\n");
		return 0;
	}

	env_set_hex("kernel_start", kernel_start);

	return 0;
}

#ifdef CONFIG_SPL
u32 spl_boot_device(void)
{
	/* RISC-V QEMU only supports RAM as SPL boot device */
	return BOOT_DEVICE_RAM;
}
#endif

#ifdef CONFIG_SPL_LOAD_FIT
int board_fit_config_name_match(const char *name)
{
	/* boot using first FIT config */
	return 0;
}
#endif

void *board_fdt_blob_setup(int *err)
{
	*err = 0;
	/* Stored the DTB address there during our init */
	return (void *)(ulong)gd->arch.firmware_fdt_addr;
}
