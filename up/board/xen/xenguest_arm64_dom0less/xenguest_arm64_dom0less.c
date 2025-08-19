
// SPDX-License-Identifier: GPL-2.0
/*
 * (C) 2013
 * David Feng <fenghua@phytium.com.cn>
 * Sharma Bhupesh <bhupesh.sharma@freescale.com>
 *
 * (C) 2020 EPAM Systems Inc
 * (C) 2025 HUSTLER
 */

#include <common.h>
#include <log.h>
#include <cpu_func.h>
#include <dm.h>
#include <errno.h>
#include <malloc.h>
#include <asm/global_data.h>
#include <virtio_types.h>
#include <virtio.h>

#include <asm/io.h>
#include <asm/armv8/mmu.h>

#include <linux/compiler.h>

DECLARE_GLOBAL_DATA_PTR;

int board_init(void)
{
	return 0;
}

/*
 * Use fdt provided by Xen: according to
 * https://www.kernel.org/doc/Documentation/arm64/booting.txt
 * x0 is the physical address of the device tree blob (dtb) in system RAM.
 * This is stored in rom_pointer during low level init.
 */
void *board_fdt_blob_setup(int *err)
{
	return NULL;
}

#define MAX_MEM_MAP_REGIONS 22
static struct mm_region xen_mem_map[MAX_MEM_MAP_REGIONS];
struct mm_region *mem_map = xen_mem_map;

void enable_caches(void)
{
	/* Re-setup the memory map as BSS gets cleared after relocation. */
	icache_enable();
	dcache_enable();
}

/* Read memory settings from the Xen provided device tree. */
int dram_init(void)
{
	return 0;
}

int dram_init_banksize(void)
{
	return fdtdec_setup_memory_banksize();
}

/*
 * Board specific reset that is system reset.
 */
void reset_cpu(void)
{
}

int ft_system_setup(void *blob, struct bd_info *bd)
{
	return 0;
}

int ft_board_setup(void *blob, struct bd_info *bd)
{
	return 0;
}

int print_cpuinfo(void)
{
	printf("Xen virtual CPU\n");
	return 0;
}

void board_cleanup_before_linux(void)
{

}
