/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Guest OS interface to RISC-V crux.
 * Initially based on the ARM implementation.
 */

#ifndef __CRUX_PUBLIC_ARCH_RISCV_H__
#define __CRUX_PUBLIC_ARCH_RISCV_H__

#if defined(__CRUX__) || defined(__CRUX_TOOLS__) || defined(__GNUC__)
#define  int64_aligned_t  int64_t __attribute__((__aligned__(8)))
#define uint64_aligned_t uint64_t __attribute__((__aligned__(8)))
#endif

#ifndef __ASSEMBLY__
#define ___DEFINE_CRUX_GUEST_HANDLE(name, type)                  \
    typedef union { type *p; unsigned long q; }                 \
        __guest_handle_ ## name;                                \
    typedef union { type *p; uint64_aligned_t q; }              \
        __guest_handle_64_ ## name

/*
 * CRUX_GUEST_HANDLE represents a guest pointer, when passed as a field
 * in a struct in memory. On RISCV is always 8 bytes sizes and 8 bytes
 * aligned.
 * CRUX_GUEST_HANDLE_PARAM represents a guest pointer, when passed as an
 * hypercall argument. It is 4 bytes on riscv32 and 8 bytes on riscv64.
 */
#define __DEFINE_CRUX_GUEST_HANDLE(name, type) \
    ___DEFINE_CRUX_GUEST_HANDLE(name, type);   \
    ___DEFINE_CRUX_GUEST_HANDLE(const_##name, const type)
#define DEFINE_CRUX_GUEST_HANDLE(name)   __DEFINE_CRUX_GUEST_HANDLE(name, name)
#define __CRUX_GUEST_HANDLE(name)        __guest_handle_64_ ## name
#define CRUX_GUEST_HANDLE(name)          __CRUX_GUEST_HANDLE(name)
#define CRUX_GUEST_HANDLE_PARAM(name)    __guest_handle_ ## name
#define set_crux_guest_handle_raw(hnd, val)                  \
    do {                                                    \
        typeof(&(hnd)) sxghr_tmp_ = &(hnd);                 \
        sxghr_tmp_->q = 0;                                  \
        sxghr_tmp_->p = (val);                              \
    } while ( 0 )
#define set_crux_guest_handle(hnd, val) set_crux_guest_handle_raw(hnd, val)

typedef uint64_t crux_pfn_t;
#define PRI_crux_pfn PRIx64
#define PRIu_crux_pfn PRIu64

typedef uint64_t crux_ulong_t;
#define PRI_crux_ulong PRIx64

#if defined(__CRUX__) || defined(__CRUX_TOOLS__)

struct vcpu_guest_context {
};
typedef struct vcpu_guest_context vcpu_guest_context_t;
DEFINE_CRUX_GUEST_HANDLE(vcpu_guest_context_t);

struct crux_arch_domainconfig {
};

#endif

/* TODO:  add a placeholder entry if no real ones surface */
struct arch_vcpu_info {
};
typedef struct arch_vcpu_info arch_vcpu_info_t;

/* TODO:  add a placeholder entry if no real ones surface */
struct arch_shared_info {
};
typedef struct arch_shared_info arch_shared_info_t;

/*
 * Maximum number of virtual CPUs in legacy multi-processor guests.
 * Only one. All other VCPUS must use VCPUOP_register_vcpu_info.
 */
#define CRUX_LEGACY_MAX_VCPUS 1

/* Stub definition of PMU structure */
typedef struct crux_pmu_arch { uint8_t dummy; } crux_pmu_arch_t;
#endif

#endif /*  __CRUX_PUBLIC_ARCH_RISCV_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
