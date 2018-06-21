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
 * support NVDIMM-P (pmem / btt) devices
 * (does not include NVDIMM-${ANYTHING_ELSE})
 *
 * /sys/dev/block/$major:$minor looks like:
 * 259:0 -> ../../devices/LNXSYSTM:00/LNXSYBUS:00/ACPI0012:00/ndbus0/region9/btt9.0/block/pmem9s
 * 259:1 -> ../../devices/LNXSYSTM:00/LNXSYBUS:00/ACPI0012:00/ndbus0/region11/btt11.0/block/pmem11s
 * 259:3 -> ../../devices/LNXSYSTM:00/LNXSYBUS:00/ACPI0012:00/ndbus0/region11/btt11.0/block/pmem11s/pmem11s1
 *
 * /sys/dev/block/259:0/device looks like:
 * device -> ../../../btt9.0
 * /sys/dev/block/259:1/device looks like:
 * device -> ../../../btt11.0
 *
 * /sys/dev/block/259:1/partition looks like:
 * $ cat partition
 * 1
 *
 * /sys/dev/block/259:0/uuid looks like:
 * $ cat uuid
 * 6e54091e-7476-47ac-824b-b6dd69878661
 *
 * pmem12s -> ../devices/LNXSYSTM:00/LNXSYBUS:00/ACPI0012:00/ndbus0/region12/btt12.1/block/pmem12s
 * dev: 259:0
 * device -> ../../../btt12.1
 * device/uuid: 0cee166e-dd56-4bc2-99d2-2544b69025b8
 * 259:0 -> ../../devices/LNXSYSTM:00/LNXSYBUS:00/ACPI0012:00/ndbus0/region12/btt12.1/block/pmem12s
 *
 * pmem12.1s -> ../devices/LNXSYSTM:00/LNXSYBUS:00/ACPI0012:00/ndbus0/region12/btt12.2/block/pmem12.1s
 * dev: 259:1
 * device -> ../../../btt12.2
 * device/uuid: 78d94521-91f7-47db-b3a7-51b764281940
 * 259:1 -> ../../devices/LNXSYSTM:00/LNXSYBUS:00/ACPI0012:00/ndbus0/region12/btt12.2/block/pmem12.1s
 *
 * pmem12.2 -> ../devices/LNXSYSTM:00/LNXSYBUS:00/ACPI0012:00/ndbus0/region12/pfn12.1/block/pmem12.2
 * dev: 259:2
 * device -> ../../../pfn12.1
 * device/uuid: 829c5205-89a5-4581-9819-df7d7754c622
 * 259:2 -> ../../devices/LNXSYSTM:00/LNXSYBUS:00/ACPI0012:00/ndbus0/region12/pfn12.1/block/pmem12.2
 */

static ssize_t
parse_pmem(struct device *dev, const char *current, const char *root UNUSED)
{
        uint8_t *filebuf = NULL;
        uint8_t system, sysbus, acpi_id;
        uint16_t pnp_id;
        int ndbus, region, btt_region_id, btt_id, rc, pos;
        char *namespace = NULL;

        debug("entry");

        if (!strcmp(dev->driver, "nd_pmem")) {
                ;
#if 0 /* dunno */
        } else if (!strcmp(dev->driver, "nd_blk")) {
                /* dunno */
                dev->inteface_type = scsi;
#endif
        } else {
                /*
                 * not a pmem device
                 */
                return 0;
        }

        /*
         * We're not actually using any of the values here except pos (our
         * return value), but rather just being paranoid that this is the sort
         * of device we care about.
         *
         * 259:0 -> ../../devices/LNXSYSTM:00/LNXSYBUS:00/ACPI0012:00/ndbus0/region12/btt12.1/block/pmem12s
         */
        rc = sscanf(current,
                    "../../devices/LNXSYSTM:%hhx/LNXSYBUS:%hhx/ACPI%hx:%hhx/ndbus%d/region%d/btt%d.%d/%n",
                    &system, &sysbus, &pnp_id, &acpi_id, &ndbus, &region,
                    &btt_region_id, &btt_id, &pos);
        if (rc < 8)
                return 0;

        /*
         * but the UUID we really do need to have.
         */
        rc = read_sysfs_file(&filebuf,
                             "class/block/%s/device/namespace", dev->disk_name);
        if ((rc < 0 && errno == ENOENT) || filebuf == NULL)
                return -1;

        rc = sscanf((char *)filebuf, "%ms[^\n]\n", &namespace);
        if (rc != 1 || namespace == NULL)
                return -1;

        filebuf = NULL;
        debug("nvdimm namespace is \"%s\"", namespace);
        rc = read_sysfs_file(&filebuf, "bus/nd/devices/%s/uuid", namespace);
        free(namespace);
        if (rc < 0 || filebuf == NULL)
                return -1;

        rc = efi_str_to_guid((char *)filebuf,
                             &dev->nvdimm_info.namespace_label);
        if (rc < 0)
                return -1;

        filebuf = NULL;
        rc = read_sysfs_file(&filebuf, "class/block/%s/device/uuid",
                             dev->disk_name);
        if (rc < 0 || filebuf == NULL)
                return -1;

        rc = efi_str_to_guid((char *)filebuf,
                             &dev->nvdimm_info.nvdimm_label);
        if (rc < 0)
                return -1;

        /*
         * Right now it's not clear what encoding NVDIMM($uuid) gets in the
         * binary format, so this will be in the mixed endian format EFI GUIDs
         * are in (33221100-1100-1100-0011-223344556677) unless you set this
         * variable.
         */
        if (getenv("LIBEFIBOOT_SWIZZLE_PMEM_UUID") != NULL) {
                swizzle_guid_to_uuid(&dev->nvdimm_info.namespace_label);
                swizzle_guid_to_uuid(&dev->nvdimm_info.nvdimm_label);
        }

        dev->interface_type = nd_pmem;

        return pos;
}

static ssize_t
dp_create_pmem(struct device *dev,
               uint8_t *buf, ssize_t size, ssize_t off)
{
        ssize_t sz, sz1;

        debug("entry");

        sz = efidp_make_nvdimm(buf + off, size ? size - off : 0,
                               &dev->nvdimm_info.namespace_label);
        if (sz < 0)
                return sz;
        off += sz;
        sz1 = efidp_make_nvdimm(buf + off, size ? size - off : 0,
                                &dev->nvdimm_info.nvdimm_label);
        if (sz1 < 0)
                return sz1;

        return sz + sz1;
}

enum interface_type pmem_iftypes[] = { nd_pmem, unknown };

struct dev_probe HIDDEN pmem_parser = {
        .name = "pmem",
        .iftypes = pmem_iftypes,
        .flags = DEV_PROVIDES_ROOT|DEV_PROVIDES_HD,
        .parse = parse_pmem,
        .create = dp_create_pmem,
};
