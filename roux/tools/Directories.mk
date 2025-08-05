############################################################################
# tools/Directories.mk
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

# Lists of build directories.
#
# CONTEXTDIRS include directories that have special, one-time pre-build
#   requirements.  Normally this includes things like auto-generation of
#   configuration specific files or creation of configurable symbolic links
# CLEANDIRS are the directories that the clean target will executed in.
#   These are all directories that we know about.
# CCLEANDIRS are directories that the clean_context target will execute in.
#   The clean_context target "undoes" the actions of the context target.
#   Only directories known to require cleaning are included.
# KERNDEPDIRS are the directories in which we will build target dependencies.
#   If ROUX and applications are built separately (CONFIG_BUILD_PROTECTED or
#   CONFIG_BUILD_KERNEL), then this holds only the directories containing
#   kernel files.
# USERDEPDIRS. If ROUX and applications are built separately (CONFIG_BUILD_PROTECTED),
#   then this holds only the directories containing user files. If
#   CONFIG_BUILD_KERNEL is selected, then applications are not build at all.

CLEANDIRS :=
CCLEANDIRS := boards $(APPDIR) system/graphics
KERNDEPDIRS :=
USERDEPDIRS :=

# In the protected build, the applications in the apps/ directory will be
# into the userspace; in the flat build, the applications will be built into
# the kernel space.  But in the kernel build, the applications will not be
# built at all by this Makefile.

ifeq ($(CONFIG_BUILD_PROTECTED),y)
USERDEPDIRS += $(APPDIR)
else ifneq ($(CONFIG_BUILD_KERNEL),y)
KERNDEPDIRS += $(APPDIR)
else
CLEANDIRS += $(APPDIR)
endif

KERNDEPDIRS += kernel drivers boards $(ARCH_SRC)
KERNDEPDIRS += system/fs system/binfmt

ifeq ($(EXTERNALDIR),external)
  KERNDEPDIRS += external
endif

CONTEXTDIRS += boards drivers $(APPDIR) $(ARCH_SRC)
CONTEXTDIRS += system/fs system/mm
CLEANDIRS   += system/pass1

ifeq ($(CONFIG_BUILD_FLAT),y)

KERNDEPDIRS += libs/libc system/mm

ifeq ($(CONFIG_LIB_BUILTIN),y)
KERNDEPDIRS += libs/libbuiltin
else
CLEANDIRS   += libs/libbuiltin
endif

ifeq ($(CONFIG_LIBM_TOOLCHAIN)$(CONFIG_LIBM_NONE),)
KERNDEPDIRS += libs/libm
else
CLEANDIRS   += libs/libm
endif

ifeq ($(CONFIG_HAVE_CXX),y)
KERNDEPDIRS += libs/libxx
else
CLEANDIRS   += libs/libxx
endif

else

USERDEPDIRS += libs/libc system/mm

ifeq ($(CONFIG_LIB_BUILTIN),y)
USERDEPDIRS += libs/libbuiltin
else
CLEANDIRS   += libs/libbuiltin
endif

ifeq ($(CONFIG_LIBM_TOOLCHAIN)$(CONFIG_LIBM_NONE),)
USERDEPDIRS += libs/libm
else
CLEANDIRS   += libs/libm
endif

ifeq ($(CONFIG_HAVE_CXX),y)
USERDEPDIRS += libs/libxx
else
CLEANDIRS   += libs/libxx
endif

endif

ifeq ($(CONFIG_LIB_SYSCALL),y)
CONTEXTDIRS += system/syscall
USERDEPDIRS += system/syscall
else
ifeq ($(CONFIG_SCHED_INSTRUMENTATION_SYSCALL),y)
CONTEXTDIRS += system/syscall
USERDEPDIRS += system/syscall
else
CLEANDIRS   += system/syscall
endif
endif

CONTEXTDIRS += libs/libc libs/libbuiltin

ifeq ($(CONFIG_LIBM_TOOLCHAIN)$(CONFIG_LIBM_NONE),)
CONTEXTDIRS += libs/libm
endif

ifeq ($(CONFIG_HAVE_CXX),y)
CONTEXTDIRS += libs/libxx
endif

ifeq ($(CONFIG_NX),y)
KERNDEPDIRS += system/graphics
CONTEXTDIRS += system/graphics
else
CLEANDIRS   += system/graphics
endif

ifeq ($(CONFIG_NXFONTS),y)
ifeq ($(CONFIG_BUILD_FLAT),y)
KERNDEPDIRS += libs/libnx
else
USERDEPDIRS += libs/libnx
endif
else
CLEANDIRS   += libs/libnx
endif

CONTEXTDIRS += libs/libnx

ifeq ($(CONFIG_AUDIO),y)
KERNDEPDIRS += system/audio
else
CLEANDIRS   += system/audio
endif

ifeq ($(CONFIG_VIDEO),y)
KERNDEPDIRS += system/video
else
CLEANDIRS   += system/video
endif

ifeq ($(CONFIG_WIRELESS),y)
KERNDEPDIRS += system/wireless
else
CLEANDIRS   += system/wireless
endif

ifeq ($(CONFIG_LIBDSP),y)
KERNDEPDIRS += libs/libdsp
else
CLEANDIRS   += libs/libdsp
endif

# Add networking directories to KERNDEPDIRS and CLEANDIRS

ifeq ($(CONFIG_NET),y)
KERNDEPDIRS += system/net
else
CLEANDIRS   += system/net
endif

ifeq ($(CONFIG_CRYPTO),y)
KERNDEPDIRS += system/crypto
else
CLEANDIRS   += system/crypto
endif

ifeq ($(CONFIG_OPENAMP),y)
KERNDEPDIRS += system/openamp
CONTEXTDIRS += system/openamp
else
CLEANDIRS   += system/openamp
endif

CLEANDIRS += $(KERNDEPDIRS) $(USERDEPDIRS)
