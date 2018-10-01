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

static int
get_port_expander_sas_address(uint64_t *sas_address, uint32_t scsi_host,
                              uint32_t local_port_id,
                              uint32_t remote_port_id, uint32_t remote_scsi_target)
{
        uint8_t *filebuf = NULL;
        int rc;

        /*
         * We find sas_address via this insanity:
         * /sys/class/scsi_host/host2 -> ../../devices/pci0000:74/0000:74:02.0/host2/scsi_host/host2
         * /sys/devices/pci0000:74/0000:74:02.0/host2/scsi_host/host2/device -> ../../../host2
         * /sys/devices/pci0000:74/0000:74:02.0/host2/device -> ../../../host2
         * /sys/devices/host2/port-2:0/expander-2:0/sas_device/expander-2:0/sas_address
         *
         * But since host2 is always host2, we can skip most of that and just
         * go for:
         * /sys/devices/pci0000:74/0000:74:02.0/host2/port-2:0/expander-2:0/port-2:0:2/end_device-2:0:2/sas_device/end_device-2:0:2/sas_address
        */

#if 0 /* previously thought this was right, but it's the expander's address, not the target's address */
        /*
         * /sys/class/scsi_host/host2/device/port-2:0/expander-2:0/sas_device/expander-2:0/sas_address
         * ... I think.  I would have expected that to be port-2:0:0 and I
         * don't understand why it isn't. (I do now; this is the expander not
         * the port.)
         */

        debug("looking for /sys/class/scsi_host/host%d/device/port-%d:%d/expander-%d:%d/sas_device/expander-%d:%d/sas_address",
              scsi_host, scsi_host, port_id, scsi_host, remote_scsi_target, scsi_host, remote_scsi_target);
        rc = read_sysfs_file(&filebuf,
                             "class/scsi_host/host%d/device/port-%d:%d/expander-%d:%d/sas_device/expander-%d:%d/sas_address",
                             scsi_host, scsi_host, port_id, scsi_host, remote_scsi_target, scsi_host, remote_scsi_target);
        if (rc < 0 || filebuf == NULL) {
                debug("didn't find it.");
                return -1;
        }
#else
        debug("looking for /sys/class/scsi_host/host%d/device/port-%d:%d/expander-%d:%d/port-%d:%d:%d/end_device-%d:%d:%d/sas_device/end_device-%d:%d:%d/sas_address",
              scsi_host,
              scsi_host, local_port_id,
              scsi_host, remote_scsi_target,
              scsi_host, remote_scsi_target, remote_port_id,
              scsi_host, remote_scsi_target, remote_port_id,
              scsi_host, remote_scsi_target, remote_port_id);
        rc = read_sysfs_file(&filebuf,
                             "class/scsi_host/host%d/device/port-%d:%d/expander-%d:%d/port-%d:%d:%d/end_device-%d:%d:%d/sas_device/end_device-%d:%d:%d/sas_address",
                             scsi_host,
                             scsi_host, local_port_id,
                             scsi_host, remote_scsi_target,
                             scsi_host, remote_scsi_target, remote_port_id,
                             scsi_host, remote_scsi_target, remote_port_id,
                             scsi_host, remote_scsi_target, remote_port_id);
        if (rc < 0 || filebuf == NULL) {
                debug("didn't find it.");
                return -1;
        }
#endif

        rc = sscanf((char *)filebuf, "%"PRIx64, sas_address);
        if (rc != 1)
                return -1;

        return 0;
}

static int
get_local_sas_address(uint64_t *sas_address, struct device *dev)
{
        int rc;
        char *filebuf = NULL;

        rc = read_sysfs_file(&filebuf,
                             "class/block/%s/device/sas_address",
                             dev->disk_name);
        if (rc < 0 || filebuf == NULL)
                return -1;

        rc = sscanf((char *)filebuf, "%"PRIx64, sas_address);
        if (rc != 1)
                return -1;

        return 0;
}

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
 *
 * There are also other devices that look like:
 *
 * 8:0 -> ../../devices/pci0000:74/0000:74:02.0/host2/port-2:0/expander-2:0/port-2:0:2/end_device-2:0:2/target2:0:0/2:0:0:0/block/sda
 * 8:1 -> ../../devices/pci0000:74/0000:74:02.0/host2/port-2:0/expander-2:0/port-2:0:2/end_device-2:0:2/target2:0:0/2:0:0:0/block/sda/sda1
 *
 * /sys/dev/block/8:0/device -> ../../../2:0:0:0
 *
 * This exists:
 *
 * /sys/class/scsi_host/host2 -> ../../devices/pci0000:74/0000:74:02.0/host2/scsi_host/host2
 * /sys/devices/pci0000:74/0000:74:02.0/host2/scsi_host/host2/device -> ../../../host2
 * /sys/devices/pci0000:74/0000:74:02.0/host2/device -> ../../../host2
 * /sys/devices/pci0000:74/0000:74:02.0/host2/port-2:0/expander-2:0/sas_device/expander-2:0/sas_address
 *
 * but the device doesn't actually have a sas_host_address, because it's on a
 * port expander, and sas_address doesn't directly exist under /sys/class/
 * anywhere.
 */
static ssize_t
parse_sas(struct device *dev, const char *current, const char *root UNUSED)
{
        struct stat statbuf = { 0, };
        int rc;
        uint32_t scsi_host, scsi_bus, scsi_device, scsi_target;
        uint32_t local_port_id = 0, remote_port_id = 0;
        uint32_t remote_scsi_target = 0;
        uint64_t scsi_lun;
        ssize_t pos;
        uint64_t sas_address = 0;

        debug("entry");

        pos = parse_scsi_link(current, &scsi_host,
                              &scsi_bus, &scsi_device,
                              &scsi_target, &scsi_lun,
                              &local_port_id, &remote_port_id,
                              &remote_scsi_target);
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
        debug("looking for /sys/class/scsi_host/host%d/host_sas_address", scsi_host);
        rc = sysfs_stat(&statbuf,
                        "class/scsi_host/host%d/host_sas_address",
                        scsi_host);
        /*
         * If we can't parse the scsi data, it isn't a /SAS/ device, so return
         * 0 not error. Later errors mean it is an ata device, but we can't
         * parse it right, so they return -1.
         */
        if (rc < 0) {
                debug("didn't find it.");
                /*
                 * If it's on a port expander, it won't have the
                 * host_sas_address, so we need to check if it's a sas_host
                 * instead.
                 * It may work to just check this to begin with, but I don't
                 * have such a device in front of me right now.
                 */
                debug("looking for /sys/class/sas_host/host%d", scsi_host);
                rc = sysfs_stat(&statbuf,
                                "class/sas_host/host%d", scsi_host);
                if (rc < 0) {
                        debug("didn't find it.");
                        return 0;
                }
                debug("found it.");

                /*
                 * So it *is* a sas_host, and we have to fish the sas_address
                 * from the remote port
                 */
                rc = get_port_expander_sas_address(&sas_address, scsi_host,
                                                   local_port_id,
                                                   remote_port_id,
                                                   remote_scsi_target);
                if (rc < 0) {
                        debug("Couldn't find port expander sas address");
                        return 0;
                }
        } else {
                /*
                 * we also need to get the actual sas_address from someplace...
                 */
                debug("found it.");
                rc = get_local_sas_address(&sas_address, dev);
                if (rc < 0) {
                        debug("Couldn't find sas address");
                        return 0;
                }
        }
        debug("sas address is 0x%"PRIx64, sas_address);

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

        debug("entry");

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
