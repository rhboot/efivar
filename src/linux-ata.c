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
 * support for old-school ATA devices
 *
 * /sys/dev/block/$major:$minor looks like:
 * 8:0 -> ../../devices/pci0000:00/0000:00:17.0/ata2/host1/target1:0:0/1:0:0:0/block/sda
 * 8:1 -> ../../devices/pci0000:00/0000:00:17.0/ata2/host1/target1:0:0/1:0:0:0/block/sda/sda1
 * 11:0 -> ../../devices/pci0000:00/0000:00:11.5/ata3/host2/target2:0:0/2:0:0:0/block/sr0
 */
static ssize_t
parse_ata(struct device *dev, const char *current UNUSED)
{
        debug(DEBUG, "entry");
        /* IDE disks can have up to 64 partitions, or 6 bits worth,
         * and have one bit for the disk number.
         * This leaves an extra bit at the top.
         */
        if (dev->major == 3) {
                dev->disknum = (dev->minor >> 6) & 1;
                dev->controllernum = (dev->major - 3 + 0) + dev->disknum;
                dev->interface_type = ata;
                set_part(dev, dev->minor & 0x3F);
        } else if (dev->major == 22) {
                dev->disknum = (dev->minor >> 6) & 1;
                dev->controllernum = (dev->major - 22 + 2) + dev->disknum;
                dev->interface_type = ata;
                set_part(dev, dev->minor & 0x3F);
        } else if (dev->major >= 33 && dev->major <= 34) {
                dev->disknum = (dev->minor >> 6) & 1;
                dev->controllernum = (dev->major - 33 + 4) + dev->disknum;
                dev->interface_type = ata;
                set_part(dev, dev->minor & 0x3F);
        } else if (dev->major >= 56 && dev->major <= 57) {
                dev->disknum = (dev->minor >> 6) & 1;
                dev->controllernum = (dev->major - 56 + 8) + dev->disknum;
                dev->interface_type = ata;
                set_part(dev, dev->minor & 0x3F);
        } else if (dev->major >= 88 && dev->major <= 91) {
                dev->disknum = (dev->minor >> 6) & 1;
                dev->controllernum = (dev->major - 88 + 12) + dev->disknum;
                dev->interface_type = ata;
                set_part(dev, dev->minor & 0x3F);
        } else {
                /*
                 * If it isn't one of those majors, it isn't a PATA device.
                 */
                return 0;
        }

        if (!strncmp(dev->driver, "pata_", 5) ||
                   !(strcmp(dev->driver, "ata_piix"))) {
                dev->interface_type = ata;
        } else {
                /*
                 * If it isn't one of the pata drivers or ata_piix, it isn't a
                 * PATA device.
                 */
                return 0;
        }

        char *block = strstr(current, "/block/");
        if (!block)
                return -1;
        return block + 1 - current;
}

enum interface_type ata_iftypes[] = { ata, atapi, unknown };

struct dev_probe ata_parser = {
        .name = "ata",
        .iftypes = ata_iftypes,
        .flags = DEV_PROVIDES_HD,
        .parse = parse_ata,
        .create = NULL,
};
