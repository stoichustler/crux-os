#######################################################
#                      _____     __  __
#             /\/\/\/\/ __  \/\ / _\/__\
#             \ - \ \ \ \/ // // _\/  \
#              \/\/\_/\_/\/ \__\__\\/\/ @2025
#
#                  - Hustle Embedded -
#
#######################################################

# Don't refresh this files during e.g., 'sudo make install'
quiet_cmd_compile.h =
define cmd_compile.h
    if [ ! -r $@ -o -O $@ ]; then \
	cat scripts/banner; \
	sed -e 's/@@date@@/$(CRUX_BUILD_DATE)/g' \
	    -e 's/@@time@@/$(CRUX_BUILD_TIME)/g' \
	    -e 's/@@whoami@@/$(CRUX_WHOAMI)/g' \
	    -e 's/@@domain@@/$(CRUX_DOMAIN)/g' \
	    -e 's/@@hostname@@/$(CRUX_BUILD_HOST)/g' \
	    -e 's!@@compiler@@!$(shell $(CC) --version 2>&1 | head -1)!g' \
	    -e 's/@@version@@/$(CRUX_VERSION)/g' \
	    -e 's/@@subversion@@/$(CRUX_SUBVERSION)/g' \
	    -e 's/@@extraversion@@/$(CRUX_EXTRAVERSION)/g' \
	    -e 's!@@changeset@@!$(shell $(srctree)/tools/scmversion $(CRUX_ROOT) || echo "unavailable")!g' \
	    < $< > $(dot-target).tmp; \
	sed -rf $(srctree)/tools/process-banner.sed < scripts/banner >> $(dot-target).tmp; \
	mv -f $(dot-target).tmp $@; \
    fi
endef

include/crux/compile.h: include/crux/compile.h.in FORCE
	$(if $(filter-out FORCE,$?),$(Q)rm -fv $@)
	$(call if_changed,compile.h)

targets += include/crux/compile.h

-include $(wildcard .asm-offsets.s.d)
asm-offsets.s: arch/$(SRCARCH)/$(ARCH)/asm-offsets.c
	$(Q)$(CC) $(call cpp_flags,$(c_flags)) -S -g0 -o $@.new -MQ $@ $<
	$(call move-if-changed,$@.new,$@)

arch/$(SRCARCH)/include/asm/asm-offsets.h: asm-offsets.s
	@(set -e; \
	  echo "/*"; \
	  echo " * DO NOT MODIFY."; \
	  echo " *"; \
	  echo " * This file was auto-generated from $<"; \
	  echo " *"; \
	  echo " */"; \
	  echo ""; \
	  echo "#ifndef __ASM_OFFSETS_H__"; \
	  echo "#define __ASM_OFFSETS_H__"; \
	  echo ""; \
	  sed -rne "/^[^#].*==>/{s:.*==>(.*)<==.*:\1:; s: [\$$#]: :; p;}"; \
	  echo ""; \
	  echo "#endif") <$< >$@

build-dirs := $(patsubst %/built_in.o,%,$(filter %/built_in.o,$(ALL_OBJS) $(ALL_LIBS)))

# The actual objects are generated when descending,
# make sure no implicit rule kicks in
$(sort $(ALL_OBJS) $(ALL_LIBS)): $(build-dirs) ;

PHONY += $(build-dirs)
$(build-dirs): FORCE
	$(Q)$(MAKE) $(build)=$@ need-builtin=1

ifeq ($(CONFIG_LTO),y)
# Gather all LTO objects together
prelink_lto.o: $(ALL_OBJS) $(ALL_LIBS)
	$(LD_LTO) -r -o $@ $(filter-out %.a,$^) --start-group $(filter %.a,$^) --end-group

# Link it with all the binary objects
prelink.o: $(patsubst %/built_in.o,%/built_in_bin.o,$(ALL_OBJS)) prelink_lto.o FORCE
	$(call if_changed,ld)
else
prelink.o: $(ALL_OBJS) $(ALL_LIBS) FORCE
	$(call if_changed,ld)
endif

targets += prelink.o

$(TARGET): prelink.o FORCE
	$(Q)$(MAKE) $(build)=arch/$(SRCARCH) $@
