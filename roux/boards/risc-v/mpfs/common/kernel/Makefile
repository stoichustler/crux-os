############################################################################
# boards/risc-v/mpfs/common/kernel/Makefile
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

include $(TOPDIR)/Make.defs

# The entry point name (if none is provided in the .config file)

CONFIG_INIT_ENTRYPOINT ?= user_start
ENTRYPT = $(patsubst "%",%,$(CONFIG_INIT_ENTRYPOINT))

# The memory layout

MEM_LAYOUT = memory.ld

# Get the paths to the libraries and the links script path in format that
# is appropriate for the host OS

USER_LIBPATHS = $(addprefix -L,$(call CONVERT_PATH,$(addprefix $(TOPDIR)/,$(dir $(USERLIBS)))))
USER_LDSCRIPT = -T $(call CONVERT_PATH,$(BOARD_DIR)/scripts/memory.ld)
USER_LDSCRIPT += -T $(call CONVERT_PATH,$(BOARD_DIR)/scripts/user-space.ld)
USER_HEXFILE += $(call CONVERT_PATH,$(TOPDIR)/roux_user.hex)
USER_SRECFILE += $(call CONVERT_PATH,$(TOPDIR)/roux_user.srec)
USER_BINFILE += $(call CONVERT_PATH,$(TOPDIR)/roux_user.bin)

USER_LDFLAGS = -melf64lriscv --undefined=$(ENTRYPT) --entry=$(ENTRYPT) $(USER_LDSCRIPT)
USER_LDLIBS = $(patsubst lib%,-l%,$(basename $(notdir $(USERLIBS))))
USER_LIBGCC = "${shell "$(CC)" $(ARCHCPUFLAGS) -print-libgcc-file-name}"

# Source files

CSRCS = mpfs_userspace.c
COBJS = $(CSRCS:.c=$(OBJEXT))
OBJS  = $(COBJS)

# Targets:

all: $(TOPDIR)/roux_user.elf $(TOPDIR)/User.map
.PHONY: roux_user.elf depend clean distclean

$(COBJS): %$(OBJEXT): %.c
	$(call COMPILE, $<, $@)

# Create the roux_user.elf file containing all of the user-mode code

roux_user.elf: $(OBJS)
	$(Q) $(LD) -o $@ $(USER_LDFLAGS) $(USER_LIBPATHS) $(OBJS) --start-group $(USER_LDLIBS) --end-group $(USER_LIBGCC)

$(TOPDIR)/roux_user.elf: roux_user.elf
	@echo "LD: roux_user.elf"
	$(Q) cp -a roux_user.elf $(TOPDIR)/roux_user.elf
ifeq ($(CONFIG_INTELHEX_BINARY),y)
	@echo "CP: roux_user.hex"
	$(Q) $(OBJCOPY) $(OBJCOPYARGS) -O ihex roux_user.elf $(USER_HEXFILE)
endif
ifeq ($(CONFIG_MOTOROLA_SREC),y)
	@echo "CP: roux_user.srec"
	$(Q) $(OBJCOPY) $(OBJCOPYARGS) -O srec roux_user.elf $(USER_SRECFILE)
endif
ifeq ($(CONFIG_RAW_BINARY),y)
	@echo "CP: roux_user.bin"
	$(Q) $(OBJCOPY) $(OBJCOPYARGS) -O binary roux_user.elf $(USER_BINFILE)
endif

$(TOPDIR)/User.map: roux_user.elf
	@echo "MK: User.map"
	$(Q) $(NM) -n roux_user.elf >$(TOPDIR)/User.map
	$(Q) $(CROSSDEV)size roux_user.elf

.depend:

depend: .depend

clean:
	$(call DELFILE, roux_user.elf)
	$(call DELFILE, "$(TOPDIR)/roux_user.*")
	$(call DELFILE, "$(TOPDIR)/User.map")
	$(call CLEAN)

distclean: clean
