/*
 * Copyright (C) 2023 The Android Open Source Project
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <android_bootloader_oemlock.h>
#include <android_bootloader_transport.h>
#include <dm/device.h>
#include <dm/device-internal.h>
#include <dm/uclass.h>
#include <serial.h>
#include <virtio.h>

enum oemlock_field {
	ALLOWED_BY_CARRIER = 0,
	ALLOWED_BY_DEVICE,
	ALLOWED,
	LOCKED,
};

static const int console_index = CONFIG_ANDROID_BOOTLOADER_OEMLOCK_VIRTIO_CONSOLE_INDEX;

static struct udevice* get_console(void)
{
	static struct udevice *console = NULL;
	if (console == NULL) {
		int single_port = uclass_get_nth_device_by_driver_name(
				UCLASS_SERIAL, console_index, VIRTIO_CONSOLE_DRV_NAME, &console);
		if (single_port == 0) {
			return console;
		}
		// TODO(schuffelen): Do this by console name
		int multi_port = uclass_get_nth_device_by_driver_name(
				UCLASS_SERIAL, console_index - 1, VIRTIO_CONSOLE_PORT_DRV_NAME, &console);
		if (multi_port == 0) {
			return console;
		}
		log_err("Failed to initialize oemlock console: %d, %d\n", single_port, multi_port);
		return NULL;
	}
	return console;
}

static int oemlock_get_field(enum oemlock_field field)
{
	struct udevice *console = get_console();
	if (console == NULL) {
		return -EINVAL;
	}

	bool response = false;
	int ret = android_bootloader_request_response(console, field,
						      NULL, 0,
						      &response, sizeof(response));
	if (ret != 0) {
		log_err("Failed to get oemlock value for field: %d status: %d\n", field, ret);
		return -EINVAL;
	}

	return response;
}

static int oemlock_set_field(enum oemlock_field field, bool value)
{
	struct udevice *console = get_console();
	if (console == NULL) {
		return -EINVAL;
	}

	bool response = false;
	int ret = android_bootloader_request_response(console, field,
						      &value, sizeof(value),
						      &response, sizeof(response));

	if (ret != 0) {
		log_err("Failed to set oemlock value for field: %d status: %d\n", field, ret);
		return -EINVAL;
	}

	return response;
}

int oemlock_is_allowed(void)
{
	return oemlock_get_field(ALLOWED);
}

int oemlock_set_locked(bool locked)
{
	return oemlock_set_field(LOCKED, locked);
}

int oemlock_is_locked(void)
{
	return oemlock_get_field(LOCKED);
}
