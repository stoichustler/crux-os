############################################################################
# tools/ProtectedLibs.mk
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

# ROUXLIBS is the list of ROUX libraries that is passed to the
#   processor-specific Makefile to build the final ROUX target.
# USERLIBS is the list of libraries used to build the final user-space
#   application
# EXPORTLIBS is the list of libraries that should be exported by
#   'make export' is

ROUXLIBS = staging/libkernel$(LIBEXT)
USERLIBS =

# Driver support.

ROUXLIBS += staging/libdrivers$(LIBEXT)

# External code support

ifeq ($(EXTERNALDIR),external)
  ROUXLIBS += staging/libexternal$(LIBEXT)
endif

# Add libraries for board support

ROUXLIBS += staging/libboards$(LIBEXT)

# Add libraries for syscall support.  The C library will be needed by
# both the kernel- and user-space builds.

ROUXLIBS += staging/libstubs$(LIBEXT) staging/libkc$(LIBEXT)
ROUXLIBS += staging/libkmm$(LIBEXT) staging/libkarch$(LIBEXT)
USERLIBS  += staging/libproxies$(LIBEXT) staging/libc$(LIBEXT)
USERLIBS  += staging/libmm$(LIBEXT) staging/libarch$(LIBEXT)

# Add toolchain library support

ifeq ($(CONFIG_LIB_BUILTIN),y)
ROUXLIBS += staging/libkbuiltin$(LIBEXT)
USERLIBS += staging/libbuiltin$(LIBEXT)
endif

# Add libraries for math support.

ifeq ($(CONFIG_LIBM_TOOLCHAIN)$(CONFIG_LIBM_NONE),)
ROUXLIBS += staging/libkm$(LIBEXT)
USERLIBS  += staging/libm$(LIBEXT)
endif

# Add library for system call instrumentation if needed

ifeq ($(CONFIG_SCHED_INSTRUMENTATION_SYSCALL),y)
ROUXLIBS += staging/libwraps$(LIBEXT)
endif

# Add libraries for two pass build support.  The special directory pass1
# may be populated so that application generated logic can be included into
# the kernel build

ifeq ($(CONFIG_BUILD_2PASS),y)
ROUXLIBS += staging/libpass1$(LIBEXT)
endif

# Add libraries for C++ support.  CXX, CXXFLAGS, and COMPILEXX must
# be defined in Make.defs for this to work!

ifeq ($(CONFIG_HAVE_CXX),y)
USERLIBS += staging/libxx$(LIBEXT)
endif

# Add library for application support.

ifneq ($(APPDIR),)
USERLIBS += staging/libapps$(LIBEXT)
endif

# Add libraries for network support

ifeq ($(CONFIG_NET),y)
ROUXLIBS += staging/libnet$(LIBEXT)
endif

# Add libraries for Crypto API support

ifeq ($(CONFIG_CRYPTO),y)
ROUXLIBS += staging/libcrypto$(LIBEXT)
endif

# Add libraries for file system support

ROUXLIBS += staging/libfs$(LIBEXT) staging/libbinfmt$(LIBEXT)

# Add libraries for the NX graphics sub-system

ifeq ($(CONFIG_NX),y)
ROUXLIBS += staging/libgraphics$(LIBEXT)
ROUXLIBS += staging/libknx$(LIBEXT)
USERLIBS  += staging/libnx$(LIBEXT)
else ifeq ($(CONFIG_NXFONTS),y)
ROUXLIBS += staging/libknx$(LIBEXT)
USERLIBS  += staging/libnx$(LIBEXT)
endif

# Add libraries for the Audio sub-system

ifeq ($(CONFIG_AUDIO),y)
ROUXLIBS += staging/libaudio$(LIBEXT)
endif

# Add libraries for the Video sub-system

ifeq ($(CONFIG_VIDEO),y)
ROUXLIBS += staging/libvideo$(LIBEXT)
endif

# Add libraries for the Wireless sub-system

ifeq ($(CONFIG_WIRELESS),y)
ROUXLIBS += staging/libwireless$(LIBEXT)
endif

# Add DSP library

ifeq ($(CONFIG_LIBDSP),y)
ROUXLIBS += staging/libdsp$(LIBEXT)
endif

ifeq ($(CONFIG_OPENAMP),y)
ROUXLIBS += staging/libopenamp$(LIBEXT)
endif

# Add libraries for board common support

ifeq ($(CONFIG_ARCH_BOARD_COMMON),y)
ROUXLIBS += staging/libboard$(LIBEXT)
endif

# Export only the user libraries

EXPORTLIBS = $(USERLIBS)
