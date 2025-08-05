
ifneq ($(XEN_TARGET_ARCH),x86_32)
# xen 32-bit x86 hypervisor no longer supported, so has no test livepatches
subdir-y += livepatch
endif

install uninstall: %:
	set -e; for s in $(subdir-y); do \
		$(MAKE) $(build)=$$s $*; \
	done
