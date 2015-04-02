/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012-2015 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <scsi/scsi.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/nvme.h>

#include "efivar.h"
#include "dp.h"

#ifndef SCSI_IOCTL_GET_IDLUN
#define SCSI_IOCTL_GET_IDLUN 0x5382
#endif

int
__attribute__((__visibility__ ("default")))
efi_linux_nvme_ns_id(int fd, uint32_t *ns_id)
{
	uint64_t ret = ioctl(fd, NVME_IOCTL_ID, NULL);
	if ((int)ret < 0)
		return ret;
	*ns_id = (uint32_t)ret;
	return 0;
}

typedef struct scsi_idlun_s {
	uint32_t	dev_id;
	uint32_t	host_unique_id;
} scsi_idlun;

int
__attribute__((__visibility__ ("default")))
efi_linux_scsi_idlun(int fd, uint8_t *host, uint8_t *channel, uint8_t *id,
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

	*host =		(idlun.dev_id >> 24) & 0xff;
	*channel =	(idlun.dev_id >> 16) & 0xff;
	*lun =		(idlun.dev_id >>  8) & 0xff;
	*id =		(idlun.dev_id      ) & 0xff;
	return 0;
}

#ifndef SCSI_IOCTL_GET_PCI
#define SCSI_IOCTL_GET_PCI 0x5387
#endif

/* see scsi_ioctl_get_pci() in linux/drivers/scsi/scsi_ioctl.c */
#define SLOT_NAME_SIZE ((size_t)21)

/* TODO: move this to get it from sysfs? */
int
__attribute__((__visibility__ ("default")))
efi_linux_scsi_pci(int fd, char *slot_name, size_t size)
{
	char buf[SLOT_NAME_SIZE] = "";
	int rc;

	rc = ioctl(fd, SCSI_IOCTL_GET_PCI, buf);
	if (rc < 0)
		return rc;

	rc = snprintf(slot_name, size, "%s", buf);
	if (rc < 0)
		return rc;
	return 0;
}
