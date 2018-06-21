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
 * support for ACPI-like platform root hub and devices
 *
 * various devices /sys/dev/block/$major:$minor start with:
 * maj:min -> ../../devices/ACPI0000:00/$PCI_DEVICES/$BLOCKDEV_STUFF/block/$DISK/$PART
 * i.e.:                    APMC0D0D:00/ata1/host0/target0:0:0/0:0:0:0/block/sda
 *                          ^ root hub ^blockdev stuff
 * or:
 * maj:min -> ../../devices/ACPI0000:00/$PCI_DEVICES/$BLOCKDEV_STUFF/block/$DISK/$PART
 * i.e.:                    APMC0D0D:00/0000:00:1d.0/0000:05:00.0/ata1/host0/target0:0:0/0:0:0:0/block/sda
 *                          ^ root hub ^pci dev      ^pci dev     ^ blockdev stuff
 */
static ssize_t
parse_acpi_root(struct device *dev, const char *current, const char *root UNUSED)
{
        int rc;
        int pos;
        uint16_t pad0;
        uint8_t pad1;
        char *acpi_header = NULL;
        char *colon;

        const char *devpart = current;
        char *spaces;

        pos = strlen(current);
        spaces = alloca(pos+1);
        memset(spaces, ' ', pos+1);
        spaces[pos] = '\0';
        pos = 0;

        debug("entry");

        /*
         * find the ACPI root dunno0 and dunno1; they basically look like:
         * ABCD0000:00/
         *     ^d0  ^d1
         * This is annoying because "/%04ms%h:%hhx/" won't bind from the right
         * side in sscanf.
         */
        rc = sscanf(devpart, "../../devices/platform/%n", &pos);
        debug("devpart:\"%s\" rc:%d pos:%d", devpart, rc, pos);
        if (rc != 0 || pos < 1)
                return 0;
        devpart += pos;

        /*
         * If it's too short to be A0000:00, it's not an ACPI string
         */
        if (strlen(devpart) < 8)
                return 0;

        colon = strchr(devpart, ':');
        if (!colon)
                return 0;
        pos = colon - devpart;

        /*
         * If colon doesn't point at something between one of these:
         * A0000:00 ACPI0000:00
         *      ^ 5         ^ 8
         * Then it's not an ACPI string.
         */
        if (pos < 5 || pos > 8)
                return 0;

        dev->acpi_root.acpi_hid_str = strndup(devpart, pos + 1);
        if (!dev->acpi_root.acpi_hid_str) {
                efi_error("Could not allocate memory");
                return -1;
        }
        dev->acpi_root.acpi_hid_str[pos] = 0;
        debug("acpi_hid_str:\"%s\"", dev->acpi_root.acpi_hid_str);

        pos -= 4;
        debug("devpart:\"%s\" rc:%d pos:%d", devpart, rc, pos);
        acpi_header = strndupa(devpart, pos);
        if (!acpi_header)
                return 0;
        acpi_header[pos] = 0;
        debug("devpart:\"%s\" acpi_header:\"%s\"", devpart, acpi_header);
        devpart += pos;

        /*
         * If we can't find these numbers, it's not an ACPI string
         */
        rc = sscanf(devpart, "%hx:%hhx/%n", &pad0, &pad1, &pos);
        if (rc != 2) {
                efi_error("Could not parse ACPI path \"%s\"", devpart);
                return 0;
        }
        debug("devpart:\"%s\" parsed:%04hx:%02hhx pos:%d rc:%d",
              devpart, pad0, pad1, pos, rc);

        devpart += pos;

        rc = parse_acpi_hid_uid(dev, "devices/platform/%s%04hX:%02hhX",
                                acpi_header, pad0, pad1);
        debug("rc:%d acpi_header:%s pad0:%04hX pad1:%02hhX",
              rc, acpi_header, pad0, pad1);
        if (rc < 0 && errno == ENOENT) {
                rc = parse_acpi_hid_uid(dev, "devices/platform/%s%04hx:%02hhx",
                                acpi_header, pad0, pad1);
                debug("rc:%d acpi_header:%s pad0:%04hx pad1:%02hhx",
                      rc, acpi_header, pad0, pad1);
        }
        if (rc < 0) {
                efi_error("Could not parse hid/uid");
                return rc;
        }
        debug("Parsed HID:0x%08x UID:0x%"PRIx64" uidstr:\"%s\" path:\"%s\"",
              dev->acpi_root.acpi_hid, dev->acpi_root.acpi_uid,
              dev->acpi_root.acpi_uid_str,
              dev->acpi_root.acpi_cid_str);

        return devpart - current;
}

static ssize_t
dp_create_acpi_root(struct device *dev,
                    uint8_t *buf, ssize_t size, ssize_t off)
{
        ssize_t sz = 0, new = 0;

        debug("entry buf:%p size:%zd off:%zd", buf, size, off);

        if (dev->acpi_root.acpi_uid_str || dev->acpi_root.acpi_cid_str) {
                debug("creating acpi_hid_ex dp hid:0x%08x uid:0x%"PRIx64" uidstr:\"%s\" cidstr:\"%s\"",
                      dev->acpi_root.acpi_hid, dev->acpi_root.acpi_uid,
                      dev->acpi_root.acpi_uid_str, dev->acpi_root.acpi_cid_str);
                new = efidp_make_acpi_hid_ex(buf + off, size ? size - off : 0,
                                            dev->acpi_root.acpi_hid,
                                            dev->acpi_root.acpi_uid,
                                            dev->acpi_root.acpi_cid,
                                            dev->acpi_root.acpi_hid_str,
                                            dev->acpi_root.acpi_uid_str,
                                            dev->acpi_root.acpi_cid_str);
                if (new < 0) {
                        efi_error("efidp_make_acpi_hid_ex() failed");
                        return new;
                }
        } else {
                debug("creating acpi_hid dp hid:0x%08x uid:0x%0"PRIx64,
                      dev->acpi_root.acpi_hid,
                      dev->acpi_root.acpi_uid);
                new = efidp_make_acpi_hid(buf + off, size ? size - off : 0,
                                         dev->acpi_root.acpi_hid,
                                         dev->acpi_root.acpi_uid);
                if (new < 0) {
                        efi_error("efidp_make_acpi_hid() failed");
                        return new;
                }
        }
        sz += new;

        debug("returning %zd", sz);
        return sz;
}

enum interface_type acpi_root_iftypes[] = { acpi_root, unknown };

struct dev_probe HIDDEN acpi_root_parser = {
        .name = "acpi_root",
        .iftypes = acpi_root_iftypes,
        .flags = DEV_PROVIDES_ROOT,
        .parse = parse_acpi_root,
        .create = dp_create_acpi_root,
};
