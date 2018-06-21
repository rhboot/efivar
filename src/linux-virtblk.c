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
 * support for virtio block devices
 * /sys/dev/block/maj:min look like:
 * 252:0 -> ../../devices/pci0000:00/0000:00:07.0/virtio2/block/vda
 * 252:1 -> ../../devices/pci0000:00/0000:00:07.0/virtio2/block/vda/vda1
 *
 * vda/device looks like:
 * device -> ../../../virtio2
 *
 * In this case we get 1 storage controller per pci device, so we actually
 * write the device path as:
 *
 * PciRoot(0x0)/Pci(0x7,0x0)/HD(1,GPT,14d30998-b5e3-4b8f-9be3-58a454dd06bf,0x800,0x53000)/File(\EFI\fedora\shimx64.efi)
 *
 * But usually we just write the HD() entry, of course.
 */
static ssize_t
parse_virtblk(struct device *dev, const char *current, const char *root UNUSED)
{
        uint32_t tosser;
        int pos;
        int rc;
        char *spaces;

        pos = strlen(current);
        spaces = alloca(pos+1);
        memset(spaces, ' ', pos+1);
        spaces[pos] = '\0';
        pos = 0;

        debug("entry");

        debug("searching for virtio0/");
        rc = sscanf(current, "virtio%x/%n", &tosser, &pos);
        debug("current:\"%s\" rc:%d pos:%d\n", current, rc, pos);
        arrow(LOG_DEBUG, spaces, 9, pos, rc, 1);
        /*
         * If we couldn't find virtioX/ then it isn't a virtio device.
         */
        if (rc < 1)
                return 0;

        dev->interface_type = virtblk;

        return pos;
}

enum interface_type virtblk_iftypes[] = { virtblk, unknown };

struct dev_probe HIDDEN virtblk_parser = {
        .name = "virtio block",
        .iftypes = virtblk_iftypes,
        .flags = DEV_PROVIDES_HD,
        .parse = parse_virtblk,
        .create = NULL,
};

