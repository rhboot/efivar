/*
 * gpt.[ch]
 * Copyright (C) 2000-2001 Dell Computer Corporation <Matt_Domsch@dell.com>
 *
 * EFI GUID Partition Table handling
 * Per Intel EFI Specification v1.02
 * http://developer.intel.com/technology/efi/efi.htm
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _EFIBOOT_GPT_H
#define _EFIBOOT_GPT_H

#include <inttypes.h>

#define EFI_PMBR_OSTYPE_EFI 0xEF
#define EFI_PMBR_OSTYPE_EFI_GPT 0xEE
#define MSDOS_MBR_SIGNATURE 0xaa55
#define GPT_BLOCK_SIZE 512

#define GPT_HEADER_SIGNATURE ((uint64_t)(0x5452415020494645ULL))
#define GPT_HEADER_REVISION_V1_02 0x00010200
#define GPT_HEADER_REVISION_V1_00 0x00010000
#define GPT_HEADER_REVISION_V0_99 0x00009900
#define GPT_PRIMARY_PARTITION_TABLE_LBA 1

#define PARTITION_SYSTEM_GUID                           \
        EFI_GUID(0xC12A7328, 0xF81F, 0x11d2, 0xBA4B,    \
                 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B)
#define LEGACY_MBR_PARTITION_GUID                       \
        EFI_GUID(0x024DEE41, 0x33E7, 0x11d3, 0x9D69,    \
                 0x00, 0x08, 0xC7, 0x81, 0xF3, 0x9F)
#define PARTITION_MSFT_RESERVED_GUID                    \
        EFI_GUID(0xE3C9E316, 0x0B5C, 0x4DB8, 0x817D,    \
                 0xF9, 0x2D, 0xF0, 0x02, 0x15, 0xAE)
#define PARTITION_BASIC_DATA_GUID                       \
        EFI_GUID(0xEBD0A0A2, 0xB9E5, 0x4433, 0x87C0,    \
                 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7)
#define PARTITION_LINUX_RAID_GUID                       \
        EFI_GUID(0xa19d880f, 0x05fc, 0x4d3b, 0xA006,    \
                 0x74, 0x3f, 0x0f, 0x84, 0x91, 0x1e)
#define PARTITION_LINUX_SWAP_GUID                       \
        EFI_GUID(0x0657fd6d, 0xa4ab, 0x43c4, 0x84E5,    \
                 0x09, 0x33, 0xc8, 0x4b, 0x4f, 0x4f)
#define PARTITION_LINUX_LVM_GUID                        \
        EFI_GUID(0xe6d6d379, 0xf507, 0x44c2, 0xa23c,    \
                 0x23, 0x8f, 0x2a, 0x3d, 0xf9, 0x28)

typedef struct _gpt_header {
	uint64_t signature;
	uint32_t revision;
	uint32_t header_size;
	uint32_t header_crc32;
	uint32_t reserved1;
	uint64_t my_lba;
	uint64_t alternate_lba;
	uint64_t first_usable_lba;
	uint64_t last_usable_lba;
	efi_guid_t disk_guid;
	uint64_t partition_entry_lba;
	uint32_t num_partition_entries;
	uint32_t sizeof_partition_entry;
	uint32_t partition_entry_array_crc32;
	uint8_t reserved2[GPT_BLOCK_SIZE - 92];
} PACKED gpt_header;

typedef struct _gpt_entry_attributes {
	uint64_t required_to_function:1;
	uint64_t reserved:47;
        uint64_t type_guid_specific:16;
} PACKED gpt_entry_attributes;

typedef struct _gpt_entry {
	efi_guid_t partition_type_guid;
	efi_guid_t unique_partition_guid;
	uint64_t starting_lba;
	uint64_t ending_lba;
	gpt_entry_attributes attributes;
	uint16_t partition_name[72 / sizeof(uint16_t)];
} PACKED gpt_entry;

/*
 * These values are only defaults.  The actual on-disk structures
 * may define different sizes, so use those unless creating a new GPT disk!
 */
#define GPT_DEFAULT_RESERVED_PARTITION_ENTRY_ARRAY_SIZE 16384

/*
 * Number of actual partition entries should be calculated as:
 */
#define GPT_DEFAULT_RESERVED_PARTITION_ENTRIES \
        (GPT_DEFAULT_RESERVED_PARTITION_ENTRY_ARRAY_SIZE / \
         sizeof(gpt_entry))

typedef struct _partition_record {
	uint8_t boot_indicator; /* Not used by EFI firmware. Set to 0x80 to indicate that this
                                   is the bootable legacy partition. */
	uint8_t start_head;     /* Start of partition in CHS address, not used by EFI firmware. */
	uint8_t start_sector;   /* Start of partition in CHS address, not used by EFI firmware. */
	uint8_t start_track;    /* Start of partition in CHS address, not used by EFI firmware. */
	uint8_t os_type;        /* OS type. A value of 0xEF defines an EFI system partition.
                                   Other values are reserved for legacy operating systems, and
                                   allocated independently of the EFI specification. */
	uint8_t end_head;       /* End of partition in CHS address, not used by EFI firmware. */
	uint8_t end_sector;     /* End of partition in CHS address, not used by EFI firmware. */
	uint8_t end_track;      /* End of partition in CHS address, not used by EFI firmware. */
	uint32_t starting_lba;  /* Starting LBA address of the partition on the disk. Used by
                                   EFI firmware to define the start of the partition. */
	uint32_t size_in_lba;   /* Size of partition in LBA. Used by EFI firmware to determine
                                   the size of the partition. */
} PACKED partition_record;

/*
 * Protected Master Boot Record & Legacy MBR share same structure, which needs
 * to be packed because the uint16_t members force misalignment.
 */
typedef struct _legacy_mbr {
	uint8_t bootcode[440];
	uint32_t unique_mbr_signature;
	uint16_t unknown;
	partition_record partition[4];
	uint16_t signature;
} PACKED legacy_mbr;

#define EFI_GPT_PRIMARY_PARTITION_TABLE_LBA 1

/* Functions */
extern int NONNULL(3, 4, 5, 6, 7) HIDDEN
gpt_disk_get_partition_info (int fd, uint32_t num, uint64_t *start,
                             uint64_t *size, uint8_t *signature,
                             uint8_t *mbr_type, uint8_t *signature_type,
                             int ignore_pmbr_error, int logical_sector_size);

#endif /* _EFIBOOT_GPT_H */
