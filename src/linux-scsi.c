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
 * support for Old-school SCSI devices
 */

/*
 * helper for scsi formats...
 */
ssize_t HIDDEN
parse_scsi_link(const char *current, uint32_t *scsi_host,
                uint32_t *scsi_bus, uint32_t *scsi_device,
                uint32_t *scsi_target, uint64_t *scsi_lun)
{
        int rc;
        int sz = 0;
        int pos0 = 0, pos1 = 0;
        char *spaces;

        sz = strlen(current);
        spaces = alloca(sz+1);
        memset(spaces, ' ', sz+1);
        spaces[sz] = '\0';
        sz = 0;

        debug(DEBUG, "entry");
        /*
         * This structure is completely ridiculous.
         *
         * /dev/sdc as SAS looks like:
         * /sys/dev/block/8:32 -> ../../devices/pci0000:00/0000:00:01.0/0000:01:00.0/host4/port-4:0/end_device-4:0/target4:0:0/4:0:0:0/block/sdc
         * /dev/sdc1 looks like:
         * /sys/dev/block/8:33 -> ../../devices/pci0000:00/0000:00:01.0/0000:01:00.0/host4/port-4:0/end_device-4:0/target4:0:0/4:0:0:0/block/sdc/sdc1
         *
         * OR
         *
         * /dev/sdc as SAS looks like:
         * /sys/dev/block/8:32 -> ../../devices/pci0000:00/0000:00:01.0/0000:01:00.0/host4/port-4:2:0/end_device-4:2:0/target4:2:0/4:2:0:0/block/sdc
         * /dev/sdc1 looks like:
         * /sys/dev/block/8:33 -> ../../devices/pci0000:00/0000:00:01.0/0000:01:00.0/host4/port-4:2:0/end_device-4:2:0/target4:2:0/4:2:0:0/block/sdc/sdc1
         *
         * /sys/block/sdc/device looks like:
         * device-> ../../../4:2:0:0
         *
         */

        /*
         * So we start when current is:
         * host4/port-4:0/end_device-4:0/target4:0:0/4:0:0:0/block/sdc/sdc1
         */
        uint32_t tosser0, tosser1, tosser2;

        /* ignore a bunch of stuff
         *    host4/port-4:0
         * or host4/port-4:0:0
         */
        debug(DEBUG, "searching for host4/");
        rc = sscanf(current, "host%d/%n", scsi_host, &pos0);
        debug(DEBUG, "current:\"%s\" rc:%d pos0:%d\n", current+sz, rc, pos0);
        arrow(DEBUG, spaces, 9, pos0, rc, 1);
        if (rc != 1)
                return -1;
        sz += pos0;
        pos0 = 0;

        debug(DEBUG, "searching for port-4:0 or port-4:0:0");
        rc = sscanf(current, "port-%d:%d%n:%d%n", &tosser0,
                    &tosser1, &pos0, &tosser2, &pos1);
        debug(DEBUG, "current:\"%s\" rc:%d pos0:%d pos1:%d\n", current+sz, rc, pos0, pos1);
        arrow(DEBUG, spaces, 9, pos0, rc, 2);
        arrow(DEBUG, spaces, 9, pos1, rc, 3);
        if (rc == 2 || rc == 3) {
                sz += pos0;
                pos0 = 0;

                /* next:
                 *    /end_device-4:0
                 * or /end_device-4:0:0
                 * awesomely these are the exact same fields that go into port-blah,
                 * but we don't care for now about any of them anyway.
                 */
                debug(DEBUG, "searching for /end_device-4:0/ or /end_device-4:0:0/");
                rc = sscanf(current + sz, "/end_device-%d:%d%n", &tosser0, &tosser1, &pos0);
                debug(DEBUG, "current:\"%s\" rc:%d pos0:%d\n", current+sz, rc, pos0);
                arrow(DEBUG, spaces, 9, pos0, rc, 2);
                if (rc != 2)
                        return -1;
                sz += pos0;
                pos0 = 0;

                rc = sscanf(current + sz, ":%d%n", &tosser0, &pos0);
                debug(DEBUG, "current:\"%s\" rc:%d pos0:%d\n", current+sz, rc, pos0);
                arrow(DEBUG, spaces, 9, pos0, rc, 2);
                if (rc != 0 && rc != 1)
                        return -1;
                sz += pos0;
                pos0 = 0;

                if (current[sz] == '/')
                        sz += 1;
        } else if (rc != 0) {
                return -1;
        }

        /* now:
         * /target4:0:0/
         */
        uint64_t tosser3;
        debug(DEBUG, "searching for target4:0:0/");
        rc = sscanf(current + sz, "target%d:%d:%"PRIu64"/%n", &tosser0, &tosser1,
                    &tosser3, &pos0);
        debug(DEBUG, "current:\"%s\" rc:%d pos0:%d\n", current+sz, rc, pos0);
        arrow(DEBUG, spaces, 9, pos0, rc, 3);
        if (rc != 3)
                return -1;
        sz += pos0;
        pos0 = 0;

        /* now:
         * %d:%d:%d:%llu/
         */
        debug(DEBUG, "searching for 4:0:0:0/");
        rc = sscanf(current + sz, "%d:%d:%d:%"PRIu64"/%n",
                    scsi_bus, scsi_device, scsi_target, scsi_lun, &pos0);
        debug(DEBUG, "current:\"%s\" rc:%d pos0:%d\n", current+sz, rc, pos0);
        arrow(DEBUG, spaces, 9, pos0, rc, 4);
        if (rc != 4)
                return -1;
        sz += pos0;

        return sz;
}

static ssize_t
parse_scsi(struct device *dev, const char *current)
{
        uint32_t scsi_host, scsi_bus, scsi_device, scsi_target;
        uint64_t scsi_lun;
        ssize_t sz;
        int pos;
        int rc;
        char *spaces;

        pos = strlen(current);
        spaces = alloca(pos+1);
        memset(spaces, ' ', pos+1);
        spaces[pos] = '\0';
        pos = 0;

        debug(DEBUG, "entry");

        debug(DEBUG, "searching for ../../../0:0:0:0");
        rc = sscanf(dev->device, "../../../%d:%d:%d:%"PRIu64"%n",
                    &dev->scsi_info.scsi_bus,
                    &dev->scsi_info.scsi_device,
                    &dev->scsi_info.scsi_target,
                    &dev->scsi_info.scsi_lun,
                    &pos);
        debug(DEBUG, "current:\"%s\" rc:%d pos:%d\n", dev->device, rc, pos);
        arrow(DEBUG, spaces, 9, pos, rc, 3);
        if (rc != 4)
                return 0;

        sz = parse_scsi_link(current, &scsi_host,
                              &scsi_bus, &scsi_device,
                              &scsi_target, &scsi_lun);
        if (sz < 0)
                return 0;

        /*
         * SCSI disks can have up to 16 partitions, or 4 bits worth
         * and have one bit for the disk number.
         */
        if (dev->major == 8) {
                dev->interface_type = scsi;
                dev->disknum = (dev->minor >> 4);
                set_part(dev, dev->minor & 0xF);
        } else if (dev->major >= 65 && dev->major <= 71) {
                dev->interface_type = scsi;
                dev->disknum = 16*(dev->major-64) + (dev->minor >> 4);
                set_part(dev, dev->minor & 0xF);
        } else if (dev->major >= 128 && dev->major <= 135) {
                dev->interface_type = scsi;
                dev->disknum = 16*(dev->major-128) + (dev->minor >> 4);
                set_part(dev, dev->minor & 0xF);
        } else {
                efi_error("couldn't parse scsi major/minor");
                return -1;
        }

        return sz;
}

static ssize_t
dp_create_scsi(struct device *dev,
               uint8_t *buf,  ssize_t size, ssize_t off)
{
        ssize_t sz = 0;

        debug(DEBUG, "entry");

        sz = efidp_make_scsi(buf + off, size ? size - off : 0,
                             dev->scsi_info.scsi_target,
                             dev->scsi_info.scsi_lun);
        if (sz < 0)
                efi_error("efidp_make_scsi() failed");

        return sz;
}

enum interface_type scsi_iftypes[] = { scsi, unknown };

struct dev_probe HIDDEN scsi_parser = {
        .name = "scsi",
        .iftypes = scsi_iftypes,
        .flags = DEV_PROVIDES_HD,
        .parse = parse_scsi,
        .create = dp_create_scsi,
};
