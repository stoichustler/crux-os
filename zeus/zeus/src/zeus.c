/*
 * Copyright (c) 2025 HUSTLER
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define SELF_BANNER                              \
"           _____     __  __\n"                  \
"  /\\/\\/\\/\\/ __  \\/\\ / _\\/__\\\n"         \
"  \\ - \\ \\ \\ \\/ // // _\\/  \\\n"           \
"   \\/\\/\\_/\\_/\\/ \\__\\__\\\\/\\/ zeus\n"   \
"\n"

void main(void)
{
#if defined(CONFIG_XEN_DOM0LESS)
	unsigned long count = 0;
#endif

	printk(SELF_BANNER);

	while (1) {
#if defined(CONFIG_XEN_DOM0LESS)
		printk("[%16lu] -- [activated] --\n", count++);
#endif
		k_sleep(K_SECONDS(60));
	}
}
