########################################
# RISCV-specific definitions

$(call cc-options-add,CFLAGS,CC,$(EMBEDDED_EXTRA_CFLAGS))

riscv-abi-$(CONFIG_RISCV_32) := -mabi=ilp32
riscv-abi-$(CONFIG_RISCV_64) := -mabi=lp64

riscv-march-$(CONFIG_RISCV_64) := rv64
riscv-march-y += ima
riscv-march-$(CONFIG_RISCV_ISA_C) += c

riscv-generic-flags := $(riscv-abi-y) -march=$(subst $(space),,$(riscv-march-y))

# check-extension: Check whether extenstion is supported by a compiler and
#                  an assembler.
# Usage: $(call check-extension,extension_name).
#        it should be defined variable with following name:
#          <extension name>-insn := "insn"
#        which represents an instruction of extension support of which is
#        going to be checked.
define check-extension =
$(eval $(1) := \
	$(call as-insn,$(CC) $(riscv-generic-flags)_$(1),$(value $(1)-insn),_$(1)))
endef

h-insn := "hfence.gvma"
$(call check-extension,h)

ifneq ($(h),_h)
hh-insn := "hfence.gvma"
$(call check-extension,hh)
endif

zihintpause-insn := "pause"
$(call check-extension,zihintpause)

extensions := $(h) $(hh) $(zihintpause) _zicsr_zifencei_zbb

extensions := $(subst $(space),,$(extensions))

# Note that -mcmodel=medany is used so that Xen can be mapped
# into the upper half _or_ the lower half of the address space.
# -mcmodel=medlow would force Xen into the lower half.

CFLAGS += $(riscv-generic-flags)$(extensions) -mstrict-align -mcmodel=medany
