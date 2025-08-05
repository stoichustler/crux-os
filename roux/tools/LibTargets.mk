############################################################################
# tools/LibTargets.mk
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

# Archive targets.  The target build sequence will first create a series of
# libraries, one per configured source file directory.  The final ROUX
# execution will then be built from those libraries.  The following targets
# build those libraries.
#
# Possible kernel-mode builds

libs/libbuiltin/libkbuiltin$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C libs/libbuiltin libkbuiltin$(LIBEXT) BINDIR=kbin EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libkbuiltin$(LIBEXT): libs/libbuiltin/libkbuiltin$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

libs/libc/libkc$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C libs/libc libkc$(LIBEXT) BINDIR=kbin EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libkc$(LIBEXT): libs/libc/libkc$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

libs/libm/libkm$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C libs/libm libkm$(LIBEXT) BINDIR=kbin EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libkm$(LIBEXT): libs/libm/libkm$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

libs/libnx/libknx$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C libs/libnx libknx$(LIBEXT) BINDIR=kbin EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libknx$(LIBEXT): libs/libnx/libknx$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

system/mm/libkmm$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C system/mm libkmm$(LIBEXT) BINDIR=kbin EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libkmm$(LIBEXT): system/mm/libkmm$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

$(ARCH_SRC)/libkarch$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C $(ARCH_SRC) libkarch$(LIBEXT) EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libkarch$(LIBEXT): $(ARCH_SRC)/libkarch$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

pass1/libpass1$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C pass1 libpass1$(LIBEXT) EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libpass1$(LIBEXT): pass1/libpass1$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

kernel/libkernel$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C kernel libkernel$(LIBEXT) EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libkernel$(LIBEXT): kernel/libkernel$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

system/net/libnet$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C system/net libnet$(LIBEXT) EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libnet$(LIBEXT): system/net/libnet$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

boards/libboards$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C boards libboards$(LIBEXT) EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libboards$(LIBEXT): boards/libboards$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

$(ARCH_SRC)/board/libboard$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C $(ARCH_SRC)/board libboard$(LIBEXT) EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libboard$(LIBEXT): $(ARCH_SRC)/board/libboard$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

system/crypto/libcrypto$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C system/crypto libcrypto$(LIBEXT) EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libcrypto$(LIBEXT): system/crypto/libcrypto$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

system/fs/libfs$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C system/fs libfs$(LIBEXT) EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libfs$(LIBEXT): system/fs/libfs$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

drivers/libdrivers$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C drivers libdrivers$(LIBEXT) EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libdrivers$(LIBEXT): drivers/libdrivers$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

external/libexternal$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C external libexternal$(LIBEXT) EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libexternal$(LIBEXT): external/libexternal$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

system/binfmt/libbinfmt$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C system/binfmt libbinfmt$(LIBEXT) EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libbinfmt$(LIBEXT): system/binfmt/libbinfmt$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

system/graphics/libgraphics$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C system/graphics libgraphics$(LIBEXT) EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libgraphics$(LIBEXT): system/graphics/libgraphics$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

system/audio/libaudio$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C system/audio libaudio$(LIBEXT) EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libaudio$(LIBEXT): system/audio/libaudio$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

system/video/libvideo$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C system/video libvideo$(LIBEXT) EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libvideo$(LIBEXT): system/video/libvideo$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

system/wireless/libwireless$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C system/wireless libwireless$(LIBEXT) EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libwireless$(LIBEXT): system/wireless/libwireless$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

system/openamp/libopenamp$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C system/openamp libopenamp$(LIBEXT) EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libopenamp$(LIBEXT): system/openamp/libopenamp$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

system/syscall/libstubs$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C syscall libstubs$(LIBEXT) EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libstubs$(LIBEXT): system/syscall/libstubs$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

system/syscall/libwraps$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C syscall libwraps$(LIBEXT) EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"

staging/libwraps$(LIBEXT): system/syscall/libwraps$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

# Special case

ifeq ($(CONFIG_BUILD_FLAT),y)
$(ARCH_SRC)/libarch$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C $(ARCH_SRC) libarch$(LIBEXT) EXTRAFLAGS="$(KDEFINE) $(EXTRAFLAGS)"
else
$(ARCH_SRC)/libarch$(LIBEXT): pass1dep
	$(Q) $(MAKE) -C $(ARCH_SRC) libarch$(LIBEXT) EXTRAFLAGS="$(EXTRAFLAGS)"
endif

staging/libarch$(LIBEXT): $(ARCH_SRC)/libarch$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

ifeq ($(CONFIG_BUILD_FLAT),y)
libs/libbuiltin/libbuiltin$(LIBEXT): pass2dep
else
libs/libbuiltin/libbuiltin$(LIBEXT): pass1dep
endif
	$(Q) $(MAKE) -C libs/libbuiltin libbuiltin$(LIBEXT) EXTRAFLAGS="$(EXTRAFLAGS)"

staging/libbuiltin$(LIBEXT): libs/libbuiltin/libbuiltin$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

# Possible user-mode builds

ifeq ($(CONFIG_BUILD_FLAT),y)
libs/libc/libc$(LIBEXT): pass2dep
else
libs/libc/libc$(LIBEXT): pass1dep
endif
	$(Q) $(MAKE) -C libs/libc libc$(LIBEXT) EXTRAFLAGS="$(EXTRAFLAGS)"

staging/libc$(LIBEXT): libs/libc/libc$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

ifeq ($(CONFIG_BUILD_FLAT),y)
libs/libm/libm$(LIBEXT): pass2dep
else
libs/libm/libm$(LIBEXT): pass1dep
endif
	$(Q) $(MAKE) -C libs/libm libm$(LIBEXT) EXTRAFLAGS="$(EXTRAFLAGS)"

staging/libm$(LIBEXT): libs/libm/libm$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

ifeq ($(CONFIG_BUILD_FLAT),y)
libs/libnx/libnx$(LIBEXT): pass2dep
else
libs/libnx/libnx$(LIBEXT): pass1dep
endif
	$(Q) $(MAKE) -C libs/libnx libnx$(LIBEXT) EXTRAFLAGS="$(EXTRAFLAGS)"

staging/libnx$(LIBEXT): libs/libnx/libnx$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

ifeq ($(CONFIG_BUILD_FLAT),y)
system/mm/libmm$(LIBEXT): pass2dep
else
system/mm/libmm$(LIBEXT): pass1dep
endif
	$(Q) $(MAKE) -C system/mm libmm$(LIBEXT) EXTRAFLAGS="$(EXTRAFLAGS)"

staging/libmm$(LIBEXT): system/mm/libmm$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

libs/libxx/libxx$(LIBEXT): pass1dep
	$(Q) $(MAKE) -C libs/libxx libxx$(LIBEXT) EXTRAFLAGS="$(EXTRAFLAGS)"

staging/libxx$(LIBEXT): libs/libxx/libxx$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

libs/libdsp/libdsp$(LIBEXT): pass2dep
	$(Q) $(MAKE) -C libs/libdsp libdsp$(LIBEXT) EXTRAFLAGS="$(EXTRAFLAGS)"

staging/libdsp$(LIBEXT): libs/libdsp/libdsp$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

ifeq ($(CONFIG_BUILD_FLAT),y)
$(APPDIR)/libapps$(LIBEXT): pass2dep
else
$(APPDIR)/libapps$(LIBEXT): pass1dep
endif
	$(Q) $(MAKE) -C $(APPDIR) EXTRAFLAGS="$(EXTRAFLAGS)"

staging/libapps$(LIBEXT): $(APPDIR)/libapps$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)

system/syscall/libproxies$(LIBEXT): pass1dep
	$(Q) $(MAKE) -C system/syscall libproxies$(LIBEXT) EXTRAFLAGS="$(EXTRAFLAGS)"

staging/libproxies$(LIBEXT): system/syscall/libproxies$(LIBEXT)
	$(Q) $(call INSTALL_LIB,$<,$@)
