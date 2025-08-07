/* SPDX-License-Identifier: MIT */
/******************************************************************************
 * crux-compat.h
 *
 * Guest OS interface to crux.  Compatibility layer.
 *
 * Copyright (c) 2006, Christian Limpach
 */

#ifndef __CRUX_PUBLIC_CRUX_COMPAT_H__
#define __CRUX_PUBLIC_CRUX_COMPAT_H__

#define __CRUX_LATEST_INTERFACE_VERSION__ 0x00041300

#if defined(__CRUX__) || defined(__CRUX_TOOLS__)
/* crux is built with matching headers and implements the latest interface. */
#define __CRUX_INTERFACE_VERSION__ __CRUX_LATEST_INTERFACE_VERSION__
#elif !defined(__CRUX_INTERFACE_VERSION__)
/* Guests which do not specify a version get the legacy interface. */
#define __CRUX_INTERFACE_VERSION__ 0x00000000
#endif

#if __CRUX_INTERFACE_VERSION__ > __CRUX_LATEST_INTERFACE_VERSION__
#error "These header files do not support the requested interface version."
#endif

#define COMPAT_FLEX_ARRAY_DIM CRUX_FLEX_ARRAY_DIM

#endif /* __CRUX_PUBLIC_CRUX_COMPAT_H__ */
