/*
 * include/asm-i386/xstate.h
 *
 * x86 extended state (xsave/xrstor) related definitions
 * 
 */

#ifndef __ASM_XSTATE_H
#define __ASM_XSTATE_H

#include <xen/sched.h>
#include <asm/cpufeature.h>
#include <asm/x86-defns.h>

#define FCW_DEFAULT               0x037f
#define FCW_RESET                 0x0040
#define FXSAVE_FTW_RESET          0xFF /* Abridged Tag Word format */
#define MXCSR_DEFAULT             0x1f80

extern uint32_t mxcsr_mask;

#define XSTATE_CPUID              0x0000000d

#define XCR_XFEATURE_ENABLED_MASK 0x00000000  /* index of XCR0 */

#define XSAVE_HDR_SIZE            64
#define XSAVE_SSE_OFFSET          160
#define XSTATE_YMM_SIZE           256
#define FXSAVE_SIZE               512
#define XSAVE_HDR_OFFSET          FXSAVE_SIZE
#define XSTATE_AREA_MIN_SIZE      (FXSAVE_SIZE + XSAVE_HDR_SIZE)

#define XSTATE_FP_SSE  (X86_XCR0_X87 | X86_XCR0_SSE)
#define XCNTXT_MASK    (X86_XCR0_X87 | X86_XCR0_SSE | X86_XCR0_YMM | \
                        X86_XCR0_OPMASK | X86_XCR0_ZMM | X86_XCR0_HI_ZMM | \
                        XSTATE_NONLAZY)

#define XSTATE_ALL     (~(1ULL << 63))
#define XSTATE_NONLAZY (X86_XCR0_BNDREGS | X86_XCR0_BNDCSR | X86_XCR0_PKRU | \
                        X86_XCR0_TILE_CFG | X86_XCR0_TILE_DATA)
#define XSTATE_LAZY    (XSTATE_ALL & ~XSTATE_NONLAZY)
#define XSTATE_XSAVES_ONLY         0
#define XSTATE_COMPACTION_ENABLED  (1ULL << 63)

#define XSTATE_XSS     (1U << 0)
#define XSTATE_ALIGN64 (1U << 1)

extern u64 xfeature_mask;
extern u64 xstate_align;
extern unsigned int *xstate_offsets;
extern unsigned int *xstate_sizes;

/* extended state save area */
struct __attribute__((aligned (64))) xsave_struct
{
    union __attribute__((aligned(16))) {     /* FPU/MMX, SSE */
        char x[512];
        struct {
            uint16_t fcw;
            uint16_t fsw;
            uint8_t ftw;
            uint8_t rsvd1;
            uint16_t fop;
            union {
                uint64_t addr;
                struct {
                    uint32_t offs;
                    uint16_t sel;
                    uint16_t rsvd;
                };
            } fip, fdp;
            uint32_t mxcsr;
            uint32_t mxcsr_mask;
            /* data registers follow here */
        };
    } fpu_sse;

    struct xsave_hdr {
        u64 xstate_bv;
        u64 xcomp_bv;
        u64 reserved[6];
    } xsave_hdr;                             /* The 64-byte header */

    char data[];                             /* Variable layout states */
};

typedef typeof(((struct xsave_struct){}).fpu_sse) fpusse_t;

struct xstate_bndcsr {
    uint64_t bndcfgu;
    uint64_t bndstatus;
};

/* extended state operations */
bool __must_check set_xcr0(u64 xfeatures);
uint64_t get_xcr0(void);
void set_msr_xss(u64 xss);
uint64_t get_msr_xss(void);
uint64_t read_bndcfgu(void);
void xsave(struct vcpu *v, uint64_t mask);
void xrstor(struct vcpu *v, uint64_t mask);
void xstate_set_init(uint64_t mask);
bool xsave_enabled(const struct vcpu *v);
int __must_check validate_xstate(const struct domain *d,
                                 uint64_t xcr0, uint64_t xcr0_accum,
                                 const struct xsave_hdr *hdr);
int __must_check handle_xsetbv(u32 index, u64 new_bv);
void expand_xsave_states(const struct vcpu *v, void *dest, unsigned int size);
void compress_xsave_states(struct vcpu *v, const void *src, unsigned int size);

/* extended state init and cleanup functions */
void xstate_free_save_area(struct vcpu *v);
int xstate_alloc_save_area(struct vcpu *v);
void xstate_init(struct cpuinfo_x86 *c);
unsigned int xstate_uncompressed_size(uint64_t xcr0);
unsigned int xstate_compressed_size(uint64_t xstates);

static inline uint64_t xgetbv(unsigned int index)
{
    uint32_t lo, hi;

    ASSERT(index); /* get_xcr0() should be used instead. */
    asm volatile ( ".byte 0x0f,0x01,0xd0" /* xgetbv */
                   : "=a" (lo), "=d" (hi) : "c" (index) );

    return lo | ((uint64_t)hi << 32);
}

static inline bool __nonnull(1)
xsave_area_compressed(const struct xsave_struct *xsave_area)
{
    return xsave_area->xsave_hdr.xcomp_bv & XSTATE_COMPACTION_ENABLED;
}

static inline bool xstate_all(const struct vcpu *v)
{
    /*
     * XSTATE_FP_SSE may be excluded, because the offsets of XSTATE_FP_SSE
     * (in the legacy region of xsave area) are fixed, so saving
     * XSTATE_FP_SSE will not cause overwriting problem with XSAVES/XSAVEC.
     */
    return xsave_area_compressed(v->arch.xsave_area) &&
           (v->arch.xcr0_accum & XSTATE_LAZY & ~XSTATE_FP_SSE);
}

/*
 * Fetch a pointer to a vCPU's XSAVE area
 *
 * TL;DR: If v == current, the mapping is guaranteed to already exist.
 *
 * Despite the name, this macro might not actually map anything. The only case
 * in which a mutation of page tables is strictly required is when ASI==on &&
 * v!=current. For everything else the mapping already exists and needs not
 * be created nor destroyed.
 *
 *                         +-----------------+--------------+
 *                         |   v == current  | v != current |
 *          +--------------+-----------------+--------------+
 *          | ASI  enabled | per-vCPU fixmap |  actual map  |
 *          +--------------+-----------------+--------------+
 *          | ASI disabled |             directmap          |
 *          +--------------+--------------------------------+
 *
 * There MUST NOT be outstanding maps of XSAVE areas of the non-current vCPU
 * at the point of context switch. Otherwise, the unmap operation will
 * misbehave.
 *
 * TODO: Expand the macro to the ASI cases after infra to do so is in place.
 *
 * @param v Owner of the XSAVE area
 */
#define VCPU_MAP_XSAVE_AREA(v) ((v)->arch.xsave_area)

/*
 * Drops the mapping of a vCPU's XSAVE area and nullifies its pointer on exit
 *
 * See VCPU_MAP_XSAVE_AREA() for additional information on the persistence of
 * these mappings. This macro only tears down the mappings in the ASI=on &&
 * v!=current case.
 *
 * TODO: Expand the macro to the ASI cases after infra to do so is in place.
 *
 * @param v Owner of the XSAVE area
 * @param x XSAVE blob of v
 */
#define VCPU_UNMAP_XSAVE_AREA(v, x) do { (void)(v); (x) = NULL; } while (0)

#endif /* __ASM_XSTATE_H */
