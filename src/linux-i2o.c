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
 * support for I2O devices
 * ... probably doesn't work.
 */
static ssize_t
parse_i2o(struct device *dev, const char *current, const char *root UNUSED)
{
	debug("entry");
	/* I2O disks can have up to 16 partitions, or 4 bits worth. */
	if (dev->major >= 80 && dev->major <= 87) {
	        dev->interface_type = i2o;
	        dev->disknum = 16*(dev->major-80) + (dev->minor >> 4);
	        set_part(dev, dev->minor & 0xF);
	} else {
	        /* If it isn't those majors, it's not an i2o dev */
	        return 0;
	}

	char *block = strstr(current, "/block/");
	ssize_t sz = block ? block + 1 - current : -1;
	debug("current:'%s' sz:%zd", current, sz);
	return sz;
}

enum interface_type i2o_iftypes[] = { i2o, unknown };

struct dev_probe HIDDEN i2o_parser = {
	.name = "i2o",
	.iftypes = i2o_iftypes,
	.flags = DEV_PROVIDES_HD,
	.parse = parse_i2o,
	.create = NULL,
};

// vim:fenc=utf-8:tw=75:noet
