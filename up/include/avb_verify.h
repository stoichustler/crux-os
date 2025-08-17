/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2018, Linaro Limited
 */

#ifndef _AVB_VERIFY_H
#define _AVB_VERIFY_H

#include <../lib/libavb/libavb.h>
#include <blk.h>
#include <command.h>
#include <mapmem.h>
#include <part.h>

// The maximum number of kernel command line arguments to process.
#define AVB_MAX_ARGS 1024
// The command-line flags to control restart if verity table is corrupted.
#define VERITY_TABLE_OPT_RESTART "restart_on_corruption"
#define VERITY_TABLE_OPT_LOGGING "ignore_corruption"
// The command-line flags to control verity mode.
#define AVB_VERITY_MODE_OPT(state) \
    "androidboot.veritymode=" state
#define AVB_VERITY_MODE_ENFORCING AVB_VERITY_MODE_OPT("enforcing")
#define AVB_VERITY_MODE_IGNORE_CORRUPTION AVB_VERITY_MODE_OPT("eio")
// The minimum alignment in bytes for I/O buffers.
#define ALLOWED_BUF_ALIGN 8

/*
 * Verified boot states for Android.
 * AVB_GREEN represents that the system is in the LOCKED state (refer to
 * `androidboot.vbmeta.device_state`) and the key used for verification was not
 * set by the end user.
 *
 * AVB_YELLOW represents that the system is in the LOCKED state (refer to
 * `androidboot.vbmeta.device_state`) and the key used for verification was set
 * by the end user.
 *
 * AVB_ORANGE represents that the system is in the UNLOCKED state (refer to
 * `androidboot.vbmeta.device_state`).
 *
 * AVB_RED represents that the system is in the LOCKED state (refer to
 * `androidboot.vbmeta.device_state`) and failed verification.
 */
enum avb_boot_state { AVB_GREEN, AVB_YELLOW, AVB_ORANGE, AVB_RED };

// Verified boot state command line options.
#define AVB_VERIFIED_BOOT_STATE_OPT(state) \
    "androidboot.verifiedbootstate=" state
#define AVB_VERIFIED_BOOT_STATE_GREEN AVB_VERIFIED_BOOT_STATE_OPT("green")
#define AVB_VERIFIED_BOOT_STATE_YELLOW AVB_VERIFIED_BOOT_STATE_OPT("yellow")
#define AVB_VERIFIED_BOOT_STATE_ORANGE AVB_VERIFIED_BOOT_STATE_OPT("orange")

// Represents the contents of a preloaded partition.
struct preloaded_partition {
  // The buffer containing partition data.
  uint8_t *addr;
  /*
   * The size of the `addr` buffer. A size of 0 indicates that the
   * partition has not been preloaded yet.
   */
  size_t size;
};

// Represents the set of verified boot data.
struct AvbOpsData {
  // The libavb operation handles to perform verification.
  struct AvbOps ops;
  // The device interface name to read partition data from.
  const char *iface;
  // The device number of the partition.
  const char *devnum;
  // The verified boot state of the partition.
  enum avb_boot_state boot_state;
#ifdef CONFIG_OPTEE_TA_AVB
  // A handle to the Trusted Execution Environment (TEE).
  struct udevice *tee;
  // The TEE session ID.
  u32 session;
#endif
  /*
   * The slot suffix for A/B partitions. For partitions that do not use
   * the A/B scheme, this value may be unset.
   */
  const char *slot_suffix;
  // The preloaded 'boot' partition.
  struct preloaded_partition boot;
  // The preloaded 'vendor_boot' partition.
  struct preloaded_partition vendor_boot;
  // The preloaded 'init_boot' partition.
  struct preloaded_partition init_boot;
};

/*
 * Describes the block and disk partition information of a verified boot
 * partition for I/O operations.
 */
struct avb_part {
  // Handle to inspect block data and perform read/write/erase operations.
  struct blk_desc *blk;
  // Block and UUID/GUID information.
  struct disk_partition info;
};

// I/O operation types.
enum io_type { IO_READ, IO_WRITE };

/*
 * Allocates and returns an AvbOps handle to perform verified boot operations on
 * the provided device interface and device number. Callers of this function
 * must later free the allocated memory via |avb_ops_free|.
 *
 * Returns: An AvbOps handle if sufficient memory is available, NULL otherwise.
 */
AvbOps *avb_ops_alloc(const char *iface, const char *devnum);

/*
 * Frees a previously allocated AvbOps handle. If a TEE session handle was
 * allocated during operation, it is also closed here. This operation
 * invalidates the entire AvbOps structure.
 */
void avb_ops_free(AvbOps *ops);

/*
 * Sets the verified boot state in the provided `ops` handle and returns the
 * corresponding "android.verifiedbootstate" string to append to the
 * command-line.
 *
 * Precondition: `ops->user_data` must be non-NULL.
 *
 * Returns: The "android.verifiedbootstate" string to append to the
 * command-line on success, NULL otherwise.
 */
const char *avb_set_state(AvbOps *ops, enum avb_boot_state boot_state);

/*
 * Returns a new command-line that appends the flag to require verity to the
 * provided `cmdline`. Callers must later free the allocated memory via
 * |avb_free|.
 *
 * Precondition: `cmdline` must contain less than AVB_MAX_ARGS arguments.
 *
 * Returns: A command-line string that contains the verity flag on success, NULL
 * on failure.
 */
char *avb_set_enforce_verity(const char *cmdline);

/*
 * Returns a new command-line that appends the flag to require verity to the
 * provided `cmdline`. Callers must later free the allocated memory via
 * |avb_free|.
 *
 * Precondition: `cmdline` must contain less than AVB_MAX_ARGS arguments.
 *
 * Returns: A command-line string that contains the ignore-corruption flag on
 * success, NULL on failure.
 */
char *avb_set_ignore_corruption(const char *cmdline);

/*
 * Verifies the set of requested partitions with the given slot suffix. If all
 * slots are successfully verified, the verification data for each slot is
 * returned in `out_data` and the corresponding kernel command-line parameters
 * are returned in `out_cmdline`. This function returns whether all slots were
 * successfully verified or not.
 *
 * Precondition: The `ops` handle must be non-NULL.
 * Precondition: If using A/B partitions, `slot_suffix` must contain the leading
 * underscore as well, e.g. "_a". If not using A/B partitions, `slot_suffix`
 * must be non-NULL, i.e. "" (empty-string).
 * Precondition: The `requested_partitions` list must be terminated with 'NULL'.
 *	e.g. { "boot", NULL }
 * Precondition: The `requested_partitions` list must be of size >= 1.
 *
 * Returns:
 * CMD_RET_SUCCESS - All partitions were verified successfully. If `out_data` is
 * non-NULL, it will be populated with verification data. If `out_cmdline` is
 * non-NULL, it will be populated with the relevant kernel command-line
 * parameters. CMD_RET_FAILURE - One or more partions failed verification.
 */
int avb_verify_partitions(struct AvbOps *ops, const char *slot_suffix,
                          const char *const requested_partitions[],
                          AvbSlotVerifyData **out_data, char **out_cmdline);

/*
 * Verifies boot, vendor_boot and init_boot partitions. This is a convenience
 * function that runs |avb_verify_partitions| with the aforementioned set.
 */
int avb_verify(struct AvbOps *ops, const char *slot_suffix,
               AvbSlotVerifyData **out_data, char **out_cmdline);

/*
 * Extracts the public key from the main vbmeta image.
 *
 * Precondition: Must not be called with verification flags set to
 * AVB_SLOT_VERIFY_FLAGS_NO_VBMETA_PARTITION.
 *
 * Returns:
 * CMD_RET_SUCCESS - The vbmeta image passed verification, the `key` output will
 * point to the public key bytes in `data->vbmeta_images`. The `size` will
 * describe the key length.
 * CMD_RET_FAILURE - The vbmeta image failed verification.
 */
int avb_find_main_pubkey(const AvbSlotVerifyData *data, const uint8_t **key,
                         size_t *size);

/*
 * Verifies that the provided public key is trusted by the platform.
 *
 * Returns:
 * CMD_RET_SUCCESS - The provided key matches the trusted public key.
 * CMD_RET_FAILURE - The provided key does not match the trusted public key.
 */
int avb_pubkey_is_trusted(const uint8_t *key, size_t size);

/**
 * ============================================================================
 * I/O helper inline functions
 * ============================================================================
 */

/*
 * Calculates the total byte offset after a given verified boot partition.
 *
 * Returns: The byte size of the partition + the requested byte offset.
 */
static inline uint64_t calc_offset(struct avb_part *part, int64_t offset)
{
	u64 part_size = part->info.size * part->info.blksz;
	if (offset < 0)
		return part_size + offset;
	return offset;
}

#endif /* _AVB_VERIFY_H */
