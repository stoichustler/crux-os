/* SPDX-License-Identifier: GPL-2.0+
 *
 * (C) Copyright 2020 EPAM Systemc Inc.
 */
#ifndef __XENGUEST_ARM64_DOM0LESS_H
#define __XENGUEST_ARM64_DOM0LESS_H

#ifndef __ASSEMBLY__
#include <linux/types.h>
#endif

#define CFG_EXTRA_ENV_SETTINGS

#undef CFG_SYS_SDRAM_BASE

#undef CFG_EXTRA_ENV_SETTINGS

#ifdef CONFIG_VIRTIO_BLK
#define CFG_EXTRA_ENV_SETTINGS	\
	"virtioboot=virtio scan; ext4load virtio 0 0x90000000 /boot/Image;" \
		"booti 0x90000000 - ${fdtcontroladdr};\0"
#else
#define CFG_EXTRA_ENV_SETTINGS	\
	"pvblockboot=ext4load pvblock 0 0x90000000 /boot/Image;" \
		"booti 0x90000000 - 0x88000000;\0"
#endif

#define CFG_PL011_CLOCK		0
#define CFG_SYS_SERIAL0		0x22000000
/*below might needs to be removed*/
#define CFG_PL01x_PORTS		{(void *)CFG_SYS_SERIAL0}

#endif /* __XENGUEST_ARM64_DOM0LESS_H */
