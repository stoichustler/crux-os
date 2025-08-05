############################################################################
# arch/arm64/src/Makefile
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

include $(TOPDIR)/tools/Make.defs
include chip/Make.defs

ARCH_SRCDIR = $(TOPDIR)/arch/$(CONFIG_ARCH)/src

INCLUDES += ${INCDIR_PREFIX}$(ARCH_SRCDIR)/chip
INCLUDES += ${INCDIR_PREFIX}$(ARCH_SRCDIR)/common
INCLUDES += ${INCDIR_PREFIX}$(TOPDIR)/kernel

CPPFLAGS += $(INCLUDES)
CFLAGS += $(INCLUDES)
CXXFLAGS += $(INCLUDES)
AFLAGS += $(INCLUDES)

ROUX = $(call CONVERT_PATH,$(TOPDIR)/roux$(EXEEXT))

# Additional rules for system call wrapper

ifeq ($(CONFIG_SCHED_INSTRUMENTATION_SYSCALL),y)
  EXTRALINKCMDS += @$(TOPDIR)/system/syscall/syscall_wraps.ldcmd
endif

# The "head" object

HEAD_OBJ = $(HEAD_ASRC:.S=$(OBJEXT))
STARTUP_OBJS ?= $(HEAD_OBJ)

# Flat build or kernel-mode objects

ASRCS = $(CHIP_ASRCS) $(CMN_ASRCS)
AOBJS = $(ASRCS:.S=$(OBJEXT))

CSRCS = $(CHIP_CSRCS) $(CMN_CSRCS)
COBJS = $(CSRCS:.c=$(OBJEXT))

SRCS = $(ASRCS) $(CSRCS)
OBJS = $(AOBJS) $(COBJS) $(HEAD_OBJ)

# User-mode objects

UASRCS = $(CHIP_UASRCS) $(CMN_UASRCS)
UAOBJS = $(UASRCS:.S=$(OBJEXT))

UCSRCS = $(CHIP_UCSRCS) $(CMN_UCSRCS)
UCOBJS = $(UCSRCS:.c=$(OBJEXT))

USRCS = $(UASRCS) $(UCSRCS)
UOBJS = $(UAOBJS) $(UCOBJS)

KBIN = libkarch$(LIBEXT)
BIN  = libarch$(LIBEXT)

$(foreach lib,$(notdir $(wildcard $(APPDIR)/staging/*$(LIBEXT))), \
  $(foreach elib,$(EXTRA_LIBS), \
    $(if $(filter $(notdir $(elib)),$(lib)), \
      $(eval NAMEFULL_LIBS+=$(elib)), \
      $(if $(filter $(notdir $(elib)),$(patsubst lib%$(LIBEXT),-l%,$(lib))), \
        $(eval NAMESPEC_LIBS+=$(elib)) \
       ) \
     ) \
   ) \
 )

EXTRA_LIBS := $(filter-out $(NAMEFULL_LIBS) $(NAMESPEC_LIBS),$(EXTRA_LIBS))
EXTRA_LIBS += $(wildcard $(APPDIR)/staging/*$(LIBEXT))

# Override in Make.defs if linker is not 'ld'

ifeq ($(CONFIG_ARM_TOOLCHAIN_ARMCLANG),)
  ifeq ($(LD),$(CC))
    ifeq ($(CONFIG_STACK_CANARIES),y)
      # filter out ssp(Stack Smashing Protector) related flags:
      #  -fstack-protector
      #  -fstack-protector-all
      #  -fstack-protector-strong
      #  -fstack-protector-explicit
      STRIPCFLAGS = $(filter -fstack-protector%,$(CFLAGS))
    endif
    LDSTARTGROUP ?= -Wl,--start-group
    LDENDGROUP   ?= -Wl,--end-group
    LDFLAGS      := $(addprefix -Xlinker ,$(LDFLAGS))
    LDFLAGS      += $(filter-out $(STRIPCFLAGS),$(CFLAGS))
  else
    LDSTARTGROUP ?= --start-group
    LDENDGROUP   ?= --end-group
  endif

  LIBPATH_OPT = -L
  LIBRARY_OPT = -l
  SCRIPT_OPT  = -T
else
  LIBPATH_OPT = --userlibpath
  LIBRARY_OPT = --library=
  SCRIPT_OPT  = --scatter=
endif

ARCHSCRIPT := $(call CONVERT_PATH,$(ARCHSCRIPT))
LDFLAGS += $(addprefix $(SCRIPT_OPT),$(addsuffix .tmp,$(ARCHSCRIPT))) $(EXTRALINKCMDS)
LIBPATHS += $(LIBPATH_OPT) $(call CONVERT_PATH,$(TOPDIR)/staging)

BOARDMAKE = $(if $(wildcard board/Makefile),y,)
ifeq ($(BOARDMAKE),y)
  LIBPATHS += $(LIBPATH_OPT) $(call CONVERT_PATH,$(TOPDIR)/arch/$(CONFIG_ARCH)/src/board)
endif

LDLIBS = $(patsubst %.a,%,$(patsubst lib%,$(LIBRARY_OPT)%,$(LINKLIBS)))
ifeq ($(BOARDMAKE),y)
  LDLIBS += $(LIBRARY_OPT)board
endif

VPATH += chip
VPATH += common
VPATH += $(ARCH_SUBDIR)

all: $(HEAD_OBJ) $(BIN)

.PHONY: board/libboard$(LIBEXT)

$(AOBJS) $(UAOBJS) $(HEAD_OBJ): %$(OBJEXT): %.S
	$(call ASSEMBLE, $<, $@)

$(COBJS) $(UCOBJS): %$(OBJEXT): %.c
	$(call COMPILE, $<, $@)

$(STARTUP_OBJS): %$(OBJEXT): %.c
	$(Q) $(CC) $(CELFFLAGS) -c common/crt0.c -o crt0$(OBJEXT)

ifeq ($(CONFIG_BUILD_FLAT),y)
$(BIN): $(OBJS) $(STARTUP_OBJS)
	$(call ARCHIVE, $@, $(OBJS))
else
$(BIN): $(UOBJS) $(STARTUP_OBJS)
	$(call ARCHIVE, $@, $(UOBJS))
endif

$(KBIN): $(OBJS)
	$(call ARCHIVE, $@, $(OBJS))

board/libboard$(LIBEXT):
	$(Q) $(MAKE) -C board libboard$(LIBEXT) EXTRAFLAGS="$(EXTRAFLAGS)"

# When multiple linking, these two additional linking objects will be included

ifeq ($(CONFIG_MM_KASAN_GLOBAL),y)
EXTRA_LIBS += kasan_globals$(OBJEXT)
endif
ifeq ($(CONFIG_ALLSYMS),y)
EXTRA_LIBS += allsyms$(OBJEXT)
endif

define LINK_ALLSYMS_KASAN
	$(if $(CONFIG_ALLSYMS),
	$(Q) $(TOPDIR)/tools/mkallsyms.py $(ROUX) allsyms.tmp --orderbyname $(CONFIG_SYMTAB_ORDEREDBYNAME)
	$(Q) $(call COMPILE, allsyms.tmp, allsyms$(OBJEXT), -x c)
	$(Q) $(call DELFILE, allsyms.tmp))
	$(if $(CONFIG_MM_KASAN_GLOBAL),
	$(Q) $(TOPDIR)/tools/kasan_global.py -e $(ROUX) -o kasan_globals.tmp -a $(CONFIG_MM_KASAN_GLOBAL_ALIGN)
	$(Q) $(call COMPILE, kasan_globals.tmp, kasan_globals$(OBJEXT) -fno-sanitize=kernel-address, -x c)
	$(Q) $(call DELFILE, kasan_globals.tmp))
	$(Q) $(LD) --entry=__start $(LDFLAGS) $(LIBPATHS) $(EXTRA_LIBPATHS) \
		-o $(ROUX) $(HEAD_OBJ) $(EXTRA_OBJS) \
		$(LDSTARTGROUP) $(LDLIBS) $(EXTRA_LIBS) $(LDENDGROUP)
endef

$(addsuffix .tmp,$(ARCHSCRIPT)): $(ARCHSCRIPT)
	$(call PREPROCESS, $(patsubst %.tmp,%,$@), $@)

roux$(EXEEXT): $(HEAD_OBJ) board/libboard$(LIBEXT) $(addsuffix .tmp,$(ARCHSCRIPT))
	$(Q) echo "(ld)    roux"
ifeq ($(CONFIG_ALLSYMS)$(CONFIG_MM_KASAN_GLOBAL),)
	$(Q) $(LD) --entry=__start $(LDFLAGS) $(LIBPATHS) $(EXTRA_LIBPATHS) \
		-o $(ROUX) $(HEAD_OBJ) $(EXTRA_OBJS) \
		$(LDSTARTGROUP) $(LDLIBS) $(EXTRA_LIBS) $(LDENDGROUP)
else
	$(Q) $(call LINK_ALLSYMS_KASAN)
	$(Q) $(call LINK_ALLSYMS_KASAN)
	$(Q) $(call LINK_ALLSYMS_KASAN)
	$(Q) $(call LINK_ALLSYMS_KASAN)
endif
ifeq ($(CONFIG_MM_KASAN_GLOBAL),y)
	$(Q) $(OBJCOPY) -R .kasan.global $(ROUX)
	$(Q) $(OBJCOPY) -R .kasan.unused $(ROUX)
endif
ifneq ($(CONFIG_WINDOWS_NATIVE),y)
	$(Q) $(NM) $(ROUX) | \
	grep -v '\(compiled\)\|\(\$(OBJEXT)$$\)\|\( [aUw] \)\|\(\.\.ng$$\)\|\(LASH[RL]DI\)' | \
	sort > $(TOPDIR)/System.map
endif
	$(Q) $(call DELFILE, $(addsuffix .tmp,$(ARCHSCRIPT)))

# This is part of the top-level export target
# Note that there may not be a head object if layout is handled
# by the linker configuration.

export_startup: $(STARTUP_OBJS)
ifneq ($(STARTUP_OBJS),)
	$(Q) if [ -d "$(EXPORT_DIR)/startup" ]; then \
		cp -f $(STARTUP_OBJS) "$(EXPORT_DIR)/startup/."; \
	 else \
		echo "$(EXPORT_DIR)/startup does not exist"; \
	exit 1; \
	fi
endif

# Dependencies

makedepfile: $(CSRCS:.c=.ddc) $(ASRCS:.S=.dds) $(HEAD_ASRC:.S=.dds)
	$(call CATFILE, Make.dep, $^)
	$(call DELFILE, $^)

.depend: Makefile chip/Make.defs $(SRCS) $(TOPDIR)/.config
ifeq ($(BOARDMAKE),y)
	$(Q) $(MAKE) -C board depend
endif
	$(Q) $(MAKE) makedepfile DEPPATH="$(patsubst %,--dep-path %,$(subst :, ,$(VPATH)))"
	$(Q) touch $@

depend: .depend

context::

clean:
ifeq ($(BOARDMAKE),y)
	$(Q) $(MAKE) -C board clean
endif
	$(call DELFILE, $(addsuffix .tmp,$(ARCHSCRIPT)))
	$(call DELFILE, $(KBIN))
	$(call DELFILE, $(BIN))
ifneq ($(EXTRADELFILE),)
	$(call DELFILE, $(EXTRADELFILE))
endif
	$(call CLEAN)

distclean:: clean
ifeq ($(BOARDMAKE),y)
	$(Q) $(MAKE) -C board distclean
endif
	$(call DELFILE, Make.dep)
	$(call DELFILE, .depend)

-include Make.dep
