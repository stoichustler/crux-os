#######################################################
#                      _____     __  __
#             /\/\/\/\/ __  \/\ / _\/__\
#             \ - \ \ \ \/ // // _\/  \
#              \/\/\_/\_/\/ \__\__\\/\/ @2025
#
#                  - Hustle Embedded -
#######################################################

dtb-y += $(TARGET).dtb

all: $(dtb-y)

HOSTCPP  = cpp
DTSFLAGS = -nostdinc -nostdlib -fno-builtin -x assembler-with-cpp \
           -include $(CRUX_ROOT)/include/generated/autoconf.h

%.dtb: %.dts
	$(Q)echo "  DTC     $@"
	$(Q)$(HOSTCPP) $(DTSFLAGS) $< | dtc -q -o $@ -I dts -O dtb
	$(Q)echo
