/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2015 Oracle and/or its affiliates. All rights reserved.
 */

#ifndef __CRUX_PUBLIC_PMU_H__
#define __CRUX_PUBLIC_PMU_H__

#include "crux.h"
#if defined(__i386__) || defined(__x86_64__)
#include "arch-x86/pmu.h"
#elif defined (__arm__) || defined (__aarch64__)
#include "arch-arm.h"
#elif defined (__powerpc64__)
#include "arch-ppc.h"
#elif defined(__riscv)
#include "arch-riscv.h"
#else
#error "Unsupported architecture"
#endif

#define CRUXPMU_VER_MAJ    0
#define CRUXPMU_VER_MIN    1

/*
 * ` enum neg_errnoval
 * ` HYPERVISOR_cruxpmu_op(enum cruxpmu_op cmd, struct cruxpmu_params *args);
 *
 * @cmd  == CRUXPMU_* (PMU operation)
 * @args == struct cruxpmu_params
 */
/* ` enum cruxpmu_op { */
#define CRUXPMU_mode_get        0 /* Also used for getting PMU version */
#define CRUXPMU_mode_set        1
#define CRUXPMU_feature_get     2
#define CRUXPMU_feature_set     3
#define CRUXPMU_init            4
#define CRUXPMU_finish          5
#define CRUXPMU_lvtpc_set       6
#define CRUXPMU_flush           7 /* Write cached MSR values to HW     */
/* ` } */

/* Parameters structure for HYPERVISOR_cruxpmu_op call */
struct crux_pmu_params {
    /* IN/OUT parameters */
    struct {
        uint32_t maj;
        uint32_t min;
    } version;
    uint64_t val;

    /* IN parameters */
    uint32_t vcpu;
    uint32_t pad;
};
typedef struct crux_pmu_params crux_pmu_params_t;
DEFINE_CRUX_GUEST_HANDLE(crux_pmu_params_t);

/* PMU modes:
 * - CRUXPMU_MODE_OFF:   No PMU virtualization
 * - CRUXPMU_MODE_SELF:  Guests can profile themselves
 * - CRUXPMU_MODE_HV:    Guests can profile themselves, dom0 profiles
 *                      itself and crux
 * - CRUXPMU_MODE_ALL:   Only dom0 has access to VPMU and it profiles
 *                      everyone: itself, the hypervisor and the guests.
 */
#define CRUXPMU_MODE_OFF           0
#define CRUXPMU_MODE_SELF          (1<<0)
#define CRUXPMU_MODE_HV            (1<<1)
#define CRUXPMU_MODE_ALL           (1<<2)

/*
 * PMU features:
 * - CRUXPMU_FEATURE_INTEL_BTS:  Intel BTS support (ignored on AMD)
 * - CRUXPMU_FEATURE_IPC_ONLY:   Restrict PMCs to the most minimum set possible.
 *                              Instructions, cycles, and ref cycles. Can be
 *                              used to calculate instructions-per-cycle (IPC)
 *                              (ignored on AMD).
 * - CRUXPMU_FEATURE_ARCH_ONLY:  Restrict PMCs to the Intel Pre-Defined
 *                              Architectural Performance Events exposed by
 *                              cpuid and listed in the Intel developer's manual
 *                              (ignored on AMD).
 */
#define CRUXPMU_FEATURE_INTEL_BTS  (1<<0)
#define CRUXPMU_FEATURE_IPC_ONLY   (1<<1)
#define CRUXPMU_FEATURE_ARCH_ONLY  (1<<2)

/*
 * Shared PMU data between hypervisor and PV(H) domains.
 *
 * The hypervisor fills out this structure during PMU interrupt and sends an
 * interrupt to appropriate VCPU.
 * Architecture-independent fields of crux_pmu_data are WO for the hypervisor
 * and RO for the guest but some fields in crux_pmu_arch can be writable
 * by both the hypervisor and the guest (see arch-$arch/pmu.h).
 */
struct crux_pmu_data {
    /* Interrupted VCPU */
    uint32_t vcpu_id;

    /*
     * Physical processor on which the interrupt occurred. On non-privileged
     * guests set to vcpu_id;
     */
    uint32_t pcpu_id;

    /*
     * domain that was interrupted. On non-privileged guests set to DOMID_SELF.
     * On privileged guests can be DOMID_SELF, DOMID_CRUX, or, when in
     * CRUXPMU_MODE_ALL mode, domain ID of another domain.
     */
    domid_t  domain_id;

    uint8_t pad[6];

    /* Architecture-specific information */
    crux_pmu_arch_t pmu;
};

#endif /* __CRUX_PUBLIC_PMU_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
