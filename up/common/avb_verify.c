// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2018, Linaro Limited
 */

#include <android_bootloader_oemlock.h>
#include <avb_verify.h>
#include <blk.h>
#include <cpu_func.h>
#include <image.h>
#include <linux/bug.h>
#include <linux/string.h>
#include <malloc.h>
#include <part.h>
#include <tee.h>
#include <tee/optee_ta_avb.h>

const char *avb_set_state(AvbOps *ops, enum avb_boot_state boot_state)
{
	struct AvbOpsData *data;

	avb_assert(ops);
	avb_assert(ops->user_data);

	data = (struct AvbOpsData *)ops->user_data;
	data->boot_state = boot_state;

	switch (data->boot_state) {
	case AVB_GREEN:
		return AVB_VERIFIED_BOOT_STATE_GREEN;
	case AVB_YELLOW:
		return AVB_VERIFIED_BOOT_STATE_YELLOW;
	case AVB_ORANGE:
		return AVB_VERIFIED_BOOT_STATE_ORANGE;
	case AVB_RED:
		// In the 'red' state, we supply no command-line arguments, to
		// indicate that Android failed verified boot.
	default:
		// In cases where an unexpected boot state has been supplied,
		// assume the device is not secure.
		data->boot_state = AVB_RED;
		return NULL;
    }
}

/*
 * Appends the given `arg` to the given `cmdline` and returns a newly allocated
 * string containing the result. Callers must later free the result using
 * |avb_free|.
 *
 * Returns: A string with the appended result, or NULL if the allocation failed.
 */
static char *append_arg_to_cmdline(const char *cmdline, const char *arg)
{
	char *cmdline_out = NULL;
	const char *cmdline_tmp = NULL;

	avb_assert(arg);

	// If no command line is supplied, insert a single space.
	cmdline_tmp = cmdline ? cmdline : " ";
	cmdline_out = avb_strdupv(cmdline_tmp, " ", arg, NULL);
	return cmdline_out;
}

static char *avb_set_enforce_option(const char *cmdline, const char *option)
{
	char *cmdline_out = NULL;

	avb_assert(cmdline);
	avb_assert(option);

	cmdline_out = avb_replace(cmdline, VERITY_TABLE_OPT_RESTART, option);
	if (!cmdline_out) {
		return NULL;
	}

	cmdline_out = avb_replace(cmdline_out, VERITY_TABLE_OPT_LOGGING, option);
	if (!cmdline_out) {
		return NULL;
	}

	if (!avb_strstr(cmdline_out, option)) {
		printf("%s: No verity options found\n", __func__);
		return NULL;
	}

	return cmdline_out;
}

char *avb_set_ignore_corruption(const char *cmdline)
{
	char *cmdline_out = NULL;

	avb_assert(cmdline);

	cmdline_out = avb_set_enforce_option(cmdline, VERITY_TABLE_OPT_LOGGING);
	if (!cmdline_out) {
		return NULL;
	}

	return append_arg_to_cmdline(cmdline_out, AVB_VERITY_MODE_IGNORE_CORRUPTION);
}

char *avb_set_enforce_verity(const char *cmdline)
{
	char *cmdline_out = NULL;

	avb_assert(cmdline);

	cmdline_out = avb_set_enforce_option(cmdline, VERITY_TABLE_OPT_RESTART);
	if (!cmdline_out) {
		return NULL;
	}

	return append_arg_to_cmdline(cmdline_out, AVB_VERITY_MODE_ENFORCING);
}

/**
 * ============================================================================
 * IO auxiliary functions
 * ============================================================================
 */
#if !defined(CONFIG_AVB_BUF_ADDR) || (CONFIG_AVB_BUF_ADDR == 0)
__attribute__((aligned(ALLOWED_BUF_ALIGN)))
static char sector_buf[CONFIG_AVB_BUF_SIZE];
static_assert(CONFIG_AVB_BUF_SIZE != 0);

static void *get_sector_buf(void)
{
	return sector_buf;
}

static size_t get_sector_buf_size(void)
{
	return sizeof(sector_buf);
}

#else
static void *get_sector_buf(void)
{
	return map_sysmem(CONFIG_AVB_BUF_ADDR, CONFIG_AVB_BUF_SIZE);
}

static size_t get_sector_buf_size(void)
{
	return (size_t)CONFIG_AVB_BUF_SIZE;
}
#endif

static bool is_buf_unaligned(void *buffer)
{
	return (bool)((uintptr_t)buffer % ALLOWED_BUF_ALIGN);
}

static unsigned long blk_read_and_flush(struct avb_part *part,
					lbaint_t start,
					lbaint_t sectors,
					void *buffer)
{
	unsigned long blks;
	void *tmp_buf;
	size_t buf_size;
	bool unaligned = is_buf_unaligned(buffer);

	if (start < part->info.start) {
		printf("%s: partition start out of bounds\n", __func__);
		return 0;
	}
	if ((start + sectors) > (part->info.start + part->info.size)) {
		sectors = part->info.start + part->info.size - start;
		printf("%s: read sector aligned to partition bounds (%ld)\n",
		       __func__, sectors);
	}

	/*
	 * Reading fails on unaligned buffers, so we have to
	 * use aligned temporary buffer and then copy to destination
	 */
	if (unaligned) {
		debug("%s: handling unaligned read buffer, addr = 0x%p\n",
		      __func__, buffer);
		tmp_buf = get_sector_buf();
		buf_size = get_sector_buf_size();
		if (sectors > buf_size / part->info.blksz)
			sectors = buf_size / part->info.blksz;
	} else {
		tmp_buf = buffer;
	}

	blks = blk_dread(part->blk,
			 start, sectors, tmp_buf);
	/* flush cache after read */
	flush_cache((ulong)tmp_buf, sectors * part->info.blksz);

	if (unaligned)
		memcpy(buffer, tmp_buf, sectors * part->info.blksz);

	return blks;
}

static unsigned long avb_blk_write(struct avb_part *part, lbaint_t start,
				   lbaint_t sectors, void *buffer)
{
	void *tmp_buf;
	size_t buf_size;
	bool unaligned = is_buf_unaligned(buffer);

	if (start < part->info.start) {
		printf("%s: partition start out of bounds\n", __func__);
		return 0;
	}
	if ((start + sectors) > (part->info.start + part->info.size)) {
		sectors = part->info.start + part->info.size - start;
		printf("%s: sector aligned to partition bounds (%ld)\n",
		       __func__, sectors);
	}
	if (unaligned) {
		tmp_buf = get_sector_buf();
		buf_size = get_sector_buf_size();
		debug("%s: handling unaligned read buffer, addr = 0x%p\n",
		      __func__, buffer);
		if (sectors > buf_size / part->info.blksz)
			sectors = buf_size / part->info.blksz;

		memcpy(tmp_buf, buffer, sectors * part->info.blksz);
	} else {
		tmp_buf = buffer;
	}

	return blk_dwrite(part->blk,
			  start, sectors, tmp_buf);
}

static struct avb_part *get_partition(AvbOps *ops, const char *partition)
{
	struct avb_part *part;
	struct AvbOpsData *data;
	size_t dev_part_str_len;
	char *dev_part_str;

	if (!ops)
		return NULL;

	data = ops->user_data;
	if (!data)
		return NULL;

	part = malloc(sizeof(*part));
	if (!part)
		return NULL;

	// format is "<devnum>#<partition>\0"
	dev_part_str_len = strlen(data->devnum) + 1 + strlen(partition) + 1;
	dev_part_str = (char *)malloc(dev_part_str_len);
	if (!dev_part_str) {
		free(part);
		return NULL;
	}

	snprintf(dev_part_str, dev_part_str_len, "%s#%s", data->devnum, partition);
	if (part_get_info_by_dev_and_name_or_num(data->iface, dev_part_str,
						 &part->blk, &part->info, false) < 0) {
		free(part);
		part = NULL;
	}

	free(dev_part_str);
	return part;
}

static AvbIOResult blk_byte_io(AvbOps *ops,
			       const char *partition,
			       s64 offset,
			       size_t num_bytes,
			       void *buffer,
			       size_t *out_num_read,
			       enum io_type io_type)
{
	AvbIOResult res = AVB_IO_RESULT_OK;
	ulong ret;
	struct avb_part *part;
	u64 start_offset, start_sector, sectors, residue;
	u8 *tmp_buf;
	size_t io_cnt = 0;

	if (!partition || !buffer || io_type > IO_WRITE)
		return AVB_IO_RESULT_ERROR_IO;

	part = get_partition(ops, partition);
	if (!part)
		return AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION;

	if (!part->info.blksz) {
		res = AVB_IO_RESULT_ERROR_IO;
		goto err;
	}

	start_offset = calc_offset(part, offset);
	while (num_bytes) {
		start_sector = start_offset / part->info.blksz;
		sectors = num_bytes / part->info.blksz;
		/* handle non block-aligned reads */
		if (start_offset % part->info.blksz ||
		    num_bytes < part->info.blksz) {
			tmp_buf = get_sector_buf();
			if (start_offset % part->info.blksz) {
				residue = part->info.blksz -
					(start_offset % part->info.blksz);
				if (residue > num_bytes)
					residue = num_bytes;
			} else {
				residue = num_bytes;
			}

			if (io_type == IO_READ) {
				ret = blk_read_and_flush(part,
							 part->info.start +
							 start_sector,
							 1, tmp_buf);

				if (ret != 1) {
					printf("%s: read error (%ld, %lld)\n",
					       __func__, ret, start_sector);
					res = AVB_IO_RESULT_ERROR_IO;
					goto err;
				}
				/*
				 * if this is not aligned at sector start,
				 * we have to adjust the tmp buffer
				 */
				tmp_buf += (start_offset % part->info.blksz);
				memcpy(buffer, (void *)tmp_buf, residue);
			} else {
				ret = blk_read_and_flush(part,
							 part->info.start +
							 start_sector,
							 1, tmp_buf);

				if (ret != 1) {
					printf("%s: read error (%ld, %lld)\n",
					       __func__, ret, start_sector);
					res = AVB_IO_RESULT_ERROR_IO;
					goto err;
				}
				memcpy((void *)tmp_buf +
					start_offset % part->info.blksz,
					buffer, residue);

				ret = avb_blk_write(part, part->info.start +
						    start_sector, 1, tmp_buf);
				if (ret != 1) {
					printf("%s: write error (%ld, %lld)\n",
					       __func__, ret, start_sector);
					res = AVB_IO_RESULT_ERROR_IO;
					goto err;
				}
			}

			io_cnt += residue;
			buffer += residue;
			start_offset += residue;
			num_bytes -= residue;
			continue;
		}

		if (sectors) {
			if (io_type == IO_READ) {
				ret = blk_read_and_flush(part,
							 part->info.start +
							 start_sector,
							 sectors, buffer);
			} else {
				ret = avb_blk_write(part,
						    part->info.start +
						    start_sector,
						    sectors, buffer);
			}

			if (!ret) {
				printf("%s: sector read error\n", __func__);
				res = AVB_IO_RESULT_ERROR_IO;
				goto err;
			}

			io_cnt += ret * part->info.blksz;
			buffer += ret * part->info.blksz;
			start_offset += ret * part->info.blksz;
			num_bytes -= ret * part->info.blksz;
		}
	}

	/* Set counter for read operation */
	if (io_type == IO_READ && out_num_read)
		*out_num_read = io_cnt;

err:
	free(part);
	return res;
}

/**
 * ============================================================================
 * AVB 2.0 operations
 * ============================================================================
 */

/**
 * read_from_partition() - reads @num_bytes from  @offset from partition
 * identified by a string name
 *
 * @ops: contains AVB ops handlers
 * @partition_name: partition name, NUL-terminated UTF-8 string
 * @offset: offset from the beginning of partition
 * @num_bytes: amount of bytes to read
 * @buffer: destination buffer to store data
 * @out_num_read:
 *
 * @return:
 *      AVB_IO_RESULT_OK, if partition was found and read operation succeed
 *      AVB_IO_RESULT_ERROR_IO, if i/o error occurred from the underlying i/o
 *            subsystem
 *      AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION, if there is no partition with
 *      the given name
 */
static AvbIOResult read_from_partition(AvbOps *ops,
				       const char *partition_name,
				       s64 offset_from_partition,
				       size_t num_bytes,
				       void *buffer,
				       size_t *out_num_read)
{
	return blk_byte_io(ops, partition_name, offset_from_partition,
			   num_bytes, buffer, out_num_read, IO_READ);
}

/**
 * write_to_partition() - writes N bytes to a partition identified by a string
 * name
 *
 * @ops: AvbOps, contains AVB ops handlers
 * @partition_name: partition name
 * @offset_from_partition: offset from the beginning of partition
 * @num_bytes: amount of bytes to write
 * @buf: data to write
 * @out_num_read:
 *
 * @return:
 *      AVB_IO_RESULT_OK, if partition was found and read operation succeed
 *      AVB_IO_RESULT_ERROR_IO, if input/output error occurred
 *      AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION, if partition, specified in
 *            @partition_name was not found
 */
static AvbIOResult write_to_partition(AvbOps *ops,
				      const char *partition_name,
				      s64 offset_from_partition,
				      size_t num_bytes,
				      const void *buffer)
{
	return blk_byte_io(ops, partition_name, offset_from_partition,
			   num_bytes, (void *)buffer, NULL, IO_WRITE);
}

/**
 * validate_vmbeta_public_key() - checks if the given public key used to sign
 * the vbmeta partition is trusted
 *
 * @ops: AvbOps, contains AVB ops handlers
 * @public_key_data: public key for verifying vbmeta partition signature
 * @public_key_length: length of public key
 * @public_key_metadata:
 * @public_key_metadata_length:
 * @out_key_is_trusted:
 *
 * @return:
 *      AVB_IO_RESULT_OK, if partition was found and read operation succeed
 */
static AvbIOResult validate_vbmeta_public_key(AvbOps *ops,
					      const u8 *public_key_data,
					      size_t public_key_length,
					      const u8
					      *public_key_metadata,
					      size_t
					      public_key_metadata_length,
					      bool *out_key_is_trusted)
{
	if (!public_key_length || !public_key_data || !out_key_is_trusted)
		return AVB_IO_RESULT_ERROR_IO;

	*out_key_is_trusted = (avb_pubkey_is_trusted(public_key_data,
						     public_key_length)
			       == CMD_RET_SUCCESS);

	return AVB_IO_RESULT_OK;
}

#ifdef CONFIG_OPTEE_TA_AVB
static int get_open_session(struct AvbOpsData *ops_data)
{
	struct udevice *tee = NULL;

	while (!ops_data->tee) {
		const struct tee_optee_ta_uuid uuid = TA_AVB_UUID;
		struct tee_open_session_arg arg;
		int rc;

		tee = tee_find_device(tee, NULL, NULL, NULL);
		if (!tee)
			return -ENODEV;

		memset(&arg, 0, sizeof(arg));
		tee_optee_ta_uuid_to_octets(arg.uuid, &uuid);
		rc = tee_open_session(tee, &arg, 0, NULL);
		if (rc || arg.ret)
			continue;

		ops_data->tee = tee;
		ops_data->session = arg.session;
	}

	return 0;
}

static AvbIOResult invoke_func(struct AvbOpsData *ops_data, u32 func,
			       ulong num_param, struct tee_param *param)
{
	struct tee_invoke_arg arg;

	if (get_open_session(ops_data))
		return AVB_IO_RESULT_ERROR_IO;

	memset(&arg, 0, sizeof(arg));
	arg.func = func;
	arg.session = ops_data->session;

	if (tee_invoke_func(ops_data->tee, &arg, num_param, param))
		return AVB_IO_RESULT_ERROR_IO;
	switch (arg.ret) {
	case TEE_SUCCESS:
		return AVB_IO_RESULT_OK;
	case TEE_ERROR_OUT_OF_MEMORY:
		return AVB_IO_RESULT_ERROR_OOM;
	case TEE_ERROR_STORAGE_NO_SPACE:
		return AVB_IO_RESULT_ERROR_INSUFFICIENT_SPACE;
	case TEE_ERROR_ITEM_NOT_FOUND:
		return AVB_IO_RESULT_ERROR_NO_SUCH_VALUE;
	case TEE_ERROR_TARGET_DEAD:
		/*
		 * The TA has paniced, close the session to reload the TA
		 * for the next request.
		 */
		tee_close_session(ops_data->tee, ops_data->session);
		ops_data->tee = NULL;
		return AVB_IO_RESULT_ERROR_IO;
	default:
		return AVB_IO_RESULT_ERROR_IO;
	}
}
#endif

/**
 * read_rollback_index() - gets the rollback index corresponding to the
 * location of given by @out_rollback_index.
 *
 * @ops: contains AvbOps handlers
 * @rollback_index_slot:
 * @out_rollback_index: used to write a retrieved rollback index.
 *
 * @return
 *       AVB_IO_RESULT_OK, if the roolback index was retrieved
 */
static AvbIOResult read_rollback_index(AvbOps *ops,
				       size_t rollback_index_slot,
				       u64 *out_rollback_index)
{
#ifndef CONFIG_OPTEE_TA_AVB
	/* For now we always return 0 as the stored rollback index. */
	debug("%s: rollback protection is not implemented\n", __func__);

	if (out_rollback_index)
		*out_rollback_index = 0;

	return AVB_IO_RESULT_OK;
#else
	AvbIOResult rc;
	struct tee_param param[2];

	if (rollback_index_slot >= TA_AVB_MAX_ROLLBACK_LOCATIONS)
		return AVB_IO_RESULT_ERROR_NO_SUCH_VALUE;

	memset(param, 0, sizeof(param));
	param[0].attr = TEE_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = rollback_index_slot;
	param[1].attr = TEE_PARAM_ATTR_TYPE_VALUE_OUTPUT;

	rc = invoke_func(ops->user_data, TA_AVB_CMD_READ_ROLLBACK_INDEX,
			 ARRAY_SIZE(param), param);
	if (rc)
		return rc;

	*out_rollback_index = (u64)param[1].u.value.a << 32 |
			      (u32)param[1].u.value.b;
	return AVB_IO_RESULT_OK;
#endif
}

/**
 * write_rollback_index() - sets the rollback index corresponding to the
 * location of given by @out_rollback_index.
 *
 * @ops: contains AvbOps handlers
 * @rollback_index_slot:
 * @rollback_index: rollback index to write.
 *
 * @return
 *       AVB_IO_RESULT_OK, if the roolback index was retrieved
 */
static AvbIOResult write_rollback_index(AvbOps *ops,
					size_t rollback_index_slot,
					u64 rollback_index)
{
#ifndef CONFIG_OPTEE_TA_AVB
	/* For now this is a no-op. */
	debug("%s: rollback protection is not implemented\n", __func__);

	return AVB_IO_RESULT_OK;
#else
	struct tee_param param[2];

	if (rollback_index_slot >= TA_AVB_MAX_ROLLBACK_LOCATIONS)
		return AVB_IO_RESULT_ERROR_NO_SUCH_VALUE;

	memset(param, 0, sizeof(param));
	param[0].attr = TEE_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = rollback_index_slot;
	param[1].attr = TEE_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[1].u.value.a = (u32)(rollback_index >> 32);
	param[1].u.value.b = (u32)rollback_index;

	return invoke_func(ops->user_data, TA_AVB_CMD_WRITE_ROLLBACK_INDEX,
			   ARRAY_SIZE(param), param);
#endif
}

/**
 * read_is_device_unlocked() - gets whether the device is unlocked
 *
 * @ops: contains AVB ops handlers
 * @out_is_unlocked: device unlock state is stored here, true if unlocked,
 *       false otherwise
 *
 * @return:
 *       AVB_IO_RESULT_OK: state is retrieved successfully
 *       AVB_IO_RESULT_ERROR_IO: an error occurred
 */
static AvbIOResult read_is_device_unlocked(AvbOps *ops, bool *out_is_unlocked)
{
#if defined(CONFIG_AVB_IS_UNLOCKED)
	*out_is_unlocked = true;
	return AVB_IO_RESULT_OK;
#elif defined(CONFIG_OPTEE_TA_AVB)
	AvbIOResult rc;
	struct tee_param param = { .attr = TEE_PARAM_ATTR_TYPE_VALUE_OUTPUT };

	rc = invoke_func(ops->user_data, TA_AVB_CMD_READ_LOCK_STATE, 1, &param);
	if (rc)
		return rc;
	*out_is_unlocked = !param.u.value.a;
	return AVB_IO_RESULT_OK;
#elif defined(CONFIG_ANDROID_BOOTLOADER_OEMLOCK_CONSOLE)
	int locked = oemlock_is_locked();
	if (locked < 0) {
		*out_is_unlocked = false;
		return AVB_IO_RESULT_ERROR_IO;
	}
	*out_is_unlocked = locked == 0;
	return AVB_IO_RESULT_OK;
#else
	*out_is_unlocked = false;
	return AVB_IO_RESULT_OK;
#endif
}

/**
 * get_unique_guid_for_partition() - gets the GUID for a partition identified
 * by a string name
 *
 * @ops: contains AVB ops handlers
 * @partition: partition name (NUL-terminated UTF-8 string)
 * @guid_buf: buf, used to copy in GUID string. Example of value:
 *      527c1c6d-6361-4593-8842-3c78fcd39219
 * @guid_buf_size: @guid_buf buffer size
 *
 * @return:
 *      AVB_IO_RESULT_OK, on success (GUID found)
 *      AVB_IO_RESULT_ERROR_INSUFFICIENT_SPACE, if incorrect buffer size
 *             (@guid_buf_size) was provided
 *      AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION, if partition was not found
 */
static AvbIOResult get_unique_guid_for_partition(AvbOps *ops,
						 const char *partition,
						 char *guid_buf,
						 size_t guid_buf_size)
{
	struct avb_part *part;

	if (guid_buf_size <= UUID_STR_LEN)
		return AVB_IO_RESULT_ERROR_INSUFFICIENT_SPACE;

	part = get_partition(ops, partition);
	if (!part)
		return AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION;

	strlcpy(guid_buf, part->info.uuid, UUID_STR_LEN + 1);

	free(part);
	return AVB_IO_RESULT_OK;
}

/**
 * get_size_of_partition() - gets the size of a partition identified
 * by a string name
 *
 * @ops: contains AVB ops handlers
 * @partition: partition name (NUL-terminated UTF-8 string)
 * @out_size_num_bytes: returns the value of a partition size
 *
 * @return:
 *      AVB_IO_RESULT_OK, on success (GUID found)
 *      AVB_IO_RESULT_ERROR_INSUFFICIENT_SPACE, out_size_num_bytes is NULL
 *      AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION, if partition was not found
 */
static AvbIOResult get_size_of_partition(AvbOps *ops,
					 const char *partition,
					 u64 *out_size_num_bytes)
{
	struct avb_part *part;

	if (!out_size_num_bytes)
		return AVB_IO_RESULT_ERROR_INSUFFICIENT_SPACE;

	part = get_partition(ops, partition);
	if (!part)
		return AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION;

	*out_size_num_bytes = part->info.blksz * part->info.size;

	free(part);
	return AVB_IO_RESULT_OK;
}

#ifdef CONFIG_OPTEE_TA_AVB
static AvbIOResult read_persistent_value(AvbOps *ops,
					 const char *name,
					 size_t buffer_size,
					 u8 *out_buffer,
					 size_t *out_num_bytes_read)
{
	AvbIOResult rc;
	struct tee_shm *shm_name;
	struct tee_shm *shm_buf;
	struct tee_param param[2];
	struct udevice *tee;
	size_t name_size = strlen(name) + 1;

	if (get_open_session(ops->user_data))
		return AVB_IO_RESULT_ERROR_IO;

	tee = ((struct AvbOpsData *)ops->user_data)->tee;

	rc = tee_shm_alloc(tee, name_size,
			   TEE_SHM_ALLOC, &shm_name);
	if (rc)
		return AVB_IO_RESULT_ERROR_OOM;

	rc = tee_shm_alloc(tee, buffer_size,
			   TEE_SHM_ALLOC, &shm_buf);
	if (rc) {
		rc = AVB_IO_RESULT_ERROR_OOM;
		goto free_name;
	}

	memcpy(shm_name->addr, name, name_size);

	memset(param, 0, sizeof(param));
	param[0].attr = TEE_PARAM_ATTR_TYPE_MEMREF_INPUT;
	param[0].u.memref.shm = shm_name;
	param[0].u.memref.size = name_size;
	param[1].attr = TEE_PARAM_ATTR_TYPE_MEMREF_INOUT;
	param[1].u.memref.shm = shm_buf;
	param[1].u.memref.size = buffer_size;

	rc = invoke_func(ops->user_data, TA_AVB_CMD_READ_PERSIST_VALUE,
			 2, param);
	if (rc)
		goto out;

	if (param[1].u.memref.size > buffer_size) {
		rc = AVB_IO_RESULT_ERROR_NO_SUCH_VALUE;
		goto out;
	}

	*out_num_bytes_read = param[1].u.memref.size;

	memcpy(out_buffer, shm_buf->addr, *out_num_bytes_read);

out:
	tee_shm_free(shm_buf);
free_name:
	tee_shm_free(shm_name);

	return rc;
}

static AvbIOResult write_persistent_value(AvbOps *ops,
					  const char *name,
					  size_t value_size,
					  const u8 *value)
{
	AvbIOResult rc;
	struct tee_shm *shm_name;
	struct tee_shm *shm_buf;
	struct tee_param param[2];
	struct udevice *tee;
	size_t name_size = strlen(name) + 1;

	if (get_open_session(ops->user_data))
		return AVB_IO_RESULT_ERROR_IO;

	tee = ((struct AvbOpsData *)ops->user_data)->tee;

	if (!value_size)
		return AVB_IO_RESULT_ERROR_NO_SUCH_VALUE;

	rc = tee_shm_alloc(tee, name_size,
			   TEE_SHM_ALLOC, &shm_name);
	if (rc)
		return AVB_IO_RESULT_ERROR_OOM;

	rc = tee_shm_alloc(tee, value_size,
			   TEE_SHM_ALLOC, &shm_buf);
	if (rc) {
		rc = AVB_IO_RESULT_ERROR_OOM;
		goto free_name;
	}

	memcpy(shm_name->addr, name, name_size);
	memcpy(shm_buf->addr, value, value_size);

	memset(param, 0, sizeof(param));
	param[0].attr = TEE_PARAM_ATTR_TYPE_MEMREF_INPUT;
	param[0].u.memref.shm = shm_name;
	param[0].u.memref.size = name_size;
	param[1].attr = TEE_PARAM_ATTR_TYPE_MEMREF_INPUT;
	param[1].u.memref.shm = shm_buf;
	param[1].u.memref.size = value_size;

	rc = invoke_func(ops->user_data, TA_AVB_CMD_WRITE_PERSIST_VALUE,
			 2, param);
	if (rc)
		goto out;

out:
	tee_shm_free(shm_buf);
free_name:
	tee_shm_free(shm_name);

	return rc;
}
#endif

/**
 * ============================================================================
 * AVB2.0 AvbOps alloc/initialisation/free
 * ============================================================================
 */
AvbOps *avb_ops_alloc(const char *iface, const char *devnum)
{
	struct AvbOpsData *ops_data;

	ops_data = avb_calloc(sizeof(struct AvbOpsData));
	if (!ops_data)
		return NULL;

	ops_data->ops.user_data = ops_data;

	ops_data->ops.read_from_partition = read_from_partition;
	ops_data->ops.write_to_partition = write_to_partition;
	ops_data->ops.validate_vbmeta_public_key = validate_vbmeta_public_key;
	ops_data->ops.read_rollback_index = read_rollback_index;
	ops_data->ops.write_rollback_index = write_rollback_index;
	ops_data->ops.read_is_device_unlocked = read_is_device_unlocked;
	ops_data->ops.get_unique_guid_for_partition =
		get_unique_guid_for_partition;
#ifdef CONFIG_OPTEE_TA_AVB
	ops_data->ops.write_persistent_value = write_persistent_value;
	ops_data->ops.read_persistent_value = read_persistent_value;
#endif
	ops_data->ops.get_size_of_partition = get_size_of_partition;
	ops_data->iface = avb_strdup(iface);
	ops_data->devnum = avb_strdup(devnum);

	printf("## Android Verified Boot 2.0 version %s\n",
	       avb_version_string());

	return &ops_data->ops;
}

void avb_ops_free(AvbOps *ops)
{
	struct AvbOpsData *ops_data;

	if (!ops)
		return;

	ops_data = ops->user_data;

	if (ops_data) {
#ifdef CONFIG_OPTEE_TA_AVB
		if (ops_data->tee)
			tee_close_session(ops_data->tee, ops_data->session);
#endif
		if (ops_data->iface)
			avb_free((void*)ops_data->iface);

		if (ops_data->devnum)
			avb_free((void*)ops_data->devnum);

		avb_free(ops_data);
	}
}

int avb_verify(struct AvbOps *ops,
	       const char *slot_suffix,
	       AvbSlotVerifyData **out_data,
	       char **out_cmdline)
{
	const char * const requested_partitions[] = {"boot", "vendor_boot", "init_boot", NULL};
	return avb_verify_partitions(ops, slot_suffix, requested_partitions, out_data, out_cmdline);
}

int avb_verify_partitions(struct AvbOps *ops,
	       const char *slot_suffix,
	       const char * const requested_partitions[],
	       AvbSlotVerifyData **out_data,
	       char **out_cmdline)
{
	AvbSlotVerifyResult slot_result;
	bool unlocked = false;
	enum avb_boot_state verified_boot_state = AVB_GREEN;
	AvbSlotVerifyFlags flags = 0;
	const char *extra_args = NULL;

	if (ops->read_is_device_unlocked(ops, &unlocked) !=
	    AVB_IO_RESULT_OK) {
		printf("Can't determine device lock state.\n");
		return CMD_RET_FAILURE;
	}

	if (unlocked) {
		verified_boot_state = AVB_ORANGE;
		flags |= AVB_SLOT_VERIFY_FLAGS_ALLOW_VERIFICATION_ERROR;
	}

	slot_result =
		avb_slot_verify(ops,
				requested_partitions,
				slot_suffix,
				flags,
				AVB_HASHTREE_ERROR_MODE_RESTART_AND_INVALIDATE,
				out_data);

	switch (slot_result) {
	case AVB_SLOT_VERIFY_RESULT_OK:
		printf("Verification passed successfully\n");
		goto success;
	case AVB_SLOT_VERIFY_RESULT_ERROR_VERIFICATION:
		printf("Verification failed\n");
		goto success_if_unlocked;
	case AVB_SLOT_VERIFY_RESULT_ERROR_IO:
		printf("I/O error occurred during verification\n");
		break;
	case AVB_SLOT_VERIFY_RESULT_ERROR_OOM:
		printf("OOM error occurred during verification\n");
		break;
	case AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA:
		printf("Corrupted dm-verity metadata detected\n");
		break;
	case AVB_SLOT_VERIFY_RESULT_ERROR_UNSUPPORTED_VERSION:
		printf("Unsupported version avbtool was used\n");
		break;
	case AVB_SLOT_VERIFY_RESULT_ERROR_ROLLBACK_INDEX:
		printf("Checking rollback index failed\n");
		goto success_if_unlocked;
	case AVB_SLOT_VERIFY_RESULT_ERROR_PUBLIC_KEY_REJECTED:
		printf("Public key was rejected\n");
		goto success_if_unlocked;
	default:
		printf("Unknown error occurred\n");
	}

	return CMD_RET_FAILURE;

success_if_unlocked:
	if (!unlocked) {
		return CMD_RET_FAILURE;
	}
	printf("Returning Verification success due to unlocked bootloader\n");
success:
	extra_args = avb_set_state(ops, verified_boot_state);
	if (out_cmdline) {
		if (extra_args) {
			*out_cmdline = append_arg_to_cmdline((*out_data)->cmdline, extra_args);
		} else {
			*out_cmdline = strdup((*out_data)->cmdline);
		}
	}
	return CMD_RET_SUCCESS;
}

int avb_find_main_pubkey(const AvbSlotVerifyData *data,
			 const uint8_t **key, size_t *size)
{
	avb_assert(data);
	avb_assert(!key || (key && size));

	/*
	 * A precondition of this function is that |avb_slot_verify| is not called
	 * with AVB_SLOT_VERIFY_FLAGS_NO_VBMETA_PARTITION. This guarantees that the
	 * primary vbmeta is at index zero.
	 */
	if (data->num_vbmeta_images == 0
	    || avb_vbmeta_image_verify(data->vbmeta_images[0].vbmeta_data,
				       data->vbmeta_images[0].vbmeta_size,
				       key, size) != AVB_VBMETA_VERIFY_RESULT_OK) {
		return CMD_RET_FAILURE;
	}
	return CMD_RET_SUCCESS;
}

int avb_pubkey_is_trusted(const uint8_t *key, size_t size)
{
	// These variables are generated from CONFIG_AVB_PUBKEY_FILE by bin2c.
	extern const char avb_pubkey[];
	extern const size_t avb_pubkey_size;

	avb_assert(key);

	if (size != avb_pubkey_size || memcmp(avb_pubkey, key, size)) {
		return CMD_RET_FAILURE;
	}
	return CMD_RET_SUCCESS;
}
