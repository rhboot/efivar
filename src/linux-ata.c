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

bool HIDDEN
is_pata(struct device *dev)
{
        if (!strncmp(dev->driver, "pata_", 5) ||
            !strncmp(dev->driver, "ata_", 4))
                return true;

        if (dev->n_pci_devs > 0 &&
            dev->pci_dev[dev->n_pci_devs - 1].driverlink) {
                char *slash = dev->pci_dev[dev->n_pci_devs - 1].driverlink;

                slash = strrchr(slash, '/');
                if (slash &&
                    (!strncmp(slash, "/ata_", 5) ||
                     !strncmp(slash, "/pata_", 6)))
                    return true;
        }

        return false;
}

/*
 * support for old-school ATA devices
 *
 * /sys/dev/block/$major:$minor looks like:
 * 8:0 -> ../../devices/pci0000:00/0000:00:17.0/ata2/host1/target1:0:0/1:0:0:0/block/sda
 * 8:1 -> ../../devices/pci0000:00/0000:00:17.0/ata2/host1/target1:0:0/1:0:0:0/block/sda/sda1
 * 11:0 -> ../../devices/pci0000:00/0000:00:11.5/ata3/host2/target2:0:0/2:0:0:0/block/sr0
 */
static ssize_t
parse_ata(struct device *dev, const char *current, const char *root UNUSED)
{
        uint32_t scsi_host, scsi_bus, scsi_device, scsi_target;
        uint64_t scsi_lun;
        int pos;

        debug("entry");
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
                debug("If this is ATA, it isn't using a traditional IDE inode.");
        }

        if (is_pata(dev)) {
                dev->interface_type = ata;
        } else {
                /*
                 * If it isn't one of the pata drivers or ata_piix, it isn't a
                 * PATA device.
                 */
                return 0;
        }

        char *host = strstr(current, "/host");
        if (!host)
                return -1;

        pos = parse_scsi_link(host + 1, &scsi_host,
                              &scsi_bus, &scsi_device,
                              &scsi_target, &scsi_lun,
                              NULL, NULL, NULL);
        if (pos < 0)
                return -1;

        dev->ata_info.scsi_host = scsi_host;
        dev->ata_info.scsi_bus = scsi_bus;
        dev->ata_info.scsi_device = scsi_device;
        dev->ata_info.scsi_target = scsi_target;
        dev->ata_info.scsi_lun = scsi_lun;

        char *block = strstr(current, "/block/");
        if (!block)
                return -1;
        return block + 1 - current;
}

static ssize_t
dp_create_ata(struct device *dev,
              uint8_t *buf, ssize_t size, ssize_t off)
{
        ssize_t sz;

        debug("entry");

        sz = efidp_make_atapi(buf + off, size ? size - off : 0,
                              dev->ata_info.scsi_device,
                              dev->ata_info.scsi_target - 1,
                              dev->ata_info.scsi_lun);
        if (sz < 0)
                efi_error("efidp_make_atapi() failed");

        return sz;
}

enum interface_type ata_iftypes[] = { ata, atapi, unknown };

struct dev_probe ata_parser = {
        .name = "ata",
        .iftypes = ata_iftypes,
        .flags = DEV_PROVIDES_HD,
        .parse = parse_ata,
        .create = dp_create_ata,
};
