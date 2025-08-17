// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2023 The Android Open Source Project
 */

#include <blk.h>
#include <div64.h>
#include <fastboot-internal.h>
#include <fastboot.h>
#include <fb_block.h>
#include <image-sparse.h>
#include <part.h>

/**
 * FASTBOOT_MAX_BLOCKS_ERASE - maximum blocks to erase per derase call
 *
 * in the ERASE case we can have much larger buffer size since
 * we're not transfering an actual buffer
 */
#define FASTBOOT_MAX_BLOCKS_ERASE 1048576
/**
 * FASTBOOT_MAX_BLOCKS_WRITE - maximum blocks to write per dwrite call
 */
#define FASTBOOT_MAX_BLOCKS_WRITE 65536

struct fb_block_sparse {
	struct blk_desc	*dev_desc;
};

static lbaint_t fb_block_write(struct blk_desc *block_dev, lbaint_t start,
			       lbaint_t blkcnt, const void *buffer)
{
	lbaint_t blk = start;
	lbaint_t blks_written = 0;
	lbaint_t cur_blkcnt = 0;
	lbaint_t blks = 0;
	int step = buffer ? FASTBOOT_MAX_BLOCKS_WRITE : FASTBOOT_MAX_BLOCKS_ERASE;
	int i;

	for (i = 0; i < blkcnt; i += step) {
		cur_blkcnt = min((int)blkcnt - i, step);
		if (buffer) {
			if (fastboot_progress_callback)
				fastboot_progress_callback("writing");
			blks_written = blk_dwrite(block_dev, blk, cur_blkcnt,
						  buffer + (i * block_dev->blksz));
		} else {
			if (fastboot_progress_callback)
				fastboot_progress_callback("erasing");
			blks_written = blk_derase(block_dev, blk, cur_blkcnt);
		}
		blk += blks_written;
		blks += blks_written;
	}
	return blks;
}

static lbaint_t fb_block_sparse_write(struct sparse_storage *info,
				      lbaint_t blk, lbaint_t blkcnt,
				      const void *buffer)
{
	struct fb_block_sparse *sparse = info->priv;
	struct blk_desc *dev_desc = sparse->dev_desc;

	return fb_block_write(dev_desc, blk, blkcnt, buffer);
}

static lbaint_t fb_block_sparse_reserve(struct sparse_storage *info,
					lbaint_t blk, lbaint_t blkcnt)
{
	return blkcnt;
}

int fastboot_block_get_part_info(const char *part_name,
				 struct blk_desc **dev_desc,
				 struct disk_partition *part_info,
				 char *response)
{
	int ret;
	const char* interface = config_opt_enabled(CONFIG_FASTBOOT_FLASH_BLOCK,
						   CONFIG_FASTBOOT_FLASH_BLOCK_INTERFACE_NAME,
						   NULL);
	const int device = config_opt_enabled(CONFIG_FASTBOOT_FLASH_BLOCK,
					      CONFIG_FASTBOOT_FLASH_BLOCK_DEVICE_ID, -1);

	if (!part_name || !strcmp(part_name, "")) {
		fastboot_fail("partition not given", response);
		return -ENOENT;
	}
	if (!interface || !strcmp(interface, "")) {
		fastboot_fail("block interface isn't provided", response);
		return -EINVAL;
	}

	*dev_desc = blk_get_dev(interface, device);
	if (!dev_desc) {
		fastboot_fail("no such device", response);
		return -ENODEV;
	}

	ret = part_get_info_by_name(*dev_desc, part_name, part_info);
	if (ret < 0)
		fastboot_fail("failed to get partition info", response);

	return ret;
}

void fastboot_block_erase(const char *part_name, char *response)
{
	struct blk_desc *dev_desc;
	struct disk_partition part_info;
	lbaint_t written;

	fastboot_block_get_part_info(part_name, &dev_desc, &part_info, response);
	if (!dev_desc)
		return;

	written = fb_block_write(dev_desc, part_info.start, part_info.size, NULL);
	if (written != part_info.size) {
		fastboot_fail("failed to erase partition", response);
		return;
	}

	fastboot_okay(NULL, response);
}

void fastboot_block_write_raw_image(struct blk_desc *dev_desc,
				    struct disk_partition *info, const char *part_name,
				    void *buffer, u32 download_bytes, char *response)
{
	lbaint_t blkcnt;
	lbaint_t blks;

	/* determine number of blocks to write */
	blkcnt = ((download_bytes + (info->blksz - 1)) & ~(info->blksz - 1));
	blkcnt = lldiv(blkcnt, info->blksz);

	if (blkcnt > info->size) {
		pr_err("too large for partition: '%s'\n", part_name);
		fastboot_fail("too large for partition", response);
		return;
	}

	puts("Flashing Raw Image\n");

	blks = fb_block_write(dev_desc, info->start, blkcnt, buffer);

	if (blks != blkcnt) {
		pr_err("failed writing to device %d\n", dev_desc->devnum);
		fastboot_fail("failed writing to device", response);
		return;
	}

	printf("........ wrote " LBAFU " bytes to '%s'\n", blkcnt * info->blksz,
	       part_name);
	fastboot_okay(NULL, response);
}

void fastboot_block_write_sparse_image(struct blk_desc *dev_desc, struct disk_partition *info,
				       const char *part_name, void *buffer, char *response)
{
	struct fb_block_sparse sparse_priv;
	struct sparse_storage sparse;
	int err;

	sparse_priv.dev_desc = dev_desc;

	sparse.blksz = info->blksz;
	sparse.start = info->start;
	sparse.size = info->size;
	sparse.write = fb_block_sparse_write;
	sparse.reserve = fb_block_sparse_reserve;
	sparse.mssg = fastboot_fail;

	printf("Flashing sparse image at offset " LBAFU "\n",
	       sparse.start);

	sparse.priv = &sparse_priv;
	err = write_sparse_image(&sparse, part_name, buffer,
				 response);
	if (!err)
		fastboot_okay(NULL, response);
}

void fastboot_block_flash_write(const char *part_name, void *download_buffer,
				u32 download_bytes, char *response)
{
	struct blk_desc *dev_desc;
	struct disk_partition part_info;

	fastboot_block_get_part_info(part_name, &dev_desc, &part_info, response);
	if (!dev_desc)
		return;

	if (is_sparse_image(download_buffer)) {
		fastboot_block_write_sparse_image(dev_desc, &part_info, part_name,
						  download_buffer, response);
	} else {
		fastboot_block_write_raw_image(dev_desc, &part_info, part_name,
					       download_buffer, download_bytes, response);
	}
}
