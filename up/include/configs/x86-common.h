/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2011 The Chromium OS Authors.
 * (C) Copyright 2008
 * Graeme Russ, graeme.russ@gmail.com.
 */

#ifndef __CONFIG_X86_COMMON_H
#define __CONFIG_X86_COMMON_H

/*-----------------------------------------------------------------------
 * USB configuration
 */

/* Default environment */
#define CONFIG_ROOTPATH		"/opt/nfsroot"
#define CONFIG_HOSTNAME		"x86"
#define CONFIG_BOOTFILE		"bzImage"
#define CONFIG_RAMDISK_ADDR	0x6400000
#if defined(CONFIG_GENERATE_ACPI_TABLE) || defined(CONFIG_EFI_STUB)
#define CFG_OTHBOOTARGS	"othbootargs=\0"
#else
#define CFG_OTHBOOTARGS	"othbootargs=acpi=off\0"
#endif

#if defined(CONFIG_DISTRO_DEFAULTS)
#define DISTRO_BOOTENV		BOOTENV
#else
#define DISTRO_BOOTENV
#endif

#if defined(CONFIG_DEFAULT_FDT_FILE)
#define FDTFILE "fdtfile=" CONFIG_DEFAULT_FDT_FILE "\0"
#else
#define FDTFILE
#endif

#ifndef SPLASH_SETTINGS
#define SPLASH_SETTINGS
#endif

#define CFG_EXTRA_ENV_SETTINGS			\
	CFG_STD_DEVICES_SETTINGS			\
	SPLASH_SETTINGS					\
	"pciconfighost=1\0"				\
	"netdev=eth0\0"					\
	"consoledev=ttyS0\0"				\
	CONFIG_OTHBOOTARGS				\
	FDTFILE						\
	"scriptaddr=0x2000000\0"			\
	"kernel_addr_r=0x2400000\0"			\
	"ramdisk_addr_r=0x6400000\0"			\
	"fdt_addr_r=0x4000000\0"			\
	"ramdiskfile=initramfs.gz\0"


#endif	/* __CONFIG_H */
