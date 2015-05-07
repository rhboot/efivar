/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2015 Red Hat, Inc.
 * Copyright (C) 2000-2001 Dell Computer Corporation <Matt_Domsch@dell.com>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <asm/byteorder.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <efivar.h>
#include <efiboot.h>

#include "util.h"
#include "crc32.h"
#include "disk.h"
#include "gpt.h"

static int report_errors;

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
	if (!mbr)
		return 0;
	return (mbr->signature == MSDOS_MBR_SIGNATURE);
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
msdos_disk_get_extended_partition_info (int fd, legacy_mbr *mbr,
					uint32_t num,
					uint64_t *start, uint64_t *size)
{
        /* Until I can handle these... */
        //fprintf(stderr, "Extended partition info not supported.\n");
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
			       uint64_t *size, uint8_t *signature,
			       uint8_t *mbr_type, uint8_t *signature_type)
{
	int rc;
	long disk_size=0;
	struct stat stat;
	struct timeval tv;

	if (!mbr)
		return -1;
	if (!is_mbr_valid(mbr))
		return -1;

	*mbr_type = 0x01;
	*signature_type = 0x01;

	if (!mbr->unique_mbr_signature && !write_signature && report_errors) {
		printf("\n\n******************************************************\n");
		printf("Warning! This MBR disk does not have a unique signature.\n");
		printf("If this is not the first disk found by EFI, you may not be able\n");
		printf("to boot from it without a unique signature.\n");
		printf("Run efibootmgr with the -w flag to write a unique signature\n");
		printf("to the disk.\n");
		printf("******************************************************\n\n");
	} else if (!mbr->unique_mbr_signature && write_signature) {
		/* MBR Signatures must be unique for the
		   EFI Boot Manager
		   to find the right disk to boot from */
		rc = fstat(fd, &stat);
		if (rc < 0) {
			if (report_errors)
				perror("stat disk");
			return rc;
		}

		rc = gettimeofday(&tv, NULL);
		if (rc < 0) {
			if (report_errors)
				perror("gettimeofday");
			return rc;
		}

		/* Write the device type to the signature.
		   This should be unique per disk per system */
		mbr->unique_mbr_signature =  tv.tv_usec << 16;
		mbr->unique_mbr_signature |= stat.st_rdev & 0xFFFF;

		/* Write it to the disk */
		lseek(fd, 0, SEEK_SET);
		rc = write(fd, mbr, sizeof(*mbr));
	}
	*(uint32_t *)signature = mbr->unique_mbr_signature;

        if (num > 4) {
		/* Extended partition */
                return msdos_disk_get_extended_partition_info(fd, mbr, num,
                                                              start, size);
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
		   uint8_t *signature, uint8_t *mbr_type,
		   uint8_t *signature_type)
{
	legacy_mbr *mbr;
	void *mbr_sector;
	size_t mbr_size;
	off_t offset __attribute__((unused));
	int this_bytes_read = 0;
	int gpt_invalid=0, mbr_invalid=0;
	int rc=0;
	int sector_size = get_sector_size(fd);

	mbr_size = lcm(sizeof(*mbr), sector_size);
	if ((rc = posix_memalign(&mbr_sector, sector_size, mbr_size)) != 0)
		goto error;
	memset(mbr_sector, '\0', mbr_size);

	offset = lseek(fd, 0, SEEK_SET);
	this_bytes_read = read(fd, mbr_sector, mbr_size);
	if (this_bytes_read < (ssize_t)sizeof(*mbr)) {
		rc=1;
		goto error_free_mbr;
	}
	mbr = (legacy_mbr *)mbr_sector;
	gpt_invalid = gpt_disk_get_partition_info(fd, part,
						  start, size,
						  signature,
						  mbr_type,
						  signature_type,
			(options & EFIBOOT_OPTIONS_IGNORE_PMBR_ERR)?1:0);
	if (gpt_invalid) {
		mbr_invalid = msdos_disk_get_partition_info(fd,
			(options & EFIBOOT_OPTIONS_WRITE_SIGNATURE)?1:0,
							    mbr, part,
							    start, size,
							    signature,
							    mbr_type,
							    signature_type);
		if (mbr_invalid) {
			rc=1;
			goto error_free_mbr;
		}
	}
 error_free_mbr:
	free(mbr_sector);
 error:
	return rc;
}

ssize_t
__attribute__((__visibility__ ("hidden")))
_make_hd_dn(uint8_t *buf, ssize_t size, int fd, uint32_t partition,
	    uint32_t options)
{
	uint64_t part_start=0, part_size = 0;
	uint8_t signature[16]="", format=0, signature_type=0;
	int rc;

	char *report=getenv("LIBEFIBOOT_REPORT_GPT_ERRORS");
	if (report)
		report_errors = 1;
	errno = 0;

	rc = get_partition_info(fd, options,
				partition>0?partition:1, &part_start,
				&part_size, signature, &format,
				&signature_type);
	if (rc < 0)
		return rc;

	return efidp_make_hd(buf, size, partition>0?partition:1, part_start,
			     part_size, signature, format, signature_type);
}
