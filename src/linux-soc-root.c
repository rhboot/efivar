/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2019 Red Hat, Inc.
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

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <unistd.h>

#include "efiboot.h"

/*
 * support for soc platforms
 *
 * various devices /sys/dev/block/$major:$minor start with:
 * maj:min ->  ../../devices/platform/soc/$DEVICETREE_NODE/$BLOCKDEV_STUFF/block/$DISK/$PART
 * i.e.:                              soc/1a400000.sata/ata1/host0/target0:0:0/0:0:0:0/block/sda/sda1
 *                                        ^ dt node     ^ blockdev stuff                     ^ disk
 * I don't *think* the devicetree nodes stack.
 */
static ssize_t
parse_soc_root(struct device *dev UNUSED, const char *path, const char *root UNUSED)
{
	const char *current = path;
	int rc;
	int pos0 = -1, pos1 = -1;

	debug("entry");

	rc = sscanf(current, "../../devices/%nplatform/soc/%*[^/]/%n", &pos0, &pos1);
	if (rc != 0 || pos0 == -1 || pos1 == -1)
	        return 0;
	debug("current:'%s' rc:%d pos0:%d pos1:%d", current, rc, pos0, pos1);
	dbgmk("         ", pos0, pos1);
	current += pos1;

	debug("current:'%s' sz:%zd\n", current, current - path);
	return current - path;
}

enum interface_type soc_root_iftypes[] = { soc_root, unknown };

struct dev_probe HIDDEN soc_root_parser = {
	.name = "soc_root",
	.iftypes = soc_root_iftypes,
	.flags = DEV_ABBREV_ONLY|DEV_PROVIDES_ROOT,
	.parse = parse_soc_root,
};

// vim:fenc=utf-8:tw=75:noet
