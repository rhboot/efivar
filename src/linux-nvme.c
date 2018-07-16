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
 * support for NVMe devices
 *
 * /sys/dev/block/$major:$minor looks like:
 * 259:0 -> ../../devices/pci0000:00/0000:00:1d.0/0000:05:00.0/nvme/nvme0/nvme0n1
 * 259:1 -> ../../devices/pci0000:00/0000:00:1d.0/0000:05:00.0/nvme/nvme0/nvme0n1/nvme0n1p1
 *
 * /sys/dev/block/259:0/device looks like:
 * device -> ../../nvme0
 *
 * /sys/dev/block/259:1/partition looks like:
 * $ cat partition
 * 1
 *
 * /sys/class/block/nvme0n1/eui looks like:
 * $ cat /sys/class/block/nvme0n1/eui
 * 00 25 38 53 5a 16 1d a9
 */

static ssize_t
parse_nvme(struct device *dev, const char *current, const char *root UNUSED)
{
        int rc;
        int32_t tosser0, tosser1, tosser2, ctrl_id, ns_id, partition;
        uint8_t *filebuf = NULL;
        int pos0 = 0, pos1 = 0;
        char *spaces;

        pos0 = strlen(current);
        spaces = alloca(pos0+1);
        memset(spaces, ' ', pos0+1);
        spaces[pos0] = '\0';
        pos0 = 0;

        debug("entry");

        debug("searching for nvme/nvme0/nvme0n1 or nvme/nvme0/nvme0n1/nvme0n1p1");
        rc = sscanf(current, "nvme/nvme%d/nvme%dn%d%n/nvme%dn%dp%d%n",
                    &tosser0, &ctrl_id, &ns_id, &pos0,
                    &tosser1, &tosser2, &partition, &pos1);
        debug("current:\"%s\" rc:%d pos0:%d pos1:%d\n", current, rc, pos0, pos1);
        arrow(LOG_DEBUG, spaces, 9, pos0, rc, 3);
        arrow(LOG_DEBUG, spaces, 9, pos1, rc, 6);
        /*
         * If it isn't of that form, it's not one of our nvme devices.
         */
        if (rc != 3 && rc != 6)
                return 0;

        dev->nvme_info.ctrl_id = ctrl_id;
        dev->nvme_info.ns_id = ns_id;
        dev->nvme_info.has_eui = 0;
        dev->interface_type = nvme;

        if (rc == 6) {
                if (dev->part == -1)
                        dev->part = partition;

                pos0 = pos1;
        }

        /*
         * now fish the eui out of sysfs is there is one...
         */
        rc = read_sysfs_file(&filebuf,
                             "class/block/nvme%dn%d/eui",
                             ctrl_id, ns_id);
        if ((rc < 0 && errno == ENOENT) || filebuf == NULL) {
                rc = read_sysfs_file(&filebuf,
                             "class/block/nvme%dn%d/device/eui",
                             ctrl_id, ns_id);
        }
        if (rc >= 0 && filebuf != NULL) {
                uint8_t eui[8];
                if (rc < 23) {
                        errno = EINVAL;
                        return -1;
                }
                rc = sscanf((char *)filebuf,
                            "%02hhx %02hhx %02hhx %02hhx "
                            "%02hhx %02hhx %02hhx %02hhx",
                            &eui[0], &eui[1], &eui[2], &eui[3],
                            &eui[4], &eui[5], &eui[6], &eui[7]);
                if (rc < 8) {
                        errno = EINVAL;
                        return -1;
                }
                dev->nvme_info.has_eui = 1;
                memcpy(dev->nvme_info.eui, eui, sizeof(eui));
        }

        return pos0;
}

static ssize_t
dp_create_nvme(struct device *dev,
               uint8_t *buf,  ssize_t size, ssize_t off)
{
        ssize_t sz;

        debug("entry");

        sz = efidp_make_nvme(buf + off, size ? size - off : 0,
                             dev->nvme_info.ns_id,
                             dev->nvme_info.has_eui ? dev->nvme_info.eui
                                                        : NULL);
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

static enum interface_type nvme_iftypes[] = { nvme, unknown };

struct dev_probe HIDDEN nvme_parser = {
        .name = "nvme",
        .iftypes = nvme_iftypes,
        .flags = DEV_PROVIDES_HD,
        .parse = parse_nvme,
        .create = dp_create_nvme,
        .make_part_name = make_part_name,
};

