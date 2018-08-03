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
 * support for emmc devices
 *
 * /sys/dev/block/$major:$minor looks like:
 * 179:0 -> ../../devices/pci0000:00/0000:00:1c.0/mmc_host/mmc0/mmc0:0001/block/mmcblk0
 * 179:1 -> ../../devices/pci0000:00/0000:00:1c.0/mmc_host/mmc0/mmc0:0001/block/mmcblk0/mmcblk0p1
 *
 * /sys/dev/block/179:0/device looks like:
 * device -> ../../../mmc0:0001
 *
 * /sys/dev/block/179:1/partition looks like:
 * $ cat partition
 * 1
 *
 */

static ssize_t
parse_emmc(struct device *dev, const char *current, const char *root UNUSED)
{
        int rc;
        int32_t tosser0, tosser1, tosser2, tosser3, slot_id, partition;
        int pos0 = 0, pos1 = 0;
        char *spaces;

        pos0 = strlen(current);
        spaces = alloca(pos0+1);
        memset(spaces, ' ', pos0+1);
        spaces[pos0] = '\0';
        pos0 = 0;

        debug("entry");

        debug("searching for mmc_host/mmc0/mmc0:0001/block/mmcblk0 or mmc_host/mmc0/mmc0:0001/block/mmcblk0/mmcblk0p1");
        rc = sscanf(current, "mmc_host/mmc%d/mmc%d:%d/block/mmcblk%d%n/mmcblk%dp%d%n",
                    &tosser0, &tosser1, &tosser2, &slot_id,
                    &pos0, &tosser3, &partition, &pos1);
        debug("current:\"%s\" rc:%d pos0:%d pos1:%d\n", current, rc, pos0, pos1);
        arrow(LOG_DEBUG, spaces, 9, pos0, rc, 4);
        arrow(LOG_DEBUG, spaces, 9, pos1, rc, 6);
        /*
         * If it isn't of that form, it's not one of our emmc devices.
         */
        if (rc != 4 && rc != 6)
                return 0;

        dev->emmc_info.slot_id = slot_id;
        dev->interface_type = emmc;

        if (rc == 6) {
                if (dev->part == -1)
                        dev->part = partition;

                pos0 = pos1;
        }

        return pos0;
}

static ssize_t
dp_create_emmc(struct device *dev,
               uint8_t *buf,  ssize_t size, ssize_t off)
{
        ssize_t sz;

        debug("entry");

        sz = efidp_make_emmc(buf + off, size ? size - off : 0,
                             dev->emmc_info.slot_id);
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

static enum interface_type emmc_iftypes[] = { emmc, unknown };

struct dev_probe HIDDEN emmc_parser = {
        .name = "emmc",
        .iftypes = emmc_iftypes,
        .flags = DEV_PROVIDES_HD,
        .parse = parse_emmc,
        .create = dp_create_emmc,
        .make_part_name = make_part_name,
};
