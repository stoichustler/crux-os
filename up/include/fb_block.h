// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2023 The Android Open Source Project
 */

#ifndef _FB_BLOCK_H_
#define _FB_BLOCK_H_

struct blk_desc;
struct disk_partition;

/**
 * fastboot_block_get_part_info() - Lookup block partion by name
 *
 * @part_name: Named partition to lookup
 * @dev_desc: Pointer to returned blk_desc pointer
 * @part_info: Pointer to returned struct disk_partition
 * @response: Pointer to fastboot response buffer
 */
int fastboot_block_get_part_info(const char *part_name,
				 struct blk_desc **dev_desc,
				 struct disk_partition *part_info,
				 char *response);

/**
 * fastboot_block_erase() - Erase partition on block device for fastboot
 *
 * @part_name: Named partition to erase
 * @response: Pointer to fastboot response buffer
 */
void fastboot_block_erase(const char *part_name, char *response);

/**
 * fastboot_block_write_raw_image() - Write raw image to block device
 *
 * @dev_desc: Block device we're going write to
 * @info: Partition we're going write to
 * @part_name: Name of partition we're going write to
 * @buffer: Downloaded buffer pointer
 * @download_bytes: Size of content on downloaded buffer pointer
 * @response: Pointer to fastboot response buffer
 */
void fastboot_block_write_raw_image(struct blk_desc *dev_desc,
				    struct disk_partition *info, const char *part_name,
				    void *buffer, u32 download_bytes, char *response);

/**
 * fastboot_block_write_sparse_image() - Write sparse image to block device
 *
 * @dev_desc: Block device we're going write to
 * @info: Partition we're going write to
 * @part_name: Name of partition we're going write to
 * @buffer: Downloaded buffer pointer
 * @response: Pointer to fastboot response buffer
 */
void fastboot_block_write_sparse_image(struct blk_desc *dev_desc, struct disk_partition *info,
				       const char *part_name, void *buffer, char *response);

/**
 * fastboot_block_flash_write() - Write image to block device for fastboot
 *
 * @part_name: Named partition to write image to
 * @download_buffer: Pointer to image data
 * @download_bytes: Size of image data
 * @response: Pointer to fastboot response buffer
 */
void fastboot_block_flash_write(const char *part_name, void *download_buffer,
				u32 download_bytes, char *response);

#endif // _FB_BLOCK_H_
