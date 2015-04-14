/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2015 Red Hat, Inc.
 * Copyright (C) 2001 Dell Computer Corporation <Matt_Domsch@dell.com>
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

#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <mntent.h>
#include <pci/pci.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "efivar.h"
#include "efiboot.h"
#include "dp.h"
#include "linux.h"

static int
__attribute__((__nonnull__ (1,2,3)))
find_file(const char const *filepath, char **devicep, char **relpathp)
{
	struct stat fsb = { 0, };
	int rc;
	int ret = -1;
	FILE *mounts = NULL;
	char linkbuf[PATH_MAX+1] = "";
	ssize_t linklen = 0;

	if (!filepath || !devicep || !relpathp) {
		errno = EINVAL;
		return -1;
	}

	linklen = strlen(filepath);
	if (linklen > PATH_MAX) {
		errno = ENAMETOOLONG;
		return -1;
	}
	strcpy(linkbuf, filepath);

	do {
		rc = stat(linkbuf, &fsb);
		if (rc < 0)
			return rc;

		if (S_ISLNK(fsb.st_mode)) {
			char tmp[PATH_MAX+1] = "";
			ssize_t l;

			l = readlink(linkbuf, tmp, PATH_MAX);
			if (l < 0)
				return -1;
			tmp[l] = '\0';
			linklen = l;
			strcpy(linkbuf, tmp);
		} else {
			break;
		}
	} while (1);

	mounts = fopen("/proc/self/mounts", "r");
	if (mounts == NULL)
		return rc;

	struct mntent *me;
	while (1) {
		struct stat dsb = { 0, };

		errno = 0;
		me = getmntent(mounts);
		if (!me) {
			if (feof(mounts))
				errno = ENOENT;
			goto err;
		}

		if (me->mnt_fsname[0] != '/')
			continue;

		rc = stat(me->mnt_fsname, &dsb);
		if (rc < 0) {
			if (errno == ENOENT)
				continue;
			goto err;
		}

		if (!S_ISBLK(dsb.st_mode))
			continue;

		if (dsb.st_rdev == fsb.st_dev) {
			ssize_t mntlen = strlen(me->mnt_dir);
			if (mntlen >= linklen) {
				errno = ENAMETOOLONG;
				goto err;
			}
			*devicep = strdup(me->mnt_fsname);
			if (!*devicep)
				goto err;
			*relpathp = strdup(linkbuf + mntlen);
			if (!*relpathp) {
				free(*devicep);
				*devicep = NULL;
				goto err;
			}
			ret = 0;
			break;
		}
	}
err:
	if (mounts)
		endmntent(mounts);
	return ret;
}

static ssize_t
make_the_whole_path(uint8_t *buf, size_t size, int fd, struct disk_info *info,
		    char *filepath, uint32_t abbrev, uint32_t extra/* :/ */)
{
	ssize_t ret=-1;
	ssize_t off=0, sz;
	struct pci_access *pacc = NULL;
	uint8_t bus, device, function;
	int rc = 0;

	if ((abbrev & EFIBOOT_ABBREV_EDD10)
	    && (!(EFIBOOT_ABBREV_FILE) && !(abbrev & EFIBOOT_ABBREV_HD))) {
		sz = efidp_make_edd10(buf, size, extra);
		if (sz < 0)
			return -1;
		off = sz;
	} else if (!(abbrev & EFIBOOT_ABBREV_FILE)
		   && !(abbrev & EFIBOOT_ABBREV_HD)) {
		pacc = pci_alloc();
		if (!pacc)
			return -1;

		pci_init(pacc);
		pci_scan_bus(pacc);

		switch (info->interface_type) {
		case ata:
			rc = eb_ide_pci(fd, info, &bus, &device, &function);
			break;
		case scsi:
			rc = eb_scsi_pci(fd, info, &bus, &device, &function);
			break;
		case i2o:
			goto err;
		case md:
			goto err;
		case virtblk:
			rc = eb_virt_pci(info, &bus, &device, &function);
			break;
		default:
			goto err;
		}
		if (rc < 0)
			goto err;

		/* If you haven't set _CID to PNP0A03 on your PCIe root hub,
		 * you're not going to get this far before it stops working.
		 */
		off = efidp_make_acpi_hid(buf+off, size?size+off:0,
					  EFIDP_ACPI_PCI_ROOT_HID, bus);
		if (off < 0)
			goto err;

		uint8_t target_bus = bus;
		struct pci_dev *dev;
		do {
			dev = NULL;
			for (struct pci_dev *p = pacc->devices; p; p=p->next) {
				if ((pci_read_word(p, PCI_HEADER_TYPE) & 0x7f)
				    != PCI_HEADER_TYPE_BRIDGE)
					continue;
				if (pci_read_byte(p, PCI_SECONDARY_BUS)
				    != target_bus)
					continue;
				sz = efidp_make_pci(buf+off, size?size+off:0,
						    p->dev, p->func);
				if (sz < 0)
					goto err;
				off += sz;
				target_bus = p->bus;
				dev = p;
				break;
			}
		} while (dev && target_bus);

		switch (info->interface_type) {
		case nvme:
			{
				uint32_t ns_id=0;
				int rc = eb_nvme_ns_id(fd, &ns_id);
				if (rc < 0)
					goto err;

				sz = efidp_make_nvme(buf+off, size?size+off:0,
						     ns_id, NULL);
				if (sz < 0)
					goto err;
				off += sz;
				break;
			}
		case virtblk:
			break;
		default:
			{
				uint8_t host = 0, channel = 0, id = 0, lun = 0;
				int rc = eb_scsi_idlun(fd, &host, &channel,
						       &id, &lun);
				if (rc < 0)
					goto err;
				sz = efidp_make_scsi(buf+off, size?size+off:0,
						     id, lun);
				if (sz < 0)
					goto err;
				off += sz;
			}
		}
	}

	if (!(abbrev & EFIBOOT_ABBREV_FILE)) {
	    /*
	     * make the HD path here
	     */
	}

	sz = efidp_make_file(buf+off, size?size+off:0, filepath);
	if (sz < 0)
		goto err;
	off += sz;

	ret = off;
err:
	if (pacc)
		pci_cleanup(pacc);

	return ret;
}

ssize_t
__attribute__((__nonnull__ (3)))
__attribute__((__visibility__ ("default")))
efi_generate_file_device_path(uint8_t *buf, ssize_t size,
			      const char const *filepath,
			      uint32_t abbrev,
			      uint32_t extra, /* :/ */
			      int __attribute__((__unused__)) ignore_fs_err
			      )
{
	int rc;
	ssize_t ret = -1;
	char *devpath = NULL;
	char *relpath = NULL;
	struct disk_info info = { 0, };
	int fd = -1;

	rc = find_file(filepath, &devpath, &relpath);
	if (rc < 0)
		return -1;
	printf("devpath: %s\nrelpath: %s\n", devpath, relpath);

	fd = open(devpath, O_RDONLY);
	if (fd < 0)
		goto err;

	rc = eb_disk_info_from_fd(fd, &info);
	if (rc < 0)
		goto err;

	ret = make_the_whole_path(buf, size, fd, &info, relpath, abbrev, extra);
err:
	if (fd >= 0)
		close(fd);
	if (devpath)
		free(devpath);
	if (relpath)
		free(relpath);
	return ret;
}

ssize_t
__attribute__((__nonnull__ (3)))
__attribute__((__visibility__ ("default")))
efi_generate_network_device_path(uint8_t *buf, ssize_t size,
			       const char const *ifname,
			       uint32_t abbrev)
{
	errno = ENOSYS;
	return -1;
}
