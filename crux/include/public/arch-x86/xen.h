/* SPDX-License-Identifier: MIT */
/******************************************************************************
 * arch-x86/crux.h
 *
 * Guest OS interface to x86 Xen.
 *
 * Copyright (c) 2004-2006, K A Fraser
 */

#ifndef __CRUX_PUBLIC_ARCH_X86_CRUX_H__
#define __CRUX_PUBLIC_ARCH_X86_CRUX_H__

/* Structural guest handles introduced in 0x00030201. */
#if __CRUX_INTERFACE_VERSION__ >= 0x00030201
#define ___DEFINE_CRUX_GUEST_HANDLE(name, type) \
    typedef struct { type *p; } __guest_handle_ ## name
#else
#define ___DEFINE_CRUX_GUEST_HANDLE(name, type) \
    typedef type * __guest_handle_ ## name
#endif

/*
 * CRUX_GUEST_HANDLE represents a guest pointer, when passed as a field
 * in a struct in memory.
 * CRUX_GUEST_HANDLE_PARAM represent a guest pointer, when passed as an
 * hypercall argument.
 * CRUX_GUEST_HANDLE_PARAM and CRUX_GUEST_HANDLE are the same on X86 but
 * they might not be on other architectures.
 */
#define __DEFINE_CRUX_GUEST_HANDLE(name, type) \
    ___DEFINE_CRUX_GUEST_HANDLE(name, type);   \
    ___DEFINE_CRUX_GUEST_HANDLE(const_##name, const type)
#define DEFINE_CRUX_GUEST_HANDLE(name)   __DEFINE_CRUX_GUEST_HANDLE(name, name)
#define __CRUX_GUEST_HANDLE(name)        __guest_handle_ ## name
#define CRUX_GUEST_HANDLE(name)          __CRUX_GUEST_HANDLE(name)
#define CRUX_GUEST_HANDLE_PARAM(name)    CRUX_GUEST_HANDLE(name)
#define set_crux_guest_handle_raw(hnd, val)  do { (hnd).p = (val); } while (0)
#define set_crux_guest_handle(hnd, val) set_crux_guest_handle_raw(hnd, val)

#if defined(__i386__)
# ifdef __CRUX__
__DeFiNe__ __DECL_REG_LO8(which) uint32_t e ## which ## x
__DeFiNe__ __DECL_REG_LO16(name) union { uint32_t e ## name; }
# endif
#include "crux-x86_32.h"
# ifdef __CRUX__
__UnDeF__ __DECL_REG_LO8
__UnDeF__ __DECL_REG_LO16
__DeFiNe__ __DECL_REG_LO8(which) e ## which ## x
__DeFiNe__ __DECL_REG_LO16(name) e ## name
# endif
#elif defined(__x86_64__)
#include "crux-x86_64.h"
#endif

#ifndef __ASSEMBLY__
typedef unsigned long crux_pfn_t;
#define PRI_crux_pfn "lx"
#define PRIu_crux_pfn "lu"
#endif

#define CRUX_HAVE_PV_GUEST_ENTRY 1

#define CRUX_HAVE_PV_UPCALL_MASK 1

/*
 * `incontents 200 segdesc Segment Descriptor Tables
 */
/*
 * ` enum neg_errnoval
 * ` HYPERVISOR_set_gdt(const crux_pfn_t frames[], unsigned int entries);
 * `
 */
/*
 * A number of GDT entries are reserved by Xen. These are not situated at the
 * start of the GDT because some stupid OSes export hard-coded selector values
 * in their ABI. These hard-coded values are always near the start of the GDT,
 * so Xen places itself out of the way, at the far end of the GDT.
 *
 * NB The LDT is set using the MMUEXT_SET_LDT op of HYPERVISOR_mmuext_op
 */
#define FIRST_RESERVED_GDT_PAGE  14
#define FIRST_RESERVED_GDT_BYTE  (FIRST_RESERVED_GDT_PAGE * 4096)
#define FIRST_RESERVED_GDT_ENTRY (FIRST_RESERVED_GDT_BYTE / 8)


/*
 * ` enum neg_errnoval
 * ` HYPERVISOR_update_descriptor(u64 pa, u64 desc);
 * `
 * ` @pa   The machine physical address of the descriptor to
 * `       update. Must be either a descriptor page or writable.
 * ` @desc The descriptor value to update, in the same format as a
 * `       native descriptor table entry.
 */

/* Maximum number of virtual CPUs in legacy multi-processor guests. */
#define CRUX_LEGACY_MAX_VCPUS 32

#ifndef __ASSEMBLY__

typedef unsigned long crux_ulong_t;
#define PRI_crux_ulong "lx"

/*
 * ` enum neg_errnoval
 * ` HYPERVISOR_stack_switch(unsigned long ss, unsigned long esp);
 * `
 * Sets the stack segment and pointer for the current vcpu.
 */

/*
 * ` enum neg_errnoval
 * ` HYPERVISOR_set_trap_table(const struct trap_info traps[]);
 * `
 */
/*
 * Send an array of these to HYPERVISOR_set_trap_table().
 * Terminate the array with a sentinel entry, with traps[].address==0.
 * The privilege level specifies which modes may enter a trap via a software
 * interrupt. On x86/64, since rings 1 and 2 are unavailable, we allocate
 * privilege levels as follows:
 *  Level == 0: Noone may enter
 *  Level == 1: Kernel may enter
 *  Level == 2: Kernel may enter
 *  Level == 3: Everyone may enter
 *
 * Note: For compatibility with kernels not setting up exception handlers
 *       early enough, Xen will avoid trying to inject #GP (and hence crash
 *       the domain) when an RDMSR would require this, but no handler was
 *       set yet. The precise conditions are implementation specific, and
 *       new code may not rely on such behavior anyway.
 */
#define TI_GET_DPL(_ti)      ((_ti)->flags & 3)
#define TI_GET_IF(_ti)       ((_ti)->flags & 4)
#define TI_SET_DPL(_ti,_dpl) ((_ti)->flags |= (_dpl))
#define TI_SET_IF(_ti,_if)   ((_ti)->flags |= ((!!(_if))<<2))
struct trap_info {
    uint8_t       vector;  /* exception vector                              */
    uint8_t       flags;   /* 0-3: privilege level; 4: clear event enable?  */
    uint16_t      cs;      /* code selector                                 */
    unsigned long address; /* code offset                                   */
};
typedef struct trap_info trap_info_t;
DEFINE_CRUX_GUEST_HANDLE(trap_info_t);

typedef uint64_t tsc_timestamp_t; /* RDTSC timestamp */

/*
 * The following is all CPU context. Note that the fpu_ctxt block is filled
 * in by FXSAVE if the CPU has feature FXSR; otherwise FSAVE is used.
 *
 * Also note that when calling DOMCTL_setvcpucontext for HVM guests, not all
 * information in this structure is updated, the fields read include: fpu_ctxt
 * (if VGCT_I387_VALID is set), flags, user_regs and debugreg[*].
 *
 * Note: VCPUOP_initialise for HVM guests is non-symetric with
 * DOMCTL_setvcpucontext, and uses struct vcpu_hvm_context from hvm/hvm_vcpu.h
 */
struct vcpu_guest_context {
    /* FPU registers come first so they can be aligned for FXSAVE/FXRSTOR. */
    struct { char x[512]; } fpu_ctxt;       /* User-level FPU registers     */
#define VGCF_I387_VALID                (1<<0)
#define VGCF_IN_KERNEL                 (1<<2)
#define _VGCF_i387_valid               0
#define VGCF_i387_valid                (1<<_VGCF_i387_valid)
#define _VGCF_in_kernel                2
#define VGCF_in_kernel                 (1<<_VGCF_in_kernel)
#define _VGCF_failsafe_disables_events 3
#define VGCF_failsafe_disables_events  (1<<_VGCF_failsafe_disables_events)
#define _VGCF_syscall_disables_events  4
#define VGCF_syscall_disables_events   (1<<_VGCF_syscall_disables_events)
#define _VGCF_online                   5
#define VGCF_online                    (1<<_VGCF_online)
    unsigned long flags;                    /* VGCF_* flags                 */
    struct cpu_user_regs user_regs;         /* User-level CPU registers     */
    struct trap_info trap_ctxt[256];        /* Virtual IDT                  */
    unsigned long ldt_base, ldt_ents;       /* LDT (linear address, # ents) */
    unsigned long gdt_frames[16], gdt_ents; /* GDT (machine frames, # ents) */
    unsigned long kernel_ss, kernel_sp;     /* Virtual TSS (only SS1/SP1)   */
    /* NB. User pagetable on x86/64 is placed in ctrlreg[1]. */
    unsigned long ctrlreg[8];               /* CR0-CR7 (control registers)  */
    unsigned long debugreg[8];              /* DB0-DB7 (debug registers)    */
#ifdef __i386__
    unsigned long event_callback_cs;        /* CS:EIP of event callback     */
    unsigned long event_callback_eip;
    unsigned long failsafe_callback_cs;     /* CS:EIP of failsafe callback  */
    unsigned long failsafe_callback_eip;
#else
    unsigned long event_callback_eip;
    unsigned long failsafe_callback_eip;
#ifdef __CRUX__
    union {
        unsigned long syscall_callback_eip;
        struct {
            unsigned int event_callback_cs;    /* compat CS of event cb     */
            unsigned int failsafe_callback_cs; /* compat CS of failsafe cb  */
        };
    };
#else
    unsigned long syscall_callback_eip;
#endif
#endif
    unsigned long vm_assist;                /* VMASST_TYPE_* bitmap */
#ifdef __x86_64__
    /* Segment base addresses. */
    uint64_t      fs_base;
    uint64_t      gs_base_kernel;
    uint64_t      gs_base_user;
#endif
};
typedef struct vcpu_guest_context vcpu_guest_context_t;
DEFINE_CRUX_GUEST_HANDLE(vcpu_guest_context_t);

struct arch_shared_info {
    /*
     * Number of valid entries in the p2m table(s) anchored at
     * pfn_to_mfn_frame_list_list and/or p2m_vaddr.
     */
    unsigned long max_pfn;
    /*
     * Frame containing list of mfns containing list of mfns containing p2m.
     * A value of 0 indicates it has not yet been set up, ~0 indicates it has
     * been set to invalid e.g. due to the p2m being too large for the 3-level
     * p2m tree. In this case the linear mapper p2m list anchored at p2m_vaddr
     * is to be used.
     */
    crux_pfn_t     pfn_to_mfn_frame_list_list;
    unsigned long nmi_reason;
    /*
     * Following three fields are valid if p2m_cr3 contains a value different
     * from 0.
     * p2m_cr3 is the root of the address space where p2m_vaddr is valid.
     * p2m_cr3 is in the same format as a cr3 value in the vcpu register state
     * and holds the folded machine frame number (via crux_pfn_to_cr3) of a
     * L3 or L4 page table.
     * p2m_vaddr holds the virtual address of the linear p2m list. All entries
     * in the range [0...max_pfn[ are accessible via this pointer.
     * p2m_generation will be incremented by the guest before and after each
     * change of the mappings of the p2m list. p2m_generation starts at 0 and
     * a value with the least significant bit set indicates that a mapping
     * update is in progress. This allows guest external software (e.g. in Dom0)
     * to verify that read mappings are consistent and whether they have changed
     * since the last check.
     * Modifying a p2m element in the linear p2m list is allowed via an atomic
     * write only.
     */
    unsigned long p2m_cr3;         /* cr3 value of the p2m address space */
    unsigned long p2m_vaddr;       /* virtual address of the p2m list */
    unsigned long p2m_generation;  /* generation count of p2m mapping */
#ifdef __i386__
    /* There's no room for this field in the generic structure. */
    uint32_t wc_sec_hi;
#endif
};
typedef struct arch_shared_info arch_shared_info_t;

#if defined(__CRUX__) || defined(__CRUX_TOOLS__)
/*
 * struct crux_arch_domainconfig's ABI is covered by
 * CRUX_DOMCTL_INTERFACE_VERSION.
 */
struct crux_arch_domainconfig {
#define _CRUX_X86_EMU_LAPIC          0
#define CRUX_X86_EMU_LAPIC           (1U<<_CRUX_X86_EMU_LAPIC)
#define _CRUX_X86_EMU_HPET           1
#define CRUX_X86_EMU_HPET            (1U<<_CRUX_X86_EMU_HPET)
#define _CRUX_X86_EMU_PM             2
#define CRUX_X86_EMU_PM              (1U<<_CRUX_X86_EMU_PM)
#define _CRUX_X86_EMU_RTC            3
#define CRUX_X86_EMU_RTC             (1U<<_CRUX_X86_EMU_RTC)
#define _CRUX_X86_EMU_IOAPIC         4
#define CRUX_X86_EMU_IOAPIC          (1U<<_CRUX_X86_EMU_IOAPIC)
#define _CRUX_X86_EMU_PIC            5
#define CRUX_X86_EMU_PIC             (1U<<_CRUX_X86_EMU_PIC)
#define _CRUX_X86_EMU_VGA            6
#define CRUX_X86_EMU_VGA             (1U<<_CRUX_X86_EMU_VGA)
#define _CRUX_X86_EMU_IOMMU          7
#define CRUX_X86_EMU_IOMMU           (1U<<_CRUX_X86_EMU_IOMMU)
#define _CRUX_X86_EMU_PIT            8
#define CRUX_X86_EMU_PIT             (1U<<_CRUX_X86_EMU_PIT)
#define _CRUX_X86_EMU_USE_PIRQ       9
#define CRUX_X86_EMU_USE_PIRQ        (1U<<_CRUX_X86_EMU_USE_PIRQ)
#define _CRUX_X86_EMU_VPCI           10
#define CRUX_X86_EMU_VPCI            (1U<<_CRUX_X86_EMU_VPCI)

#define CRUX_X86_EMU_ALL             (CRUX_X86_EMU_LAPIC | CRUX_X86_EMU_HPET |  \
                                     CRUX_X86_EMU_PM | CRUX_X86_EMU_RTC |      \
                                     CRUX_X86_EMU_IOAPIC | CRUX_X86_EMU_PIC |  \
                                     CRUX_X86_EMU_VGA | CRUX_X86_EMU_IOMMU |   \
                                     CRUX_X86_EMU_PIT | CRUX_X86_EMU_USE_PIRQ |\
                                     CRUX_X86_EMU_VPCI)
    uint32_t emulation_flags;

/*
 * Select whether to use a relaxed behavior for accesses to MSRs not explicitly
 * handled by Xen instead of injecting a #GP to the guest. Note this option
 * doesn't allow the guest to read or write to the underlying MSR.
 */
#define CRUX_X86_MSR_RELAXED (1u << 0)
    uint32_t misc_flags;
};

/* Max  CRUX_X86_* constant. Used for ABI checking. */
#define CRUX_X86_MISC_FLAGS_MAX CRUX_X86_MSR_RELAXED

#endif

/*
 * Representations of architectural CPUID and MSR information.  Used as the
 * serialised version of Xen's internal representation.
 */
typedef struct crux_cpuid_leaf {
#define CRUX_CPUID_NO_SUBLEAF 0xffffffffu
    uint32_t leaf, subleaf;
    uint32_t a, b, c, d;
} crux_cpuid_leaf_t;
DEFINE_CRUX_GUEST_HANDLE(crux_cpuid_leaf_t);

typedef struct crux_msr_entry {
    uint32_t idx;
    uint32_t flags; /* Reserved MBZ. */
    uint64_t val;
} crux_msr_entry_t;
DEFINE_CRUX_GUEST_HANDLE(crux_msr_entry_t);

#endif /* !__ASSEMBLY__ */

/*
 * ` enum neg_errnoval
 * ` HYPERVISOR_fpu_taskswitch(int set);
 * `
 * Sets (if set!=0) or clears (if set==0) CR0.TS.
 */

/*
 * ` enum neg_errnoval
 * ` HYPERVISOR_set_debugreg(int regno, unsigned long value);
 *
 * ` unsigned long
 * ` HYPERVISOR_get_debugreg(int regno);
 * For 0<=reg<=7, returns the debug register value.
 * For other values of reg, returns ((unsigned long)-EINVAL).
 * (Unfortunately, this interface is defective.)
 */

/*
 * Prefix forces emulation of some non-trapping instructions.
 * Currently only CPUID.
 */
#ifdef __ASSEMBLY__
#define CRUX_EMULATE_PREFIX .byte 0x0f,0x0b,0x78,0x65,0x6e ;
#define CRUX_CPUID          CRUX_EMULATE_PREFIX cpuid
#else
#define CRUX_EMULATE_PREFIX ".byte 0x0f,0x0b,0x78,0x65,0x6e ; "
#define CRUX_CPUID          CRUX_EMULATE_PREFIX "cpuid"
#endif

/*
 * Debug console IO port, also called "port E9 hack". Each character written
 * to this IO port will be printed on the hypervisor console, subject to log
 * level restrictions.
 */
#define CRUX_HVM_DEBUGCONS_IOPORT 0xe9

#endif /* __CRUX_PUBLIC_ARCH_X86_CRUX_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
