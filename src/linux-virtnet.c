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
 * support for virtio net devices
 * /sys/class/net/enp1s0 look like:
 * enp1s0 -> ../../devices/pci0000:00/0000:00:02.0/0000:01:00.0/virtio1/net/enp1s0
 *
 * PciRoot(0x0)/Pci(0x2,0x0)/Pci(0x0,0x0)/...
 */
static ssize_t
parse_virtnet(struct device *dev, const char *path, const char *root UNUSED)
{
	const char *current = path;
	uint32_t tosser;
	int pos0 = -1, pos1 = -1, pos2 = -1;
	int rc;

	debug("entry");

	debug("searching for virtio0/");
	rc = sscanf(current, "%nvirtio%x/%nnet/%n", &pos0, &tosser, &pos1, &pos2);
	debug("current:'%s' rc:%d pos0:%d pos1:%d pos2:%d\n", current, rc, pos0, pos1, pos2);
	dbgmk("         ", pos0, pos1, pos2);
	/*
	 * If we couldn't find virtioX/net/ then it isn't a virtio device.
	 */
	if ((rc < 1) || (pos2 == -1))
	        return 0;

	dev->interface_type = network;
	current += pos1;

	debug("current:'%s' sz:%zd\n", current, current - path);
	return current - path;
}

enum interface_type virtnet_iftypes[] = { network, unknown };

struct dev_probe HIDDEN virtnet_parser = {
	.name = "virtio net",
	.iftypes = virtnet_iftypes,
	.parse = parse_virtnet,
	.create = NULL,
};

// vim:fenc=utf-8:tw=75:noet
