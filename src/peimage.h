// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * EFI image format for PE32, PE32+ and TE. Please note some data structures
 * are different for PE32 and PE32+. efi_image_nt_headers32_t is for PE32 and
 * efi_image_nt_headers64_t is for PE32+.
 *
 * This file is coded to the Visual Studio, Microsoft Portable Executable and
 * Common Object File Format Specification, Revision 8.0 - May 16, 2006. This
 * file also includes some definitions in PI Specification, Revision 1.0.
 *
 * Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.
 * Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.
 */

#pragma once

#include <stdint.h>

#include "efivar/efivar-types.h"
#include "efivar/efisec-types.h"
#include "uchar.h"

#define SIGNATURE_16(A, B) \
	((uint16_t)(((uint16_t)(A)) | (((uint16_t)(B)) << ((uint16_t)8))))
#define SIGNATURE_32(A, B, C, D) \
	((uint32_t)(((uint32_t)SIGNATURE_16(A, B)) | \
		    (((uint32_t)SIGNATURE_16(C, D)) << (uint32_t)16)))
#define SIGNATURE_64(A, B, C, D, E, F, G, H) \
	((uint64_t)((uint64_t)SIGNATURE_32(A, B, C, D) | \
		    ((uint64_t)(SIGNATURE_32(E, F, G, H)) << (uint64_t)32)))

#define ALIGN_VALUE(Value, Alignment) ((Value) + (((Alignment) - (Value)) & ((Alignment) - 1)))
#define ALIGN_POINTER(Pointer, Alignment) ((VOID *) (ALIGN_VALUE ((uintptr_t)(Pointer), (Alignment))))

// Check if `val` is evenly aligned to the page size.
#define IS_PAGE_ALIGNED(val) (!((val) & EFI_PAGE_MASK))

/*
 * PE32+ Subsystem type for EFI images
 */
#define EFI_IMAGE_SUBSYSTEM_EFI_APPLICATION		10
#define EFI_IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER	11
#define EFI_IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER		12
#define EFI_IMAGE_SUBSYSTEM_SAL_RUNTIME_DRIVER		13 // defined PI Specification, 1.0


/*
 * PE32+ Machine type for EFI images
 */
#define IMAGE_FILE_MACHINE_I386			0x014c
#define IMAGE_FILE_MACHINE_IA64			0x0200
#define IMAGE_FILE_MACHINE_EBC			0x0EBC
#define IMAGE_FILE_MACHINE_X64			0x8664
#define IMAGE_FILE_MACHINE_ARMTHUMB_MIXED	0x01c2
#define IMAGE_FILE_MACHINE_ARM64		0xaa64

/*
 * EXE file formats
 */
#define EFI_IMAGE_DOS_SIGNATURE		SIGNATURE_16('M', 'Z')
#define EFI_IMAGE_OS2_SIGNATURE		SIGNATURE_16('N', 'E')
#define EFI_IMAGE_OS2_SIGNATURE_LE	SIGNATURE_16('L', 'E')
#define EFI_IMAGE_NT_SIGNATURE		SIGNATURE_32('P', 'E', '\0', '\0')

/*
 * PE images can start with an optional DOS header, so if an image is run
 * under DOS it can print an error message.
 */
typedef struct {
	uint16_t e_magic;	// Magic number.
	uint16_t e_cblp;	// Bytes on last page of file.
	uint16_t e_cp;		// Pages in file.
	uint16_t e_crlc;	// Relocations.
	uint16_t e_cparhdr;	// Size of header in paragraphs.
	uint16_t e_minalloc;	// Minimum extra paragraphs needed.
	uint16_t e_maxalloc;	// Maximum extra paragraphs needed.
	uint16_t e_ss;		// Initial (relative) SS value.
	uint16_t e_sp;		// Initial SP value.
	uint16_t e_csum;	// Checksum.
	uint16_t e_ip;		// Initial IP value.
	uint16_t e_cs;		// Initial (relative) CS value.
	uint16_t e_lfarlc;	// File address of relocation table.
	uint16_t e_ovno;	// Overlay number.
	uint16_t e_res[4];	// Reserved words.
	uint16_t e_oemid;	// OEM identifier (for e_oeminfo).
	uint16_t e_oeminfo;	// OEM information; e_oemid specific.
	uint16_t e_res2[10];	// Reserved words.
	uint32_t e_lfanew;	// File address of new exe header.
} efi_image_dos_header_t;

/*
 * COFF File Header (Object and Image).
 */
typedef struct {
	uint16_t machine;
	uint16_t number_of_sections;
	uint32_t time_date_stamp;
	uint32_t pointer_to_symbol_table;
	uint32_t number_of_symbols;
	uint16_t size_of_optional_header;
	uint16_t characteristics;
} efi_image_file_header_t;

/*
 * Size of efi_image_file_header_t.
 */
#define EFI_IMAGE_SIZEOF_FILE_HEADER	20

/*
 * characteristics
 */
#define EFI_IMAGE_FILE_RELOCS_STRIPPED		(1 << 0)  // 0x0001 Relocation info stripped from file.
#define EFI_IMAGE_FILE_EXECUTABLE_IMAGE		(1 << 1)  // 0x0002 File is executable  (i.e. no unresolved externel references).
#define EFI_IMAGE_FILE_LINE_NUMS_STRIPPED	(1 << 2)  // 0x0004 Line nunbers stripped from file.
#define EFI_IMAGE_FILE_LOCAL_SYMS_STRIPPED	(1 << 3)  // 0x0008 Local symbols stripped from file.
#define EFI_IMAGE_FILE_BYTES_REVERSED_LO	(1 << 7)  // 0x0080 Bytes of machine word are reversed.
#define EFI_IMAGE_FILE_32BIT_MACHINE		(1 << 8)  // 0x0100 32 bit word machine.
#define EFI_IMAGE_FILE_DEBUG_STRIPPED		(1 << 9)  // 0x0200 Debugging info stripped from file in .DBG file.
#define EFI_IMAGE_FILE_SYSTEM			(1 << 12) // 0x1000 system File.
#define EFI_IMAGE_FILE_DLL			(1 << 13) // 0x2000 File is a DLL.
#define EFI_IMAGE_FILE_BYTES_REVERSED_HI	(1 << 15) // 0x8000 Bytes of machine word are reversed.

/*
 * Header Data Directories.
 */
typedef struct {
	uint32_t virtual_address;
	uint32_t size;
} efi_image_data_directory_t;

/*
 * Directory Entries
 */
#define EFI_IMAGE_DIRECTORY_ENTRY_EXPORT	0
#define EFI_IMAGE_DIRECTORY_ENTRY_IMPORT	1
#define EFI_IMAGE_DIRECTORY_ENTRY_RESOURCE	2
#define EFI_IMAGE_DIRECTORY_ENTRY_EXCEPTION	3
#define EFI_IMAGE_DIRECTORY_ENTRY_SECURITY	4
#define EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC	5
#define EFI_IMAGE_DIRECTORY_ENTRY_DEBUG		6
#define EFI_IMAGE_DIRECTORY_ENTRY_COPYRIGHT	7
#define EFI_IMAGE_DIRECTORY_ENTRY_GLOBALPTR	8
#define EFI_IMAGE_DIRECTORY_ENTRY_TLS		9
#define EFI_IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG	10

#define EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES	16

/*
 * @attention
 * EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC means PE32 and
 * efi_image_optional_header32_t must be used. The data structures only vary
 * after NT additional fields.
 */
#define EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC 0x10b

/*
 * Optional Header Standard Fields for PE32.
 */
typedef struct {
	/*
	 * Standard fields.
	 */
	uint16_t magic;
	uint8_t major_linker_version;
	uint8_t minor_linker_version;
	uint32_t size_of_code;
	uint32_t size_of_initialized_data;
	uint32_t size_of_uninitialized_data;
	uint32_t address_of_entry_point;
	uint32_t base_of_code;
	uint32_t base_of_data;	// PE32 contains this additional field, which is absent in PE32+.
	/*
	 * Optional Header Windows-Specific Fields.
	 */
	uint32_t image_base;
	uint32_t section_alignment;
	uint32_t file_alignment;
	uint16_t major_operating_system_version;
	uint16_t minor_operating_system_version;
	uint16_t major_image_version;;
	uint16_t minor_image_version;;
	uint16_t major_subsystem_version;
	uint16_t minor_subsystem_version;
	uint32_t win32_version_value;
	uint32_t size_of_image;
	uint32_t size_of_headers;
	uint32_t checksum;
	uint16_t subsystem;
	uint16_t dll_characteristics;
	uint32_t size_of_stack_reserve;
	uint32_t size_of_stack_commit;
	uint32_t size_of_heap_reserve;
	uint32_t size_of_heap_commit;
	uint32_t loader_flags;
	uint32_t number_of_rva_and_sizes;
	efi_image_data_directory_t data_directory[EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES];
} efi_image_optional_header32_t;

/*
 * @attention
 * EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC means PE32+ and
 * efi_image_optional_header64_t must be used. The data structures only vary
 * after NT additional fields.
 */
#define EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC 0x20b

/*
 * Optional Header Standard Fields for PE32+.
 */
typedef struct {
	/*
	 * Standard fields.
	 */
	uint16_t magic;
	uint8_t major_linker_version;
	uint8_t minor_linker_version;
	uint32_t size_of_code;
	uint32_t size_of_initialized_data;
	uint32_t size_of_uninitialized_data;
	uint32_t address_of_entry_point;
	uint32_t base_of_code;
	/*
	 * Optional Header Windows-Specific Fields.
	 */
	uint64_t image_base;
	uint32_t section_alignment;
	uint32_t file_alignment;
	uint16_t major_operating_system_version;
	uint16_t minor_operating_system_version;
	uint16_t major_image_version;;
	uint16_t minor_image_version;;
	uint16_t major_subsystem_version;
	uint16_t minor_subsystem_version;
	uint32_t win32_version_value;
	uint32_t size_of_image;
	uint32_t size_of_headers;
	uint32_t checksum;
	uint16_t subsystem;
	uint16_t dll_characteristics;
	uint64_t size_of_stack_reserve;
	uint64_t size_of_stack_commit;
	uint64_t size_of_heap_reserve;
	uint64_t size_of_heap_commit;
	uint32_t loader_flags;
	uint32_t number_of_rva_and_sizes;
	efi_image_data_directory_t data_directory[EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES];
} efi_image_optional_header64_t;

#define EFI_IMAGE_DLLCHARACTERISTICS_RESERVED_0001		0x0001
#define EFI_IMAGE_DLLCHARACTERISTICS_RESERVED_0002		0x0002
#define EFI_IMAGE_DLLCHARACTERISTICS_RESERVED_0004		0x0004
#define EFI_IMAGE_DLLCHARACTERISTICS_RESERVED_0008		0x0008
#if 0 /* This is not in the PE spec. */
#define EFI_IMAGE_DLLCHARACTERISTICS_RESERVED_0010		0x0010
#endif
#define EFI_IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA		0x0020
#define EFI_IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE		0x0040
#define EFI_IMAGE_DLLCHARACTERISTICS_FORCE_INTEGRITY		0x0080
#define EFI_IMAGE_DLLCHARACTERISTICS_NX_COMPAT			0x0100
#define EFI_IMAGE_DLLCHARACTERISTICS_NO_ISOLATION		0x0200
#define EFI_IMAGE_DLLCHARACTERISTICS_NO_SEH			0x0400
#define EFI_IMAGE_DLLCHARACTERISTICS_NO_BIND			0x0800
#define EFI_IMAGE_DLLCHARACTERISTICS_APPCONTAINER		0x1000
#define EFI_IMAGE_DLLCHARACTERISTICS_WDM_DRIVER			0x2000
#define EFI_IMAGE_DLLCHARACTERISTICS_GUARD_CF			0x4000
#define EFI_IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE	0x8000

/*
 * @attention
 * efi_image_nt_headers32_t is for use ONLY by tools.
 */
typedef struct {
	uint32_t signature;
	efi_image_file_header_t file_header;
	efi_image_optional_header32_t optional_header;
} efi_image_nt_headers32_t;

#define EFI_IMAGE_SIZEOF_NT_OPTIONAL32_HEADER sizeof (efi_image_nt_headers32_t)

/*
 * @attention
 * efi_image_nt_headers64_t is for use ONLY by tools.
 */
typedef struct {
	uint32_t signature;
	efi_image_file_header_t file_header;
	efi_image_optional_header64_t optional_header;
} efi_image_nt_headers64_t;

#define EFI_IMAGE_SIZEOF_NT_OPTIONAL64_HEADER sizeof (efi_image_nt_headers64_t)

/*
 * Other Windows Subsystem Values
 */
#define EFI_IMAGE_SUBSYSTEM_UNKNOWN	0
#define EFI_IMAGE_SUBSYSTEM_NATIVE	1
#define EFI_IMAGE_SUBSYSTEM_WINDOWS_GUI	2
#define EFI_IMAGE_SUBSYSTEM_WINDOWS_CUI	3
#define EFI_IMAGE_SUBSYSTEM_OS2_CUI	5
#define EFI_IMAGE_SUBSYSTEM_POSIX_CUI	7

/*
 * Length of ShortName.
 */
#define EFI_IMAGE_SIZEOF_SHORT_NAME	8

/*
 * Section Table. This table immediately follows the optional header.
 */
typedef struct {
	uint8_t name[EFI_IMAGE_SIZEOF_SHORT_NAME];
	union {
		uint32_t physical_address;
		uint32_t virtual_size;
	} misc;
	uint32_t virtual_address;
	uint32_t size_of_raw_data;
	uint32_t pointer_to_raw_data;
	uint32_t pointer_to_relocations;
	uint32_t pointer_to_linenumbers;
	uint16_t number_of_relocations;
	uint16_t number_of_linenumbers;
	uint32_t characteristics;
} efi_image_section_header_t;

/*
 * Size of efi_image_section_header_t.
 */
#define EFI_IMAGE_SIZEOF_SECTION_HEADER	40

/*
 * Section Flags Values
 */
#define EFI_IMAGE_SCN_RESERVED_00000000		0x00000000
#define EFI_IMAGE_SCN_RESERVED_00000001		0x00000001
#define EFI_IMAGE_SCN_RESERVED_00000002		0x00000002
#define EFI_IMAGE_SCN_RESERVED_00000004		0x00000004
#define EFI_IMAGE_SCN_TYPE_NO_PAD		0x00000008
#define EFI_IMAGE_SCN_RESERVED_00000010		0x00000010
#define EFI_IMAGE_SCN_CNT_CODE			0x00000020
#define EFI_IMAGE_SCN_CNT_INITIALIZED_DATA	0x00000040
#define EFI_IMAGE_SCN_CNT_UNINITIALIZED_DATA	0x00000080
#define EFI_IMAGE_SCN_LNK_OTHER			0x00000100
#define EFI_IMAGE_SCN_LNK_INFO			0x00000200
#define EFI_IMAGE_SCN_RESERVED_00000400		0x00000400
#define EFI_IMAGE_SCN_LNK_REMOVE		0x00000800
#define EFI_IMAGE_SCN_LNK_COMDAT		0x00001000
#define EFI_IMAGE_SCN_RESERVED_00002000		0x00002000
#define EFI_IMAGE_SCN_RESERVED_00004000		0x00004000
#define EFI_IMAGE_SCN_GPREL			0x00008000
/*
 * PE 9.3 says both IMAGE_SCN_MEM_PURGEABLE and IMAGE_SCN_MEM_16BIT are
 * 0x00020000, but I think it's wrong. --pjones
 */
#define EFI_IMAGE_SCN_MEM_PURGEABLE		0x00010000 // "Reserved for future use."
#define EFI_IMAGE_SCN_MEM_16BIT			0x00020000 // "Reserved for future use."
#define EFI_IMAGE_SCN_MEM_LOCKED		0x00040000 // "Reserved for future use."
#define EFI_IMAGE_SCN_MEM_PRELOAD		0x00080000 // "Reserved for future use."
#define EFI_IMAGE_SCN_ALIGN_1BYTES		0x00100000
#define EFI_IMAGE_SCN_ALIGN_2BYTES		0x00200000
#define EFI_IMAGE_SCN_ALIGN_4BYTES		0x00300000
#define EFI_IMAGE_SCN_ALIGN_8BYTES		0x00400000
#define EFI_IMAGE_SCN_ALIGN_16BYTES		0x00500000
#define EFI_IMAGE_SCN_ALIGN_32BYTES		0x00600000
#define EFI_IMAGE_SCN_ALIGN_64BYTES		0x00700000
#define EFI_IMAGE_SCN_ALIGN_128BYTES		0x00800000
#define EFI_IMAGE_SCN_ALIGN_256BYTES		0x00900000
#define EFI_IMAGE_SCN_ALIGN_512BYTES		0x00a00000
#define EFI_IMAGE_SCN_ALIGN_1024BYTES		0x00b00000
#define EFI_IMAGE_SCN_ALIGN_2048BYTES		0x00c00000
#define EFI_IMAGE_SCN_ALIGN_4096BYTES		0x00d00000
#define EFI_IMAGE_SCN_ALIGN_8192BYTES		0x00e00000
#define EFI_IMAGE_SCN_LNK_NRELOC_OVFL		0x01000000
#define EFI_IMAGE_SCN_MEM_DISCARDABLE		0x02000000
#define EFI_IMAGE_SCN_MEM_NOT_CACHED		0x04000000
#define EFI_IMAGE_SCN_MEM_NOT_PAGED		0x08000000
#define EFI_IMAGE_SCN_MEM_SHARED		0x10000000
#define EFI_IMAGE_SCN_MEM_EXECUTE		0x20000000
#define EFI_IMAGE_SCN_MEM_READ			0x40000000
#define EFI_IMAGE_SCN_MEM_WRITE			0x80000000

/*
 * Size of a Symbol Table Record.
 */
#define EFI_IMAGE_SIZEOF_SYMBOL	18

/*
 * Symbols have a section number of the section in which they are
 * defined. Otherwise, section numbers have the following meanings:
 */
#define EFI_IMAGE_SYM_UNDEFINED	(uint16_t) 0  // Symbol is undefined or is common.
#define EFI_IMAGE_SYM_ABSOLUTE	(uint16_t) -1 // Symbol is an absolute value.
#define EFI_IMAGE_SYM_DEBUG	(uint16_t) -2 // Symbol is a special debug item.

/*
 * Symbol Type (fundamental) values.
 */
#define EFI_IMAGE_SYM_TYPE_NULL		0  // no type.
#define EFI_IMAGE_SYM_TYPE_VOID		1  // no valid type.
#define EFI_IMAGE_SYM_TYPE_CHAR		2  // type character.
#define EFI_IMAGE_SYM_TYPE_SHORT	3  // type short integer.
#define EFI_IMAGE_SYM_TYPE_INT		4
#define EFI_IMAGE_SYM_TYPE_LONG		5
#define EFI_IMAGE_SYM_TYPE_FLOAT	6
#define EFI_IMAGE_SYM_TYPE_DOUBLE	7
#define EFI_IMAGE_SYM_TYPE_STRUCT	8
#define EFI_IMAGE_SYM_TYPE_UNION	9
#define EFI_IMAGE_SYM_TYPE_ENUM		10 // enumeration.
#define EFI_IMAGE_SYM_TYPE_MOE		11 // member of enumeration.
#define EFI_IMAGE_SYM_TYPE_BYTE		12
#define EFI_IMAGE_SYM_TYPE_WORD		13
#define EFI_IMAGE_SYM_TYPE_UINT		14
#define EFI_IMAGE_SYM_TYPE_DWORD	15

/*
 * Symbol Type (derived) values.
 */
#define EFI_IMAGE_SYM_DTYPE_NULL	0 // no derived type.
#define EFI_IMAGE_SYM_DTYPE_POINTER	1
#define EFI_IMAGE_SYM_DTYPE_FUNCTION	2
#define EFI_IMAGE_SYM_DTYPE_ARRAY	3

/*
 * Storage classes.
 */
#define EFI_IMAGE_SYM_CLASS_END_OF_FUNCTION	((uint8_t) -1)
#define EFI_IMAGE_SYM_CLASS_NULL		0
#define EFI_IMAGE_SYM_CLASS_AUTOMATIC		1
#define EFI_IMAGE_SYM_CLASS_EXTERNAL		2
#define EFI_IMAGE_SYM_CLASS_STATIC		3
#define EFI_IMAGE_SYM_CLASS_REGISTER		4
#define EFI_IMAGE_SYM_CLASS_EXTERNAL_DEF	5
#define EFI_IMAGE_SYM_CLASS_LABEL		6
#define EFI_IMAGE_SYM_CLASS_UNDEFINED_LABEL	7
#define EFI_IMAGE_SYM_CLASS_MEMBER_OF_STRUCT	8
#define EFI_IMAGE_SYM_CLASS_ARGUMENT		9
#define EFI_IMAGE_SYM_CLASS_STRUCT_TAG		10
#define EFI_IMAGE_SYM_CLASS_MEMBER_OF_UNION	11
#define EFI_IMAGE_SYM_CLASS_UNION_TAG		12
#define EFI_IMAGE_SYM_CLASS_TYPE_DEFINITION	13
#define EFI_IMAGE_SYM_CLASS_UNDEFINED_STATIC	14
#define EFI_IMAGE_SYM_CLASS_ENUM_TAG		15
#define EFI_IMAGE_SYM_CLASS_MEMBER_OF_ENUM	16
#define EFI_IMAGE_SYM_CLASS_REGISTER_PARAM	17
#define EFI_IMAGE_SYM_CLASS_BIT_FIELD		18
#define EFI_IMAGE_SYM_CLASS_BLOCK		100
#define EFI_IMAGE_SYM_CLASS_FUNCTION		101
#define EFI_IMAGE_SYM_CLASS_END_OF_STRUCT	102
#define EFI_IMAGE_SYM_CLASS_FILE		103
#define EFI_IMAGE_SYM_CLASS_SECTION		104
#define EFI_IMAGE_SYM_CLASS_WEAK_EXTERNAL	105

/*
 * type packing constants
 */
#define EFI_IMAGE_N_BTMASK	017
#define EFI_IMAGE_N_TMASK	060
#define EFI_IMAGE_N_TMASK1	0300
#define EFI_IMAGE_N_TMASK2	0360
#define EFI_IMAGE_N_BTSHFT	4
#define EFI_IMAGE_N_TSHIFT	2

/*
 * Communal selection types.
 */
#define EFI_IMAGE_COMDAT_SELECT_NODUPLICATES	1
#define EFI_IMAGE_COMDAT_SELECT_ANY		2
#define EFI_IMAGE_COMDAT_SELECT_SAME_SIZE	3
#define EFI_IMAGE_COMDAT_SELECT_EXACT_MATCH	4
#define EFI_IMAGE_COMDAT_SELECT_ASSOCIATIVE	5

/*
 * "the following values only be referred in PeCoff, not defined in PECOFF."
 */
#define EFI_IMAGE_WEAK_EXTERN_SEARCH_NOLIBRARY	1
#define EFI_IMAGE_WEAK_EXTERN_SEARCH_LIBRARY	2
#define EFI_IMAGE_WEAK_EXTERN_SEARCH_ALIAS	3

/*
 * Relocation format.
 */
typedef struct {
	uint32_t virtual_address;
	uint32_t symbol_table_index;
	uint16_t type;
} efi_image_relocation_t;

/*
 * Size of efi_image_relocation_t
 */
#define EFI_IMAGE_SIZEOF_RELOCATION 10

/*
 * I386 relocation types.
 */
#define EFI_IMAGE_REL_I386_ABSOLUTE	0x0000 // Reference is absolute, no relocation is necessary.
#define EFI_IMAGE_REL_I386_DIR16	0x0001 // Direct 16-bit reference to the symbols virtual address.
#define EFI_IMAGE_REL_I386_REL16	0x0002 // PC-relative 16-bit reference to the symbols virtual address.
#define EFI_IMAGE_REL_I386_DIR32	0x0006 // Direct 32-bit reference to the symbols virtual address.
#define EFI_IMAGE_REL_I386_DIR32NB	0x0007 // Direct 32-bit reference to the symbols virtual address, base not included.
#define EFI_IMAGE_REL_I386_SEG12	0x0009 // Direct 16-bit reference to the segment-selector bits of a 32-bit virtual address.
#define EFI_IMAGE_REL_I386_SECTION	0x000A
#define EFI_IMAGE_REL_I386_SECREL	0x000B
#define EFI_IMAGE_REL_I386_REL32	0x0014 // PC-relative 32-bit reference to the symbols virtual address.

/*
 * x64 processor relocation types.
 */
#define IMAGE_REL_AMD64_ABSOLUTE	0x0000
#define IMAGE_REL_AMD64_ADDR64		0x0001
#define IMAGE_REL_AMD64_ADDR32		0x0002
#define IMAGE_REL_AMD64_ADDR32NB	0x0003
#define IMAGE_REL_AMD64_REL32		0x0004
#define IMAGE_REL_AMD64_REL32_1		0x0005
#define IMAGE_REL_AMD64_REL32_2		0x0006
#define IMAGE_REL_AMD64_REL32_3		0x0007
#define IMAGE_REL_AMD64_REL32_4		0x0008
#define IMAGE_REL_AMD64_REL32_5		0x0009
#define IMAGE_REL_AMD64_SECTION		0x000A
#define IMAGE_REL_AMD64_SECREL		0x000B
#define IMAGE_REL_AMD64_SECREL7		0x000C
#define IMAGE_REL_AMD64_TOKEN		0x000D
#define IMAGE_REL_AMD64_SREL32		0x000E
#define IMAGE_REL_AMD64_PAIR		0x000F
#define IMAGE_REL_AMD64_SSPAN32		0x0010

/*
 * Based relocation format.
 */
typedef struct {
	uint32_t virtual_address;
	uint32_t size_of_block;
} efi_image_base_relocation_t;

/*
 * Size of efi_image_base_relocation_t.
 */
#define EFI_IMAGE_SIZEOF_BASE_RELOCATION	8

/*
 * Based relocation types.
 */
#define EFI_IMAGE_REL_BASED_ABSOLUTE		0
#define EFI_IMAGE_REL_BASED_HIGH		1
#define EFI_IMAGE_REL_BASED_LOW			2
#define EFI_IMAGE_REL_BASED_HIGHLOW		3
#define EFI_IMAGE_REL_BASED_HIGHADJ		4
#define EFI_IMAGE_REL_BASED_MIPS_JMPADDR	5
#define EFI_IMAGE_REL_BASED_ARM_MOV32A		5
#define EFI_IMAGE_REL_BASED_ARM_MOV32T		7
#define EFI_IMAGE_REL_BASED_IA64_IMM64		9
#define EFI_IMAGE_REL_BASED_MIPS_JMPADDR16	9
#define EFI_IMAGE_REL_BASED_DIR64		10

/*
 * Line number format.
 */
typedef struct {
	union {
		uint32_t symbol_table_index;	// Symbol table index of function name if Linenumber is 0.
		uint32_t virtual_address;	// Virtual address of line number.
	} type;
	uint16_t linenumber;
} efi_image_linenumber_t;

/*
 * Size of efi_image_linenumber_t.
 */
#define EFI_IMAGE_SIZEOF_LINENUMBER	6

/*
 * Archive format.
 */
#define EFI_IMAGE_ARCHIVE_START_SIZE		8
#define EFI_IMAGE_ARCHIVE_START			"!<arch>\n"
#define EFI_IMAGE_ARCHIVE_END			"`\n"
#define EFI_IMAGE_ARCHIVE_PAD			"\n"
#define EFI_IMAGE_ARCHIVE_LINKER_MEMBER		"/               "
#define EFI_IMAGE_ARCHIVE_LONGNAMES_MEMBER	"//              "

/*
 * Archive Member Headers
 */
typedef struct {
	uint8_t name[16];	// File member name - `/' terminated.
	uint8_t date[12];	// File member date - decimal.
	uint8_t user_id[6];	// File member user id - decimal.
	uint8_t group_id[6];	// File member group id - decimal.
	uint8_t mode[8];	// File member mode - octal.
	uint8_t size[10];	// File member size - decimal.
	uint8_t end_header[2];	// String to end header. (0x60 0x0A).
} efi_image_archive_member_header_t;

/*
 *
 * Size of efi_image_archive_member_header_t.
 *
 */
#define EFI_IMAGE_SIZEOF_ARCHIVE_MEMBER_HDR 60


/**************************
 * DLL Support
 *************************/

/*
 * Export Directory Table.
 */
typedef struct {
	uint32_t characteristics;
	uint32_t time_date_stamp;
	uint16_t major_version;
	uint16_t minor_version;
	uint32_t name;
	uint32_t base;
	uint32_t number_of_functions;
	uint32_t number_of_names;
	uint32_t address_of_functions;
	uint32_t address_of_names;
	uint32_t address_of_name_ordinals;
} efi_image_export_directory_t;

/*
 * Hint/Name Table.
 */
typedef struct {
	uint16_t hint;
	uint8_t name[1];
} efi_image_import_by_name_t;

/*
 * Import Address Table rva (Thunk Table).
 */
typedef struct {
	union {
		uint32_t function;
		uint32_t ordinal;
		efi_image_import_by_name_t *address_of_data;
	} u1;
} efi_image_thunk_data_t;

#define EFI_IMAGE_ORDINAL_FLAG			BIT31 // Flag for PE32.
#define EFI_IMAGE_SNAP_BY_ORDINAL(Ordinal)	((Ordinal & EFI_IMAGE_ORDINAL_FLAG) != 0)
#define EFI_IMAGE_ORDINAL(Ordinal)		(Ordinal & 0xffff)

/*
 * Import Directory Table
 */
typedef struct {
	uint32_t characteristics;
	uint32_t time_date_stamp;
	uint32_t forwarder_chain;
	uint32_t name;
	efi_image_thunk_data_t *first_thunk;
} efi_image_import_descriptor_t;

/*
 * Debug Directory Format.
 */
typedef struct {
	uint32_t characteristics;
	uint32_t time_date_stamp;
	uint16_t major_version;
	uint16_t minor_version;
	uint32_t type;
	uint32_t size_of_data;
	uint32_t rva;		// The address of the debug data when loaded, relative to the image base.
	uint32_t file_offset;	// The file pointer to the debug data.
} efi_image_debug_directory_entry_t;

#define EFI_IMAGE_DEBUG_TYPE_CODEVIEW 2	// The Visual C++ debug information.

/*
 * Debug Data Structure defined in Microsoft C++.
 */
#define CODEVIEW_SIGNATURE_NB10 SIGNATURE_32('N', 'B', '1', '0')
typedef struct {
	uint32_t signature;	// "NB10"
	uint32_t unknown;
	uint32_t unknown2;
	uint32_t unknown3;
	/*
	 * Filename of .PDB goes here
	 */
} efi_image_debug_codeview_nb10_entry_t;

/*
 * Debug Data Structure defined in Microsoft C++.
 */
#define CODEVIEW_SIGNATURE_RSDS SIGNATURE_32('R', 'S', 'D', 'S')
typedef struct {
	uint32_t signature;	// "RSDS".
	uint32_t unknown;
	uint32_t unknown2;
	uint32_t unknown3;
	uint32_t unknown4;
	uint32_t unknown5;
	/*
	 * Filename of .PDB goes here
	 */
} efi_image_debug_codeview_rsds_entry_t;


/*
 * Debug Data Structure defined by Apple Mach-O to Coff utility.
 */
#define CODEVIEW_SIGNATURE_MTOC SIGNATURE_32('M', 'T', 'O', 'C')
typedef struct {
	uint32_t signature;	// "MTOC".
	efi_guid_t mach_o_uuid;
	/*
	 * Filename of .DLL (Mach-O with debug info) goes here
	 */
} efi_image_debug_codeview_mtoc_entry_t;

/*
 *
 * Resource format.
 *
 */
typedef struct {
	uint32_t characteristics;
	uint32_t time_date_stamp;
	uint16_t major_version;
	uint16_t minor_version;
	uint16_t number_of_named_entities;
	uint16_t number_of_id_entries;
	/*
	 * Array of efi_image_resource_directory_entry_t entries goes here.
	 */
} efi_image_resource_directory_t;

/*
 * Resource directory entry format.
 */
typedef struct {
	union {
		struct {
			uint32_t name_offset:31;
			uint32_t name_is_string:1;
		} s;
		uint32_t id;
	} u1;
	union {
		uint32_t offset_to_data;
		struct {
			uint32_t offset_to_directory:31;
			uint32_t data_is_directory:1;
		} s;
	} u2;
} efi_image_resource_directory_entry_t;

/*
 * Resource directory entry for string.
 */
typedef struct {
	uint16_t length;
	char16_t string[1];
} efi_image_resource_directory_string_t;

/*
 * Resource directory entry for data array.
 */
typedef struct {
	uint32_t offset_to_data;
	uint32_t size;
	uint32_t code_page;
	uint32_t reserved;
} efi_image_resource_data_entry_t;

/*
 * Header format for TE images, defined in the PI Specification, 1.0.
 */
typedef struct {
	uint16_t signature;		// The signature for TE format = "VZ".
	uint16_t machine;		// From the original file header.
	uint8_t number_of_ections;	// From the original file header.
	uint8_t subsystem;		// From original optional header.
	uint16_t stripped_size;		// Number of bytes we removed from the header.
	uint32_t address_of_entry_point;// Offset to entry point -- from original optional header.
	uint32_t base_of_code;		// From original image -- required for ITP debug.
	uint64_t image_base;		// From original file header.
	efi_image_data_directory_t data_directory[2]; // Only base relocation and debug directory.
} efi_te_image_header_t;


#define EFI_TE_IMAGE_HEADER_T_SIGNATURE SIGNATURE_16('V', 'Z')

/*
 * Data directory indexes in our TE image header
 */
#define EFI_TE_IMAGE_DIRECTORY_ENTRY_BASERELOC	0
#define EFI_TE_IMAGE_DIRECTORY_ENTRY_DEBUG	1

/*
 *
 * Union of PE32, PE32+, and TE headers.
 */
typedef union {
	efi_image_nt_headers32_t pe32;
	efi_image_nt_headers64_t pe32plus;
	efi_te_image_header_t te;
} efi_image_optional_header_union_t;

typedef union {
	efi_image_nt_headers32_t *pe32;
	efi_image_nt_headers64_t *pe32plus;
	efi_te_image_header_t *te;
	efi_image_optional_header_union_t *header_union;
} efi_image_optional_header_ptr_union_t;

typedef struct {
	win_certificate_header_t hdr;
	uint8_t cert_data[1];
} win_certificate_efi_pkcs_t;

// vim:fenc=utf-8:tw=75:noet
