/* SPDX-License-Identifier: MIT */
/******************************************************************************
 * protocols.h
 *
 * Copyright (c) 2008, Keir Fraser
 */

#ifndef __CRUX_PROTOCOLS_H__
#define __CRUX_PROTOCOLS_H__

#define CRUX_IO_PROTO_ABI_X86_32     "x86_32-abi"
#define CRUX_IO_PROTO_ABI_X86_64     "x86_64-abi"
#define CRUX_IO_PROTO_ABI_ARM        "arm-abi"

#if defined(__i386__)
# define CRUX_IO_PROTO_ABI_NATIVE CRUX_IO_PROTO_ABI_X86_32
#elif defined(__x86_64__)
# define CRUX_IO_PROTO_ABI_NATIVE CRUX_IO_PROTO_ABI_X86_64
#elif defined(__arm__) || defined(__aarch64__)
# define CRUX_IO_PROTO_ABI_NATIVE CRUX_IO_PROTO_ABI_ARM
#else
# error arch fixup needed here
#endif

#endif
