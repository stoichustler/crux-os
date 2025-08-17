/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (C) 2024 The Android Open Source Project
 */

#ifndef __EFI_GBL_AB_H__
#define __EFI_GBL_AB_H__

#include <efi_api.h>

#define EFI_GBL_AB_PROTOCOL_REVISION 0x00010000

enum GBL_EFI_UNBOOTABLE_REASON {
	UNKNOWN_REASON = 0,
	NO_MORE_TRIES,
	SYSTEM_UPDATE,
	USER_REQUESTED,
	VERIFICATION_FAILURE,
};

enum GBL_EFI_BOOT_REASON {
	EMPTY_EFI_BOOT_REASON = 0,
	UNKNOWN_EFI_BOOT_REASON = 1,
	WATCHDOG = 14,
	KERNEL_PANIC = 15,
	RECOVERY = 3,
	BOOTLOADER = 55,
	COLD = 56,
	HARD = 57,
	WARM = 58,
	SHUTDOWN,
	REBOOT = 18,
};

struct efi_gbl_slot_info {
	// One UTF-8 encoded single character
	u32 suffix;
	// Any value other than those explicitly enumerated in EFI_UNBOOTABLE_REASON
	// will be interpreted as UNKNOWN_REASON.
	u32 unbootable_reason;
	u8 priority;
	u8 tries;
	// Value of 1 if slot has successfully booted.
	u8 successful;
	u8 merge_status;
};

struct efi_gbl_slot_metadata_block {
	// Value of 1 if persistent metadata tracks slot unbootable reasons.
	u8 unbootable_metadata;
	u8 max_retries;
	u8 slot_count;
};

extern const efi_guid_t efi_gbl_ab_boot_guid;

struct efi_gbl_slot_protocol {
	// Currently must contain 0x00010000
	u32 version;
	// Slot metadata query methods
	efi_status_t(EFIAPI * load_boot_data) (struct efi_gbl_slot_protocol *,
					       struct efi_gbl_slot_metadata_block * /* out param*/);
	efi_status_t(EFIAPI * get_slot_info) (struct efi_gbl_slot_protocol *,
					      u8,
					      struct efi_gbl_slot_info * /* out param */);
	efi_status_t(EFIAPI * get_current_slot) (struct efi_gbl_slot_protocol *,
						 struct efi_gbl_slot_info * /* out param */);
	// Slot metadata manipulation methods
	efi_status_t(EFIAPI * set_active_slot) (struct efi_gbl_slot_protocol *,
						u8);
	efi_status_t(EFIAPI * set_slot_unbootable) (struct efi_gbl_slot_protocol *, u8, u32);
	efi_status_t(EFIAPI * mark_boot_attempt) (struct efi_gbl_slot_protocol *);
	efi_status_t(EFIAPI * reinitialize) (struct efi_gbl_slot_protocol *);
	// Miscellaneous methods
	efi_status_t(EFIAPI * get_boot_reason) (struct efi_gbl_slot_protocol *,
						u32 * /* out param */,
						size_t * /* in-out param */,
						u8 * /* out param*/);
	efi_status_t(EFIAPI * set_boot_reason) (struct efi_gbl_slot_protocol *,
						u32, size_t,
						const u8 *);
	efi_status_t(EFIAPI * flush) (struct efi_gbl_slot_protocol *);
};

efi_status_t efi_gbl_ab_register(void);

#endif /* __EFI_GBL_AB_H__ */
