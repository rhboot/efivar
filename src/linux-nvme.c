// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2019 Red Hat, Inc.
 */

#include "fix_coverity.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <sys/param.h>
#include <unistd.h>

#include "efiboot.h"

/*
 * support for NVMe devices
 *
 * /sys/dev/block/$major:$minor looks like:
 * 259:0 -> ../../devices/pci0000:00/0000:00:1d.0/0000:05:00.0/nvme/nvme0/nvme0n1
 * 259:1 -> ../../devices/pci0000:00/0000:00:1d.0/0000:05:00.0/nvme/nvme0/nvme0n1/nvme0n1p1
 * or:
 * 259:0 ->../../devices/virtual/nvme-fabrics/ctl/nvme0/nvme0n1
 * 259:1 ->../../devices/virtual/nvme-fabrics/ctl/nvme0/nvme0n1/nvme0n1p1
 * or:
 * 259:5 -> ../../devices/virtual/nvme-subsystem/nvme-subsys0/nvme0n1
 * 259:6 -> ../../devices/virtual/nvme-subsystem/nvme-subsys0/nvme0n1/nvme0n1p1
 *
 * /sys/dev/block/259:0/device looks like:
 * device -> ../../nvme0
 *
 * /sys/dev/block/259:1/partition looks like:
 * $ cat partition
 * 1
 *
 * /sys/class/block/nvme0n1/eui looks like:
 * $ cat /sys/class/block/nvme0n1/eui
 * 00 25 38 53 5a 16 1d a9
 */

static ssize_t
parse_nvme(struct device *dev, const char *path, const char *root UNUSED)
{
	const char *current = path;
	int i, rc;
	int32_t tosser0, tosser1, tosser2, ctrl_id, ns_id, partition;
	uint8_t *filebuf = NULL;
	int pos0 = -1, pos1 = -1, pos2 = -1;
	struct subdir {
		const char * const name;
		const char * const fmt;
		int *pos0, *pos1;
	} subdirs[] = {
		{"nvme-subsysN/", "%nnvme-subsys%d/%n", &pos0, &pos2},
		{"ctl/", "%nctl/%n%n", &pos0, &pos1},
		{"nvme/", "%nnvme/%n%n", &pos0, &pos1},
		{NULL, }
	};

	debug("entry");

	/*
	 * in this case, *any* of these is okay.
	 */
	for (i = 0; subdirs[i].name; i++) {
		debug("searching for %s", subdirs[i].name);
		pos0 = tosser0 = pos1 = -1;
		rc = sscanf(current, subdirs[i].fmt, &pos0, &pos1, &pos2);
		debug("current:'%s' rc:%d pos0:%d pos1:%d\n", current, rc,
		      *subdirs[i].pos0, *subdirs[i].pos1);
		dbgmk("         ", *subdirs[i].pos0, *subdirs[i].pos1);
		if (*subdirs[i].pos0 >= 0 && *subdirs[i].pos1 >= *subdirs[i].pos0) {
			current += *subdirs[i].pos1;
			break;
		}
	}

	if (!subdirs[i].name)
		return 0;

	debug("searching for nvme-subsysN/");
	if (!strncmp("nvme-subsysN/", subdirs[i].name, 13)) {
		debug("searching for nvme0n1");
		rc = sscanf(current, "%nnvme%dn%d%n",
			    &pos0, &ctrl_id, &ns_id, &pos1);
		debug("current:'%s' rc:%d pos0:%d pos1:%d\n", current, rc, pos0, pos1);
		dbgmk("         ", pos0, pos1);
		if (rc != 2)
			return 0;
	} else {
		debug("searching for nvme0/nvme0n1 or nvme0/nvme0n1/nvme0n1p1");
		rc = sscanf(current, "%nnvme%d/nvme%dn%d%n/nvme%dn%dp%d%n",
			    &pos0, &tosser0, &ctrl_id, &ns_id, &pos1,
			    &tosser1, &tosser2, &partition, &pos2);
		debug("current:'%s' rc:%d pos0:%d pos1:%d pos2:%d\n", current, rc, pos0, pos1, pos2);
		dbgmk("         ", pos0, MAX(pos1,pos2));
		/*
		 * If it isn't of that form, it's not one of our nvme devices.
		 */
		if (rc != 3 && rc != 6)
			return 0;
		if (rc == 3)
			pos2 = pos1;
	}

	dev->nvme_info.ctrl_id = ctrl_id;
	dev->nvme_info.ns_id = ns_id;
	dev->nvme_info.has_eui = 0;
	dev->interface_type = nvme;

	if (rc == 6) {
	        if (dev->part == -1)
	                dev->part = partition;

		pos1 = pos2;
	}

	current += pos1;

	/*
	 * now fish the eui out of sysfs is there is one...
	 */
	debug("looking for the eui");
	char *euipath = NULL;
	rc = read_sysfs_file(&filebuf, "class/block/nvme%dn%d/eui", ctrl_id, ns_id);
	if (rc < 0 && (errno == ENOENT || errno == ENOTDIR)) {
		rc = find_device_file(&euipath, "eui", "class/block/nvme%dn%d", ctrl_id, ns_id);
		if (rc >= 0 && euipath != NULL)
			rc = read_sysfs_file(&filebuf, "%s", euipath);
	}
	if (rc >= 0 && filebuf != NULL) {
	        uint8_t eui[8];
	        if (rc < 23) {
	                errno = EINVAL;
	                return -1;
	        }
	        rc = sscanf((char *)filebuf,
	                    "%02hhx %02hhx %02hhx %02hhx "
	                    "%02hhx %02hhx %02hhx %02hhx",
	                    &eui[0], &eui[1], &eui[2], &eui[3],
	                    &eui[4], &eui[5], &eui[6], &eui[7]);
	        if (rc < 8) {
	                errno = EINVAL;
	                return -1;
	        }
		debug("eui is %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
		      eui[0], eui[1], eui[2], eui[3],
		      eui[4], eui[5], eui[6], eui[7]);
	        dev->nvme_info.has_eui = 1;
	        memcpy(dev->nvme_info.eui, eui, sizeof(eui));
	}

	debug("current:'%s' sz:%zd", current, current - path);
	return current - path;
}

static ssize_t
dp_create_nvme(struct device *dev,
	       uint8_t *buf,  ssize_t size, ssize_t off)
{
	ssize_t sz;

	debug("entry");

	sz = efidp_make_nvme(buf + off, size ? size - off : 0,
	                     dev->nvme_info.ns_id,
	                     dev->nvme_info.has_eui ? dev->nvme_info.eui
	                                                : NULL);
	return sz;
}

static char *
make_part_name(struct device *dev)
{
	char *ret = NULL;
	ssize_t rc;

	if (dev->part < 1)
	        return NULL;

	rc = asprintf(&ret, "%sp%d", dev->disk_name, dev->part);
	if (rc < 0) {
	        efi_error("could not allocate memory");
	        return NULL;
	}

	return ret;
}

static enum interface_type nvme_iftypes[] = { nvme, unknown };

struct dev_probe HIDDEN nvme_parser = {
	.name = "nvme",
	.iftypes = nvme_iftypes,
	.flags = DEV_PROVIDES_HD,
	.parse = parse_nvme,
	.create = dp_create_nvme,
	.make_part_name = make_part_name,
};

// vim:fenc=utf-8:tw=75:noet
