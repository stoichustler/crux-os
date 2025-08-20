/* added by alix for booting threadxen from xen */

#define PAGE_SHIFT              12

#define __HEAD_FLAG_PAGE_SIZE   ((PAGE_SHIFT - 10) / 2)

#define __HEAD_FLAG_PHYS_BASE   1

#define __HEAD_FLAGS            ((__HEAD_FLAG_PAGE_SIZE << 1) | \
                                 (__HEAD_FLAG_PHYS_BASE << 3))

.extern __top_of_ram

/*
 * Emit a 64-bit absolute little endian symbol reference in a way that
 * ensures that it will be resolved at build time, even when building a
 * PIE binary. This requires cooperation from the linker script, which
 * must emit the lo32/hi32 halves individually.
 */
	.section .pecoff, "ax", @progbits
.global _efi_head;
_efi_head:
	/*
	 * This add instruction has no meaningful effect except that
	 * its opcode forms the magic "MZ" signature required by UEFI.
	 */
	add	x13, x18, #0x16
	b el1_entry_aarch64
	.quad	0	// Image load offset from start of RAM, little-endian
	.quad	__top_of_ram - _efi_head        // Effective size of kernel image, little-endian
	.quad	__HEAD_FLAGS                    // Informative flags, little-endian
	.quad	0				                // reserved
	.quad	0				                // reserved
	.quad	0				                // reserved
	.ascii	"ARM\x64"			            // Magic number
	.long	_pe_header - _efi_head		    // Offset to the PE header.

	.align 3
_pe_header:
	.ascii  "PE"
	.short  0
coff_header:
	.short  0xaa64                          /* AArch64 */
	.short  2                               /* nr_sections */
	.long   0                               /* TimeDateStamp */
	.long   0                               /* PointerToSymbolTable */
	.long   1                               /* NumberOfSymbols */
	.short  section_table - optional_header /* SizeOfOptionalHeader */
	.short  0x206                           /* Characteristics. */
                                            /* IMAGE_FILE_DEBUG_STRIPPED | */
                                            /* IMAGE_FILE_EXECUTABLE_IMAGE | */
                                            /* IMAGE_FILE_LINE_NUMS_STRIPPED */
optional_header:
	.short  0x20b                           /* PE32+ format */
	.byte   0x02                            /* MajorLinkerVersion */
	.byte   0x14                            /* MinorLinkerVersion */
	.long   __top_of_ram - _pe_header_end   /* SizeOfCode */
	.long   0                               /* SizeOfInitializedData */
	.long   0                               /* SizeOfUninitializedData */
	.long   start64 - _efi_head             /* AddressOfEntryPoint */
	.long   _pe_header_end - _efi_head      /* BaseOfCode */

extra_header_fields:
	.quad   0                               /* ImageBase */
	.long   0x1000                          /* SectionAlignment (4 KByte) */
	.long   0x8                             /* FileAlignment */
	.short  0                               /* MajorOperatingSystemVersion */
	.short  0                               /* MinorOperatingSystemVersion */
	.short  0                               /* MajorImageVersion */
	.short  0                               /* MinorImageVersion */
	.short  0                               /* MajorSubsystemVersion */
	.short  0                               /* MinorSubsystemVersion */
	.long   0                               /* Win32VersionValue */

	.long   __top_of_ram - _efi_head        /* SizeOfImage */

	/* Everything before the kernel image is considered part of the header */
	.long   _pe_header_end - _efi_head      /* SizeOfHeaders */
	.long   0                               /* CheckSum */
	.short  0xa                             /* Subsystem (EFI application) */
	.short  0                               /* DllCharacteristics */
	.quad   0                               /* SizeOfStackReserve */
	.quad   0                               /* SizeOfStackCommit */
	.quad   0                               /* SizeOfHeapReserve */
	.quad   0                               /* SizeOfHeapCommit */
	.long   0                               /* LoaderFlags */
	.long   0x6                             /* NumberOfRvaAndSizes */

	.quad   0                               /* ExportTable */
	.quad   0                               /* ImportTable */
	.quad   0                               /* ResourceTable */
	.quad   0                               /* ExceptionTable */
	.quad   0                               /* CertificationTable */
	.quad   0                               /* BaseRelocationTable */

        /* Section table */
section_table:

	/*
	 * The EFI application loader requires a relocation section
	 * because EFI applications must be relocatable.  This is a
	 * dummy section as far as we are concerned.
	 */
	.ascii  ".reloc"
	.byte   0
	.byte   0                               /* end of 0 padding of section name */
	.long   0
	.long   0
	.long   0                               /* SizeOfRawData */
	.long   0                               /* PointerToRawData */
	.long   0                               /* PointerToRelocations */
	.long   0                               /* PointerToLineNumbers */
	.short  0                               /* NumberOfRelocations */
	.short  0                               /* NumberOfLineNumbers */
	.long   0x42100040                      /* Characteristics (section flags) */


	.ascii  ".text"
	.byte   0
	.byte   0
	.byte   0                               /* end of 0 padding of section name */
	.long   __top_of_ram - _pe_header_end   /* VirtualSize */
	.long   _pe_header_end - _efi_head      /* VirtualAddress */
	.long   __top_of_ram - _pe_header_end   /* SizeOfRawData */
	.long   _pe_header_end - _efi_head      /* PointerToRawData */

	.long   0                /* PointerToRelocations (0 for executables) */
	.long   0                /* PointerToLineNumbers (0 for executables) */
	.short  0                /* NumberOfRelocations  (0 for executables) */
	.short  0                /* NumberOfLineNumbers  (0 for executables) */
	.long   0xe0500020       /* Characteristics (section flags) */
_pe_header_end:
