/*
 * Copyright (C) 2023 The Android Open Source Project
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef __ANDROID_BOOTLOADER_OEMLOCK_H
#define __ANDROID_BOOTLOADER_OEMLOCK_H

#include <linux/types.h>

int oemlock_is_allowed(void);
int oemlock_set_locked(bool locked);
int oemlock_is_locked(void);

#endif /* __ANDROID_BOOTLOADER_OEMLOCK_H */