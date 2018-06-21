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
 * support for soc platforms
 *
 * various devices /sys/dev/block/$major:$minor start with:
 * maj:min ->  ../../devices/platform/soc/$DEVICETREE_NODE/$BLOCKDEV_STUFF/block/$DISK/$PART
 * i.e.:                              soc/1a400000.sata/ata1/host0/target0:0:0/0:0:0:0/block/sda/sda1
 *                                        ^ dt node     ^ blockdev stuff                     ^ disk
 * I don't *think* the devicetree nodes stack.
 */
static ssize_t
parse_soc_root(struct device *dev UNUSED, const char *current, const char *root UNUSED)
{
        int rc;
        int pos;
        const char *devpart = current;
        char *spaces;

        pos = strlen(current);
        spaces = alloca(pos+1);
        memset(spaces, ' ', pos+1);
        spaces[pos] = '\0';
        pos = 0;

        debug("entry");

        rc = sscanf(devpart, "../../devices/platform/soc/%*[^/]/%n", &pos);
        if (rc != 0)
                return 0;
        devpart += pos;
        debug("new position is \"%s\"", devpart);

        return devpart - current;
}

enum interface_type soc_root_iftypes[] = { soc_root, unknown };

struct dev_probe HIDDEN soc_root_parser = {
        .name = "soc_root",
        .iftypes = soc_root_iftypes,
        .flags = DEV_ABBREV_ONLY|DEV_PROVIDES_ROOT,
        .parse = parse_soc_root,
};
