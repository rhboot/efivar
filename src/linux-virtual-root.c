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
 */

#include "fix_coverity.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <unistd.h>

#include "efiboot.h"

/*
 * Support virtually rooted devices (fibre+nvme, etc.)
 *
 * /sys/dev/block/$major:$minor looks like:
 * 259:0 ->../../devices/virtual/nvme-fabrics/ctl/nvme0/nvme0n1
 * 259:1 ->../../devices/virtual/nvme-fabrics/ctl/nvme0/nvme0n1/nvme0n1p1
 * or:
 * 259:5 -> ../../devices/virtual/nvme-subsystem/nvme-subsys0/nvme0n1
 * 259:6 -> ../../devices/virtual/nvme-subsystem/nvme-subsys0/nvme0n1/nvme0n1p1
 */

static ssize_t
parse_virtual_root(struct device *dev UNUSED, const char *current, const char *root UNUSED)
{
	int rc;
	ssize_t sz = 0;
	int pos0 = 0, pos1 = 0;
	struct subdir {
		const char * const name;
		const char * const fmt;
	} subdirs[] = {
		{"../../devices/virtual", "%n../../devices/virtual/%n"},
		{"nvme-subsystem/", "%nnvme-subsystem/%n"},
		{"nvme-fabrics/ctl/", "%nnvme-fabrics/ctl/%n"},
		{NULL, NULL}
	};

	debug("entry");

	for (int i = 0; subdirs[i].name; i++) {
		debug("searching for %s", subdirs[i].name);
		pos0 = pos1 = -1;
		rc = sscanf(current, subdirs[i].fmt, &pos0, &pos1);
		debug("current:'%s' rc:%d pos0:%d pos1:%d\n", current, rc, pos0, pos1);
		dbgmk("         ", pos0, pos1);
		if (rc == 1) {
			sz += pos1;
			current += pos1;
			if (i > 0)
				goto found;
		}
	}

	sz = 0;
found:
	debug("current:'%s' sz:%zd\n", current, sz);
	return sz;
}

static enum interface_type virtual_root_iftypes[] = { virtual_root, unknown };

struct dev_probe HIDDEN virtual_root_parser = {
	.name = "virtual_root",
	.iftypes = virtual_root_iftypes,
	.flags = DEV_ABBREV_ONLY|DEV_PROVIDES_ROOT,
	.parse = parse_virtual_root,
};

// vim:fenc=utf-8:tw=75:noet
