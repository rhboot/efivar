/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2015 Red Hat, Inc.
 * Copyright (C) 2001 Dell Computer Corporation <Matt_Domsch@dell.com>
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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <linux/ethtool.h>
#include <linux/version.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <scsi/scsi.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

#include "efiboot.h"

int HIDDEN
set_disk_and_part_name(struct disk_info *info)
{
	char *linkbuf;
	ssize_t rc;

	rc = sysfs_readlink(&linkbuf, "dev/block/%"PRIu64":%"PRIu32,
		      info->major, info->minor);
	if (rc < 0 || !linkbuf)
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

	/*
	 * If there's a better way to figure this out, it'd be good, because
	 * I don't want to have to change this for every new disk type...
	 */
	if (!strncmp(ultimate, "pmem", 4)) {
		if (!info->disk_name) {
			info->disk_name = strdup(ultimate);
			if (!info->disk_name)
				return -1;
		}
	} else if (!strcmp(penultimate, "block")) {
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

int HIDDEN
get_partition_number(const char *devpath)
{
	struct stat statbuf = { 0, };
	int rc;
	unsigned int maj, min;
	char *linkbuf;
	uint8_t *partbuf = NULL; /* XXX this is wrong and the code below will be wrong */
	int ret = -1;

	rc = stat(devpath, &statbuf);
	if (rc < 0) {
		efi_error("couldn't stat %s\n", devpath);
		return -1;
	}

	if (!S_ISBLK(statbuf.st_mode)) {
		efi_error("%s is not a block device\n", devpath);
		errno = EINVAL;
		return -1;
	}

	maj = major(statbuf.st_rdev);
	min = minor(statbuf.st_rdev);

	rc = sysfs_readlink(&linkbuf, "dev/block/%u:%u", maj, min);
	if (rc < 0 || !linkbuf) {
		efi_error("couldn't get partition number for %u:%u", maj, min);
		return -1;
	}

	rc = read_sysfs_file(&partbuf, "dev/block/%s/partition", linkbuf);
	if (rc < 0) {
		efi_error("couldn't get partition number for %s", linkbuf);
		/* This isn't strictly an error for e.g. nvdimm pmem devices */
		return 0;
	}

	rc = sscanf((char *)partbuf, "%d\n", &ret);
	if (rc != 1) {
		efi_error("couldn't get partition number for %s", partbuf);
		return -1;
	}
	return ret;
}

int HIDDEN
find_parent_devpath(const char * const child, char **parent)
{
	int ret;
	char *node;
	char *linkbuf;

	/* strip leading /dev/ */
	node = strrchr(child, '/');
	if (!node)
		return -1;
	node++;

	/* look up full path symlink */
	ret = sysfs_readlink(&linkbuf, "class/block/%s", node);
	if (ret < 0 || !linkbuf)
		return ret;

	/* strip child */
	node = strrchr(linkbuf, '/');
	if (!node)
		return -1;
	*node = '\0';

	/* read parent */
	node = strrchr(linkbuf, '/');
	if (!node)
		return -1;
	*node = '\0';
	node++;

	/* write out new path */
	ret = asprintf(parent, "/dev/%s", node);
	if (ret < 0)
		return ret;

	return 0;
}

/* NVDIMM-P paths */
static int
sysfs_test_pmem(const char *buf)
{
	char *driverbuf = NULL;
	int rc;

	rc = sysfs_readlink(&driverbuf,
			    "dev/block/%s/device/driver", buf);
	if (rc < 0 || !driverbuf)
		return 0;

	char *driver = strrchr(driverbuf, '/');
	if (!driver || !*driver)
		return -1;
	driver+=1;
	if (!strcmp(driver, "nd_pmem"))
		return 1;
	return 0;
}

/* pmem12s -> ../devices/LNXSYSTM:00/LNXSYBUS:00/ACPI0012:00/ndbus0/region12/btt12.1/block/pmem12s
 * dev: 259:0
 * device -> ../../../btt12.1
 * device/uuid: 0cee166e-dd56-4bc2-99d2-2544b69025b8
 * 259:0 -> ../../devices/LNXSYSTM:00/LNXSYBUS:00/ACPI0012:00/ndbus0/region12/btt12.1/block/pmem12s
 *
 * pmem12.1s -> ../devices/LNXSYSTM:00/LNXSYBUS:00/ACPI0012:00/ndbus0/region12/btt12.2/block/pmem12.1s
 * dev: 259:1
 * device -> ../../../btt12.2
 * device/uuid: 78d94521-91f7-47db-b3a7-51b764281940
 * 259:1 -> ../../devices/LNXSYSTM:00/LNXSYBUS:00/ACPI0012:00/ndbus0/region12/btt12.2/block/pmem12.1s
 *
 * pmem12.2 -> ../devices/LNXSYSTM:00/LNXSYBUS:00/ACPI0012:00/ndbus0/region12/pfn12.1/block/pmem12.2
 * dev: 259:2
 * device -> ../../../pfn12.1
 * device/uuid: 829c5205-89a5-4581-9819-df7d7754c622
 * 259:2 -> ../../devices/LNXSYSTM:00/LNXSYBUS:00/ACPI0012:00/ndbus0/region12/pfn12.1/block/pmem12.2
 */
static ssize_t
sysfs_parse_pmem(uint8_t *buf,  ssize_t size, ssize_t *off,
                 const char *pbuf, ssize_t psize UNUSED,
                 ssize_t *poff UNUSED, struct disk_info *info)
{
	uint8_t *filebuf = NULL;
	int rc;

	rc = read_sysfs_file(&filebuf,
			     "class/block/%s/device/uuid", pbuf);
	if ((rc < 0 && errno == ENOENT) || filebuf == NULL)
		return -1;

	rc = efi_str_to_guid((char *)filebuf, &info->nvdimm_label);
	if (rc < 0)
		return -1;

	/* UUIDs are stored opposite Endian from GUIDs, so our normal GUID
	 * parser is giving us the wrong thing; swizzle those bytes around.
	 */
	swizzle_guid_to_uuid(&info->nvdimm_label);

	*off = efidp_make_nvdimm(buf, size, &info->nvdimm_label);
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

	uint8_t *fbuf = NULL;
	rc = read_sysfs_file(&fbuf,
			     "devices/pci%04hx:%02hhx/firmware_node/hid",
			     root_domain, root_bus);
	if (rc < 0 || fbuf == NULL)
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
			     "devices/pci%04hx:%02hhx/firmware_node/uid",
			     root_domain, root_bus);
	if ((rc <= 0 && errno != ENOENT) || fbuf == NULL)
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

int HIDDEN
make_blockdev_path(uint8_t *buf, ssize_t size, struct disk_info *info)
{
	char *linkbuf = NULL;
	char *driverbuf = NULL;
	ssize_t off=0, sz=0, loff=0;
	int lsz = 0;
	int rc;
	int found = 0;

	rc = sysfs_readlink(&linkbuf, "dev/block/%"PRIu64":%u",
			    info->major, info->minor);
	if (rc < 0 || !linkbuf) {
		efi_error("couldn't read link for /sys/dev/block/%"PRIu64":%u",
			  info->major, info->minor);
		return -1;
	}

	/*
	 * the sysfs path basically looks like one of:
	 * ../../devices/pci$PCI_STUFF/$BLOCKDEV_STUFF/block/$DISK/$PART
	 * ../../devices/LNXSYSTM:00/LNXSYBUS:00/ACPI0012:00/ndbus0/region12/btt12.1/block/pmem12s
	 */
	rc = sysfs_test_pmem(linkbuf+loff);
	if (rc < 0) {
		efi_error("sysfs_test_pmem(\"%s\") failed", linkbuf+loff);
		return -1;
	} else if (rc > 0) {
		ssize_t linksz=0;
		info->interface_type = nd_pmem;
		rc = sysfs_parse_pmem(buf+off, size?size-off:0, &sz,
				      linkbuf+loff, PATH_MAX-off,
				      &linksz, info);
		if (rc < 0)
			return -1;
		loff += linksz;
		off += sz;
		found = 1;
	}

	if (!found) {
		rc = sscanf(linkbuf+loff, "../../devices/%n", &lsz);
		if (rc != 0) {
			efi_error("scanf(\"%s\", %s, &lz) failed",
				  linkbuf+loff, "../../devices/%n");
			return -1;
		}
		loff += lsz;

		ssize_t tmplsz=0;
		sz = make_pci_path(buf, size, linkbuf+loff, &tmplsz);
		if (sz < 0)
			return -1;
		loff += tmplsz;
		off += sz;

		char *tmppath;
		tmppath = strdupa(linkbuf);
		if (!tmppath)
			return -1;
		tmppath[loff] = '\0';
		rc = sysfs_readlink(&driverbuf, "dev/block/%s/driver",
				    tmppath);
		if (rc < 0 || !driverbuf)
			return -1;

		char *driver = strrchr(driverbuf, '/');
		if (!driver || !*driver)
			return -1;
		driver+=1;

	}

	if (!found) {
		errno = ENOENT;
		return -1;
	}

	return off;
}

int HIDDEN
eb_disk_info_from_fd(int fd, struct disk_info *info)
{
	struct stat buf;
	char *driver;
	int rc;

	memset(info, 0, sizeof *info);

	info->pci_root.pci_root_domain = 0xffff;
	info->pci_root.pci_root_bus = 0xff;

	memset(&buf, 0, sizeof(struct stat));
	rc = fstat(fd, &buf);
	if (rc == -1) {
		efi_error("fstat() failed: %m");
		return 1;
	}
	if (S_ISBLK(buf.st_mode)) {
		info->major = major(buf.st_rdev);
		info->minor = minor(buf.st_rdev);
	} else if (S_ISREG(buf.st_mode)) {
		info->major = major(buf.st_dev);
		info->minor = minor(buf.st_dev);
	} else {
		efi_error("Cannot stat non-block or non-regular file");
		return 1;
	}

	rc = sysfs_readlink(&driver, "dev/block/%"PRIu64":%"PRIu32"/device/driver",
			    info->major, info->minor);
	if (rc > 0) {
		char *last = strrchr(driver, '/');
		if (last) {
			if (!strcmp(last+1, "nd_pmem")) {
				info->interface_type = nd_pmem;
				return 0;
#if 0 /* dunno */
			} else if (!strcmp(last+1, "nd_blk")) {
				/* dunno */
				info->inteface_type = scsi;
#endif
			}
		}
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

	rc = sysfs_readlink(&linkbuf, "class/net/%s", ifname);
	if (rc < 0 || !linkbuf)
		return -1;

	/*
	 * the sysfs path basically looks like:
	 * ../../devices/$PCI_STUFF/net/$IFACE
	 */
	rc = sscanf(linkbuf, "../../devices/%n", &lsz);
	if (rc != 0)
		return -1;
	loff += lsz;

	ssize_t tmplsz = 0;
	sz = make_pci_path(buf, size, linkbuf+loff, &tmplsz);
	if (sz < 0)
		return -1;
	off += sz;
	/*
	 * clang-analyzer complains about this because they don't believe in
	 * writing code to avoid introducing bugs later.
	 */
	//loff += tmplsz;

	return off;
}

ssize_t HIDDEN
make_mac_path(uint8_t *buf, ssize_t size, const char * const ifname)
{
	struct ifreq ifr;
	struct ethtool_drvinfo drvinfo = { 0, };
	int fd, rc;
	ssize_t ret = -1, sz, off=0;
	char busname[PATH_MAX+1] = "";

	memset(&ifr, 0, sizeof (ifr));
	strncpy(ifr.ifr_name, ifname, IF_NAMESIZE);
	ifr.ifr_name[IF_NAMESIZE-1] = '\0';
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
		goto err;

	off += sz;
	ret = off;
err:
	if (fd >= 0)
		close(fd);
	return ret;
}

/************************************************************
 * get_sector_size
 * Requires:
 *  - filedes is an open file descriptor, suitable for reading
 * Modifies: nothing
 * Returns:
 *  sector size, or 512.
 ************************************************************/
int UNUSED
get_sector_size(int filedes)
{
        int rc, sector_size = 512;

        rc = ioctl(filedes, BLKSSZGET, &sector_size);
        if (rc)
                sector_size = 512;
        return sector_size;
}
