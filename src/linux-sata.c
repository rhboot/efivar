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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <unistd.h>

#include "efiboot.h"

/*
 * Support for SATA (and in some cases other ATA) devices
 *
 * /dev/sda as SATA looks like:
 * /sys/dev/block/8:0 -> ../../devices/pci0000:00/0000:00:1f.2/ata1/host0/target0:0:0/0:0:0:0/block/sda
 *                                                                ^     ^       ^ ^ ^ ^ ^ ^ ^
 *                                                                |     |       | | | | | | lun again
 *                                                                |     |       | | | | | target again
 *                                                                |     |       | | | | device again
 *                                                                |     |       | | | bus again
 *                                                                |     |       | | 64-bit lun
 *                                                                |     |       | 32-bit target
 *                                                                |     |       32-bit device
 *                                                                |     32-bit scsi "bus"
 *                                                                ata print id
 *
 * This is not only highly repetitive, but most of it is always 0 anyway.
 *
 * /dev/sda1 looks like:
 * /sys/dev/block/8:1 -> ../../devices/pci0000:00/0000:00:1f.2/ata1/host0/target0:0:0/0:0:0:0/block/sda/sda1
 *
 * sda/device looks like:
 * device -> ../../../0:0:0:0
 *
 * For all of these we're searching for a match for $PRINT_ID and then getting
 * the other info - if there's a port multiplier and if so what's the PMPN,
 * and what's the port number:
 * /sys/class/ata_device/dev$PRINT_ID.$PMPN.$DEVICE  - values "print id", pmp id, and device number
 * /sys/class/ata_device/dev$PRINT_ID.$DEVICE - values are "print id" and device number
 * /sys/class/ata_port/ata$PRINT_ID/port_no - contains off-by-one port number)
 *
 */

static ssize_t
sysfs_sata_get_port_info(uint32_t print_id, struct device *dev)
{
        DIR *d;
        struct dirent *de;
        uint8_t *buf = NULL;
        int rc;

        d = sysfs_opendir("class/ata_device/");
        if (!d) {
                efi_error("could not open /sys/class/ata_device/");
                return -1;
        }

        while ((de = readdir(d)) != NULL) {
                uint32_t found_print_id;
                uint32_t found_pmp;
                uint32_t found_devno = 0;

                if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
                        continue;

                rc = sscanf(de->d_name, "dev%d.%d.%d", &found_print_id,
                            &found_pmp, &found_devno);
                if (rc < 2 || rc > 3) {
                        closedir(d);
                        errno = EINVAL;
                        return -1;
                } else if (found_print_id != print_id) {
                        continue;
                } else if (rc == 3) {
                        /*
                         * the kernel doesn't ever tell us the SATA PMPN
                         * sentinal value, it'll give us devM.N instead of
                         * devM.N.O in that case instead.
                         */
                        if (found_pmp > 0x7fff) {
                                closedir(d);
                                errno = EINVAL;
                                return -1;
                        }
                        dev->sata_info.ata_devno = 0;
                        dev->sata_info.ata_pmp = found_pmp;
                        break;
                } else if (rc == 2) {
                        dev->sata_info.ata_devno = 0;
                        dev->sata_info.ata_pmp = 0xffff;
                        break;
                }
        }
        closedir(d);

        rc = read_sysfs_file(&buf, "class/ata_port/ata%d/port_no",
                             print_id);
        if (rc <= 0 || buf == NULL)
                return -1;

        rc = sscanf((char *)buf, "%d", &dev->sata_info.ata_port);
        if (rc != 1)
                return -1;

        /*
         * ata_port numbers are 1-indexed from libata in the kernel, but
         * they're 0-indexed in the spec.  For maximal confusion.
         */
        if (dev->sata_info.ata_port == 0) {
                errno = EINVAL;
                return -1;
        } else {
                dev->sata_info.ata_port -= 1;
        }

        return 0;
}

static ssize_t
parse_sata(struct device *dev, const char *devlink, const char *root UNUSED)
{
        const char *current = devlink;
        uint32_t print_id;
        uint32_t scsi_bus, tosser0;
        uint32_t scsi_device, tosser1;
        uint32_t scsi_target, tosser2;
        uint64_t scsi_lun, tosser3;
        int pos = 0;
        int rc;
        char *spaces;

        pos = strlen(current);
        spaces = alloca(pos+1);
        memset(spaces, ' ', pos+1);
        spaces[pos] = '\0';
        pos = 0;

        debug("entry");
        if (is_pata(dev)) {
                debug("This is a PATA device; skipping.");
                return 0;
        }

        /* find the ata info:
         * ata1/host0/target0:0:0/0:0:0:0
         *    ^dev  ^host   x y z
         */
        debug("searching for ata1/");
        rc = sscanf(current, "ata%"PRIu32"/%n", &print_id, &pos);
        debug("current:\"%s\" rc:%d pos:%d\n", current, rc, pos);
        arrow(LOG_DEBUG, spaces, 9, pos, rc, 1);
        /*
         * If we don't find this one, it isn't an ata device, so return 0 not
         * error.  Later errors mean it is an ata device, but we can't parse
         * it right, so they return -1.
         */
        if (rc != 1)
                return 0;
        current += pos;
        pos = 0;

        debug("searching for host0/");
        rc = sscanf(current, "host%"PRIu32"/%n", &scsi_bus, &pos);
        debug("current:\"%s\" rc:%d pos:%d\n", current, rc, pos);
        arrow(LOG_DEBUG, spaces, 9, pos, rc, 1);
        if (rc != 1)
                return -1;
        current += pos;
        pos = 0;

        debug("searching for target0:0:0:0/");
        rc = sscanf(current, "target%"PRIu32":%"PRIu32":%"PRIu64"/%n",
                    &scsi_device, &scsi_target, &scsi_lun, &pos);
        debug("current:\"%s\" rc:%d pos:%d\n", current, rc, pos);
        arrow(LOG_DEBUG, spaces, 9, pos, rc, 3);
        if (rc != 3)
                return -1;
        current += pos;
        pos = 0;

        debug("searching for 0:0:0:0/");
        rc = sscanf(current, "%"PRIu32":%"PRIu32":%"PRIu32":%"PRIu64"/%n",
                    &tosser0, &tosser1, &tosser2, &tosser3, &pos);
        debug("current:\"%s\" rc:%d pos:%d\n", current, rc, pos);
        arrow(LOG_DEBUG, spaces, 9, pos, rc, 4);
        if (rc != 4)
                return -1;
        current += pos;

        rc = sysfs_sata_get_port_info(print_id, dev);
        if (rc < 0)
                return -1;

        dev->sata_info.scsi_bus = scsi_bus;
        dev->sata_info.scsi_device = scsi_device;
        dev->sata_info.scsi_target = scsi_target;
        dev->sata_info.scsi_lun = scsi_lun;

        if (dev->interface_type == unknown)
                dev->interface_type = sata;

        return current - devlink;
}

static ssize_t
dp_create_sata(struct device *dev,
               uint8_t *buf, ssize_t size, ssize_t off)
{
        ssize_t sz = -1;

        debug("entry buf:%p size:%zd off:%zd", buf, size, off);

        if (dev->interface_type == ata || dev->interface_type == atapi) {
                sz = efidp_make_atapi(buf + off, size ? size - off : 0,
                                      dev->sata_info.ata_port,
                                      dev->sata_info.ata_pmp,
                                      dev->sata_info.ata_devno);
                if (sz < 0)
                        efi_error("efidp_make_atapi() failed");
        } else if (dev->interface_type == sata) {
                sz = efidp_make_sata(buf + off, size ? size - off : 0,
                                     dev->sata_info.ata_port,
                                     dev->sata_info.ata_pmp,
                                     dev->sata_info.ata_devno);
                if (sz < 0)
                        efi_error("efidp_make_sata() failed");
        } else {
                return -EINVAL;
        }

        return sz;
}

enum interface_type sata_iftypes[] = { sata, unknown };

struct dev_probe HIDDEN sata_parser = {
        .name = "sata",
        .iftypes = sata_iftypes,
        .flags = DEV_PROVIDES_HD,
        .parse = parse_sata,
        .create = dp_create_sata,
};
