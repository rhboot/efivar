// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2015 Red Hat, Inc.
 * Copyright (C) 2000-2001 Dell Computer Corporation <Matt_Domsch@dell.com>
 */

#include "fix_coverity.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "efiboot.h"

/**
 * is_mbr_valid(): test MBR for validity
 * @mbr: pointer to a legacy mbr structure
 *
 * Description: Returns 1 if MBR is valid, 0 otherwise.
 * Validity depends on one thing:
 *  1) MSDOS signature is in the last two bytes of the MBR
 */
static inline int
is_mbr_valid(legacy_mbr *mbr)
{
	int ret;
	if (!mbr)
		return 0;
	ret = (mbr->magic == MSDOS_MBR_MAGIC);
	if (!ret) {
		errno = ENOTTY;
		efi_error("mbr magic is 0x%04hx not MSDOS_MBR_MAGIC (0x%04hx)",
			  mbr->magic, MSDOS_MBR_MAGIC);
	}
	return ret;
}

/************************************************************
 * msdos_disk_get_extended partition_info()
 * Requires:
 *  - open file descriptor fd
 *  - start, size
 * Modifies: all these
 * Returns:
 *  0 on success
 *  non-zero on failure
 *
 ************************************************************/
static int
msdos_disk_get_extended_partition_info (int fd UNUSED,
					legacy_mbr *mbr UNUSED,
					uint32_t num UNUSED,
					uint64_t *start UNUSED,
					uint64_t *size UNUSED)
{
	/* Until I can handle these... */
	//fprintf(stderr, "Extended partition info not supported.\n");
	errno = ENOSYS;
	efi_error("extended partition info is not supported");
	return -1;
}

/************************************************************
 * msdos_disk_get_partition_info()
 * Requires:
 *  - mbr
 *  - open file descriptor fd (for extended partitions)
 *  - start, size, signature, mbr_type, signature_type
 * Modifies: all these
 * Returns:
 *  0 on success
 *  non-zero on failure
 *
 ************************************************************/
static int
msdos_disk_get_partition_info (int fd, int write_signature,
			       legacy_mbr *mbr, uint32_t num, uint64_t *start,
			       uint64_t *size, uint32_t *signature,
			       uint8_t *mbr_type, uint8_t *signature_type)
{
	int rc;
	long disk_size=0;
	struct stat stat;
	struct timeval tv;

	if (!mbr) {
		errno = EINVAL;
		efi_error("mbr argument must not be NULL");
		return -1;
	}
	if (!is_mbr_valid(mbr)) {
		errno = ENOENT;
		efi_error("mbr is not valid");
		return -1;
	}

	*mbr_type = 0x01;
	*signature_type = 0x01;

	if (!mbr->unique_mbr_signature && !write_signature) {
		efi_error("\n******************************************************\n"
			  "Warning! This MBR disk does not have a unique signature.\n"
			  "If this is not the first disk found by EFI, you may not be able\n"
			  "to boot from it without a unique signature.\n"
			  "Run efibootmgr with the -w flag to write a unique signature\n"
			  "to the disk.\n"
			  "******************************************************");
	} else if (!mbr->unique_mbr_signature && write_signature) {
		/* MBR Signatures must be unique for the
		   EFI Boot Manager
		   to find the right disk to boot from */
		rc = fstat(fd, &stat);
		if (rc < 0) {
			efi_error("could not fstat disk");
			return rc;
		}

		rc = gettimeofday(&tv, NULL);
		if (rc < 0) {
			efi_error("gettimeofday failed");
			return rc;
		}

		/* Write the device type to the signature.
		   This should be unique per disk per system */
		mbr->unique_mbr_signature =  tv.tv_usec << 16;
		mbr->unique_mbr_signature |= stat.st_rdev & 0xFFFF;

		/* Write it to the disk */
		lseek(fd, 0, SEEK_SET);
		rc = write(fd, mbr, sizeof(*mbr));
		if (rc < 0) {
			efi_error("could not write MBR signature");
			return rc;
		}
	}
	*signature = mbr->unique_mbr_signature;

	if (num > 4) {
		/* Extended partition */
		rc = msdos_disk_get_extended_partition_info(fd, mbr, num,
							    start, size);
		if (rc < 0) {
			efi_error("could not get extended partition info");
			return rc;
		}
	} else if (num == 0) {
		/* Whole disk */
		*start = 0;
		ioctl(fd, BLKGETSIZE, &disk_size);
		*size = disk_size;
	} else if (num >= 1 && num <= 4) {
		/* Primary partition */
		*start = mbr->partition[num-1].starting_lba;
		*size  = mbr->partition[num-1].size_in_lba;
	}
	return 0;
}

static int
get_partition_info(int fd, uint32_t options,
		   uint32_t part, uint64_t *start, uint64_t *size,
		   partition_signature_t *signature, uint8_t *mbr_type,
		   uint8_t *signature_type)
{
	legacy_mbr *mbr;
	void *mbr_sector;
	size_t mbr_size;
	off_t offset UNUSED;
	int this_bytes_read = 0;
	int gpt_invalid=0, mbr_invalid=0;
	int rc=0;
	int sector_size = get_sector_size(fd);

	mbr_size = lcm(sizeof(*mbr), sector_size);
	if ((rc = posix_memalign(&mbr_sector, sector_size, mbr_size)) != 0) {
		efi_error("posix_memalign failed");
		goto error;
	}
	memset(mbr_sector, '\0', mbr_size);

	offset = lseek(fd, 0, SEEK_SET);
	this_bytes_read = read(fd, mbr_sector, mbr_size);
	if (this_bytes_read < (ssize_t)sizeof(*mbr)) {
		efi_error("short read trying to read mbr data");
		rc = -1;
		goto error_free_mbr;
	}
	mbr = (legacy_mbr *)mbr_sector;
	gpt_invalid = gpt_disk_get_partition_info(
	    fd, part, start, size, &signature->gpt_signature, mbr_type,
	    signature_type, (options & EFIBOOT_OPTIONS_IGNORE_PMBR_ERR) ? 1 : 0,
	    sector_size);
	if (gpt_invalid < 0) {
		mbr_invalid = msdos_disk_get_partition_info(
		    fd, (options & EFIBOOT_OPTIONS_WRITE_SIGNATURE) ? 1 : 0,
		    mbr, part, start, size, &signature->mbr_signature, mbr_type,
		    signature_type);
		if (mbr_invalid < 0) {
			efi_error("neither MBR nor GPT is valid");
			rc = -1;
			goto error_free_mbr;
		}
		efi_error_clear();
	}
 error_free_mbr:
	free(mbr_sector);
 error:
	return rc;
}

bool HIDDEN
is_partitioned(int fd)
{
	int rc;
	uint32_t options = 0;
	uint32_t part = 1;
	uint64_t start = 0, size = 0;
	uint8_t mbr_type = 0, signature_type = 0;
	partition_signature_t signature;

	memset(&signature, 0, sizeof(signature));
	rc = get_partition_info(fd, options, part, &start, &size,
				&signature, &mbr_type, &signature_type);
	if (rc < 0)
		return false;
	return true;
}

ssize_t HIDDEN
make_hd_dn(uint8_t *buf, ssize_t size, int fd, int32_t partition,
	   uint32_t options)
{
	uint64_t part_start=0, part_size = 0;
	partition_signature_t signature;
	uint8_t format=0, signature_type=0;
	int rc;

	errno = 0;

	if (partition <= 0)
		return 0;

	memset(&signature, 0, sizeof(signature));
	rc = get_partition_info(fd, options, partition, &part_start,
				&part_size, &signature, &format,
				&signature_type);
	if (rc < 0) {
		efi_error("could not get partition info");
		return rc;
	}

	rc = efidp_make_hd(buf, size, partition, part_start, part_size,
			   (uint8_t *)&signature, format, signature_type);
	if (rc < 0)
		efi_error("could not make HD DP node");
	return rc;
}

// vim:fenc=utf-8:tw=75:noet
