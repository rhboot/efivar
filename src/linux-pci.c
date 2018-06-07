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
 * support for PCI root hub and devices
 *
 * various devices /sys/dev/block/$major:$minor start with:
 * maj:min -> ../../devices/pci$PCIROOT/$PCI_DEVICES/$BLOCKDEV_STUFF/block/$DISK/$PART
 * i.e.:                    pci0000:00/0000:00:1d.0/0000:05:00.0/
 *                          ^ root hub ^device      ^device
 *
 * for network devices, we also get:
 * /sys/class/net/$IFACE -> ../../devices/$PCI_STUFF/net/$IFACE
 *
 */
static ssize_t
parse_pci(struct device *dev, const char *current)
{
        int rc;
        int pos;
        uint16_t root_domain;
        uint8_t root_bus;
        uint32_t acpi_hid = 0;
        uint64_t acpi_uid_int = 0;
        const char *devpart = current;
        char *fbuf = NULL;
        uint16_t tmp16 = 0;
        char *spaces;

        pos = strlen(current);
        spaces = alloca(pos+1);
        memset(spaces, ' ', pos+1);
        spaces[pos] = '\0';
        pos = 0;

        debug(DEBUG, "entry");

        /*
         * find the pci root domain and port; they basically look like:
         * pci0000:00/
         *    ^d   ^p
         */
        rc = sscanf(devpart, "../../devices/pci%hx:%hhx/%n", &root_domain, &root_bus, &pos);
        /*
         * If we can't find that, it's not a PCI device.
         */
        if (rc != 2)
                return 0;
        devpart += pos;

        dev->pci_root.pci_root_domain = root_domain;
        dev->pci_root.pci_root_bus = root_bus;

        rc = read_sysfs_file(&fbuf,
                             "devices/pci%04hx:%02hhx/firmware_node/hid",
                             root_domain, root_bus);
        if (rc < 0 || fbuf == NULL)
                return -1;

        rc = sscanf((char *)fbuf, "PNP%hx", &tmp16);
        if (rc != 1)
                return -1;
        acpi_hid = EFIDP_EFI_PNP_ID(tmp16);

        /*
         * Apparently basically nothing can look up a PcieRoot() node,
         * because they just check _CID.  So since _CID for the root pretty
         * much always has to be PNP0A03 anyway, just use that no matter
         * what.
         */
        if (acpi_hid == EFIDP_ACPI_PCIE_ROOT_HID)
                acpi_hid = EFIDP_ACPI_PCI_ROOT_HID;
        dev->pci_root.pci_root_acpi_hid = acpi_hid;

        errno = 0;
        fbuf = NULL;
        rc = read_sysfs_file(&fbuf,
                             "devices/pci%04hx:%02hhx/firmware_node/uid",
                             root_domain, root_bus);
        if ((rc <= 0 && errno != ENOENT) || fbuf == NULL)
                return -1;
        if (rc > 0) {
                rc = sscanf((char *)fbuf, "%"PRIu64"\n", &acpi_uid_int);
                if (rc == 1) {
                        dev->pci_root.pci_root_acpi_uid = acpi_uid_int;
                } else {
                        /* kernel uses "%s\n" to print it, so there
                         * should always be some value and a newline... */
                        int l = strlen((char *)fbuf);
                        if (l >= 1) {
                                fbuf[l-1] = '\0';
                                dev->pci_root.pci_root_acpi_uid_str = fbuf;
                        }
                }
        }
        errno = 0;

        /* find the pci domain/bus/device/function:
         * 0000:00:01.0/0000:01:00.0/
         *              ^d   ^b ^d ^f (of the last one in the series)
         */
        while (*devpart) {
                uint16_t domain;
                uint8_t bus, device, function;
                struct pci_dev_info *pci_dev;
                unsigned int i = dev->n_pci_devs;

                pos = 0;
                debug(DEBUG, "searching for 0000:00:00.0/");
                rc = sscanf(devpart, "%hx:%hhx:%hhx.%hhx/%n",
                            &domain, &bus, &device, &function, &pos);
                debug(DEBUG, "current:\"%s\" rc:%d pos:%d\n", devpart, rc, pos);
                arrow(DEBUG, spaces, 9, pos, rc, 3);
                if (rc != 4)
                        break;
                devpart += pos;

                debug(DEBUG, "found pci domain %04hx:%02hhx:%02hhx.%02hhx",
                      domain, bus, device, function);
                pci_dev = realloc(dev->pci_dev,
                                  sizeof(*pci_dev) * (i + 1));
                if (!pci_dev) {
                        efi_error("realloc(%p, %zd * (%d + 1)) failed",
                                  dev->pci_dev, sizeof(*pci_dev), i);
                        return -1;
                }
                dev->pci_dev = pci_dev;

                dev->pci_dev[i].pci_domain = domain;
                dev->pci_dev[i].pci_bus = bus;
                dev->pci_dev[i].pci_device = device;
                dev->pci_dev[i].pci_function = function;
                char *tmp = strndup(current, devpart-current+1);
                char *linkbuf = NULL;
                if (!tmp) {
                        efi_error("could not allocate memory");
                        return -1;
                }
                tmp[devpart - current] = '\0';
                rc = sysfs_readlink(&linkbuf, "class/block/%s/driver", tmp);
                free(tmp);
                if (rc < 0) {
                        efi_error("Could not find driver for pci device");
                        return -1;
                }
                dev->pci_dev[i].driverlink = strdup(linkbuf);
                debug(DEBUG, "driver:%s\n", linkbuf);
                dev->n_pci_devs += 1;
        }

        return devpart - current;
}

static ssize_t
dp_create_pci(struct device *dev,
              uint8_t *buf, ssize_t size, ssize_t off)
{
        ssize_t sz = 0, new = 0;

        debug(DEBUG, "entry buf:%p size:%zd off:%zd", buf, size, off);

        if (dev->pci_root.pci_root_acpi_uid_str) {
                debug(DEBUG, "creating acpi_hid_ex dp hid:0x%08x uid:\"%s\"",
                      dev->pci_root.pci_root_acpi_hid,
                      dev->pci_root.pci_root_acpi_uid_str);
                new = efidp_make_acpi_hid_ex(buf + off, size ? size - off : 0,
                                            dev->pci_root.pci_root_acpi_hid,
                                            0, 0, "",
                                            dev->pci_root.pci_root_acpi_uid_str,
                                            "");
                if (new < 0) {
                        efi_error("efidp_make_acpi_hid_ex() failed");
                        return new;
                }
        } else {
                debug(DEBUG, "creating acpi_hid dp hid:0x%08x uid:0x%0"PRIx64,
                      dev->pci_root.pci_root_acpi_hid,
                      dev->pci_root.pci_root_acpi_uid);
                new = efidp_make_acpi_hid(buf + off, size ? size - off : 0,
                                         dev->pci_root.pci_root_acpi_hid,
                                         dev->pci_root.pci_root_acpi_uid);
                if (new < 0) {
                        efi_error("efidp_make_acpi_hid() failed");
                        return new;
                }
        }
        off += new;
        sz += new;

        debug(DEBUG, "creating PCI device path nodes");
        for (unsigned int i = 0; i < dev->n_pci_devs; i++) {
                debug(DEBUG, "creating PCI device path node %u", i);
                new = efidp_make_pci(buf + off, size ? size - off : 0,
                                    dev->pci_dev[i].pci_device,
                                    dev->pci_dev[i].pci_function);
                if (new < 0) {
                        efi_error("efidp_make_pci() failed");
                        return new;
                }
                sz += new;
                off += new;
        }

        debug(DEBUG, "returning %zd", sz);
        return sz;
}

enum interface_type pci_iftypes[] = { pci, unknown };

struct dev_probe HIDDEN pci_parser = {
        .name = "pci",
        .iftypes = pci_iftypes,
        .flags = DEV_PROVIDES_ROOT,
        .parse = parse_pci,
        .create = dp_create_pci,
};
