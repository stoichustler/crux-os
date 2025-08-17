/*
 * Copyright (C) 2023 The Android Open Source Project
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef __ANDROID_BOOTLOADER_TRANSPORT_H
#define __ANDROID_BOOTLOADER_TRANSPORT_H

#include <dm/device.h>
#include <linux/types.h>

int android_bootloader_request_response(struct udevice* console, uint32_t command,
					const void* request, size_t request_size,
					void* response, size_t response_size);

#endif /* __ANDROID_BOOTLOADER_TRANSPORT_H */