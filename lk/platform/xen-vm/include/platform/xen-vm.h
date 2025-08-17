/*
 * Copyright (c) 2015 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#pragma once

#define MEMORY_BASE_PHYS (0)
#if ARCH_ARM64
// 3584MB
#define MEMORY_APERTURE_SIZE 0xE0000000
#else
#error "Only ARM64 is supported"
#endif

/* Hyper-V has 512MB MMIO */
#define PERIPHERAL_BASE_PHYS (0xE0000000)
#define PERIPHERAL_BASE_SIZE (0x20000000UL) // 512MB

#if ARCH_ARM64
#define PERIPHERAL_BASE_VIRT (0xffffffffc0000000ULL) // -1GB
#else
#error "Only ARM64 is supported"
#endif

/* individual peripherals in this mapping */
#define UART0_BASE (PERIPHERAL_BASE_VIRT + 0xFFEC000)
#define UART0_SIZE (0x00001000)

// Alias UART0 to UART
#define UART_BASE UART0_BASE
#define UART_SIZE UART0_SIZE

#define UART1_BASE (PERIPHERAL_BASE_VIRT + 0xFFEB000)
#define UART1_SIZE (0x00001000)

/* interrupts */
#define ARM_GENERIC_TIMER_PHYSICAL_INT 19
#define ARM_GENERIC_TIMER_VIRTUAL_INT 20
#define UART0_INT (32 + 1)
#define UART1_INT (32 + 2)

#define MAX_INT 128

// Assuming it's GICv3 yet

// GICD Physical Address at 0xFFFF0000
#define HV_GICD_ADDRESS (PERIPHERAL_BASE_VIRT + 0x1FFF0000)

// GICR Base at 0xEFFEE000
#define HV_GICR_BASE (PERIPHERAL_BASE_VIRT + 0xFFEE000)
#define HV_GICR_ADDRESS(n) (HV_GICR_BASE + 0x20000 * (n))
