/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (C) 2025 The Android Open Source Project
 */

#include <efi.h>
#include <efi_api.h>
#include <efi_gbl_fastboot.h>
#include <efi_loader.h>
#include <log.h>

const efi_guid_t efi_gbl_fastboot_guid = EFI_GBL_FASTBOOT_PROTOCOL_GUID;
static struct gbl_efi_fastboot_protocol gbl_efi_fastboot_proto;

// Deliberately simplified fastboot variable representation.
struct fastboot_var {
	// NULL terminated array of strings
	// representing a variable-argument tuple.
	char const *const *const args;
	// String representation of the variable's value.
	char const *const val;
};

// Array of fastboot variables with a NULL sentinel.
static struct fastboot_var vars[] = {
	{ .args = NULL, .val = NULL }, // Sentinel
};

size_t args_len(struct fastboot_var *var)
{
	size_t i = 0;
	while (var->args[i]) {
		i++;
	}
	return i;
}

static bool args_match_var(const char *const *args, size_t num_args,
			   const struct fastboot_var *var)
{
	int i;
	for (i = 0; i < num_args && var->args[i]; i++) {
		if (strcmp(args[i], var->args[i])) {
			return false;
		}
	}

	return (i == num_args && !var->args[i]);
}

static efi_status_t EFIAPI get_var(struct gbl_efi_fastboot_protocol *this,
				   const char *const *fb_args, size_t num_args,
				   char *buf, size_t *bufsize)
{
	EFI_ENTRY("%p, %p, %lu, %p, %p", this, fb_args, num_args, buf, bufsize);
	if (this != &gbl_efi_fastboot_proto || fb_args == NULL || buf == NULL ||
	    bufsize == NULL) {
		return EFI_EXIT(EFI_INVALID_PARAMETER);
	}

	for (struct fastboot_var *var = &vars[0]; var->val; var++) {
		if (args_match_var(fb_args, num_args, var)) {
			size_t val_len = strlen(var->val);
			efi_status_t ret;
			if (val_len <= *bufsize) {
				memcpy(buf, var->val, val_len);
				ret = EFI_SUCCESS;
			} else {
				ret = EFI_BUFFER_TOO_SMALL;
			}
			return EFI_EXIT(ret);
		}
	}

	return EFI_EXIT(EFI_NOT_FOUND);
}

static efi_status_t EFIAPI get_var_all(struct gbl_efi_fastboot_protocol *this,
				       void *ctx, get_var_all_callback cb)
{
	EFI_ENTRY("%p, %p, %p", this, ctx, cb);
	if (this != &gbl_efi_fastboot_proto || cb == NULL) {
		return EFI_EXIT(EFI_INVALID_PARAMETER);
	}

	for (struct fastboot_var *var = &vars[0]; var->args; var++) {
		cb(ctx, var->args, args_len(var), var->val);
	}

	return EFI_EXIT(EFI_SUCCESS);
}

static efi_status_t EFIAPI
run_oem_function(struct gbl_efi_fastboot_protocol *this, const char *command,
		 size_t command_len, char *buf, size_t *bufsize)
{
	EFI_ENTRY("%p, %p, %lu, %p, %p", this, command, command_len, buf,
		  bufsize);

	return EFI_EXIT(EFI_UNSUPPORTED);
}

static efi_status_t EFIAPI get_policy(struct gbl_efi_fastboot_protocol *this,
				      struct gbl_efi_fastboot_policy *policy)
{
	EFI_ENTRY("%p, %p", this, policy);

	return EFI_EXIT(EFI_UNSUPPORTED);
}

static efi_status_t EFIAPI set_lock(struct gbl_efi_fastboot_protocol *this,
				    u64 lock_state)
{
	EFI_ENTRY("%p, %lu", this, lock_state);

	return EFI_EXIT(EFI_UNSUPPORTED);
}

static efi_status_t EFIAPI clear_lock(struct gbl_efi_fastboot_protocol *this,
				      u64 lock_state)
{
	EFI_ENTRY("%p, %lu", this, lock_state);

	return EFI_EXIT(EFI_UNSUPPORTED);
}

// Structure to store local session context.
struct fastboot_context {};
static struct fastboot_context context;

static efi_status_t EFIAPI
start_local_session(struct gbl_efi_fastboot_protocol *this, void **ctx)
{
	EFI_ENTRY("%p, %p", this, ctx);
	if (this != &gbl_efi_fastboot_proto || ctx == NULL) {
		return EFI_EXIT(EFI_INVALID_PARAMETER);
	}

	*ctx = &context;

	return EFI_EXIT(EFI_SUCCESS);
}

static efi_status_t EFIAPI
update_local_session(struct gbl_efi_fastboot_protocol *this, void *ctx,
		     char *buf, size_t *bufsize)
{
	EFI_ENTRY_NO_LOG("%p, %p, %p, %p", this, ctx, buf, bufsize);
	struct fastboot_context *fb_ctx = (struct fastboot_context *)ctx;
	if (this != &gbl_efi_fastboot_proto || fb_ctx != &context ||
	    buf == NULL || bufsize == NULL) {
		return EFI_EXIT(EFI_INVALID_PARAMETER);
	}

	*bufsize = 0;
	return EFI_EXIT_NO_LOG(EFI_SUCCESS);
}

static efi_status_t EFIAPI
close_local_session(struct gbl_efi_fastboot_protocol *this, void *ctx)
{
	EFI_ENTRY("%p, %p", this, ctx);
	struct fastboot_context *fb_ctx = (struct fastboot_context *)ctx;
	if (this != &gbl_efi_fastboot_proto || fb_ctx != &context) {
		return EFI_EXIT(EFI_INVALID_PARAMETER);
	}

	return EFI_EXIT(EFI_SUCCESS);
}

static efi_status_t EFIAPI get_partition_permissions(
	struct gbl_efi_fastboot_protocol *this, const char *part_name,
	size_t part_name_len, u64 *permissions)
{
	EFI_ENTRY("%p, %p, %lu, %p", this, part_name, part_name_len,
		  permissions);

	return EFI_EXIT(EFI_UNSUPPORTED);
}

static efi_status_t EFIAPI wipe_user_data(struct gbl_efi_fastboot_protocol *this)
{
	EFI_ENTRY("%p", this);

	return EFI_EXIT(EFI_UNSUPPORTED);
}

static bool EFIAPI should_enter_fastboot(struct gbl_efi_fastboot_protocol *this)
{
	EFI_ENTRY("%p", this);

	return EFI_EXIT(false);
}

static struct gbl_efi_fastboot_protocol gbl_efi_fastboot_proto = {
	.version = 1,
	.serial_number = "cuttlefish-0xCAFED00D",
	.get_var = get_var,
	.get_var_all = get_var_all,
	.run_oem_function = run_oem_function,
	.get_policy = get_policy,
	.set_lock = set_lock,
	.clear_lock = clear_lock,
	.start_local_session = start_local_session,
	.update_local_session = update_local_session,
	.close_local_session = close_local_session,
	.get_partition_permissions = get_partition_permissions,
	.wipe_user_data = wipe_user_data,
	.should_enter_fastboot = should_enter_fastboot,
};

efi_status_t efi_gbl_fastboot_register(void)
{
	efi_status_t ret = efi_add_protocol(efi_root, &efi_gbl_fastboot_guid,
					    &gbl_efi_fastboot_proto);
	if (ret != EFI_SUCCESS) {
		log_err("Failed to install GBL_EFI_FASTBOOT_PROTOCOL: 0x%lx\n",
			ret);
	}

	return ret;
}
