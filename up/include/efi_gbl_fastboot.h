/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (C) 2025 The Android Open Source Project
 */

#ifndef __EFI_GBL_FASTBOOT_H__
#define __EFI_GBL_FASTBOOT_H__

#include <efi.h>
#include <efi_api.h>

#define GBL_EFI_FASTBOOT_SERIAL_NUMBER_MAX_LEN_UTF8 32

struct gbl_efi_fastboot_policy {
	// Indicates whether device can be unlocked
	bool can_unlock;
	// Device firmware supports 'critical' partition locking
	bool has_critical_lock;
	// Indicates whether device allows booting from image loaded directly from
	// RAM.
	bool can_ram_boot;
};

// Callback function pointer passed to GblEfiFastbootProtocol.get_var_all.
//
// context: Caller specific context.
// args: An array of NULL-terminated strings that contains the variable name
//       followed by additional arguments if any.
// val: A NULL-terminated string representing the value.
typedef void (*get_var_all_callback)(void *context, const char *const *args,
				     size_t num_args, const char *val);

// Firmware can read the given partition and send its data to fastboot client.
#define GBL_EFI_FASTBOOT_PARTITION_READ (0x1 << 0)
// Firmware can overwrite the given partition.
#define GBL_EFI_FASTBOOT_PARTITION_WRITE (0x1 << 1)
// Firmware can erase the given partition.
#define GBL_EFI_FASTBOOT_PARTITION_ERASE (0x1 << 2)

// All device partitions are locked.
#define GBL_EFI_FASTBOOT_LOCKED (0x1 << 0)
// All 'critical' device partitions are locked.
#define GBL_EFI_FASTBOOT_CRITICAL_LOCKED (0x1 << 1)

extern const efi_guid_t efi_gbl_fastboot_guid;

struct gbl_efi_fastboot_protocol {
	// Revision of the protocol supported.
	u32 version;
	// Null-terminated UTF-8 encoded string
	char serial_number[GBL_EFI_FASTBOOT_SERIAL_NUMBER_MAX_LEN_UTF8];

	// Fastboot variable methods
	efi_status_t(EFIAPI *get_var)(struct gbl_efi_fastboot_protocol *this,
				      const char *const *args, size_t num_args,
				      char *buf, size_t *bufsize);
	efi_status_t(EFIAPI *get_var_all)(struct gbl_efi_fastboot_protocol *self,
					  void *ctx, get_var_all_callback cb);

	// Fastboot oem function methods
	efi_status_t(EFIAPI *run_oem_function)(
		struct gbl_efi_fastboot_protocol *this, const char *command,
		size_t command_len, char *buf, size_t *bufsize);

	// Device lock methods
	efi_status_t(EFIAPI *get_policy)(struct gbl_efi_fastboot_protocol *this,
					 struct gbl_efi_fastboot_policy *policy);
	efi_status_t(EFIAPI *set_lock)(struct gbl_efi_fastboot_protocol *this,
				       u64 lock_state);
	efi_status_t(EFIAPI *clear_lock)(struct gbl_efi_fastboot_protocol *this,
					 u64 lock_state);
	// Local session methods
	efi_status_t(EFIAPI *start_local_session)(
		struct gbl_efi_fastboot_protocol *this, void **ctx);
	efi_status_t(EFIAPI *update_local_session)(
		struct gbl_efi_fastboot_protocol *this, void *ctx, char *buf,
		size_t *bufsize);
	efi_status_t(EFIAPI *close_local_session)(
		struct gbl_efi_fastboot_protocol *this, void *ctx);
	// Misc methods
	efi_status_t(EFIAPI *get_partition_permissions)(
		struct gbl_efi_fastboot_protocol *this, const char *part_name,
		size_t part_name_len, u64 *permissions);
	efi_status_t(EFIAPI *wipe_user_data)(
		struct gbl_efi_fastboot_protocol *this);
	bool(EFIAPI *should_enter_fastboot)(
		struct gbl_efi_fastboot_protocol *this);
};

efi_status_t efi_gbl_fastboot_register(void);

#endif /* __EFI_GBL_FASTBOOT_H__ */
