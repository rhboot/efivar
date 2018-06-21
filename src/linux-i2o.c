/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2018 Red Hat, Inc.
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
 * support for I2O devices
 * ... probably doesn't work.
 */
static ssize_t
parse_i2o(struct device *dev, const char *current UNUSED, const char *root UNUSED)
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
        if (!block)
                return -1;
        return block + 1 - current;
}

enum interface_type i2o_iftypes[] = { i2o, unknown };

struct dev_probe HIDDEN i2o_parser = {
        .name = "i2o",
        .iftypes = i2o_iftypes,
        .flags = DEV_PROVIDES_HD,
        .parse = parse_i2o,
        .create = NULL,
};
