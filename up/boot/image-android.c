// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2011 Sebastian Andrzej Siewior <bigeasy@linutronix.de>
 */

#include <common.h>
#include <env.h>
#include <image.h>
#include <image-android-dt.h>
#include <android_image.h>
#include <malloc.h>
#include <mapmem.h>
#include <errno.h>
#include <asm/unaligned.h>
#include <xbc.h>
#include <part.h>
#include <log.h>
#include <linux/libfdt.h>
#include <linux/sizes.h>
#include <avb_verify.h>

#define BLK_CNT(_num_bytes, _block_size) ((_num_bytes + _block_size - 1) / \
    _block_size)
#define ANDROID_ARG_SLOT_SUFFIX "androidboot.slot_suffix="
#define ANDROID_NORMAL_BOOT "androidboot.force_normal_boot=1\n"

/**
 * android_image_get_kernel() - processes kernel part of Android boot images
 * @hdr:	Pointer to image header, which is at the start
 *			of the image.
 * @verify:	Checksum verification flag. Currently unimplemented.
 * @os_data:	Pointer to a ulong variable, will hold os data start
 *			address.
 * @os_len:	Pointer to a ulong variable, will hold os data length.
 *
 * This function returns the os image's start address and length. Also,
 * it appends the kernel command line to the bootargs env variable.
 *
 * Return: Zero, os start address and length on success,
 *		otherwise on failure.
 */
int android_image_get_kernel(const struct andr_boot_info *boot_info, int verify,
			     ulong *os_data, ulong *os_len)
{
	*os_len = boot_info->kernel_size;
	return 0;
}

int android_image_check_header(const struct andr_boot_info *boot_info)
{
	return 0;
}

ulong android_image_get_end(const struct andr_boot_info *boot_info)
{
	return 0;
}

ulong android_image_get_kload(const struct andr_boot_info *boot_info)
{
	return (ulong)(boot_info->kernel_addr);
}

ulong android_image_get_kcomp(const struct andr_boot_info *boot_info)
{
	return 0;
}

int android_image_get_ramdisk(const struct andr_boot_info *boot_info,
			      ulong *rd_data, ulong *rd_len)
{
	*rd_data = (ulong)(boot_info->vendor_ramdisk_addr);
	*rd_len = boot_info->vendor_ramdisk_size + boot_info->boot_ramdisk_size
		+ boot_info->vendor_bootconfig_size;
	return 0;
}

const char* android_image_get_kernel_cmdline(
		const struct andr_boot_info *boot_info) {
	return boot_info->cmdline;
}

bool android_image_is_bootconfig_used(const struct andr_boot_info *boot_info) {
	return boot_info->vendor_bootconfig_size > 0;
}

/* Read `size` amount of data from `offset` in `part` to `dest`. */
static bool android_read_from_avb_partition(const AvbPartitionData *part,
				    size_t offset, void *dest, size_t size) {
	if (offset + size > part->data_size) {
		debug("Attempted to read past the %s partition boundary. "
			"Offset: %zu, read size: %zu partition size: %zu\n",
			part->partition_name, offset, size, part->data_size);
		return false;
	}
	// memmove, not memcpy as dest might overlap with the source location in case when the part
	// is preloaded.
	memmove(dest, part->data + offset, size);
	return true;
}

/* Read from either block device or buffer that was already filled by libAVB.
 * `name` is a debug string that represents the data to be read.
 * `block_dev` is the block device from which the data is read. This is used
 * only when the buffer from libAVB is empty. i.e. avb_part == NULL.
 * `part`is the partition in the block device where `offset` is based at.
 * `verified_part` is the buffer filled by libAVB from which the data is read.
 * Data should be read from here if this is not NULL.
 * `offset` is the start potition of the data to be read in either `part` or
 * `avb_part`.
 * `dest` is the adddress where the read data is written. The area should be
 * large enough to hold ALIGN(size, part->blksz) amount of data.
 * `size` is the amount of data to be read
 */
static bool android_read_data(const char *name,
			      struct blk_desc *block_dev,
			      const struct disk_partition *part,
			      const AvbPartitionData *verified_part,
			      size_t offset, // from the start of partition
			      void *dest,
			      size_t size) {
	if (size > ALIGN(size, part->blksz)) {
		debug("%s size %zu does align with block boundaries",
		      name, size);
		return false;
	}

	// If verified data is available, we shouldn't do additional I/O again.
	if (verified_part != NULL) {
		return android_read_from_avb_partition(
			verified_part, offset, dest, size);
	}

	// Fallback to I/O. Note that this is a block-based I/O, which means we may read data more
	// than what is requested (i.e. when size is not a multiple of blksz). In that case, the
	// area that will be overwritten by the extra amount of data is saved to a temporary
	// buffer. We then do the block I/O, and copy the temporary buffer back to its original
	// location.
	ulong blksz = part->blksz;
	lbaint_t start = part->start + BLK_CNT(offset, blksz);
	lbaint_t blk_cnt = BLK_CNT(size, blksz);
	size_t overwritten = (blk_cnt * blksz) - size;
	void *temp = NULL;
	if (overwritten > 0) {
		temp = malloc(overwritten);
		if (temp == NULL) {
			debug("Failed to allocate temp buffer while reading partition %s\n", name);
			goto err;
		}
		memcpy(temp, dest + size, overwritten);
	}
	unsigned long blks_read  = blk_dread(block_dev, start, blk_cnt, dest);
	if(blks_read != blk_cnt) {
		debug("%s blk cnt is %ld and blks read is %ld\n",
			name, blk_cnt, blks_read);
		goto err;
	}
	if (overwritten > 0) {
		memcpy(dest + size, temp, overwritten);
		free(temp);
	}
	return true;
err:
	if (temp != NULL) {
		free(temp);
	}
	return false;
}

static struct boot_img_hdr_v4* _extract_boot_image_header(
		struct blk_desc *dev_desc,
		const struct disk_partition *boot_img,
		const AvbPartitionData *verified_boot_img) {
	long blk_cnt = BLK_CNT(sizeof(struct boot_img_hdr_v4), boot_img->blksz);

	struct boot_img_hdr_v4 *boot_hdr = (struct boot_img_hdr_v4*)
		(malloc(blk_cnt * boot_img->blksz));

	if(!boot_hdr) {
		return NULL;
	}

	size_t offset = 0; // header is at the front of the partition
	size_t size = sizeof(struct boot_img_hdr_v4);
	void *laddr = boot_hdr;
	if (!android_read_data("boot header",
			       dev_desc, boot_img, verified_boot_img,
			       offset, laddr, size)) {
		free(boot_hdr);
		return NULL;
	}

	if(strncmp(ANDR_BOOT_MAGIC, (const char *)boot_hdr->magic,
		   ANDR_BOOT_MAGIC_SIZE)) {
		debug("boot header magic is invalid.\n");
		free(boot_hdr);
		return NULL;
	}

	if(boot_hdr->header_version < 3) {
		debug("boot header is less than v3.\n");
                free(boot_hdr);
		return NULL;
	}

	// TODO Add support for boot headers v1 and v2.
	return boot_hdr;
}

static struct vendor_boot_img_hdr_v4* _extract_vendor_boot_image_header(
		struct blk_desc *dev_desc,
		const struct disk_partition *vendor_boot_img,
		const AvbPartitionData *loaded_vendor_boot_img) {
	long blk_cnt = BLK_CNT(sizeof(struct vendor_boot_img_hdr_v4),
			vendor_boot_img->blksz);

	struct vendor_boot_img_hdr_v4 *vboot_hdr =
		(struct vendor_boot_img_hdr_v4*)
		(malloc(blk_cnt * vendor_boot_img->blksz));

	if(!vboot_hdr) {
		return NULL;
	}

	size_t offset = 0; // header is at the front of the partition
	size_t size = sizeof(struct vendor_boot_img_hdr_v4);
	void *laddr = vboot_hdr;
	if (!android_read_data("vendor boot header",
			       dev_desc, vendor_boot_img, loaded_vendor_boot_img,
			       offset, laddr, size)) {
	      free(vboot_hdr);
	      return NULL;
	}

	if(strncmp(VENDOR_BOOT_MAGIC, (const char *)vboot_hdr->magic,
		   VENDOR_BOOT_MAGIC_SIZE)) {
		debug("vendor boot header magic is invalid.\n");
		free(vboot_hdr);
		return NULL;
	}

	if(vboot_hdr->header_version < 3) {
		debug("vendor boot header is less than v3.\n");
		free(vboot_hdr);
		return NULL;
	}

	return vboot_hdr;
}

static void _populate_boot_info(const struct boot_img_hdr_v4* boot_hdr,
		const struct vendor_boot_img_hdr_v4* vboot_hdr,
		const struct boot_img_hdr_v4* init_boot_hdr,
		const void* load_addr,
		struct andr_boot_info *boot_info) {
	boot_info->kernel_size = boot_hdr->kernel_size;
	boot_info->boot_ramdisk_size = (init_boot_hdr && init_boot_hdr->ramdisk_size) ? 
		init_boot_hdr->ramdisk_size : boot_hdr->ramdisk_size;
	boot_info->boot_header_version = boot_hdr->header_version;
	boot_info->vendor_ramdisk_size = vboot_hdr->vendor_ramdisk_size;
	boot_info->tags_addr = vboot_hdr->tags_addr;
	boot_info->os_version = boot_hdr->os_version;
	boot_info->page_size = vboot_hdr->page_size;
	boot_info->dtb_size = vboot_hdr->dtb_size;
	boot_info->dtb_addr = vboot_hdr->dtb_addr;
	boot_info->vendor_header_version = vboot_hdr->header_version;
	if (vboot_hdr->header_version > 3) {
		boot_info->vendor_ramdisk_table_size = vboot_hdr->vendor_ramdisk_table_size;
		boot_info->vendor_ramdisk_table_entry_num = vboot_hdr->vendor_ramdisk_table_entry_num;
		boot_info->vendor_ramdisk_table_entry_size = vboot_hdr->vendor_ramdisk_table_entry_size;
		boot_info->vendor_bootconfig_size = vboot_hdr->vendor_bootconfig_size;
	} else {
		boot_info->vendor_ramdisk_table_size = 0;
		boot_info->vendor_ramdisk_table_entry_num = 0;
		boot_info->vendor_ramdisk_table_entry_size = 0;
		boot_info->vendor_bootconfig_size = 0;
	}

	memset(boot_info->name, 0, ANDR_BOOT_NAME_SIZE);
	strncpy(boot_info->name, (const char *)vboot_hdr->name,
		ANDR_BOOT_NAME_SIZE);

	memset(boot_info->cmdline, 0, TOTAL_BOOT_ARGS_SIZE);

	strncpy(boot_info->cmdline, (const char *)boot_hdr->cmdline,
		sizeof(boot_hdr->cmdline));
	strncat(boot_info->cmdline, " ", 1);
	strncat(boot_info->cmdline, (const char *)vboot_hdr->cmdline,
		sizeof(vboot_hdr->cmdline));

	boot_info->kernel_addr = (ulong)load_addr;
	/* The "kernel_addr" is already aligned to 2MB. */
	if (IS_ENABLED(CONFIG_X86))
		/*
		  We multiple the kernel_size by 3 to let it leave
		  some spaces if the kernel is self-decompressed in-place.
		*/
		boot_info->vendor_ramdisk_addr = boot_info->kernel_addr +
				ALIGN(boot_info->kernel_size, SZ_64M) * 3;
	else
		boot_info->vendor_ramdisk_addr = boot_info->kernel_addr +
				ALIGN(boot_info->kernel_size, SZ_64M);
	boot_info->boot_ramdisk_addr = boot_info->vendor_ramdisk_addr
		+ boot_info->vendor_ramdisk_size;

	boot_info->vendor_bootconfig_addr = boot_info->boot_ramdisk_addr
		+ boot_info->boot_ramdisk_size;
}

static bool _read_in_kernel(struct blk_desc *dev_desc,
		const struct disk_partition *boot_img,
		const struct andr_boot_info *boot_info,
		const AvbPartitionData *verified_boot_img) {

	// kernel is at the block next to the boot header
	size_t page = boot_info->page_size;
	size_t offset = ALIGN(ANDR_BOOT_IMG_HDR_SIZE, page);
	size_t size = boot_info->kernel_size;
	void *laddr = (void*)(uintptr_t)boot_info->kernel_addr;
	if (!android_read_data("kernel", dev_desc, boot_img, verified_boot_img,
			       offset, laddr, size)) {
	      return false;
	}

	return true;
}

static bool _read_in_vendor_ramdisk(struct blk_desc *dev_desc,
		const struct disk_partition *vendor_boot_img,
		const struct andr_boot_info *boot_info,
		const AvbPartitionData *verified_vendor_boot_img) {

	// Vendor ramdisk is next to the vendor boot header
	size_t page = boot_info->page_size;
	size_t offset = ALIGN(sizeof(struct vendor_boot_img_hdr_v4), page);
	size_t size = boot_info->vendor_ramdisk_size;
	void *laddr = (void*)(uintptr_t)boot_info->vendor_ramdisk_addr;
	if (!android_read_data("vendor ramdisk", dev_desc, vendor_boot_img,
			       verified_vendor_boot_img, offset, laddr, size)) {
	      return false;
	}

	return true;
}

static bool _read_in_bootconfig(struct blk_desc *dev_desc,
		const struct disk_partition *vendor_boot_img,
		struct andr_boot_info *boot_info, const char *slot_suffix,
		const bool normal_boot,
		const char *extra_bootconfig,
		struct blk_desc *persistent_dev_desc,
		const struct disk_partition *device_specific_bootconfig_img,
		const AvbPartitionData *verified_bootconfig_img,
		const AvbPartitionData *verified_vendor_boot_img) {
	if (boot_info->vendor_header_version < 4
		|| boot_info->vendor_bootconfig_size == 0) {
		/*
		 * no error, just nothing to do for versions less than 4 or
		 * when vendor boot image has no bootconfig
		 */
		return true;
	}

	long bootconfig_size = 0;

	// Vendor bootconfig is after vendor boot hader, ramdisk, dtb, and
	// ramdisk table
	size_t page = boot_info->page_size;
	size_t offset =
		ALIGN(sizeof(struct vendor_boot_img_hdr_v4), page) +
		ALIGN(boot_info->vendor_ramdisk_size, page) +
		ALIGN(boot_info->dtb_size, page) +
		ALIGN(boot_info->vendor_ramdisk_table_size, page);
	size_t size = boot_info->vendor_bootconfig_size;
	void *laddr = (void*)(uintptr_t)boot_info->vendor_bootconfig_addr;
	if (!android_read_data("vendor bootconfig", dev_desc, vendor_boot_img,
			       verified_vendor_boot_img, offset, laddr, size)) {
	      return false;
	}

	bootconfig_size += boot_info->vendor_bootconfig_size;

	// Add any additional boot config parameters from the boot loader here. The
	// final size of the boot config section will need to be tracked.
	const uint64_t bootconfig_start_addr =
		boot_info->boot_ramdisk_addr + boot_info->boot_ramdisk_size;

	/* The |slot_suffix| needs to be passed to Android init to know what
	 * slot to boot from.
	 */
	char* allocated_suffix = NULL;
	uint32_t suffix_param_size_bytes = strlen(ANDROID_ARG_SLOT_SUFFIX) +
					  strlen(slot_suffix) + 1;
	allocated_suffix = malloc(suffix_param_size_bytes);
	if (!allocated_suffix) {
		debug("Failed to allocate memory for slot_suffix\n");
		return false;
	}
	strcpy(allocated_suffix, ANDROID_ARG_SLOT_SUFFIX);
	strcat(allocated_suffix, slot_suffix);
	strcat(allocated_suffix, "\n");
	int ret = addBootConfigParameters(allocated_suffix, suffix_param_size_bytes,
					  bootconfig_start_addr, bootconfig_size);
	if (ret <= 0) {
		debug("Failed to apply slot_suffix bootconfig param\n");
	} else {
		bootconfig_size += ret;
	}
#ifdef CONFIG_ANDROID_USES_RECOVERY_AS_BOOT
	/* The force_normal_boot param must be passed to android's init sequence
	 * to avoid booting into recovery mode when using recovery as boot.
	 * Refer to link below under "Early Init Boot Sequence"
	 * https://source.android.com/devices/architecture/kernel/mounting-partitions-early
	 */
	if (normal_boot) {
		ret = addBootConfigParameters(ANDROID_NORMAL_BOOT, strlen(ANDROID_NORMAL_BOOT),
					      bootconfig_start_addr, bootconfig_size);
		if (ret <= 0) {
			debug("Failed to apply force_normal_boot bootconfig param\n");
		} else {
			bootconfig_size += ret;
		}
	}
#endif
#ifdef CONFIG_ANDROID_PERSISTENT_RAW_DISK_DEVICE
	if (device_specific_bootconfig_img) {
		// Add persistent factory information
		size_t bootconfig_buffer_size =
			device_specific_bootconfig_img->size * device_specific_bootconfig_img->blksz;
		if (verified_bootconfig_img != NULL) {
			bootconfig_buffer_size = verified_bootconfig_img->data_size;
		}
		char *bootconfig_buffer = (char*)(malloc(bootconfig_buffer_size));
		if (!bootconfig_buffer) {
			printf("Failed to allocate memory for bootconfig_buffer.\n");
			return false;
		}
		size_t offset = 0;
		size_t size = bootconfig_buffer_size;
		void *laddr = bootconfig_buffer;
		if (!android_read_data("device specific bootconfig",
				       persistent_dev_desc, device_specific_bootconfig_img,
				       verified_bootconfig_img,
				       offset, laddr, size)) {
			free(bootconfig_buffer);
			return NULL;
		}

		// blk_dread reads data in block unit (usually 512 bytes). The actual content
		// is likely to be smaller than the read blocks. Don't copy the entire blocks,
		// but only the actual content. Otherwise, bootconfig parameters added via
		// subsequent calls to addBootConfigParameters will be ignored due to null
		// terminators in the extra bits copied here.
		size_t real_size = min(bootconfig_buffer_size, strlen(bootconfig_buffer));
		ret = addBootConfigParameters(bootconfig_buffer, real_size,
					      bootconfig_start_addr, bootconfig_size);
		if (ret <= 0) {
			debug("Failed to apply the persistent bootconfig params\n");
		} else {
			bootconfig_size += ret;
		}
		free(bootconfig_buffer);
	}
#endif /* CONFIG_ANDROID_PERSISTENT_RAW_DISK_DEVICE */

	if (extra_bootconfig) {
		ret = addBootConfigParameters(extra_bootconfig, strlen(extra_bootconfig),
					      bootconfig_start_addr, bootconfig_size);
		if (ret <= 0) {
			debug("Failed to apply extra bootconfig params\n");
		} else {
		  bootconfig_size += ret;
		}
	}

	// Need to update the size after adding parameters
	boot_info->vendor_bootconfig_size = bootconfig_size;

	return true;
}

static bool _read_in_ramdisk(struct blk_desc *dev_desc,
		const struct disk_partition *boot_img,
		const struct andr_boot_info *boot_info,
		const AvbPartitionData *verified_boot_img,
		size_t aligned_kernel_offset) {

	// Ramdisk is after the kernel
	size_t offset =
		ALIGN(ANDR_BOOT_IMG_HDR_SIZE, ANDR_BOOT_IMG_HDR_SIZE) +
		aligned_kernel_offset;
	size_t size = boot_info->boot_ramdisk_size;
	void *laddr = (void*)(uintptr_t)boot_info->boot_ramdisk_addr;
	if (!android_read_data("ramdisk", dev_desc, boot_img, verified_boot_img,
			       offset, laddr, size)) {
	      return false;
	}

	return true;
}

static bool _read_in_boot_ramdisk(struct blk_desc *dev_desc,
		const struct disk_partition *boot_img,
		const struct andr_boot_info *boot_info,
		const AvbPartitionData *verified_boot_img) {

	// Ramdisk is after the kernel
	size_t aligned_kernel_offset =
		ALIGN(boot_info->kernel_size, ANDR_BOOT_IMG_HDR_SIZE);
	return _read_in_ramdisk(dev_desc, boot_img, boot_info, verified_boot_img,
							aligned_kernel_offset);
}

static bool _read_in_init_boot_ramdisk(struct blk_desc *dev_desc,
		const struct disk_partition *boot_img,
		const struct andr_boot_info *boot_info,
		const AvbPartitionData *verified_boot_img) {
	// init_boot does not contain a kernel
	return _read_in_ramdisk(dev_desc, boot_img, boot_info, verified_boot_img,
							/* aligned_kernel_offset */ 0);
}

/* Tests if partition == name + slot_suffix */
static bool is_same_partition(const char *partition, const char *name, const char *slot_suffix) {
	const size_t name_len = strlen(name);
	const size_t slot_suffix_len = slot_suffix ? strlen(slot_suffix) : 0;
	return strncmp(partition, name, name_len) == 0 &&
	    strncmp(partition + name_len, slot_suffix, slot_suffix_len) == 0 &&
	    (strlen(partition) == name_len + slot_suffix_len);
}

AvbIOResult android_get_preloaded_partition(AvbOps *ops,
                                            const char *partition,
                                            size_t num_bytes,
                                            uint8_t **out_pointer,
                                            size_t *out_num_bytes_preloaded) {
	struct AvbOpsData *data = (struct AvbOpsData *)(ops->user_data);
	struct preloaded_partition *preload_info = NULL;

	if (is_same_partition(partition, "boot", data->slot_suffix)) {
		preload_info = &(data->boot);
	} else if (is_same_partition(partition, "vendor_boot", data->slot_suffix)) {
		preload_info = &(data->vendor_boot);
	} else if (is_same_partition(partition, "init_boot", data->slot_suffix)) {
		preload_info = &(data->init_boot);
	}
	if (preload_info == NULL) {
		// Nothing to preload is not an error
		return AVB_IO_RESULT_OK;
	}
	debug("preloading %s at %p (size=0x%zx)\n", partition, preload_info->addr, num_bytes);

	// If the partition hasn't yet been preloaded, do it now.
	if (preload_info->size == 0) {
		AvbIOResult res = ops->read_from_partition(ops, partition, /* offset */ 0,
							   num_bytes, preload_info->addr,
							   &(preload_info->size));
		if (res != AVB_IO_RESULT_OK) {
			return res;
		}
	}
	*out_pointer = preload_info->addr;
	*out_num_bytes_preloaded = preload_info->size;
	return AVB_IO_RESULT_OK;
}

struct andr_boot_info* android_image_load(struct blk_desc *dev_desc,
			const struct disk_partition *boot_img,
			const struct disk_partition *vendor_boot_img,
			const struct disk_partition *init_boot_img,
			unsigned long load_address, const char *slot_suffix,
			const bool normal_boot,
			const char *extra_bootconfig,
			struct blk_desc *persistent_dev_desc,
			const struct disk_partition *device_specific_bootconfig_img,
			const AvbPartitionData *verified_boot_img,
			const AvbPartitionData *verified_vendor_boot_img,
			const AvbPartitionData *verified_bootconfig_img,
			const AvbPartitionData *verified_init_boot_img) {
	struct boot_img_hdr_v4 *boot_hdr = NULL;
	struct boot_img_hdr_v4 *init_boot_hdr = NULL;
	struct vendor_boot_img_hdr_v4 *vboot_hdr = NULL;
	struct andr_boot_info *boot_info = NULL;
	void *kernel_rd_addr = NULL;

	if(!dev_desc || !boot_img || !vendor_boot_img || !load_address) {
		debug("Android Image load inputs are invalid.\n");
		goto image_load_exit;
	}
	if(!init_boot_img) {
		debug("Failed to find init_boot partition.\n");
	}

	boot_hdr = _extract_boot_image_header(dev_desc, boot_img,
					      verified_boot_img);
	init_boot_hdr = _extract_boot_image_header(dev_desc, init_boot_img,
					      verified_init_boot_img);
	vboot_hdr = _extract_vendor_boot_image_header(dev_desc, vendor_boot_img,
						      verified_vendor_boot_img);
	if(!boot_hdr || !vboot_hdr) {
		goto image_load_exit;
	}
	if(!init_boot_hdr) {
		debug("Failed to extract init_boot boot header.\n");
	}

	boot_info = (struct andr_boot_info*)malloc(sizeof(struct andr_boot_info));
	if(!boot_info) {
		debug("Couldn't allocate memory for boot info.\n");
		goto image_load_exit;
	}

	// Read in kernel and ramdisk.
	// TODO cap this memory eventually by only mapping exactly as much
	// memory as needed
	kernel_rd_addr = map_sysmem(load_address, 0 /* size */);
	if(!kernel_rd_addr) {
		debug("Can't map the input load address.\n");
		goto image_load_exit;
	}

	_populate_boot_info(boot_hdr, vboot_hdr, init_boot_hdr, kernel_rd_addr, boot_info);
	// Note: Order is significant here due to preloading. Especially, bootconfig must be read-in
	// prior to boot_ramdisk. This is because bootconfig was preloaded right next to
	// vendor_ramdisk, where boot_ramdisk is read-in. Therefore, reading-in boot_ramdisk might
	// overwrite bootconfig if done before bootconfig is copied to the proper location.
	if(!_read_in_kernel(dev_desc, boot_img, boot_info, verified_boot_img)
		|| !_read_in_vendor_ramdisk(dev_desc, vendor_boot_img,
					    boot_info, verified_vendor_boot_img)
		|| !_read_in_bootconfig(dev_desc, vendor_boot_img, boot_info,
					slot_suffix, normal_boot, extra_bootconfig,
					persistent_dev_desc,
					device_specific_bootconfig_img,
					verified_bootconfig_img,
					verified_vendor_boot_img)
		|| !(((init_boot_hdr != NULL) && _read_in_init_boot_ramdisk(dev_desc, init_boot_img, boot_info,
					  verified_init_boot_img)) ||
			 _read_in_boot_ramdisk(dev_desc, boot_img, boot_info,
					  verified_boot_img))) {
		goto image_load_exit;
	}

	free(boot_hdr);
	free(vboot_hdr);
	free(init_boot_hdr);
	return boot_info;

image_load_exit:
	free(boot_hdr);
	free(vboot_hdr);
	free(boot_info);
	unmap_sysmem(kernel_rd_addr);
	return NULL;
}

int android_image_get_second(const struct andr_boot_info *boot_info,
			      ulong *second_data, ulong *second_len)
{
	return -1;
}

/**
 * android_image_get_dtbo() - Get address and size of recovery DTBO image.
 * @hdr_addr: Boot image header address
 * @addr: If not NULL, will contain address of recovery DTBO image
 * @size: If not NULL, will contain size of recovery DTBO image
 *
 * Get the address and size of DTBO image in "Recovery DTBO" area of Android
 * Boot Image in RAM. The format of this image is Android DTBO (see
 * corresponding "DTB/DTBO Partitions" AOSP documentation for details). Once
 * the address is obtained from this function, one can use 'adtimg' U-Boot
 * command or android_dt_*() functions to extract desired DTBO blob.
 *
 * This DTBO (included in boot image) is only needed for non-A/B devices, and it
 * only can be found in recovery image. On A/B devices we can always rely on
 * "dtbo" partition. See "Including DTBO in Recovery for Non-A/B Devices" in
 * AOSP documentation for details.
 *
 * Return: true on success or false on error.
 */
bool android_image_get_dtbo(ulong hdr_addr, ulong *addr, u32 *size)
{
	return false;
}

/**
 * android_image_get_dtb_by_index() - Get address and size of blob in DTB area.
 * @hdr_addr: Boot image header address
 * @index: Index of desired DTB in DTB area (starting from 0)
 * @addr: If not NULL, will contain address to specified DTB
 * @size: If not NULL, will contain size of specified DTB
 *
 * Get the address and size of DTB blob by its index in DTB area of Android
 * Boot Image in RAM.
 *
 * Return: true on success or false on error.
 */
bool android_image_get_dtb_by_index(ulong hdr_addr, u32 index, ulong *addr,
				    u32 *size)
{
	return false;
}

#if !defined(CONFIG_SPL_BUILD)
/**
 * android_print_contents - prints out the contents of the Android format image
 * @hdr: pointer to the Android format image header
 *
 * android_print_contents() formats a multi line Android image contents
 * description.
 * The routine prints out Android image properties
 *
 * returns:
 *     no returned results
 */
void android_print_contents(const struct andr_boot_info *boot_info)
{
	const char * const p = IMAGE_INDENT_STRING;
	/* os_version = ver << 11 | lvl */
	u32 os_ver = boot_info->os_version >> 11;
	u32 os_lvl = boot_info->os_version & ((1U << 11) - 1);

	printf("%skernel size:          %x\n", p, boot_info->kernel_size);
	printf("%skernel address:       %x\n", p, boot_info->kernel_addr);
	printf("%sramdisk size:         %x\n", p,
		boot_info->vendor_ramdisk_size + boot_info->boot_ramdisk_size);
	printf("%sramdisk address:      %x\n", p,
		boot_info->vendor_ramdisk_addr);
	printf("%stags address:         %x\n", p, boot_info->tags_addr);
	printf("%spage size:            %x\n", p, boot_info->page_size);
	/* ver = A << 14 | B << 7 | C         (7 bits for each of A, B, C)
	 * lvl = ((Y - 2000) & 127) << 4 | M  (7 bits for Y, 4 bits for M) */
	printf("%sos_version:           %x (ver: %u.%u.%u, level: %u.%u)\n",
	       p, boot_info->os_version,
	       (os_ver >> 7) & 0x7F, (os_ver >> 14) & 0x7F, os_ver & 0x7F,
	       (os_lvl >> 4) + 2000, os_lvl & 0x0F);
	printf("%sname:                 %s\n", p, boot_info->name);
	printf("%scmdline:              %s\n", p, boot_info->cmdline);
}

/**
 * android_image_print_dtb_contents() - Print info for DTB blobs in DTB area.
 * @hdr_addr: Boot image header address
 *
 * DTB payload in Android Boot Image v2+ can be in one of following formats:
 *   1. Concatenated DTB blobs
 *   2. Android DTBO format (see CONFIG_CMD_ADTIMG for details)
 *
 * This function does next:
 *   1. Prints out the format used in DTB area
 *   2. Iterates over all DTB blobs in DTB area and prints out the info for
 *      each blob.
 *
 * Return: true on success or false on error.
 */
bool android_image_print_dtb_contents(ulong hdr_addr)
{
	return true;
}
#endif
