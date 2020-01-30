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
#include <unistd.h>

#include "efiboot.h"

/*
 * support for virtio block devices
 * /sys/dev/block/maj:min look like:
 * 252:0 -> ../../devices/pci0000:00/0000:00:07.0/virtio2/block/vda
 * 252:1 -> ../../devices/pci0000:00/0000:00:07.0/virtio2/block/vda/vda1
 *
 * vda/device looks like:
 * device -> ../../../virtio2
 *
 * In this case we get 1 storage controller per pci device, so we actually
 * write the device path as:
 *
 * PciRoot(0x0)/Pci(0x7,0x0)/HD(1,GPT,14d30998-b5e3-4b8f-9be3-58a454dd06bf,0x800,0x53000)/File(\EFI\fedora\shimx64.efi)
 *
 * But usually we just write the HD() entry, of course.
 */
static ssize_t
parse_virtblk(struct device *dev, const char *path, const char *root UNUSED)
{
	const char *current = path;
	uint32_t tosser;
	int pos0 = -1, pos1 = -1;
	int rc;

	debug("entry");

	debug("searching for virtio0/");
	rc = sscanf(current, "%nvirtio%x/%n", &pos0, &tosser, &pos1);
	debug("current:'%s' rc:%d pos0:%d pos1:%d\n", current, rc, pos0, pos1);
	dbgmk("         ", pos0, pos1);
	/*
	 * If we couldn't find virtioX/ then it isn't a virtio device.
	 */
	if (rc < 1)
	        return 0;

	dev->interface_type = virtblk;
	current += pos1;

	debug("current:'%s' sz:%zd\n", current, current - path);
	return current - path;
}

enum interface_type virtblk_iftypes[] = { virtblk, unknown };

struct dev_probe HIDDEN virtblk_parser = {
	.name = "virtio block",
	.iftypes = virtblk_iftypes,
	.flags = DEV_PROVIDES_HD,
	.parse = parse_virtblk,
	.create = NULL,
};

// vim:fenc=utf-8:tw=75:noet
