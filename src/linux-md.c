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
 * "support" for partitioned md devices - basically we just need to format
 * the partition name.
 *
 * /sys/dev/block/$major:$minor looks like:
 * 259:0 -> ../../devices/virtual/block/md1/md1p1
 * 9:1 -> ../../devices/virtual/block/md1
 *
 */

static ssize_t
parse_md(struct device *dev, const char *current, const char *root UNUSED)
{
	int rc;
	int32_t md, tosser0, part;
	int pos0 = 0, pos1 = 0;

	debug("entry");

	debug("searching for mdM/mdMpN");
	rc = sscanf(current, "md%d/%nmd%dp%d%n",
	            &md, &pos0, &tosser0, &part, &pos1);
	debug("current:'%s' rc:%d pos0:%d pos1:%d\n", current, rc, pos0, pos1);
	dbgmk("         ", pos0, pos1);
	/*
	 * If it isn't of that form, it's not one of our partitioned md devices.
	 */
	if (rc != 3)
	        return 0;

	dev->interface_type = md;

	if (dev->part == -1)
	        dev->part = part;

	debug("current:'%s' sz:%d\n", current, pos1);
	return pos1;
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

static enum interface_type md_iftypes[] = { md, unknown };

struct dev_probe HIDDEN md_parser = {
	.name = "md",
	.iftypes = md_iftypes,
	.flags = DEV_PROVIDES_HD,
	.parse = parse_md,
	.make_part_name = make_part_name,
};

// vim:fenc=utf-8:tw=75:noet
