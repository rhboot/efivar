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
        char *spaces;

        pos0 = strlen(current);
        spaces = alloca(pos0+1);
        memset(spaces, ' ', pos0+1);
        spaces[pos0] = '\0';
        pos0 = 0;

        debug("entry");

        debug("searching for mdM/mdMpN");
        rc = sscanf(current, "md%d/%nmd%dp%d%n",
                    &md, &pos0, &tosser0, &part, &pos1);
        debug("current:\"%s\" rc:%d pos0:%d pos1:%d\n", current, rc, pos0, pos1);
        arrow(LOG_DEBUG, spaces, 9, pos0, rc, 3);
        /*
         * If it isn't of that form, it's not one of our partitioned md devices.
         */
        if (rc != 3)
                return 0;

        dev->interface_type = md;

        if (dev->part == -1)
                dev->part = part;

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
