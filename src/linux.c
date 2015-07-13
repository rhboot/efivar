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
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <linux/ethtool.h>
#include <linux/nvme.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <scsi/scsi.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

#include <efivar.h>
#include <efiboot.h>

#include "dp.h"
#include "linux.h"
#include "util.h"

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

int
__attribute__((__visibility__ ("hidden")))
set_disk_and_part_name(struct disk_info *info)
{
	char *linkbuf;
	ssize_t rc;

	rc = sysfs_readlink(&linkbuf, "/sys/dev/block/%"PRIu64":%hhd",
		      info->major, info->minor);
	if (rc < 0)
		return -1;

	char *ultimate;
	ultimate = strrchr(linkbuf, '/');
	if (!ultimate) {
		errno = EINVAL;
		return -1;
	}
	*ultimate = '\0';
	ultimate++;

	char *penultimate;
	penultimate = strrchr(linkbuf, '/');
	if (!penultimate) {
		errno = EINVAL;
		return -1;
	}
	penultimate++;

	if (!strcmp(penultimate, "block")) {
		if (!info->disk_name) {
			info->disk_name = strdup(ultimate);
			if (!info->disk_name)
				return -1;
		}
		if (!info->part_name) {
			rc = asprintf(&info->part_name, "%s%d", info->disk_name,
				      info->part);
			if (rc < 0)
				return -1;
		}
	} else {
		if (!info->disk_name) {
			info->disk_name = strdup(penultimate);
			if (!info->disk_name)
				return -1;
		}
		if (!info->part_name) {
			info->part_name = strdup(ultimate);
			if (!info->part_name)
				return -1;
		}
	}

	return 0;
}

int
__attribute__((__visibility__ ("hidden")))
get_partition_number(const char *devpath)
{
	struct stat statbuf = { 0, };
	int rc;
	unsigned int maj, min;
	char *linkbuf;
	char *partbuf;
	int ret = -1;

	rc = stat(devpath, &statbuf);
	if (rc < 0)
		return -1;

	if (!S_ISBLK(statbuf.st_mode)) {
		errno = EINVAL;
		return -1;
	}

	maj = gnu_dev_major(statbuf.st_rdev);
	min = gnu_dev_minor(statbuf.st_rdev);

	rc = sysfs_readlink(&linkbuf, "/sys/dev/block/%u:%u", maj, min);
	if (rc < 0)
		return -1;

	rc = read_sysfs_file(&partbuf, "/sys/dev/block/%s/partition", linkbuf);
	if (rc < 0)
		return -1;

	rc = sscanf(partbuf, "%d\n", &ret);
	if (rc != 1)
		return -1;
	return ret;
}

static int
sysfs_test_sata(const char *buf, ssize_t size)
{
	if (!strncmp(buf, "ata", MIN(size,3)))
		return 1;
	return 0;
}

static int
sysfs_test_sas(const char *buf, ssize_t size, struct disk_info *info)
{
	int rc;
	char *path;
	struct stat statbuf = { 0, };

	int host;
	int sz;

	errno = 0;
	rc = sscanf(buf, "host%d/%n", &host, &sz);
	if (rc < 1)
		return (errno == 0) ? 0 : -1;

	rc = asprintfa(&path, "/sys/class/scsi_host/host%d/host_sas_address",
			host);
	if (rc < 0)
		return -1;

	rc = stat(path, &statbuf);
	if (rc >= 0)
		return 1;
	return 0;
}

static ssize_t
sysfs_sata_get_port_info(uint32_t print_id, struct disk_info *info)
{
	DIR *d;
	struct dirent *de;
	int saved_errno;
	uint8_t *buf = NULL;
	int rc;

	d = opendir("/sys/class/ata_device/");
	if (!d)
		return -1;

	while ((de = readdir(d)) != NULL) {
		uint32_t found_print_id;
		uint32_t found_pmp;
		uint32_t found_devno = 0;

		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;

		int rc;
		rc = sscanf(de->d_name, "dev%d.%d.%d", &found_print_id,
			    &found_pmp, &found_devno);
		if (rc == 2) {
			found_devno = found_pmp;
			found_pmp=0;
		} else if (rc != 3) {
			saved_errno = errno;
			closedir(d);
			errno = saved_errno;
			return -1;
		}
		if (found_print_id == print_id) {
			info->sata_info.ata_devno = found_devno;
			info->sata_info.ata_pmp = found_pmp;
			break;
		}
	}
	closedir(d);

	rc = read_sysfs_file(&buf, "/sys/class/ata_port/ata%d/port_no",
			     print_id);
	if (rc <= 0)
		return -1;

	rc = sscanf((char *)buf, "%d", &info->sata_info.ata_port);
	if (rc != 1)
		return -1;

	info->sata_info.ata_port -= 1;
	return 0;
}

static ssize_t
sysfs_parse_sata(uint8_t *buf, ssize_t size, ssize_t *off,
		 const char *pbuf, ssize_t psize, ssize_t *poff,
		 struct disk_info *info)
{
	int psz = 0;
	int rc;

	*poff = 0;
	*off = 0;

	uint32_t print_id;

	uint32_t scsi_bus;
	uint32_t scsi_device;
	uint32_t scsi_target;
	uint32_t scsi_lun;

	/* find the ata info:
	 * ata1/host0/target0:0:0/
	 *    ^dev  ^host   x y z
	 */
	rc = sscanf(pbuf, "ata%d/host%d/target%d:%d:%d/%n",
		    &print_id, &scsi_bus, &scsi_device, &scsi_target, &scsi_lun,
		    &psz);
	if (rc != 5)
		return -1;
	*poff += psz;

	/* find the emulated scsi bits (and ignore them)
	 * 0:0:0:0/
	 */
	uint32_t dummy0, dummy1, dummy2;
	uint64_t dummy3;
	rc = sscanf(pbuf+*poff, "%d:%d:%d:%"PRIu64"/%n", &dummy0, &dummy1,
		    &dummy2, &dummy3, &psz);
	if (rc != 4)
		return -1;
	*poff += psz;

	/* what's left is:
	 * block/sda/sda4
	 */
	char *disk_name = NULL;
	char *part_name = NULL;
	int psz1 = 0;
	rc = sscanf(pbuf+*poff, "block/%m[^/]%n/%m[^/]%n", &disk_name, &psz,
		    &part_name, &psz1);
	if (rc == 1) {
		rc = asprintf(&part_name, "%s%d", disk_name, info->part);
		if (rc < 0) {
			free(disk_name);
			errno = EINVAL;
			return -1;
		}
		*poff += psz;
	} else if (rc != 2) {
		errno = EINVAL;
		return -1;
	} else {
		*poff += psz1;
	}

	info->sata_info.scsi_bus = scsi_bus;
	info->sata_info.scsi_device = scsi_device;
	info->sata_info.scsi_target = scsi_target;
	info->sata_info.scsi_lun = scsi_lun;

	rc = sysfs_sata_get_port_info(print_id, info);
	if (rc < 0) {
		free(disk_name);
		free(part_name);
		return -1;
	}

	if (pbuf[*poff] != '\0') {
		free(disk_name);
		free(part_name);
		errno = EINVAL;
		return -1;
	}

	info->disk_name = disk_name;
	info->part_name = part_name;
	if (info->interface_type == interface_type_unknown)
		info->interface_type = sata;

	if (info->interface_type == ata) {
		*off = efidp_make_atapi(buf, size, info->sata_info.ata_port,
					info->sata_info.ata_pmp,
					info->sata_info.ata_devno);
	} else {
		*off = efidp_make_sata(buf, size, info->sata_info.ata_port,
				       info->sata_info.ata_pmp,
				       info->sata_info.ata_devno);
	}
	return *off;
}

static ssize_t
sysfs_parse_sas(uint8_t *buf, ssize_t size, ssize_t *off,
		const char *pbuf, ssize_t psize, ssize_t *poff,
		struct disk_info *info)
{
	int rc;
	int psz = 0;
	char *filebuf = NULL;
	uint64_t sas_address;

	*poff = 0;
	*off = 0;

	/* buf is:
	 * host4/port-4:0/end_device-4:0/target4:0:0/4:0:0:0/block/sdc/sdc1
	 */
	uint32_t tosser0, tosser1, tosser2;

	/* ignore a bunch of stuff
	 *    host4/port-4:0
	 * or host4/port-4:0:0
	 */
	rc = sscanf(pbuf+*poff, "host%d/port-%d:%d%n", &tosser0, &tosser1,
		    &tosser2, &psz);
	if (rc != 3)
		return -1;
	*poff += psz;

	psz = 0;
	rc = sscanf(pbuf+*poff, ":%d%n", &tosser0, &psz);
	if (rc != 0 && rc != 1)
		return -1;
	*poff += psz;

	/* next:
	 *    /end_device-4:0
	 * or /end_device-4:0:0
	 * awesomely these are the exact same fields that go into port-blah,
	 * but we don't care for now about any of them anyway.
	 */
	rc = sscanf(pbuf+*poff, "/end_device-%d:%d%n", &tosser0, &tosser1,
		    &psz);
	if (rc != 2)
		return -1;
	*poff += psz;

	psz = 0;
	rc = sscanf(pbuf+*poff, ":%d%n", &tosser0, &psz);
	if (rc != 0 && rc != 1)
		return -1;
	*poff += psz;

	/* now:
	 * /target4:0:0/
	 */
	uint64_t tosser3;
	rc = sscanf(pbuf+*poff, "/target%d:%d:%"PRIu64"/%n", &tosser0, &tosser1,
		    &tosser3, &psz);
	if (rc != 3)
		return -1;
	*poff += psz;

	/* now:
	 * %d:%d:%d:%llu/
	 */
	rc = sscanf(pbuf+*poff, "%d:%d:%d:%"PRIu64"/%n",
		    &info->sas_info.scsi_bus,
		    &info->sas_info.scsi_device,
		    &info->sas_info.scsi_target,
		    &info->sas_info.scsi_lun, &psz);
	if (rc != 4)
		return -1;
	*poff += psz;

	/* what's left is:
	 * block/sdc/sdc1
	 */
	char *disk_name = NULL;
	char *part_name = NULL;
	rc = sscanf(pbuf+*poff, "block/%m[^/]/%m[^/]%n", &disk_name, &part_name,
		    &psz);
	if (rc != 2)
		return -1;
	*poff += psz;

	if (pbuf[*poff] != '\0') {
		free(disk_name);
		free(part_name);
		errno = EINVAL;
		return -1;
	}

	/*
	 * we also need to get the actual sas_address from someplace...
	 */
	rc = read_sysfs_file(&filebuf,
			     "/sys/class/block/%s/device/sas_address",
			     disk_name);
	if (rc < 0)
		return -1;

	rc = sscanf(filebuf, "%"PRIx64, &sas_address);
	if (rc != 1)
		return -1;

	info->sas_info.sas_address = sas_address;
	info->disk_name = disk_name;
	info->part_name = part_name;
	info->interface_type = sas;

	*off = efidp_make_sas(buf, size, sas_address);
	return *off;
}

static ssize_t
make_pci_path(uint8_t *buf, ssize_t size, char *pathstr, ssize_t *pathoff)
{
	ssize_t off=0, sz=0;
	ssize_t poff = 0;
	int psz;
	int rc;

	if (pathstr == NULL || pathoff == NULL || pathstr[0] == '\0') {
		errno = EINVAL;
		return -1;
	}

	*pathoff = 0;

	uint16_t root_domain;
	uint8_t root_bus;
	uint32_t acpi_hid = 0;
	uint64_t acpi_uid_int = 0;
	/*
	 * find the pci root domain and port; they basically look like:
	 * pci0000:00/
	 *    ^d   ^p
	 */
	rc = sscanf(pathstr+poff, "pci%hx:%hhx/%n", &root_domain,
		    &root_bus, &psz);
	if (rc != 2)
		return -1;
	poff += psz;

	char *fbuf = NULL;
	rc = read_sysfs_file(&fbuf,
			     "/sys/devices/pci%04x:%02x/firmware_node/hid",
			     root_domain, root_bus);
	if (rc < 0)
		return -1;

	uint16_t tmp16 = 0;
	rc = sscanf((char *)fbuf, "PNP%hx", &tmp16);
	if (rc != 1)
		return -1;
	acpi_hid = EFIDP_EFI_PNP_ID(tmp16);

	/* Apparently basically nothing can look up a PcieRoot() node,
	 * because they just check _CID.  So since _CID for the root pretty
	 * much always has to be PNP0A03 anyway, just use that no matter
	 * what.
	 */
	if (acpi_hid == EFIDP_ACPI_PCIE_ROOT_HID)
		acpi_hid = EFIDP_ACPI_PCI_ROOT_HID;

	errno = 0;
	fbuf = NULL;
	int use_uid_str = 0;
	rc = read_sysfs_file(&fbuf,
			     "/sys/devices/pci%4x:%02x/firmware_node/uid",
			     root_domain, root_bus);
	if (rc <= 0 && errno != ENOENT)
		return -1;
	if (rc > 0) {
		rc = sscanf((char *)fbuf, "%"PRIu64"\n", &acpi_uid_int);
		if (rc != 1) {
			/* kernel uses "%s\n" to print it, so there
			 * should always be some value and a newline... */
			int l = strlen((char *)buf);
			if (l >= 1) {
				use_uid_str=1;
				fbuf[l-1] = '\0';
			}
		}
	}
	errno = 0;

	if (use_uid_str) {
		sz = efidp_make_acpi_hid_ex(buf+off, size?size-off:0,
					    acpi_hid, 0, 0, "", (char *)fbuf,
					    "");
	} else {
		sz = efidp_make_acpi_hid(buf+off, size?size-off:0,
					 acpi_hid, acpi_uid_int);
	}
	if (sz < 0)
		return -1;
	off += sz;

	/* find the pci domain/bus/device/function:
	 * 0000:00:01.0/0000:01:00.0/
	 *              ^d   ^b ^d ^f (of the last one in the series)
	 */
	int found=0;
	while (1) {
		uint16_t domain;
		uint8_t bus, device, function;
		rc = sscanf(pathstr+poff, "%hx:%hhx:%hhx.%hhx/%n",
			    &domain, &bus, &device, &function, &psz);
		if (rc != 4)
			break;
		found=1;
		poff += psz;

		sz = efidp_make_pci(buf+off, size?size-off:0, device, function);
		if (sz < 0)
			return -1;
		off += sz;
	}
	if (!found)
		return -1;

	*pathoff = poff;
	return off;
}

int
__attribute__((__visibility__ ("hidden")))
make_blockdev_path(uint8_t *buf, ssize_t size, int fd, struct disk_info *info)
{
	char *linkbuf = NULL;
	char *driverbuf = NULL;
	ssize_t off=0, sz=0, loff=0;
	int lsz = 0;
	int rc;
	int found = 0;

	rc = sysfs_readlink(&linkbuf, "/sys/dev/block/%"PRIu64":%u",
			    info->major, info->minor);
	if (rc < 0)
		return -1;

	/*
	 * the sysfs path basically looks like:
	 * ../../devices/pci$PCI_STUFF/$BLOCKDEV_STUFF/block/$DISK/$PART
	 */
	rc = sscanf(linkbuf+loff, "../../devices/%n", &lsz);
	if (rc != 0)
		return -1;
	loff += lsz;

	ssize_t tmplsz=0;
	sz = make_pci_path(buf, size, linkbuf+loff, &tmplsz);
	if (sz < 0)
		return -1;
	loff += tmplsz;
	off += sz;

	char *tmppath = strdupa(linkbuf);
	if (!tmppath)
		return -1;
	tmppath[loff] = '\0';
	rc = sysfs_readlink(&driverbuf, "/sys/dev/block/%s/driver", tmppath);
	if (rc < 0)
		return -1;

	char *driver = strrchr(driverbuf, '/');
	if (!driver || !*driver)
		return -1;
	driver+=1;

	if (!strncmp(driver, "pata_", 5) || !(strcmp(driver, "ata_piix")))
		info->interface_type = ata;

	if (info->interface_type == interface_type_unknown ||
	    info->interface_type == atapi ||
	    info->interface_type == usb ||
	    info->interface_type == i1394 ||
	    info->interface_type == fibre ||
	    info->interface_type == i2o ||
	    info->interface_type == md) {
		uint32_t tosser;
		int tmpoff;

		rc = sscanf(linkbuf+loff, "virtio%x/%n", &tosser, &tmpoff);
		if (rc < 0) {
			return -1;
		} else if (rc == 1) {
			info->interface_type = virtblk;
			loff += tmpoff;
			found = 1;
		}

		if (!found) {
			uint32_t ns_id=0;
			int rc = eb_nvme_ns_id(fd, &ns_id);
			if (rc >= 0) {
				sz = efidp_make_nvme(buf+off, size?size-off:0,
						     ns_id, NULL);
				if (sz < 0)
					return -1;

				info->interface_type = nvme;
				off += sz;
				found = 1;
			}
		}
	}

	/* /dev/sda as SATA looks like:
	 * /sys/dev/block/8:0 -> ../../devices/pci0000:00/0000:00:1f.2/ata1/host0/target0:0:0/0:0:0:0/block/sda
	 */
	if (!found) {
		rc = sysfs_test_sata(linkbuf+loff, PATH_MAX-off);
		if (rc < 0)
			return -1;
		if (!found && rc > 0) {
			ssize_t linksz=0;
			rc = sysfs_parse_sata(buf+off, size?size-off:0, &sz,
					      linkbuf+loff, PATH_MAX-off,
					      &linksz, info);
			if (rc < 0)
				return -1;
			loff += linksz;
			off += sz;
			found = 1;
		}
	}

	/* /dev/sdc as SAS looks like:
	 * /sys/dev/block/8:32 -> ../../devices/pci0000:00/0000:00:01.0/0000:01:00.0/host4/port-4:0/end_device-4:0/target4:0:0/4:0:0:0/block/sdc
	 */
	if (!found) {
		rc = sysfs_test_sas(linkbuf+loff, PATH_MAX-off, info);
		if (rc < 0)
			return -1;
		else if (rc > 0) {
			ssize_t linksz=0;
			rc = sysfs_parse_sas(buf+off, size?size-off:0, &sz,
					     linkbuf+loff, PATH_MAX-off,
					     &linksz, info);
			if (rc < 0)
				return -1;
			loff += linksz;
			off += sz;
			found = 1;
		}
	}

	if (!found && info->interface_type == scsi) {
		char *linkbuf;

		rc = sysfs_readlink(&linkbuf, "/sys/class/block/%s/device",
			      info->disk_name);
		if (rc < 0)
			return 0;

		rc = sscanf(linkbuf, "../../../%d:%d:%d:%"PRIu64,
			    &info->scsi_info.scsi_bus,
			    &info->scsi_info.scsi_device,
			    &info->scsi_info.scsi_target,
			    &info->scsi_info.scsi_lun);
		if (rc != 4)
			return -1;

		sz = efidp_make_scsi(buf+off, size?size-off:0,
				     info->scsi_info.scsi_target,
				     info->scsi_info.scsi_lun);
		if (sz < 0)
			return -1;
		off += sz;
		found = 1;
	}

	if (!found) {
		errno = ENOENT;
		return -1;
	}

	return off;
}

int
__attribute__((__visibility__ ("hidden")))
eb_disk_info_from_fd(int fd, struct disk_info *info)
{
	struct stat buf;
	int rc;

	memset(info, 0, sizeof *info);

	info->pci_root.root_pci_domain = 0xffff;
	info->pci_root.root_pci_bus = 0xff;

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

	errno = ENOSYS;
	return -1;
}

static ssize_t
make_net_pci_path(uint8_t *buf, ssize_t size, const char * const ifname)
{
	char *linkbuf = NULL;
	ssize_t off=0, sz=0, loff=0;
	int lsz = 0;
	int rc;

	rc = sysfs_readlink(&linkbuf, "/sys/class/net/%s", ifname);
	if (rc < 0)
		return -1;

	/*
	 * the sysfs path basically looks like:
	 * ../../devices/$PCI_STUFF/net/$IFACE
	 */
	rc = sscanf(linkbuf+loff, "../../devices/%n", &lsz);
	if (rc != 0)
		return -1;
	loff += lsz;

	ssize_t tmplsz = 0;
	sz = make_pci_path(buf, size, linkbuf+loff, &tmplsz);
	if (sz < 0)
		return -1;
	off += sz;
	loff += tmplsz;

	return off;
}

ssize_t
__attribute__((__visibility__ ("hidden")))
make_mac_path(uint8_t *buf, ssize_t size, const char * const ifname)
{
	struct ifreq ifr = { 0, };
	struct ethtool_drvinfo drvinfo = { 0, };
	int fd, rc;
	ssize_t ret = -1, sz, off=0;
	char busname[PATH_MAX+1] = "";

	strncpy(ifr.ifr_name, ifname, IF_NAMESIZE);
	drvinfo.cmd = ETHTOOL_GDRVINFO;
	ifr.ifr_data = (caddr_t)&drvinfo;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return -1;

	rc = ioctl(fd, SIOCETHTOOL, &ifr);
	if (rc < 0)
		goto err;

	strncpy(busname, drvinfo.bus_info, PATH_MAX);

	rc = ioctl(fd, SIOCGIFHWADDR, &ifr);
	if (rc < 0)
		goto err;

	sz = make_net_pci_path(buf, size, ifname);
	if (sz < 0)
		goto err;
	off += sz;

	sz = efidp_make_mac_addr(buf+off, size?size-off:0,
				 ifr.ifr_ifru.ifru_hwaddr.sa_family,
				 (uint8_t *)ifr.ifr_ifru.ifru_hwaddr.sa_data,
				 sizeof(ifr.ifr_ifru.ifru_hwaddr.sa_data));
	if (sz < 0)
		return -1;
	off += sz;
	ret = off;
err:
	if (fd >= 0)
		close(fd);
	return ret;
}
