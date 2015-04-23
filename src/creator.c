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
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
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

#include <efivar.h>
#include <efiboot.h>

#include "disk.h"
#include "dp.h"
#include "linux.h"
#include "list.h"

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

static inline int
is_parent_bridge(struct pci_dev *p, uint8_t target_bus)
{
#if 0
	uint32_t w = pci_read_word(p, PCI_HEADER_TYPE);
	uint32_t b = pci_read_byte(p, PCI_SECONDARY_BUS);
	printf("p: %p w: %08x w&0x7f: %08x sb: %hhx\n", p, w, w & 0x7f, b);
#endif
	if ((pci_read_word(p, PCI_HEADER_TYPE) & 0x7f)
	    != PCI_HEADER_TYPE_BRIDGE)
		return 0;
	if (pci_read_byte(p, PCI_SECONDARY_BUS) != target_bus)
		return 0;
	return 1;
}

static inline struct pci_dev *
find_parent(struct pci_access *pacc, uint8_t target_bus)
{
	struct pci_dev *p;

	for (p = pacc->devices; p; p=p->next) {
		if (is_parent_bridge(p, target_bus))
			return p;
	}
	return NULL;
}

struct device {
	struct pci_dev *pci_dev;
	struct list_head node;
};

static ssize_t
make_pci_path(uint8_t *buf, size_t size, int fd, struct disk_info *info,
	      char *devpath)
{
	struct pci_access *pacc = NULL;
	ssize_t ret=-1;
	ssize_t off=0, sz;
	int rc = 0;

	pacc = pci_alloc();
	if (!pacc)
		return -1;

	pci_init(pacc);
	pci_scan_bus(pacc);

	/*
	 * We're probably on a modern kernel, so just parse the
	 * symlink from /sys/dev/block/$major:$minor and get it
	 * from there.
	 */
	rc = eb_blockdev_pci_fill(info);
	if (rc < 0) {
		switch (info->interface_type) {
		case ata:
			rc = eb_ide_pci(fd, info, &info->pci_dev.pci_bus,
					&info->pci_dev.pci_device,
					&info->pci_dev.pci_function);
			break;
		case scsi:
			rc = eb_scsi_pci(fd, info, &info->pci_dev.pci_bus,
					 &info->pci_dev.pci_device,
					 &info->pci_dev.pci_function);
			break;
		default:
			break;
		}
		if (rc < 0)
			goto err;
	}

	LIST_HEAD(pci_parent_list);
	uint8_t bus = info->pci_dev.pci_bus;
	struct pci_dev *pci_dev = NULL;
	struct device *dev;
	do {
		pci_dev = find_parent(pacc, bus);
		if (pci_dev) {
			dev = alloca(sizeof(*dev));
			INIT_LIST_HEAD(&dev->node);
			list_add(&pci_parent_list, &dev->node);
			dev->pci_dev = pci_dev;
			bus = pci_dev->bus;
		}
	} while (pci_dev && bus);

	if (info->pci_root.root_pci_bus != 0xff) {
		/* If you haven't set _CID to PNP0A03 on your PCIe root hub,
		 * you're not going to get this far before it stops working.
		 */
		sz = efidp_make_acpi_hid(buf+off, size?size-off:0,
				 EFIDP_ACPI_PCI_ROOT_HID,
				 info->pci_root.root_pci_bus);
		if (sz < 0)
			goto err;
		off += sz;
	}

	struct list_head *pos, *n;
	list_for_each_safe(pos, n, &pci_parent_list) {
		dev = list_entry(pos, struct device, node);
		sz = efidp_make_pci(buf+off, size?size-off:0, dev->pci_dev->dev,
				    dev->pci_dev->func);
		if (sz < 0)
			goto err;
		off += sz;
		list_del(&dev->node);
	}

	sz = efidp_make_pci(buf+off, size?size-off:0, info->pci_dev.pci_device,
			    info->pci_dev.pci_function);
	if (sz < 0)
		goto err;
	off += sz;

	switch (info->interface_type) {
	case nvme:
		{
			uint32_t ns_id=0;
			int rc = eb_nvme_ns_id(fd, &ns_id);
			if (rc < 0)
				goto err;

			sz = efidp_make_nvme(buf+off, size?size-off:0,
					     ns_id, NULL);
			if (sz < 0)
				goto err;
			off += sz;
			break;
		}
	case virtblk:
		break;
	case sata:
		{
			uint8_t host = 0, channel = 0, id = 0, lun = 0;
			int rc = eb_scsi_idlun(fd, &host, &channel,
					       &id, &lun);
			if (rc < 0)
				goto err;
			sz = efidp_make_sata(buf+off, size?size-off:0,
					     info->sata_info.ata_port,
					     info->sata_info.ata_pmp,
					     info->sata_info.ata_devno);
			if (sz < 0)
				goto err;
			off += sz;
			break;
		}
	case sas:
		break;
	default:
		{
			uint8_t host = 0, channel = 0, id = 0, lun = 0;
			int rc = eb_scsi_idlun(fd, &host, &channel,
					       &id, &lun);
			if (rc < 0)
				goto err;
			sz = efidp_make_scsi(buf+off, size?size-off:0,
					     id, lun);
			if (sz < 0)
				goto err;
			off += sz;
		}
	}
	ret = off;
	errno = 0;
err:
	if (pacc)
		pci_cleanup(pacc);

	return ret;
}

static int
open_disk(struct disk_info *info, int flags)
{
	char diskname[PATH_MAX+1] = "";
	char diskpath[PATH_MAX+1] = "";
	int rc;

	rc = get_disk_name(info->major, info->minor, diskname, PATH_MAX);
	if (rc < 0)
		return -1;

	strcpy(diskpath, "/dev/");
	strncat(diskpath, diskname, PATH_MAX - 5);
	return open(diskpath, flags);
}

static ssize_t
make_the_whole_path(uint8_t *buf, size_t size, int fd, struct disk_info *info,
		    char *devpath, char *filepath, uint32_t options)
{
	ssize_t ret=-1;
	ssize_t off=0, sz;

	if ((options & EFIBOOT_ABBREV_EDD10)
			&& (!(options & EFIBOOT_ABBREV_FILE)
			    && !(options & EFIBOOT_ABBREV_HD))) {
		sz = efidp_make_edd10(buf, size, info->edd10_devicenum);
		if (sz < 0)
			return -1;
		off = sz;
	} else if (!(options & EFIBOOT_ABBREV_FILE)
		   && !(options & EFIBOOT_ABBREV_HD)) {
		sz = make_pci_path(buf, size, fd, info, devpath);
		if (sz < 0)
			return -1;
		off = sz;
	}

	if (!(options & EFIBOOT_ABBREV_FILE)) {
		int disk_fd = open_disk(info,
		    (options& EFIBOOT_OPTIONS_WRITE_SIGNATURE)?O_RDWR:O_RDONLY);
		int saved_errno;
		if (disk_fd < 0)
			goto err;

		sz = make_hd_dn(buf, size, off, disk_fd, info->part, options);
		saved_errno = errno;
		close(disk_fd);
		errno = saved_errno;
		if (sz < 0)
			goto err;
		off += sz;
	}

	sz = efidp_make_file(buf+off, size?size-off:0, filepath);
	if (sz < 0)
		goto err;
	off += sz;

	sz = efidp_make_end_entire(buf+off, size?size-off:0);
	if (sz < 0)
		goto err;
	off += sz;

	ret = off;
err:
	return ret;
}

ssize_t
__attribute__((__nonnull__ (3)))
__attribute__((__visibility__ ("default")))
efi_generate_file_device_path(uint8_t *buf, ssize_t size,
			      const char const *filepath,
			      uint32_t options, ...)
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

	fd = open(devpath, O_RDONLY);
	if (fd < 0)
		goto err;

	rc = eb_disk_info_from_fd(fd, &info);
	if (rc < 0)
		goto err;

	if (options & EFIBOOT_ABBREV_EDD10) {
		va_list ap;
		va_start(ap, options);

		info.edd10_devicenum = va_arg(ap, uint32_t);

		va_end(ap);
	}

	ret = make_the_whole_path(buf, size, fd, &info, devpath,
				  relpath, options);
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
			       uint32_t options)
{
	errno = ENOSYS;
	return -1;
}
