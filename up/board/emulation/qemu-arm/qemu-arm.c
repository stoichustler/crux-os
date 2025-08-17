// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2017 Tuomas Tynkkynen
 */

#include <common.h>
#include <cpu_func.h>
#include <dm.h>
#include <efi.h>
#include <efi_loader.h>
#include <fdtdec.h>
#include <init.h>
#include <log.h>
#include <usb.h>
#include <virtio_types.h>
#include <virtio.h>
#include <env.h>
#include <mapmem.h>

#include <linux/kernel.h>
#include <linux/sizes.h>

/* GUIDs for capsule updatable firmware images */
#define QEMU_ARM_UBOOT_IMAGE_GUID \
	EFI_GUID(0xf885b085, 0x99f8, 0x45af, 0x84, 0x7d, \
		 0xd5, 0x14, 0x10, 0x7a, 0x4a, 0x2c)

#define QEMU_ARM64_UBOOT_IMAGE_GUID \
	EFI_GUID(0x058b7d83, 0x50d5, 0x4c47, 0xa1, 0x95, \
		 0x60, 0xd8, 0x6a, 0xd3, 0x41, 0xc4)

#ifdef CONFIG_ARM64
#include <asm/armv8/mmu.h>

#if CONFIG_IS_ENABLED(EFI_HAVE_CAPSULE_SUPPORT)
struct efi_fw_image fw_images[] = {
#if defined(CONFIG_TARGET_QEMU_ARM_32BIT)
	{
		.image_type_id = QEMU_ARM_UBOOT_IMAGE_GUID,
		.fw_name = u"Qemu-Arm-UBOOT",
		.image_index = 1,
	},
#elif defined(CONFIG_TARGET_QEMU_ARM_64BIT)
	{
		.image_type_id = QEMU_ARM64_UBOOT_IMAGE_GUID,
		.fw_name = u"Qemu-Arm-UBOOT",
		.image_index = 1,
	},
#endif
};

struct efi_capsule_update_info update_info = {
	.num_images = ARRAY_SIZE(fw_images),
	.images = fw_images,
};

u8 num_image_type_guids = ARRAY_SIZE(fw_images);
#endif /* EFI_HAVE_CAPSULE_SUPPORT */

#if CONFIG_IS_ENABLED(CROSVM_MEM_MAP)

static struct mm_region crosvm_arm64_mem_map[] = {
	{
		/*
		 * Emulated I/O: 0x0000_0000-0x0001_0000
		 * PCI (virtio): 0x0001_0000-0x1110_0000
		 * GIC region  : 0x????_????-0x4000_0000
		 */
		.virt = 0x00000000UL,
		.phys = 0x00000000UL,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, /* 0x4000_0000-0x7000_0000: RESERVED */ {
		/* Firmware region */
		.virt = CONFIG_SYS_SDRAM_BASE - SZ_256M,
		.phys = CONFIG_SYS_SDRAM_BASE - SZ_256M,
		.size = SZ_256M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		 /* RAM region */
		.virt = CONFIG_SYS_SDRAM_BASE,
		.phys = CONFIG_SYS_SDRAM_BASE,
		.size = 255UL * SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map = crosvm_arm64_mem_map;

#else

static struct mm_region qemu_arm64_mem_map[] = {
	{
		/* Flash */
		.virt = 0x00000000UL,
		.phys = 0x00000000UL,
		.size = 0x08000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/* Lowmem peripherals */
		.virt = 0x08000000UL,
		.phys = 0x08000000UL,
		.size = 0x38000000,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* RAM */
		.virt = 0x40000000UL,
		.phys = 0x40000000UL,
		.size = 255UL * SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/* Highmem PCI-E ECAM memory area */
		.virt = 0x4010000000ULL,
		.phys = 0x4010000000ULL,
		.size = 0x10000000,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* Highmem PCI-E MMIO memory area */
		.virt = 0x8000000000ULL,
		.phys = 0x8000000000ULL,
		.size = 0x8000000000ULL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map = qemu_arm64_mem_map;

#endif

#endif /* ARM64 */

int board_init(void)
{
	return 0;
}

int board_late_init(void)
{
	int err;
	ulong fdtaddr = (ulong)board_fdt_blob_setup(&err);

	env_set_hex("fdtaddr", fdtaddr);
#ifdef CONFIG_SYS_LOAD_ADDR
	env_set_hex("loadaddr", (ulong)CONFIG_SYS_LOAD_ADDR);
#endif

	/*
	 * If the in-memory FDT blob defines /chosen bootargs, back them
	 * up so that the boot script can use them to define bootargs.
	 */
	void *fdt = map_sysmem(fdtaddr, 0);
	int nodeoffset = fdt_path_offset(fdt, "/chosen");
	if (nodeoffset >= 0) {
		int bootargs_len;
		const void *nodep = fdt_getprop(fdt, nodeoffset, "bootargs",
						&bootargs_len);
		if (nodep && bootargs_len > 0)
			env_set("cbootargs", (void *)nodep);
	}
	unmap_sysmem(fdt);

	/*
	 * Make sure virtio bus is enumerated so that peripherals
	 * on the virtio bus can be discovered by their drivers
	 */
	virtio_init();

	/* start usb so that usb keyboard can be used as input device */
	if (CONFIG_IS_ENABLED(USB_KEYBOARD))
		usb_init();

	return 0;
}

int dram_init(void)
{
	if (fdtdec_setup_mem_size_base() != 0)
		return -EINVAL;

	return 0;
}

int dram_init_banksize(void)
{
	fdtdec_setup_memory_banksize();

	return 0;
}

void *board_fdt_blob_setup(int *err)
{
	*err = 0;
	/* QEMU loads a generated DTB for us. */
	return (void *)CONFIG_SYS_FDT_ADDR;
}

void enable_caches(void)
{
	 icache_enable();
	 dcache_enable();
}

#ifdef CONFIG_ARM64
#define __W	"w"
#else
#define __W
#endif

u8 flash_read8(void *addr)
{
	u8 ret;

	asm("ldrb %" __W "0, %1" : "=r"(ret) : "m"(*(u8 *)addr));
	return ret;
}

u16 flash_read16(void *addr)
{
	u16 ret;

	asm("ldrh %" __W "0, %1" : "=r"(ret) : "m"(*(u16 *)addr));
	return ret;
}

u32 flash_read32(void *addr)
{
	u32 ret;

	asm("ldr %" __W "0, %1" : "=r"(ret) : "m"(*(u32 *)addr));
	return ret;
}

void flash_write8(u8 value, void *addr)
{
	asm("strb %" __W "1, %0" : "=m"(*(u8 *)addr) : "r"(value));
}

void flash_write16(u16 value, void *addr)
{
	asm("strh %" __W "1, %0" : "=m"(*(u16 *)addr) : "r"(value));
}

void flash_write32(u32 value, void *addr)
{
	asm("str %" __W "1, %0" : "=m"(*(u32 *)addr) : "r"(value));
}
