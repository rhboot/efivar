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

#include "fix_coverity.h"

#include <asm/byteorder.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "efivar.h"

#ifndef BLKGETLASTSECT
#define BLKGETLASTSECT _IO(0x12,108) /* get last sector of block device */
#endif

struct blkdev_ioctl_param {
	unsigned int block;
	size_t content_length;
	char * block_contents;
};

/**
 * efi_crc32() - EFI version of crc32 function
 * @buf: buffer to calculate crc32 of
 * @len - length of buf
 *
 * Description: Returns EFI-style CRC32 value for @buf
 *
 * This function uses the little endian Ethernet polynomial
 * but seeds the function with ~0, and xor's with ~0 at the end.
 * Note, the EFI Specification, v1.02, has a reference to
 * Dr. Dobbs Journal, May 1994 (actually it's in May 1992).
 */
static inline uint32_t
efi_crc32(const void *buf, unsigned long len)
{
	return (crc32(buf, len, ~0L) ^ ~0L);
}

/**
 * is_pmbr_valid(): test Protective MBR for validity
 * @mbr: pointer to a legacy mbr structure
 *
 * Description: Returns 1 if PMBR is valid, 0 otherwise.
 * Validity depends on two things:
 *  1) MSDOS signature is in the last two bytes of the MBR
 *  2) One partition of type 0xEE is found
 */
static int
is_pmbr_valid(legacy_mbr *mbr)
{
	int i, found = 0, signature = 0;
	if (!mbr)
		return 0;
	signature = (le16_to_cpu(mbr->signature) == MSDOS_MBR_SIGNATURE);
	for (i = 0; signature && i < 4; i++) {
		if (mbr->partition[i].os_type ==
		    EFI_PMBR_OSTYPE_EFI_GPT) {
			found = 1;
			break;
		}
	}
	return (signature && found);
}

/**
 * kernel_has_blkgetsize64()
 *
 * Returns: 0 on false, 1 on true
 * True means kernel is 2.4.x, x>=18, or
 *		   is 2.5.x, x>4, or
 *		   is > 2.5
 */
static int
kernel_has_blkgetsize64(void)
{
	int major=0, minor=0, patch=0, parsed;
	int rc;
	struct utsname u;

	memset(&u, 0, sizeof(u));
	rc = uname(&u);
	if (rc)
		return 0;

	parsed = sscanf(u.release, "%d.%d.%d", &major, &minor, &patch);
	/* If the kernel is 2.4.15-2.4.18 and 2.5.0-2.5.3, i.e. the problem
	 * kernels, then this will get 3 answers.  If it doesn't, it isn't. */
	if (parsed != 3)
		return 1;

	if (major == 2 && minor == 5 && patch < 4)
		return 0;
	if (major == 2 && minor == 4 && patch >= 15 && patch <= 18)
		return 0;
	return 1;
}

/************************************************************
 * _get_num_sectors
 * Requires:
 *  - filedes is an open file descriptor, suitable for reading
 * Modifies: nothing
 * Returns:
 *  Last LBA value on success
 *  0 on error
 *
 * Try getting BLKGETSIZE64 and BLKSSZGET first,
 * then BLKGETSIZE if necessary.
 *  Kernels 2.4.15-2.4.18 and 2.5.0-2.5.3 have a broken BLKGETSIZE64
 *  which returns the number of 512-byte sectors, not the size of
 *  the disk in bytes. Fixed in kernels 2.4.18-pre8 and 2.5.4-pre3.
 ************************************************************/
static uint64_t
_get_num_sectors(int filedes)
{
	unsigned long sectors=0;
	uint64_t bytes=0;
	int rc;
	if (kernel_has_blkgetsize64()) {
		rc = ioctl(filedes, BLKGETSIZE64, &bytes);
		if (!rc)
			return bytes / get_sector_size(filedes);
	}

	rc = ioctl(filedes, BLKGETSIZE, &sectors);
	if (rc)
		return 0;

	return sectors;
}

/************************************************************
 * last_lba(): return number of last logical block of device
 *
 * @fd
 *
 * Description: returns Last LBA value on success, 0 on error.
 * Notes: The value st_blocks gives the size of the file
 *	in 512-byte blocks, which is OK if
 *	EFI_BLOCK_SIZE_SHIFT == 9.
 ************************************************************/
static uint64_t
last_lba(int filedes)
{
	int rc;
	uint64_t sectors = 0;
	struct stat s;
	memset(&s, 0, sizeof (s));
	rc = fstat(filedes, &s);
	if (rc == -1) {
		efi_error("last_lba() could not stat: %s", strerror(errno));
		return 0;
	}

	if (S_ISBLK(s.st_mode)) {
		sectors = _get_num_sectors(filedes);
	} else {
		efi_error("last_lba(): I don't know how to handle files with mode %x",
			  s.st_mode);
		sectors = 1;
	}

	return sectors - 1;
}


static ssize_t
read_lastoddsector(int fd, void *buffer, size_t count)
{
	int rc;
	struct blkdev_ioctl_param ioctl_param;

	if (!buffer)
		return 0;

	ioctl_param.block = 0; /* read the last sector */
	ioctl_param.content_length = count;
	ioctl_param.block_contents = buffer;

	rc = ioctl(fd, BLKGETLASTSECT, &ioctl_param);
	if (rc == -1)
		efi_error("read failed");

	return !rc;
}

static ssize_t
read_lba(int fd, uint64_t lba, void *buffer, size_t bytes)
{
	int sector_size = get_sector_size(fd);
	off_t offset = lba * sector_size;
	ssize_t bytesread;
	void *iobuf;
	size_t iobuf_size;
	int rc;
	off_t new_offset;

	iobuf_size = lcm(bytes, sector_size);
	rc = posix_memalign(&iobuf, sector_size, iobuf_size);
	if (rc)
		return rc;
	memset(iobuf, 0, bytes);

	new_offset = lseek(fd, offset, SEEK_SET);
	if (new_offset == (off_t)-1) {
		free(iobuf);
		return 0;
	}
	bytesread = read(fd, iobuf, iobuf_size);
	memcpy(buffer, iobuf, bytes);
	free(iobuf);

	/* Kludge.  This is necessary to read/write the last
	   block of an odd-sized disk, until Linux 2.5.x kernel fixes.
	   This is only used by gpt.c, and only to read
	   one sector, so we don't have to be fancy.
	*/
	if (!bytesread && !(last_lba(fd) & 1) && lba == last_lba(fd)) {
		bytesread = read_lastoddsector(fd, buffer, bytes);
	}
	return bytesread;
}

/**
 * alloc_read_gpt_entries(): reads partition entries from disk
 * @fd  is an open file descriptor to the whole disk
 * @gpt is a buffer into which the GPT will be put
 * Description: Returns ptes on success,  NULL on error.
 * Allocates space for PTEs based on information found in @gpt.
 * Notes: remember to free pte when you're done!
 */
static gpt_entry *
alloc_read_gpt_entries(int fd, uint32_t nptes, uint32_t ptesz, uint64_t ptelba)
{
	gpt_entry *pte;
	size_t count = nptes * ptesz;

	if (!count)
		return NULL;

	pte = (gpt_entry *)malloc(count);
	if (!pte)
		return NULL;

	memset(pte, 0, count);
	if (!read_lba(fd, ptelba, pte, count)) {
		free(pte);
		return NULL;
	}
	return pte;
}

/**
 * alloc_read_gpt_header(): Allocates GPT header, reads into it from disk
 * @fd  is an open file descriptor to the whole disk
 * @lba is the Logical Block Address of the partition table
 *
 * Description: returns GPT header on success, NULL on error.   Allocates
 * and fills a GPT header starting at @ from @bdev.
 * Note: remember to free gpt when finished with it.
 */
static gpt_header *
alloc_read_gpt_header(int fd, uint64_t lba)
{
	gpt_header *gpt;

	gpt = (gpt_header *)
	    malloc(sizeof (gpt_header));
	if (!gpt)
		return NULL;

	memset(gpt, 0, sizeof (*gpt));
	if (!read_lba(fd, lba, gpt, sizeof (gpt_header))) {
		free(gpt);
		return NULL;
	}

	return gpt;
}

/**
 * validate_nptes(): Tries to ensure that nptes is a reasonable value
 * @first_block is the beginning LBA to bound the table
 * @pte_start is the starting LBA of the partition table
 * @last_block is the end LBA of to bound the table
 * @ptesz is the size of a partition table entry
 * @nptes is the number of entries we have.
 * @blksz is the block size of the device.
 *
 * Description: returns 0 if the partition table doesn't fit, 1 if it does
 */
static int
validate_nptes(uint64_t first_block, uint64_t pte_start, uint64_t last_block,
	       uint32_t ptesz, uint32_t nptes, uint32_t blksz)
{
	uint32_t min_entry_size = sizeof(gpt_entry);
	uint32_t min_entry_size_mod = 128 - sizeof(gpt_entry) % 128;
	uint64_t max_blocks, max_bytes;

	if (min_entry_size_mod == 128)
		min_entry_size_mod = 0;
	min_entry_size += min_entry_size_mod;

	if (ptesz < min_entry_size)
		return 0;

	if (pte_start < first_block || pte_start > last_block)
		return 0;

	max_blocks = last_block - pte_start;
	if (UINT64_MAX / blksz < max_blocks)
		return 0;

	max_bytes = max_blocks * blksz;
	if (UINT64_MAX / ptesz < max_bytes)
		return 0;

	if (ptesz > max_bytes / nptes)
		return 0;

	if (max_bytes / ptesz < nptes)
		return 0;

	return 1;
}

static int
check_lba(uint64_t lba, uint64_t lastlba, char *name)
{
	if (lba > lastlba) {
		efi_error("Invalid %s LBA %"PRIx64" max:%"PRIx64,
			  name, lba, lastlba);
		return 0;
	}
	return 1;
}

/**
 * is_gpt_valid() - tests one GPT header and PTEs for validity
 * @fd  is an open file descriptor to the whole disk
 * @lba is the logical block address of the GPT header to test
 * @gpt is a GPT header ptr, filled on return.
 * @ptes is a PTEs ptr, filled on return.
 *
 * Description: returns 1 if valid,  0 on error.
 * If valid, returns pointers to newly allocated GPT header and PTEs.
 */
static int
is_gpt_valid(int fd, uint64_t lba,
	     gpt_header ** gpt, gpt_entry ** ptes,
	     uint32_t logical_block_size)
{
	int rc = 0;		/* default to not valid */
	uint32_t crc, origcrc;
	uint64_t max_device_lba = last_lba(fd);

	if (!gpt || !ptes)
		return 0;
	if (!(*gpt = alloc_read_gpt_header(fd, lba)))
		return 0;

	/* Check the GUID Partition Table signature */
	if (le64_to_cpu((*gpt)->signature) != GPT_HEADER_SIGNATURE) {
		efi_error("GUID Partition Table Header signature is wrong: %"PRIx64" != %"PRIx64,
			  (uint64_t)le64_to_cpu((*gpt)->signature),
			  GPT_HEADER_SIGNATURE);
		free(*gpt);
		*gpt = NULL;
		return rc;
	}

	uint32_t hdrsz = le32_to_cpu((*gpt)->header_size);
	uint32_t hdrmin = MAX(92,
			      sizeof(gpt_header) - sizeof((*gpt)->reserved2));
	if (hdrsz < hdrmin || hdrsz > logical_block_size) {
		efi_error("GUID Partition Table Header size is invalid (%d < %d < %d)",
			  hdrmin, hdrsz, logical_block_size);
		free (*gpt);
		*gpt = NULL;
		return rc;
	}

	/* Check the GUID Partition Table Header CRC */
	origcrc = le32_to_cpu((*gpt)->header_crc32);
	(*gpt)->header_crc32 = 0;
	crc = efi_crc32(*gpt, le32_to_cpu((*gpt)->header_size));
	if (crc != origcrc) {
		efi_error("GPTH CRC check failed, %x != %x.",
			  origcrc, crc);
		(*gpt)->header_crc32 = cpu_to_le32(origcrc);
		free(*gpt);
		*gpt = NULL;
		return 0;
	}
	(*gpt)->header_crc32 = cpu_to_le32(origcrc);

	/* Check that the my_lba entry points to the LBA
	 * that contains the GPT we read */
	uint64_t mylba = le64_to_cpu((*gpt)->my_lba);
	uint64_t altlba = le64_to_cpu((*gpt)->alternate_lba);
	if (mylba != lba && altlba != lba) {
		efi_error("lba %"PRIx64" != lba %"PRIx64".",
			  mylba, lba);
err:
		free(*gpt);
		*gpt = NULL;
		return 0;
	}

	if (!check_lba(mylba, max_device_lba, "GPT"))
		goto err;

	if (!check_lba(altlba, max_device_lba, "GPT Alt"))
		goto err;

	uint64_t ptelba = le64_to_cpu((*gpt)->partition_entry_lba);
	uint64_t fulba = le64_to_cpu((*gpt)->first_usable_lba);
	uint64_t lulba = le64_to_cpu((*gpt)->last_usable_lba);
	uint32_t nptes = le32_to_cpu((*gpt)->num_partition_entries);
	uint32_t ptesz = le32_to_cpu((*gpt)->sizeof_partition_entry);

	if (!check_lba(ptelba, max_device_lba, "PTE"))
		goto err;
	if (!check_lba(fulba, max_device_lba, "First Usable"))
		goto err;
	if (!check_lba(lulba, max_device_lba, "Last Usable"))
		goto err;

	if (ptesz < sizeof(gpt_entry) || ptesz % 128 != 0) {
		efi_error("Invalid GPT entry size is %d.", ptesz);
		goto err;
	}

	/* There's really no good answer to maximum bounds, but this large
	 * would be completely absurd, so... */
	if (nptes > 1024) {
		efi_error("Not honoring insane number of Partition Table Entries 0x%"PRIx32".",
			  nptes);
		goto err;
	}

	if (ptesz > 4096) {
		efi_error("Not honoring insane Partition Table Entry size 0x%"PRIx32".",
			  ptesz);
		goto err;
	}

	uint64_t pte_blocks;
	uint64_t firstlba, lastlba;

	if (altlba > mylba) {
		firstlba = mylba + 1;
		lastlba = fulba;
		pte_blocks = fulba - ptelba;
		rc = validate_nptes(firstlba, ptelba, fulba,
				    ptesz, nptes, logical_block_size);
	} else {
		firstlba = lulba;
		lastlba = mylba;
		pte_blocks = mylba - ptelba;
		rc = validate_nptes(lulba, ptelba, mylba,
				    ptesz, nptes, logical_block_size);
	}
	if (!rc) {
		efi_error("%"PRIu32" partition table entries with size 0x%"PRIx32" doesn't fit in 0x%"PRIx64" blocks between 0x%"PRIx64" and 0x%"PRIx64".",
			  nptes, ptesz, pte_blocks, firstlba, lastlba);
		goto err;
	}

	if (!(*ptes = alloc_read_gpt_entries(fd, nptes, ptesz, ptelba))) {
		free(*gpt);
		*gpt = NULL;
		return 0;
	}

	/* Check the GUID Partition Entry Array CRC */
	crc = efi_crc32(*ptes, nptes * ptesz);
	if (crc != le32_to_cpu((*gpt)->partition_entry_array_crc32)) {
		efi_error("GUID Partitition Entry Array CRC check failed.");
		free(*gpt);
		*gpt = NULL;
		free(*ptes);
		*ptes = NULL;
		return 0;
	}

	/* We're done, all's well */
	return 1;
}
/**
 * compare_gpts() - Search disk for valid GPT headers and PTEs
 * @pgpt is the primary GPT header
 * @agpt is the alternate GPT header
 * @lastlba is the last LBA number
 * Description: Returns nothing.  Sanity checks pgpt and agpt fields
 * and prints warnings on discrepancies.
 *
 */
static void
compare_gpts(gpt_header *pgpt, gpt_header *agpt, uint64_t lastlba)
{
	int error_found = 0;

	if (!pgpt || !agpt)
		return;

	if (le64_to_cpu(pgpt->my_lba) != le64_to_cpu(agpt->alternate_lba)) {
		efi_error("GPT:Primary header LBA != Alt. header alternate_lba"
			  "GPT:0x%" PRIx64 " != 0x%" PRIx64,
			  (uint64_t)le64_to_cpu(pgpt->my_lba),
			  (uint64_t)le64_to_cpu(agpt->alternate_lba));
		error_found++;
	}
	if (le64_to_cpu(pgpt->alternate_lba) != le64_to_cpu(agpt->my_lba)) {
		efi_error("GPT:Primary header alternate_lba != Alt. header my_lba"
			  "GPT:0x%" PRIx64 " != 0x%" PRIx64,
			  (uint64_t)le64_to_cpu(pgpt->alternate_lba),
			  (uint64_t)le64_to_cpu(agpt->my_lba));
		error_found++;
	}
	if (le64_to_cpu(pgpt->first_usable_lba) !=
	    le64_to_cpu(agpt->first_usable_lba)) {
		efi_error("GPT:first_usable_lbas don't match."
			  "GPT:0x%" PRIx64 " != 0x%" PRIx64,
			  (uint64_t)le64_to_cpu(pgpt->first_usable_lba),
			  (uint64_t)le64_to_cpu(agpt->first_usable_lba));
		error_found++;
	}
	if (le64_to_cpu(pgpt->last_usable_lba) !=
	    le64_to_cpu(agpt->last_usable_lba)) {
		efi_error("GPT:last_usable_lbas don't match."
			  "GPT:0x%" PRIx64 " != 0x%" PRIx64,
			  (uint64_t)le64_to_cpu(pgpt->last_usable_lba),
			  (uint64_t)le64_to_cpu(agpt->last_usable_lba));
		error_found++;
	}
	if (memcmp(&pgpt->disk_guid, &agpt->disk_guid,
			sizeof (pgpt->disk_guid))) {
		efi_error("GPT:disk_guids don't match.");
		error_found++;
	}
	if (le32_to_cpu(pgpt->num_partition_entries) !=
	    le32_to_cpu(agpt->num_partition_entries)) {
		efi_error("GPT:num_partition_entries don't match: 0x%x != 0x%x",
			  le32_to_cpu(pgpt->num_partition_entries),
			  le32_to_cpu(agpt->num_partition_entries));
		error_found++;
	}
	if (le32_to_cpu(pgpt->sizeof_partition_entry) !=
	    le32_to_cpu(agpt->sizeof_partition_entry)) {
		efi_error("GPT:sizeof_partition_entry values don't match: 0x%x != 0x%x",
			  le32_to_cpu(pgpt->sizeof_partition_entry),
			  le32_to_cpu(agpt->sizeof_partition_entry));
		error_found++;
	}
	if (le32_to_cpu(pgpt->partition_entry_array_crc32) !=
	    le32_to_cpu(agpt->partition_entry_array_crc32)) {
		efi_error("GPT:partition_entry_array_crc32 values don't match: 0x%x != 0x%x",
			  le32_to_cpu(pgpt->partition_entry_array_crc32),
			  le32_to_cpu(agpt->partition_entry_array_crc32));
		error_found++;
	}
	if (le64_to_cpu(pgpt->alternate_lba) != lastlba) {
		efi_error("GPT:Primary header thinks Alt. header is not at the end of the disk."
			  "GPT:0x%" PRIx64 " != 0x%" PRIx64,
			  (uint64_t)le64_to_cpu(pgpt->alternate_lba), lastlba);
		error_found++;
	}

	if (le64_to_cpu(agpt->my_lba) != lastlba) {
		efi_error("GPT:Alternate GPT header not at the end of the disk."
			  "GPT:0x%" PRIx64 " != 0x%" PRIx64,
			  (uint64_t)le64_to_cpu(agpt->my_lba), lastlba);
		error_found++;
	}

	if (error_found)
		efi_error("GPT: Use GNU Parted to correct GPT errors.");
}

/**
 * find_valid_gpt() - Search disk for valid GPT headers and PTEs
 * @fd  is an open file descriptor to the whole disk
 * @gpt is a GPT header ptr, filled on return.
 * @ptes is a PTEs ptr, filled on return.
 * Description: Returns 1 if valid, 0 on error.
 * If valid, returns pointers to newly allocated GPT header and PTEs.
 * Validity depends on finding either the Primary GPT header and PTEs valid,
 * or the Alternate GPT header and PTEs valid, and the PMBR valid.
 */
static int
find_valid_gpt(int fd, gpt_header ** gpt, gpt_entry ** ptes,
	       int ignore_pmbr_err, int logical_block_size)
{
	int good_pgpt = 0, good_agpt = 0, good_pmbr = 0;
	gpt_header *pgpt = NULL, *agpt = NULL;
	gpt_entry *pptes = NULL, *aptes = NULL;
	legacy_mbr *legacymbr = NULL;
	uint64_t lastlba;
	int ret = -1;

	errno = EINVAL;

	if (!gpt || !ptes)
		return -1;

	lastlba = last_lba(fd);
	good_pgpt = is_gpt_valid(fd, GPT_PRIMARY_PARTITION_TABLE_LBA,
				 &pgpt, &pptes, logical_block_size);
	if (good_pgpt) {
		good_agpt = is_gpt_valid(fd,
					 le64_to_cpu(pgpt->alternate_lba),
					 &agpt, &aptes, logical_block_size);
		if (!good_agpt) {
			good_agpt = is_gpt_valid(fd, lastlba, &agpt, &aptes,
						 logical_block_size);
		}
	} else {
		good_agpt = is_gpt_valid(fd, lastlba, &agpt, &aptes,
					 logical_block_size);
	}

	/* The obviously unsuccessful case */
	if (!good_pgpt && !good_agpt) {
		goto fail;
	}

	/* This will be added to the EFI Spec. per Intel after v1.02. */
	legacymbr = malloc(sizeof (*legacymbr));
	if (legacymbr) {
		memset(legacymbr, 0, sizeof (*legacymbr));
		read_lba(fd, 0, (uint8_t *) legacymbr, sizeof (*legacymbr));
		good_pmbr = is_pmbr_valid(legacymbr);
		free(legacymbr);
		legacymbr=NULL;
	}

	/* Failure due to bad PMBR */
	if ((good_pgpt || good_agpt) && !good_pmbr && !ignore_pmbr_err) {
		efi_error("Primary GPT is invalid, using alternate GPT.");
		goto fail;
	}

	/* Would fail due to bad PMBR, but force GPT anyhow */
	if ((good_pgpt || good_agpt) && !good_pmbr && ignore_pmbr_err) {
		efi_error("  Warning: Disk has a valid GPT signature but invalid PMBR.\n"
			  "  Use GNU Parted to correct disk.\n"
			  "  gpt option taken, disk treated as GPT.");
	}

	compare_gpts(pgpt, agpt, lastlba);

	/* The good cases */
	if (good_pgpt && (good_pmbr || ignore_pmbr_err)) {
		*gpt  = pgpt;
		*ptes = pptes;
	} else if (good_agpt && (good_pmbr || ignore_pmbr_err)) {
		*gpt  = agpt;
		*ptes = aptes;
	}

	ret = 0;
	errno = 0;
 fail:
	if (pgpt && (pgpt != *gpt || ret < 0)) {
		free(pgpt);
		pgpt=NULL;
	}
	if (pptes && (pptes != *ptes || ret < 0)) {
		free(pptes);
		pptes=NULL;
	}
	if (agpt && (agpt != *gpt || ret < 0)) {
		free(agpt);
		agpt=NULL;
	}
	if (aptes && (aptes != *ptes || ret < 0)) {
		free(aptes);
		aptes=NULL;
	}
	if (ret < 0) {
		*gpt = NULL;
		*ptes = NULL;
	}
	return ret;
}


/************************************************************
 * gpt_disk_get_partition_info()
 * Requires:
 *  - open file descriptor fd
 *  - start, size, signature, mbr_type, signature_type
 * Modifies: all these
 * Returns:
 *  0 on success
 *  non-zero on failure
 *
 ************************************************************/
int NONNULL(3, 4, 5, 6, 7) HIDDEN
gpt_disk_get_partition_info(int fd, uint32_t num, uint64_t * start,
			    uint64_t * size, uint8_t *signature,
			    uint8_t * mbr_type, uint8_t * signature_type,
			    int ignore_pmbr_error, int logical_block_size)
{
	gpt_header *gpt = NULL;
	gpt_entry *ptes = NULL, *p;
	int rc = 0;

	rc = find_valid_gpt(fd, &gpt, &ptes, ignore_pmbr_error,
			    logical_block_size);
	if (rc < 0)
		return rc;

	*mbr_type = 0x02;
	*signature_type = 0x02;

	if (num > 0 && num <= le32_to_cpu(gpt->num_partition_entries)) {
		p = &ptes[num - 1];
		*start = le64_to_cpu(p->starting_lba);
		*size = le64_to_cpu(p->ending_lba) -
			le64_to_cpu(p->starting_lba) + 1;
		memcpy(signature, &p->unique_partition_guid,
		       sizeof (p->unique_partition_guid));
	} else {
		efi_error("partition %d is not valid", num);
		errno = EINVAL;
		rc = -1;
	}
	free(ptes);
	free(gpt);

	return rc;
}

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 4
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -4
 * c-argdecl-indent: 4
 * c-label-offset: -4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: 0
 * indent-tabs-mode: nil
 * tab-width: 8
 * End:
 */
