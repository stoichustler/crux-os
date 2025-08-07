/* SPDX-License-Identifier: MIT */
/*
 * arch-x86/cpufeatureset.h
 *
 * CPU featureset definitions
 *
 * Copyright (c) 2015, 2016 Citrix Systems, Inc.
 */

/*
 * There are two expected ways of including this header.
 *
 * 1) The "default" case (expected from tools etc).
 *
 * Simply #include <public/arch-x86/cpufeatureset.h>
 *
 * In this circumstance, normal header guards apply and the includer shall get
 * an enumeration in the CRUX_X86_FEATURE_xxx namespace.
 *
 * 2) The special case where the includer provides CRUX_CPUFEATURE() in scope.
 *
 * In this case, no inclusion guards apply and the caller is responsible for
 * their CRUX_CPUFEATURE() being appropriate in the included context.
 */

/* SAF-8-safe inclusion procedure left to caller */
#ifndef CRUX_CPUFEATURE

/*
 * Includer has not provided a custom CRUX_CPUFEATURE().  Arrange for normal
 * header guards, an enum and constants in the CRUX_X86_FEATURE_xxx namespace.
 */
#ifndef __CRUX_PUBLIC_ARCH_X86_CPUFEATURESET_H__
#define __CRUX_PUBLIC_ARCH_X86_CPUFEATURESET_H__

#define CRUX_CPUFEATURESET_DEFAULT_INCLUDE

#define CRUX_CPUFEATURE(name, value) CRUX_X86_FEATURE_##name = value,
enum {

#endif /* __CRUX_PUBLIC_ARCH_X86_CPUFEATURESET_H__ */
#endif /* !CRUX_CPUFEATURE */


#ifdef CRUX_CPUFEATURE
/*
 * A featureset is a bitmap of x86 features, represented as a collection of
 * 32bit words.
 *
 * Words are as specified in vendors programming manuals, and shall not
 * contain any synthesied values.  New words may be added to the end of
 * featureset.
 *
 * All featureset words currently originate from leaves specified for the
 * CPUID instruction, but this is not preclude other sources of information.
 */

/*
 * Attribute syntax:
 *
 * Attributes for a particular feature are provided as characters before the
 * first space in the comment immediately following the feature value.  Note -
 * none of these attributes form part of the crux public ABI.
 *
 * Special: '!'
 *   This bit has special properties and is not a straight indication of a
 *   piece of new functionality.  crux will handle these differently,
 *   and may override toolstack settings completely.
 *
 * Applicability to guests: 'A', 'S' or 'H'
 *   'A' = All guests.
 *   'S' = All HVM guests (not PV guests).
 *   'H' = HVM HAP guests (not PV or HVM Shadow guests).
 *   Upper case => Available by default
 *   Lower case => Can be opted-in to, but not available by default.
 *
 * Migration: '|'
 *   This bit should be visible to a guest if any anywhere it might run has
 *   the bit set.  i.e. it needs accumulating across the migration pool,
 *   rather than intersecting.
 */

/* Intel-defined CPU features, CPUID level 0x00000001.edx, word 0 */
CRUX_CPUFEATURE(FPU,           0*32+ 0) /*A  Onboard FPU */
CRUX_CPUFEATURE(VME,           0*32+ 1) /*S  Virtual Mode Extensions */
CRUX_CPUFEATURE(DE,            0*32+ 2) /*A  Debugging Extensions */
CRUX_CPUFEATURE(PSE,           0*32+ 3) /*S  Page Size Extensions */
CRUX_CPUFEATURE(TSC,           0*32+ 4) /*A  Time Stamp Counter */
CRUX_CPUFEATURE(MSR,           0*32+ 5) /*A  Model-Specific Registers, RDMSR, WRMSR */
CRUX_CPUFEATURE(PAE,           0*32+ 6) /*A  Physical Address Extensions */
CRUX_CPUFEATURE(MCE,           0*32+ 7) /*A  Machine Check Architecture */
CRUX_CPUFEATURE(CX8,           0*32+ 8) /*A  CMPXCHG8 instruction */
CRUX_CPUFEATURE(APIC,          0*32+ 9) /*!A Onboard APIC */
CRUX_CPUFEATURE(SEP,           0*32+11) /*A  SYSENTER/SYSEXIT */
CRUX_CPUFEATURE(MTRR,          0*32+12) /*S  Memory Type Range Registers */
CRUX_CPUFEATURE(PGE,           0*32+13) /*S  Page Global Enable */
CRUX_CPUFEATURE(MCA,           0*32+14) /*A  Machine Check Architecture */
CRUX_CPUFEATURE(CMOV,          0*32+15) /*A  CMOV instruction (FCMOVCC and FCOMI too if FPU present) */
CRUX_CPUFEATURE(PAT,           0*32+16) /*A  Page Attribute Table */
CRUX_CPUFEATURE(PSE36,         0*32+17) /*S  36-bit PSEs */
CRUX_CPUFEATURE(CLFLUSH,       0*32+19) /*A  CLFLUSH instruction */
CRUX_CPUFEATURE(DS,            0*32+21) /*   Debug Store */
CRUX_CPUFEATURE(ACPI,          0*32+22) /*A  ACPI via MSR */
CRUX_CPUFEATURE(MMX,           0*32+23) /*A  Multimedia Extensions */
CRUX_CPUFEATURE(FXSR,          0*32+24) /*A  FXSAVE and FXRSTOR instructions */
CRUX_CPUFEATURE(SSE,           0*32+25) /*A  Streaming SIMD Extensions */
CRUX_CPUFEATURE(SSE2,          0*32+26) /*A  Streaming SIMD Extensions-2 */
CRUX_CPUFEATURE(SS,            0*32+27) /*A  CPU self snoop */
CRUX_CPUFEATURE(HTT,           0*32+28) /*!A Hyper-Threading Technology */
CRUX_CPUFEATURE(TM1,           0*32+29) /*   Thermal Monitor 1 */
CRUX_CPUFEATURE(PBE,           0*32+31) /*   Pending Break Enable */

/* Intel-defined CPU features, CPUID level 0x00000001.ecx, word 1 */
CRUX_CPUFEATURE(SSE3,          1*32+ 0) /*A  Streaming SIMD Extensions-3 */
CRUX_CPUFEATURE(PCLMULQDQ,     1*32+ 1) /*A  Carry-less multiplication */
CRUX_CPUFEATURE(DTES64,        1*32+ 2) /*   64-bit Debug Store */
CRUX_CPUFEATURE(MONITOR,       1*32+ 3) /*   Monitor/Mwait support */
CRUX_CPUFEATURE(DSCPL,         1*32+ 4) /*   CPL Qualified Debug Store */
CRUX_CPUFEATURE(VMX,           1*32+ 5) /*h  Virtual Machine Extensions */
CRUX_CPUFEATURE(SMX,           1*32+ 6) /*   Safer Mode Extensions */
CRUX_CPUFEATURE(EIST,          1*32+ 7) /*   Enhanced SpeedStep */
CRUX_CPUFEATURE(TM2,           1*32+ 8) /*   Thermal Monitor 2 */
CRUX_CPUFEATURE(SSSE3,         1*32+ 9) /*A  Supplemental Streaming SIMD Extensions-3 */
CRUX_CPUFEATURE(SDBG,          1*32+11) /*   IA32_DEBUG_INTERFACE MSR for silicon debugging support */
CRUX_CPUFEATURE(FMA,           1*32+12) /*A  Fused Multiply Add */
CRUX_CPUFEATURE(CX16,          1*32+13) /*A  CMPXCHG16B */
CRUX_CPUFEATURE(XTPR,          1*32+14) /*   Send Task Priority Messages */
CRUX_CPUFEATURE(PDCM,          1*32+15) /*   Perf/Debug Capability MSR */
CRUX_CPUFEATURE(PCID,          1*32+17) /*H  Process Context ID */
CRUX_CPUFEATURE(DCA,           1*32+18) /*   Direct Cache Access */
CRUX_CPUFEATURE(SSE4_1,        1*32+19) /*A  Streaming SIMD Extensions 4.1 */
CRUX_CPUFEATURE(SSE4_2,        1*32+20) /*A  Streaming SIMD Extensions 4.2 */
CRUX_CPUFEATURE(X2APIC,        1*32+21) /*!S Extended xAPIC */
CRUX_CPUFEATURE(MOVBE,         1*32+22) /*A  movbe instruction */
CRUX_CPUFEATURE(POPCNT,        1*32+23) /*A  POPCNT instruction */
CRUX_CPUFEATURE(TSC_DEADLINE,  1*32+24) /*S  TSC Deadline Timer */
CRUX_CPUFEATURE(AESNI,         1*32+25) /*A  AES instructions */
CRUX_CPUFEATURE(XSAVE,         1*32+26) /*A  XSAVE/XRSTOR/XSETBV/XGETBV */
CRUX_CPUFEATURE(OSXSAVE,       1*32+27) /*!  OSXSAVE */
CRUX_CPUFEATURE(AVX,           1*32+28) /*A  Advanced Vector Extensions */
CRUX_CPUFEATURE(F16C,          1*32+29) /*A  Half-precision convert instruction */
CRUX_CPUFEATURE(RDRAND,        1*32+30) /*!A Digital Random Number Generator */
CRUX_CPUFEATURE(HYPERVISOR,    1*32+31) /*!A Running under some hypervisor */

/* AMD-defined CPU features, CPUID level 0x80000001.edx, word 2 */
CRUX_CPUFEATURE(SYSCALL,       2*32+11) /*A  SYSCALL/SYSRET */
CRUX_CPUFEATURE(NX,            2*32+20) /*A  Execute Disable */
CRUX_CPUFEATURE(MMXEXT,        2*32+22) /*A  AMD MMX extensions */
CRUX_CPUFEATURE(FFXSR,         2*32+25) /*A  FFXSR instruction optimizations */
CRUX_CPUFEATURE(PAGE1GB,       2*32+26) /*H  1Gb large page support */
CRUX_CPUFEATURE(RDTSCP,        2*32+27) /*A  RDTSCP */
CRUX_CPUFEATURE(LM,            2*32+29) /*A  Long Mode (x86-64) */
CRUX_CPUFEATURE(3DNOWEXT,      2*32+30) /*A  AMD 3DNow! extensions */
CRUX_CPUFEATURE(3DNOW,         2*32+31) /*A  3DNow! */

/* AMD-defined CPU features, CPUID level 0x80000001.ecx, word 3 */
CRUX_CPUFEATURE(LAHF_LM,       3*32+ 0) /*A  LAHF/SAHF in long mode */
CRUX_CPUFEATURE(CMP_LEGACY,    3*32+ 1) /*!A If yes HyperThreading not valid */
CRUX_CPUFEATURE(SVM,           3*32+ 2) /*h  Secure virtual machine */
CRUX_CPUFEATURE(EXTAPIC,       3*32+ 3) /*   Extended APIC space */
CRUX_CPUFEATURE(CR8_LEGACY,    3*32+ 4) /*S  CR8 in 32-bit mode */
CRUX_CPUFEATURE(ABM,           3*32+ 5) /*A  Advanced bit manipulation */
CRUX_CPUFEATURE(SSE4A,         3*32+ 6) /*A  SSE-4A */
CRUX_CPUFEATURE(MISALIGNSSE,   3*32+ 7) /*A  Misaligned SSE mode */
CRUX_CPUFEATURE(3DNOWPREFETCH, 3*32+ 8) /*A  3DNow prefetch instructions */
CRUX_CPUFEATURE(OSVW,          3*32+ 9) /*   OS Visible Workaround */
CRUX_CPUFEATURE(IBS,           3*32+10) /*   Instruction Based Sampling */
CRUX_CPUFEATURE(XOP,           3*32+11) /*A  extended AVX instructions */
CRUX_CPUFEATURE(SKINIT,        3*32+12) /*   SKINIT/STGI instructions */
CRUX_CPUFEATURE(WDT,           3*32+13) /*   Watchdog timer */
CRUX_CPUFEATURE(LWP,           3*32+15) /*   Light Weight Profiling */
CRUX_CPUFEATURE(FMA4,          3*32+16) /*A  4 operands MAC instructions */
CRUX_CPUFEATURE(NODEID_MSR,    3*32+19) /*   NodeId MSR */
CRUX_CPUFEATURE(TBM,           3*32+21) /*A  trailing bit manipulations */
CRUX_CPUFEATURE(TOPOEXT,       3*32+22) /*   topology extensions CPUID leafs */
CRUX_CPUFEATURE(DBEXT,         3*32+26) /*A  data breakpoint extension */
CRUX_CPUFEATURE(MONITORX,      3*32+29) /*   MONITOR extension (MONITORX/MWAITX) */
CRUX_CPUFEATURE(ADDR_MSK_EXT,  3*32+30) /*A  Address Mask Extentions */

/* Intel-defined CPU features, CPUID level 0x0000000D:1.eax, word 4 */
CRUX_CPUFEATURE(XSAVEOPT,      4*32+ 0) /*A  XSAVEOPT instruction */
CRUX_CPUFEATURE(XSAVEC,        4*32+ 1) /*A  XSAVEC/XRSTORC instructions */
CRUX_CPUFEATURE(XGETBV1,       4*32+ 2) /*A  XGETBV with %ecx=1 */
CRUX_CPUFEATURE(XSAVES,        4*32+ 3) /*S  XSAVES/XRSTORS instructions */
CRUX_CPUFEATURE(XFD,           4*32+ 4) /*   MSR_XFD{,_ERR} (eXtended Feature Disable) */

/* Intel-defined CPU features, CPUID level 0x00000007:0.ebx, word 5 */
CRUX_CPUFEATURE(FSGSBASE,      5*32+ 0) /*A  {RD,WR}{FS,GS}BASE instructions */
CRUX_CPUFEATURE(TSC_ADJUST,    5*32+ 1) /*S  TSC_ADJUST MSR available */
CRUX_CPUFEATURE(SGX,           5*32+ 2) /*   Software Guard extensions */
CRUX_CPUFEATURE(BMI1,          5*32+ 3) /*A  1st bit manipulation extensions */
CRUX_CPUFEATURE(HLE,           5*32+ 4) /*!a Hardware Lock Elision */
CRUX_CPUFEATURE(AVX2,          5*32+ 5) /*A  AVX2 instructions */
CRUX_CPUFEATURE(FDP_EXCP_ONLY, 5*32+ 6) /*!  x87 FDP only updated on exception. */
CRUX_CPUFEATURE(SMEP,          5*32+ 7) /*S  Supervisor Mode Execution Protection */
CRUX_CPUFEATURE(BMI2,          5*32+ 8) /*A  2nd bit manipulation extensions */
CRUX_CPUFEATURE(ERMS,          5*32+ 9) /*A  Enhanced REP MOVSB/STOSB */
CRUX_CPUFEATURE(INVPCID,       5*32+10) /*H  Invalidate Process Context ID */
CRUX_CPUFEATURE(RTM,           5*32+11) /*!A Restricted Transactional Memory */
CRUX_CPUFEATURE(PQM,           5*32+12) /*   Platform QoS Monitoring */
CRUX_CPUFEATURE(NO_FPU_SEL,    5*32+13) /*!  FPU CS/DS stored as zero */
CRUX_CPUFEATURE(MPX,           5*32+14) /*s  Memory Protection Extensions */
CRUX_CPUFEATURE(PQE,           5*32+15) /*   Platform QoS Enforcement */
CRUX_CPUFEATURE(AVX512F,       5*32+16) /*A  AVX-512 Foundation Instructions */
CRUX_CPUFEATURE(AVX512DQ,      5*32+17) /*A  AVX-512 Doubleword & Quadword Instrs */
CRUX_CPUFEATURE(RDSEED,        5*32+18) /*A  RDSEED instruction */
CRUX_CPUFEATURE(ADX,           5*32+19) /*A  ADCX, ADOX instructions */
CRUX_CPUFEATURE(SMAP,          5*32+20) /*S  Supervisor Mode Access Prevention */
CRUX_CPUFEATURE(AVX512_IFMA,   5*32+21) /*A  AVX-512 Integer Fused Multiply Add */
CRUX_CPUFEATURE(CLFLUSHOPT,    5*32+23) /*A  CLFLUSHOPT instruction */
CRUX_CPUFEATURE(CLWB,          5*32+24) /*!A CLWB instruction */
CRUX_CPUFEATURE(PROC_TRACE,    5*32+25) /*   Processor Trace */
CRUX_CPUFEATURE(AVX512PF,      5*32+26) /*   Xeon Phi AVX-512 Prefetch Instructions */
CRUX_CPUFEATURE(AVX512ER,      5*32+27) /*   Xeon Phi AVX-512 Exponent & Reciprocal Instrs */
CRUX_CPUFEATURE(AVX512CD,      5*32+28) /*A  AVX-512 Conflict Detection Instrs */
CRUX_CPUFEATURE(SHA,           5*32+29) /*A  SHA1 & SHA256 instructions */
CRUX_CPUFEATURE(AVX512BW,      5*32+30) /*A  AVX-512 Byte and Word Instructions */
CRUX_CPUFEATURE(AVX512VL,      5*32+31) /*A  AVX-512 Vector Length Extensions */

/* Intel-defined CPU features, CPUID level 0x00000007:0.ecx, word 6 */
CRUX_CPUFEATURE(PREFETCHWT1,   6*32+ 0) /*A  PREFETCHWT1 instruction */
CRUX_CPUFEATURE(AVX512_VBMI,   6*32+ 1) /*A  AVX-512 Vector Byte Manipulation Instrs */
CRUX_CPUFEATURE(UMIP,          6*32+ 2) /*S  User Mode Instruction Prevention */
CRUX_CPUFEATURE(PKU,           6*32+ 3) /*H  Protection Keys for Userspace */
CRUX_CPUFEATURE(OSPKE,         6*32+ 4) /*!  OS Protection Keys Enable */
CRUX_CPUFEATURE(WAITPKG,       6*32+ 5) /*   UMONITOR/UMWAIT/TPAUSE */
CRUX_CPUFEATURE(AVX512_VBMI2,  6*32+ 6) /*A  Additional AVX-512 Vector Byte Manipulation Instrs */
CRUX_CPUFEATURE(CET_SS,        6*32+ 7) /*   CET - Shadow Stacks */
CRUX_CPUFEATURE(GFNI,          6*32+ 8) /*A  Galois Field Instrs */
CRUX_CPUFEATURE(VAES,          6*32+ 9) /*A  Vector AES Instrs */
CRUX_CPUFEATURE(VPCLMULQDQ,    6*32+10) /*A  Vector Carry-less Multiplication Instrs */
CRUX_CPUFEATURE(AVX512_VNNI,   6*32+11) /*A  Vector Neural Network Instrs */
CRUX_CPUFEATURE(AVX512_BITALG, 6*32+12) /*A  Support for VPOPCNT[B,W] and VPSHUFBITQMB */
CRUX_CPUFEATURE(TME,           6*32+13) /*   Total Memory Encryption */
CRUX_CPUFEATURE(AVX512_VPOPCNTDQ, 6*32+14) /*A  POPCNT for vectors of DW/QW */
CRUX_CPUFEATURE(LA57,          6*32+16) /*   5-level paging (57-bit linear address) */
CRUX_CPUFEATURE(RDPID,         6*32+22) /*A  RDPID instruction */
CRUX_CPUFEATURE(BLD,           6*32+24) /*   BusLock Detect (#DB trap) support */
CRUX_CPUFEATURE(CLDEMOTE,      6*32+25) /*A  CLDEMOTE instruction */
CRUX_CPUFEATURE(MOVDIRI,       6*32+27) /*a  MOVDIRI instruction */
CRUX_CPUFEATURE(MOVDIR64B,     6*32+28) /*a  MOVDIR64B instruction */
CRUX_CPUFEATURE(ENQCMD,        6*32+29) /*   ENQCMD{,S} instructions */
CRUX_CPUFEATURE(SGX_LC,        6*32+30) /*   SGX Launch Configuration */
CRUX_CPUFEATURE(PKS,           6*32+31) /*H  Protection Key for Supervisor */

/* AMD-defined CPU features, CPUID level 0x80000007.edx, word 7 */
CRUX_CPUFEATURE(HW_PSTATE,     7*32+ 7) /*   Hardware Pstates */
CRUX_CPUFEATURE(ITSC,          7*32+ 8) /*a  Invariant TSC */
CRUX_CPUFEATURE(CPB,           7*32+ 9) /*   Core Performance Boost (Turbo) */
CRUX_CPUFEATURE(EFRO,          7*32+10) /*   APERF/MPERF Read Only interface */

/* AMD-defined CPU features, CPUID level 0x80000008.ebx, word 8 */
CRUX_CPUFEATURE(CLZERO,        8*32+ 0) /*A  CLZERO instruction */
CRUX_CPUFEATURE(RSTR_FP_ERR_PTRS, 8*32+ 2) /*A  (F)X{SAVE,RSTOR} always saves/restores FPU Error pointers */
CRUX_CPUFEATURE(WBNOINVD,      8*32+ 9) /*   WBNOINVD instruction */
CRUX_CPUFEATURE(IBPB,          8*32+12) /*A  IBPB support only (no IBRS, used by AMD) */
CRUX_CPUFEATURE(IBRS,          8*32+14) /*S  MSR_SPEC_CTRL.IBRS */
CRUX_CPUFEATURE(AMD_STIBP,     8*32+15) /*S  MSR_SPEC_CTRL.STIBP */
CRUX_CPUFEATURE(IBRS_ALWAYS,   8*32+16) /*S  IBRS preferred always on */
CRUX_CPUFEATURE(STIBP_ALWAYS,  8*32+17) /*S  STIBP preferred always on */
CRUX_CPUFEATURE(IBRS_FAST,     8*32+18) /*S  IBRS preferred over software options */
CRUX_CPUFEATURE(IBRS_SAME_MODE, 8*32+19) /*S  IBRS provides same-mode protection */
CRUX_CPUFEATURE(NO_LMSL,       8*32+20) /*S| EFER.LMSLE no longer supported. */
CRUX_CPUFEATURE(AMD_PPIN,      8*32+23) /*   Protected Processor Inventory Number */
CRUX_CPUFEATURE(AMD_SSBD,      8*32+24) /*S  MSR_SPEC_CTRL.SSBD available */
CRUX_CPUFEATURE(VIRT_SSBD,     8*32+25) /*!  MSR_VIRT_SPEC_CTRL.SSBD */
CRUX_CPUFEATURE(SSB_NO,        8*32+26) /*A  Hardware not vulnerable to SSB */
CRUX_CPUFEATURE(CPPC,          8*32+27) /*   Collaborative Processor Performance Control */
CRUX_CPUFEATURE(PSFD,          8*32+28) /*S  MSR_SPEC_CTRL.PSFD */
CRUX_CPUFEATURE(BTC_NO,        8*32+29) /*A  Hardware not vulnerable to Branch Type Confusion */
CRUX_CPUFEATURE(IBPB_RET,      8*32+30) /*A  IBPB clears RSB/RAS too. */

/* Intel-defined CPU features, CPUID level 0x00000007:0.edx, word 9 */
CRUX_CPUFEATURE(SGX_KEYS,      9*32+ 1) /*   SGX Attestation Service */
CRUX_CPUFEATURE(AVX512_4VNNIW, 9*32+ 2) /*   Xeon Phi AVX512 Neural Network Instructions */
CRUX_CPUFEATURE(AVX512_4FMAPS, 9*32+ 3) /*   Xeon Phi AVX512 Multiply Accumulation Single Precision */
CRUX_CPUFEATURE(FSRM,          9*32+ 4) /*A  Fast Short REP MOVS */
CRUX_CPUFEATURE(UINTR,         9*32+ 5) /*   User-mode Interrupts */
CRUX_CPUFEATURE(AVX512_VP2INTERSECT, 9*32+8) /*a  VP2INTERSECT{D,Q} insns */
CRUX_CPUFEATURE(SRBDS_CTRL,    9*32+ 9) /*   MSR_MCU_OPT_CTRL and RNGDS_MITG_DIS. */
CRUX_CPUFEATURE(MD_CLEAR,      9*32+10) /*!A| VERW clears microarchitectural buffers */
CRUX_CPUFEATURE(RTM_ALWAYS_ABORT, 9*32+11) /*! RTM disabled (but XBEGIN wont fault) */
CRUX_CPUFEATURE(TSX_FORCE_ABORT, 9*32+13) /* MSR_TSX_FORCE_ABORT.RTM_ABORT */
CRUX_CPUFEATURE(SERIALIZE,     9*32+14) /*A  SERIALIZE insn */
CRUX_CPUFEATURE(HYBRID,        9*32+15) /*   Heterogeneous platform */
CRUX_CPUFEATURE(TSXLDTRK,      9*32+16) /*a  TSX load tracking suspend/resume insns */
CRUX_CPUFEATURE(PCONFIG,       9*32+18) /*   PCONFIG instruction */
CRUX_CPUFEATURE(ARCH_LBR,      9*32+19) /*   Architectural Last Branch Record */
CRUX_CPUFEATURE(CET_IBT,       9*32+20) /*   CET - Indirect Branch Tracking */
CRUX_CPUFEATURE(AMX_BF16,      9*32+22) /*   AMX BFloat16 instruction */
CRUX_CPUFEATURE(AVX512_FP16,   9*32+23) /*A  AVX512 FP16 instructions */
CRUX_CPUFEATURE(AMX_TILE,      9*32+24) /*   AMX Tile architecture */
CRUX_CPUFEATURE(AMX_INT8,      9*32+25) /*   AMX 8-bit integer instructions */
CRUX_CPUFEATURE(IBRSB,         9*32+26) /*A  IBRS and IBPB support (used by Intel) */
CRUX_CPUFEATURE(STIBP,         9*32+27) /*A  STIBP */
CRUX_CPUFEATURE(L1D_FLUSH,     9*32+28) /*S  MSR_FLUSH_CMD and L1D flush. */
CRUX_CPUFEATURE(ARCH_CAPS,     9*32+29) /*!A IA32_ARCH_CAPABILITIES MSR */
CRUX_CPUFEATURE(CORE_CAPS,     9*32+30) /*   IA32_CORE_CAPABILITIES MSR */
CRUX_CPUFEATURE(SSBD,          9*32+31) /*A  MSR_SPEC_CTRL.SSBD available */

/* Intel-defined CPU features, CPUID level 0x00000007:1.eax, word 10 */
CRUX_CPUFEATURE(SHA512,       10*32+ 0) /*A  SHA512 Instructions */
CRUX_CPUFEATURE(SM3,          10*32+ 1) /*A  SM3 Instructions */
CRUX_CPUFEATURE(SM4,          10*32+ 2) /*A  SM4 Instructions */
CRUX_CPUFEATURE(AVX_VNNI,     10*32+ 4) /*A  AVX-VNNI Instructions */
CRUX_CPUFEATURE(AVX512_BF16,  10*32+ 5) /*A  AVX512 BFloat16 Instructions */
CRUX_CPUFEATURE(LASS,         10*32+ 6) /*   Linear Address Space Separation */
CRUX_CPUFEATURE(CMPCCXADD,    10*32+ 7) /*a  CMPccXADD Instructions */
CRUX_CPUFEATURE(ARCH_PERF_MON, 10*32+8) /*   Architectural Perfmon */
CRUX_CPUFEATURE(FZRM,         10*32+10) /*A  Fast Zero-length REP MOVSB */
CRUX_CPUFEATURE(FSRS,         10*32+11) /*A  Fast Short REP STOSB */
CRUX_CPUFEATURE(FSRCS,        10*32+12) /*A  Fast Short REP CMPSB/SCASB */
CRUX_CPUFEATURE(WRMSRNS,      10*32+19) /*S  WRMSR Non-Serialising */
CRUX_CPUFEATURE(AMX_FP16,     10*32+21) /*   AMX FP16 instruction */
CRUX_CPUFEATURE(AVX_IFMA,     10*32+23) /*A  AVX-IFMA Instructions */
CRUX_CPUFEATURE(LAM,          10*32+26) /*   Linear Address Masking */
CRUX_CPUFEATURE(MSRLIST,      10*32+27) /*   {RD,WR}MSRLIST instructions */
CRUX_CPUFEATURE(NO_INVD,      10*32+30) /*   INVD instruction unusable */

/* AMD-defined CPU features, CPUID level 0x80000021.eax, word 11 */
CRUX_CPUFEATURE(NO_NEST_BP,         11*32+ 0) /*A  No Nested Data Breakpoints */
CRUX_CPUFEATURE(FS_GS_NS,           11*32+ 1) /*S| FS/GS base MSRs non-serialising */
CRUX_CPUFEATURE(LFENCE_DISPATCH,    11*32+ 2) /*A  LFENCE always serializing */
CRUX_CPUFEATURE(VERW_CLEAR,         11*32+ 5) /*!A| VERW clears microarchitectural buffers */
CRUX_CPUFEATURE(NSCB,               11*32+ 6) /*A  Null Selector Clears Base (and limit too) */
CRUX_CPUFEATURE(AUTO_IBRS,          11*32+ 8) /*S  Automatic IBRS */
CRUX_CPUFEATURE(AMD_FSRS,           11*32+10) /*A  Fast Short REP STOSB */
CRUX_CPUFEATURE(AMD_FSRC,           11*32+11) /*A  Fast Short REP CMPSB */
CRUX_CPUFEATURE(CPUID_USER_DIS,     11*32+17) /*   CPUID disable for CPL > 0 software */
CRUX_CPUFEATURE(EPSF,               11*32+18) /*A  Enhanced Predictive Store Forwarding */
CRUX_CPUFEATURE(FSRSC,              11*32+19) /*A  Fast Short REP SCASB */
CRUX_CPUFEATURE(AMD_PREFETCHI,      11*32+20) /*A  PREFETCHIT{0,1} Instructions */
CRUX_CPUFEATURE(SBPB,               11*32+27) /*A  Selective Branch Predictor Barrier */
CRUX_CPUFEATURE(IBPB_BRTYPE,        11*32+28) /*A  IBPB flushes Branch Type predictions too */
CRUX_CPUFEATURE(SRSO_NO,            11*32+29) /*A  Hardware not vulnerable to Speculative Return Stack Overflow */
CRUX_CPUFEATURE(SRSO_US_NO,         11*32+30) /*A! Hardware not vulnerable to SRSO across the User/Supervisor boundary */
CRUX_CPUFEATURE(SRSO_MSR_FIX,       11*32+31) /*   MSR_BP_CFG.BP_SPEC_REDUCE available */

/* Intel-defined CPU features, CPUID level 0x00000007:1.ebx, word 12 */
CRUX_CPUFEATURE(INTEL_PPIN,         12*32+ 0) /*   Protected Processor Inventory Number */

/* Intel-defined CPU features, CPUID level 0x00000007:2.edx, word 13 */
CRUX_CPUFEATURE(INTEL_PSFD,         13*32+ 0) /*A  MSR_SPEC_CTRL.PSFD */
CRUX_CPUFEATURE(IPRED_CTRL,         13*32+ 1) /*S  MSR_SPEC_CTRL.IPRED_DIS_* */
CRUX_CPUFEATURE(RRSBA_CTRL,         13*32+ 2) /*S  MSR_SPEC_CTRL.RRSBA_DIS_* */
CRUX_CPUFEATURE(DDP_CTRL,           13*32+ 3) /*   MSR_SPEC_CTRL.DDP_DIS_U */
CRUX_CPUFEATURE(BHI_CTRL,           13*32+ 4) /*S  MSR_SPEC_CTRL.BHI_DIS_S */
CRUX_CPUFEATURE(MCDT_NO,            13*32+ 5) /*A  MCDT_NO */
CRUX_CPUFEATURE(UC_LOCK_DIS,        13*32+ 6) /*   UC-lock disable */

/* Intel-defined CPU features, CPUID level 0x00000007:1.ecx, word 14 */

/* Intel-defined CPU features, CPUID level 0x00000007:1.edx, word 15 */
CRUX_CPUFEATURE(AVX_VNNI_INT8,      15*32+ 4) /*A  AVX-VNNI-INT8 Instructions */
CRUX_CPUFEATURE(AVX_NE_CONVERT,     15*32+ 5) /*A  AVX-NE-CONVERT Instructions */
CRUX_CPUFEATURE(AMX_COMPLEX,        15*32+ 8) /*   AMX Complex Instructions */
CRUX_CPUFEATURE(AVX_VNNI_INT16,     15*32+10) /*A  AVX-VNNI-INT16 Instructions */
CRUX_CPUFEATURE(PREFETCHI,          15*32+14) /*A  PREFETCHIT{0,1} Instructions */
CRUX_CPUFEATURE(UIRET_UIF,          15*32+17) /*   UIRET updates UIF */
CRUX_CPUFEATURE(CET_SSS,            15*32+18) /*   CET Supervisor Shadow Stacks safe to use */
CRUX_CPUFEATURE(SLSM,               15*32+24) /*   Static Lockstep Mode */

/* Intel-defined CPU features, MSR_ARCH_CAPS 0x10a.eax, word 16 */
CRUX_CPUFEATURE(RDCL_NO,            16*32+ 0) /*A  No Rogue Data Cache Load (Meltdown) */
CRUX_CPUFEATURE(EIBRS,              16*32+ 1) /*A  Enhanced IBRS */
CRUX_CPUFEATURE(RSBA,               16*32+ 2) /*!  RSB Alternative (Retpoline not safe) */
CRUX_CPUFEATURE(SKIP_L1DFL,         16*32+ 3) /*   Don't need to flush L1D on VMEntry */
CRUX_CPUFEATURE(INTEL_SSB_NO,       16*32+ 4) /*A  No Speculative Store Bypass */
CRUX_CPUFEATURE(MDS_NO,             16*32+ 5) /*A  No Microarchitectural Data Sampling */
CRUX_CPUFEATURE(IF_PSCHANGE_MC_NO,  16*32+ 6) /*A  No Instruction fetch #MC */
CRUX_CPUFEATURE(TSX_CTRL,           16*32+ 7) /*   MSR_TSX_CTRL */
CRUX_CPUFEATURE(TAA_NO,             16*32+ 8) /*A  No TSX Async Abort */
CRUX_CPUFEATURE(MCU_CTRL,           16*32+ 9) /*   MSR_MCU_CTRL */
CRUX_CPUFEATURE(MISC_PKG_CTRL,      16*32+10) /*   MSR_MISC_PKG_CTRL */
CRUX_CPUFEATURE(ENERGY_FILTERING,   16*32+11) /*   MSR_MISC_PKG_CTRL.ENERGY_FILTERING */
CRUX_CPUFEATURE(DOITM,              16*32+12) /*   Data Operand Invariant Timing Mode */
CRUX_CPUFEATURE(SBDR_SSDP_NO,       16*32+13) /*A  No Shared Buffer Data Read or Sideband Stale Data Propagation */
CRUX_CPUFEATURE(FBSDP_NO,           16*32+14) /*A  No Fill Buffer Stale Data Propagation */
CRUX_CPUFEATURE(PSDP_NO,            16*32+15) /*A  No Primary Stale Data Propagation */
CRUX_CPUFEATURE(MCU_EXT,            16*32+16) /*   MCU_STATUS/ENUM MSRs */
CRUX_CPUFEATURE(FB_CLEAR,           16*32+17) /*!A| Fill Buffers cleared by VERW */
CRUX_CPUFEATURE(FB_CLEAR_CTRL,      16*32+18) /*   MSR_OPT_CPU_CTRL.FB_CLEAR_DIS */
CRUX_CPUFEATURE(RRSBA,              16*32+19) /*!  Restricted RSB Alternative */
CRUX_CPUFEATURE(BHI_NO,             16*32+20) /*A  No Branch History Injection  */
CRUX_CPUFEATURE(XAPIC_STATUS,       16*32+21) /*   MSR_XAPIC_DISABLE_STATUS */
CRUX_CPUFEATURE(OVRCLK_STATUS,      16*32+23) /*   MSR_OVERCLOCKING_STATUS */
CRUX_CPUFEATURE(PBRSB_NO,           16*32+24) /*A  No Post-Barrier RSB predictions */
CRUX_CPUFEATURE(GDS_CTRL,           16*32+25) /*   MCU_OPT_CTRL.GDS_MIT_{DIS,LOCK} */
CRUX_CPUFEATURE(GDS_NO,             16*32+26) /*A  No Gather Data Sampling */
CRUX_CPUFEATURE(RFDS_NO,            16*32+27) /*A  No Register File Data Sampling */
CRUX_CPUFEATURE(RFDS_CLEAR,         16*32+28) /*!A| Register File(s) cleared by VERW */
CRUX_CPUFEATURE(IGN_UMONITOR,       16*32+29) /*   MCU_OPT_CTRL.IGN_UMONITOR */
CRUX_CPUFEATURE(MON_UMON_MITG,      16*32+30) /*   MCU_OPT_CTRL.MON_UMON_MITG */

/* Intel-defined CPU features, MSR_ARCH_CAPS 0x10a.edx, word 17 (express in terms of word 16) */
CRUX_CPUFEATURE(PB_OPT_CTRL,        16*32+32) /*   MSR_PB_OPT_CTRL.IBPB_ALT */
CRUX_CPUFEATURE(ITS_NO,             16*32+62) /*!A No Indirect Target Selection */

/* AMD-defined CPU features, CPUID level 0x80000021.ecx, word 18 */
CRUX_CPUFEATURE(TSA_SQ_NO,          18*32+ 1) /*A  No Store Queue Transitive Scheduler Attacks */
CRUX_CPUFEATURE(TSA_L1_NO,          18*32+ 2) /*A  No L1D Transitive Scheduler Attacks */

#endif /* CRUX_CPUFEATURE */

/* Clean up from a default include.  Close the enum (for C). */
#ifdef CRUX_CPUFEATURESET_DEFAULT_INCLUDE
#undef CRUX_CPUFEATURESET_DEFAULT_INCLUDE
#undef CRUX_CPUFEATURE
};

#endif /* CRUX_CPUFEATURESET_DEFAULT_INCLUDE */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
