/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <dm/device.h>
#include <../lib/libavb/libavb.h>

/** write_avb_to_keymint_console - write OS version information into a console
 *
 * This writes messages to a serial d
 *
 * @avb_data			validated AVB information
 * @km_console			console output to send keymint messages to
 */
int write_avb_to_keymint_console(AvbSlotVerifyData* avb_data,
				 struct udevice* km_console);
