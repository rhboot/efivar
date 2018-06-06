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
 * support for SAS devices
 *
 * /sys/dev/block/$major:$minor looks like:
 * 8:32 -> ../../devices/pci0000:00/0000:00:01.0/0000:01:00.0/host4/port-4:0/end_device-4:0/target4:0:0/4:0:0:0/block/sdc
 * 8:33 -> ../../devices/pci0000:00/0000:00:01.0/0000:01:00.0/host4/port-4:0/end_device-4:0/target4:0:0/4:0:0:0/block/sdc/sdc1
 *
 * /sys/dev/block/8:32/device looks like:
 * DUNNO.  But I suspect it's: ../../../4:0:0:0
 *
 * These things are also things in this case:
 * /sys/class/scsi_host/host4/host_sas_address
 * /sys/class/block/sdc/device/sas_address
 *
 * I'm not sure at the moment if they're the same or not.
 */
static ssize_t
parse_sas(struct device *dev, const char *current)
{
        struct stat statbuf = { 0, };
        int rc;
        uint32_t scsi_host, scsi_bus, scsi_device, scsi_target;
        uint64_t scsi_lun;
        ssize_t pos;
        uint8_t *filebuf = NULL;
        uint64_t sas_address;

        debug(DEBUG, "entry");

        pos = parse_scsi_link(current, &scsi_host,
                              &scsi_bus, &scsi_device,
                              &scsi_target, &scsi_lun);
        /*
         * If we can't parse the scsi data, it isn't a sas device, so return 0
         * not error.
         */
        if (pos < 0)
                return 0;

        /*
         * Make sure it has the actual /SAS/ bits before we continue
         * validating all this junk.
         */
        rc = sysfs_stat(&statbuf,
                        "class/scsi_host/host%d/host_sas_address",
                        scsi_host);
        /*
         * If we can't parse the scsi data, it isn't a /SAS/ device, so return
         * 0 not error. Later errors mean it is an ata device, but we can't
         * parse it right, so they return -1.
         */
        if (rc < 0)
                return 0;

        /*
         * we also need to get the actual sas_address from someplace...
         */
        rc = read_sysfs_file(&filebuf,
                             "class/block/%s/device/sas_address",
                             dev->disk_name);
        if (rc < 0 || filebuf == NULL)
                return -1;

        rc = sscanf((char *)filebuf, "%"PRIx64, &sas_address);
        if (rc != 1)
                return -1;

        dev->sas_info.sas_address = sas_address;

        dev->scsi_info.scsi_bus = scsi_bus;
        dev->scsi_info.scsi_device = scsi_device;
        dev->scsi_info.scsi_target = scsi_target;
        dev->scsi_info.scsi_lun = scsi_lun;
        dev->interface_type = sas;
        return pos;
}

static ssize_t
dp_create_sas(struct device *dev,
              uint8_t *buf,  ssize_t size, ssize_t off)
{
        ssize_t sz;

        debug(DEBUG, "entry");

        sz = efidp_make_sas(buf + off, size ? size - off : 0,
                            dev->sas_info.sas_address);

        return sz;
}

enum interface_type sas_iftypes[] = { sas, unknown };

struct dev_probe HIDDEN sas_parser = {
        .name = "sas",
        .iftypes = sas_iftypes,
        .flags = DEV_PROVIDES_HD,
        .parse = parse_sas,
        .create = dp_create_sas,
};
