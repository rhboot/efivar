/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2015 Red Hat, Inc.
 * Copyright (C) 2001 Dell Computer Corporation <Matt_Domsch@dell.com>
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
#define _GNU_SOURCE 1
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <linux/nvme.h>
#include <scsi/scsi.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <efivar.h>
#include <efiboot.h>
#include "dp.h"
#include "linux.h"

#ifndef SCSI_IOCTL_GET_IDLUN
#define SCSI_IOCTL_GET_IDLUN 0x5382
#endif

int
__attribute__((__visibility__ ("hidden")))
eb_nvme_ns_id(int fd, uint32_t *ns_id)
{
	uint64_t ret = ioctl(fd, NVME_IOCTL_ID, NULL);
	if ((int)ret < 0)
		return ret;
	*ns_id = (uint32_t)ret;
	return 0;
}

typedef struct scsi_idlun_s {
	uint32_t dev_id;
	uint32_t host_unique_id;
} scsi_idlun;

int
__attribute__((__visibility__ ("hidden")))
eb_scsi_idlun(int fd, uint8_t *host, uint8_t *channel, uint8_t *id,
		   uint8_t *lun)
{
	int rc;
	scsi_idlun idlun = {0, 0};

	if (fd < 0 || !host || !channel || !id || !lun) {
		errno = EINVAL;
		return -1;
	}

	rc = ioctl(fd, SCSI_IOCTL_GET_IDLUN, &idlun);
	if (rc < 0)
		return rc;

	*host =	(idlun.dev_id >> 24) & 0xff;
	*channel = (idlun.dev_id >> 16) & 0xff;
	*lun = (idlun.dev_id >> 8) & 0xff;
	*id = idlun.dev_id & 0xff;
	return 0;
}

/*
 * Look up dynamic major device node numbers.
 */
static int
get_dynamic_major(char *name, int block)
{
	static int cached;
	static char cached_name[1024] = "";
	static int cached_block;

	FILE *f;
	char line[256];
	int seen_block = 0;
	char namenl[strlen(name)+2];

	if (cached != 0 && block==cached_block &&
	    !strncmp(cached_name, name, 1023)) {
		return cached;
	}
	strcpy(namenl, name);
	strcat(namenl, "\n");

	cached = -1;
	f = fopen("/proc/devices", "r");
	if (f == NULL)
		return cached;

	while (fgets(line, sizeof line, f) != NULL) {
		size_t len = strlen(line);
		int major, scanned = 0;

		if (!strcmp(line, "Block devices:\n"))
			seen_block = 1;
		if (len == 0 || line[len - 1] != '\n') {
			break;
		}
		if (seen_block == block &&
		    sscanf(line, "%d %n", &major, &scanned) == 1 &&
		    strcmp(line + scanned, namenl) == 0) {
			cached = major;
			strncpy(cached_name, name, 1023);
			cached_block = block;
			break;
		}
	}
	fclose(f);
	return cached;
}

int
__attribute__((__visibility__ ("hidden")))
eb_ide_pci(int fd, const struct disk_info *info, uint8_t *bus, uint8_t *device,
	   uint8_t *function)
{
	return -1;
}

#ifndef SCSI_IOCTL_GET_PCI
#define SCSI_IOCTL_GET_PCI 0x5387
#endif

/* see scsi_ioctl_get_pci() in linux/drivers/scsi/scsi_ioctl.c */
#define SLOT_NAME_SIZE ((size_t)21)

/* TODO: move this to get it from sysfs? */
int
__attribute__((__visibility__ ("hidden")))
eb_scsi_pci(int fd, const struct disk_info *info, uint8_t *bus,
	    uint8_t *device, uint8_t *function)
{
	char buf[SLOT_NAME_SIZE] = "";
	int rc;
	unsigned int b=0,d=0,f=0;

	/*
	 * Maybe if we're on an old enough kernel,
	 * SCSI_IOCTL_GET_PCI gives b:d.f ...
	 */
	rc = ioctl(fd, SCSI_IOCTL_GET_PCI, buf);
	if (rc < 0)
		return rc;

	rc = sscanf(buf, "%x:%x:%x", &b, &d, &f);
	if (rc != 3) {
		errno = EINVAL;
		return -1;
	}

	*bus = b & 0xff;
	*device = d & 0xff;
	*function = f & 0xff;
	return 0;
}

int
__attribute__((__visibility__ ("hidden")))
get_disk_name(uint64_t major, unsigned char minor,
	      char *pathname, size_t max)
{
	char path[PATH_MAX+1] = "";
	char linkbuf[PATH_MAX+1] = "";
	ssize_t rc;

	rc = snprintf(path, PATH_MAX, "/sys/dev/block/%"PRIu64":%hhd",
		      major, minor);
	if (rc < 0)
		return -1;

	rc = readlink(path, linkbuf, PATH_MAX);
	if (rc < 0)
		return -1;
	linkbuf[PATH_MAX]='\0';

	char *dev;
	dev = strrchr(linkbuf, '/');
	if (!dev) {
		errno = EINVAL;
		return -1;
	}
	*dev = '\0';

	dev = strrchr(linkbuf, '/');
	if (!dev) {
		errno = EINVAL;
		return -1;
	}

	strncpy(pathname, dev+1, max);
	return 0;
}

int
__attribute__((__visibility__ ("hidden")))
eb_blockdev_pci_fill(struct disk_info *info)
{
	char leftbuf[PATH_MAX+1]="", rightbuf[PATH_MAX+1]="";
	ssize_t linksz;
	int off = 0;
	int sz = 0;
	int rc;

	rc = snprintf(leftbuf, PATH_MAX, "/sys/dev/block/%"PRIu64":%u",
		      info->major, info->minor);
	if (rc < 0)
		return -1;

	linksz = readlink(leftbuf, rightbuf, sizeof (rightbuf));
	rightbuf[linksz] = '\0';

	off = strlen("../../devices/pci0000:00/");
	int found=0;
	while (1) {
		uint16_t domain;
		uint8_t bus, device, function;
		rc = sscanf(rightbuf+off, "%hx:%hhx:%hhx.%hhx/%n",
			    &domain, &bus, &device, &function, &sz);
		if (rc != 4)
			break;
		info->pci_domain = domain;
		info->pci_bus = bus;
		info->pci_device = device;
		info->pci_function = function;
		found=1;
		off += sz;
	}
	if (!found)
		return -1;

	if (info->interface_type == scsi) {
		char diskname[PATH_MAX+1]="";
		rc = get_disk_name(info->major, info->minor, diskname,
				   PATH_MAX);
		if (rc < 0)
			return 0;

		rc = snprintf(rightbuf, PATH_MAX, "/sys/class/block/%s/device",
			      diskname);
		if (rc < 0)
			return 0;

		linksz = readlink(rightbuf, leftbuf, PATH_MAX);
		leftbuf[linksz] = '\0';

		rc = sscanf(leftbuf, "../../../%d:%d:%d:%llu",
			    &info->scsi_bus, &info->scsi_device,
			    &info->scsi_target,
			    (unsigned long long int *)&info->scsi_lun);
		if (rc != 4)
			return -1;

		if (!strncmp(rightbuf+off, "ata", 3)) {
			printf("rightbuf: %s\n", rightbuf);
			info->interface_type = sata;
		} else {
			struct stat statbuf = { 0, };

			rc = snprintf(leftbuf, PATH_MAX,
			    "/sys/dev/block/%"PRIu64":%u/device/sas_address",
			    info->major, info->minor);
			if (rc < 0)
				return 0;

			rc = stat(leftbuf, &statbuf);
			if (rc >= 0)
				info->interface_type = sas;
		}
	}

	return 0;
}

int
__attribute__((__visibility__ ("hidden")))
eb_disk_info_from_fd(int fd, struct disk_info *info)
{
	struct stat buf;
	int rc;

	memset(info, 0, sizeof *info);
	memset(&buf, 0, sizeof(struct stat));
	rc = fstat(fd, &buf);
	if (rc == -1) {
		perror("stat");
		return 1;
	}
	if (S_ISBLK(buf.st_mode)) {
		info->major = buf.st_rdev >> 8;
		info->minor = buf.st_rdev & 0xFF;
	} else if (S_ISREG(buf.st_mode)) {
		info->major = buf.st_dev >> 8;
		info->minor = buf.st_dev & 0xFF;
	} else {
		printf("Cannot stat non-block or non-regular file\n");
		return 1;
	}

	/* IDE disks can have up to 64 partitions, or 6 bits worth,
	 * and have one bit for the disk number.
	 * This leaves an extra bit at the top.
	 */
	if (info->major == 3) {
		info->disknum = (info->minor >> 6) & 1;
		info->controllernum = (info->major - 3 + 0) + info->disknum;
		info->interface_type = ata;
		info->part    = info->minor & 0x3F;
		return 0;
	} else if (info->major == 22) {
		info->disknum = (info->minor >> 6) & 1;
		info->controllernum = (info->major - 22 + 2) + info->disknum;
		info->interface_type = ata;
		info->part    = info->minor & 0x3F;
		return 0;
	} else if (info->major >= 33 && info->major <= 34) {
		info->disknum = (info->minor >> 6) & 1;
		info->controllernum = (info->major - 33 + 4) + info->disknum;
		info->interface_type = ata;
		info->part    = info->minor & 0x3F;
		return 0;
	} else if (info->major >= 56 && info->major <= 57) {
		info->disknum = (info->minor >> 6) & 1;
		info->controllernum = (info->major - 56 + 8) + info->disknum;
		info->interface_type = ata;
		info->part    = info->minor & 0x3F;
		return 0;
	} else if (info->major >= 88 && info->major <= 91) {
		info->disknum = (info->minor >> 6) & 1;
		info->controllernum = (info->major - 88 + 12) + info->disknum;
		info->interface_type = ata;
		info->part    = info->minor & 0x3F;
		return 0;
	}

        /* I2O disks can have up to 16 partitions, or 4 bits worth. */
	if (info->major >= 80 && info->major <= 87) {
		info->interface_type = i2o;
		info->disknum = 16*(info->major-80) + (info->minor >> 4);
		info->part    = (info->minor & 0xF);
		return 0;
	}

	/* SCSI disks can have up to 16 partitions, or 4 bits worth
	 * and have one bit for the disk number.
	 */
	if (info->major == 8) {
		info->interface_type = scsi;
		info->disknum = (info->minor >> 4);
		info->part    = (info->minor & 0xF);
		return 0;
	} else  if (info->major >= 65 && info->major <= 71) {
		info->interface_type = scsi;
		info->disknum = 16*(info->major-64) + (info->minor >> 4);
		info->part    = (info->minor & 0xF);
		return 0;
	} else  if (info->major >= 128 && info->major <= 135) {
		info->interface_type = scsi;
		info->disknum = 16*(info->major-128) + (info->minor >> 4);
		info->part    = (info->minor & 0xF);
		return 0;
	}

	int64_t major = get_dynamic_major("nvme", 1);
	if (major >= 0 && (uint64_t)major == info->major) {
		info->interface_type = nvme;
		return 0;
	}

	major = get_dynamic_major("virtblk", 1);
	if (major >= 0 && (uint64_t)major == info->major) {
		info->interface_type = virtblk;
		info->disknum = info->minor >> 4;
		info->part = info->minor & 0xF;
		return 0;
	}

	errno = ENOSYS;
	return -1;
}
