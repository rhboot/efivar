/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2015 Red Hat, Inc.
 * Copyright (C) 2001 Dell Computer Corporation <Matt_Domsch@dell.com>
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
#include <limits.h>
#include <linux/ethtool.h>
#include <linux/version.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <scsi/scsi.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

#include "efiboot.h"

int HIDDEN
find_parent_devpath(const char * const child, char **parent)
{
        int ret;
        char *node;
        char *linkbuf;

        /* strip leading /dev/ */
        node = strrchr(child, '/');
        if (!node)
                return -1;
        node++;

        /* look up full path symlink */
        ret = sysfs_readlink(&linkbuf, "class/block/%s", node);
        if (ret < 0 || !linkbuf)
                return ret;

        /* strip child */
        node = strrchr(linkbuf, '/');
        if (!node)
                return -1;
        *node = '\0';

        /* read parent */
        node = strrchr(linkbuf, '/');
        if (!node)
                return -1;
        *node = '\0';
        node++;

        /* write out new path */
        ret = asprintf(parent, "/dev/%s", node);
        if (ret < 0)
                return ret;

        return 0;
}

int HIDDEN
set_part_name(struct device *dev, const char * const fmt, ...)
{
        ssize_t rc;
        va_list ap;
        int error;

        if (dev->part <= 0)
                return 0;

        va_start(ap, fmt);
        rc = vasprintf(&dev->part_name, fmt, ap);
        error = errno;
        va_end(ap);
        errno = error;
        if (rc < 0)
                efi_error("could not allocate memory");

        return rc;
}

int HIDDEN
reset_part_name(struct device *dev)
{
        char *part = NULL;
        int rc;

        if (dev->part_name) {
                free(dev->part_name);
                dev->part_name = NULL;
        }

        if (dev->part < 1)
                return 0;

        if (dev->n_probes > 0 &&
            dev->probes[dev->n_probes-1] &&
            dev->probes[dev->n_probes-1]->make_part_name) {
                part = dev->probes[dev->n_probes]->make_part_name(dev);
                dev->part_name = part;
                rc = 0;
        } else {
                rc = asprintf(&dev->part_name, "%s%d",
                              dev->disk_name, dev->part);
                if (rc < 0)
                        efi_error("could not allocate memory");
        }
        return rc;
}

int HIDDEN
set_part(struct device *dev, int value)
{
        int rc;

        if (dev->part == value)
                return 0;

        dev->part = value;
        rc = reset_part_name(dev);
        if (rc < 0)
                efi_error("reset_part_name() failed");

        return rc;
}

int HIDDEN
set_disk_name(struct device *dev, const char * const fmt, ...)
{
        ssize_t rc;
        va_list ap;
        int error;

        va_start(ap, fmt);
        rc = vasprintf(&dev->disk_name, fmt, ap);
        error = errno;
        va_end(ap);
        errno = error;
        if (rc < 0)
                efi_error("could not allocate memory");

        return rc;
}

int HIDDEN
set_disk_and_part_name(struct device *dev)
{
        /*
         * results are like such:
         * maj:min -> ../../devices/pci$PCI_STUFF/$BLOCKDEV_STUFF/block/$DISK/$PART
         */

        char *ultimate = pathseg(dev->link, -1);
        char *penultimate = pathseg(dev->link, -2);
        char *approximate = pathseg(dev->link, -3);
        char *proximate = pathseg(dev->link, -4);

        errno = 0;
        debug("dev->disk_name:%p dev->part_name:%p", dev->disk_name, dev->part_name);
        debug("dev->part:%d", dev->part);
        debug("ultimate:\"%s\"", ultimate ? : "");
        debug("penultimate:\"%s\"", penultimate ? : "");
        debug("approximate:\"%s\"", approximate ? : "");
        debug("proximate:\"%s\"", proximate ? : "");

        if (ultimate && penultimate &&
            ((proximate && !strcmp(proximate, "nvme")) ||
             (approximate && !strcmp(approximate, "block")))) {
                /*
                 * 259:1 -> ../../devices/pci0000:00/0000:00:1d.0/0000:05:00.0/nvme/nvme0/nvme0n1/nvme0n1p1
                 * 8:1 -> ../../devices/pci0000:00/0000:00:17.0/ata2/host1/target1:0:0/1:0:0:0/block/sda/sda1
                 * 8:33 -> ../../devices/pci0000:00/0000:00:01.0/0000:01:00.0/host4/port-4:0/end_device-4:0/target4:0:0/4:0:0:0/block/sdc/sdc1
                 * 252:1 -> ../../devices/pci0000:00/0000:00:07.0/virtio2/block/vda/vda1
                 * 259:3 -> ../../devices/LNXSYSTM:00/LNXSYBUS:00/ACPI0012:00/ndbus0/region11/btt11.0/block/pmem11s/pmem11s1
                 */
                set_disk_name(dev, "%s", penultimate);
                set_part_name(dev, "%s", ultimate);
                debug("disk:%s part:%s", penultimate, ultimate);
        } else if (ultimate && approximate && !strcmp(approximate, "nvme")) {
                /*
                 * 259:0 -> ../../devices/pci0000:00/0000:00:1d.0/0000:05:00.0/nvme/nvme0/nvme0n1
                 */
                set_disk_name(dev, "%s", ultimate);
                set_part_name(dev, "%sp%d", ultimate, dev->part);
                debug("disk:%s part:%sp%d", ultimate, ultimate, dev->part);
        } else if (ultimate && penultimate && !strcmp(penultimate, "block")) {
                /*
                 * 253:0 -> ../../devices/virtual/block/dm-0 (... I guess)
                 * 8:0 -> ../../devices/pci0000:00/0000:00:17.0/ata2/host1/target1:0:0/1:0:0:0/block/sda
                 * 11:0 -> ../../devices/pci0000:00/0000:00:11.5/ata3/host2/target2:0:0/2:0:0:0/block/sr0
                 * 8:32 -> ../../devices/pci0000:00/0000:00:01.0/0000:01:00.0/host4/port-4:0/end_device-4:0/target4:0:0/4:0:0:0/block/sdc
                 * 252:0 -> ../../devices/pci0000:00/0000:00:07.0/virtio2/block/vda
                 * 259:0 -> ../../devices/LNXSYSTM:00/LNXSYBUS:00/ACPI0012:00/ndbus0/region9/btt9.0/block/pmem9s
                 * 259:1 -> ../../devices/LNXSYSTM:00/LNXSYBUS:00/ACPI0012:00/ndbus0/region11/btt11.0/block/pmem11s
                 */
                set_disk_name(dev, "%s", ultimate);
                set_part_name(dev, "%s%d", ultimate, dev->part);
                debug("disk:%s part:%s%d", ultimate, ultimate, dev->part);
        } else if (ultimate && approximate && !strcmp(approximate, "mtd")) {
                /*
                 * 31:0 -> ../../devices/platform/1e000000.palmbus/1e000b00.spi/spi_master/spi32766/spi32766.0/mtd/mtd0/mtdblock0
                 */
                set_disk_name(dev, "%s", ultimate);
                debug("disk:%s", ultimate);
        }

        return 0;
}

static struct dev_probe *dev_probes[] = {
        /*
         * pmem needs to be before PCI, so if it provides root it'll
         * be found first.
         */
        &pmem_parser,
        &acpi_root_parser,
        &pci_root_parser,
        &soc_root_parser,
        &pci_parser,
        &virtblk_parser,
        &sas_parser,
        &sata_parser,
        &nvme_parser,
        &ata_parser,
        &scsi_parser,
        &i2o_parser,
        &emmc_parser,
        NULL
};

static inline bool
supports_iface(struct dev_probe *probe, enum interface_type iftype)
{
        for (unsigned int i = 0; probe->iftypes[i] != unknown; i++)
                if (probe->iftypes[i] == iftype)
                        return true;
        return false;
}

void HIDDEN
device_free(struct device *dev)
{
        if (!dev)
                return;
        if (dev->link)
                free(dev->link);

        if (dev->device)
                free(dev->device);

        if (dev->driver)
                free(dev->driver);

        if (dev->probes)
                free(dev->probes);

        if (dev->acpi_root.acpi_hid_str)
                free(dev->acpi_root.acpi_hid_str);
        if (dev->acpi_root.acpi_uid_str)
                free(dev->acpi_root.acpi_uid_str);
        if (dev->acpi_root.acpi_cid_str)
                free(dev->acpi_root.acpi_cid_str);

        if (dev->interface_type == network) {
                if (dev->ifname)
                        free(dev->ifname);
        } else {
                if (dev->disk_name)
                        free(dev->disk_name);
                if (dev->part_name)
                        free(dev->part_name);
        }

        for (unsigned int i = 0; i < dev->n_pci_devs; i++)
                if (dev->pci_dev[i].driverlink)
                        free(dev->pci_dev[i].driverlink);

        if (dev->pci_dev)
                free(dev->pci_dev);

        memset(dev, 0, sizeof(*dev));
        free(dev);
}

struct device HIDDEN
*device_get(int fd, int partition)
{
        struct device *dev;
        char *linkbuf = NULL, *tmpbuf = NULL;
        int i = 0;
        unsigned int n = 0;
        int rc;

        size_t nmemb = (sizeof(dev_probes)
                        / sizeof(dev_probes[0])) + 1;

        dev = calloc(1, sizeof(*dev));
        if (!dev) {
                efi_error("could not allocate %zd bytes", sizeof(*dev));
                return NULL;
        }

        dev->part = partition;
        debug("partition:%d dev->part:%d", partition, dev->part);
        dev->probes = calloc(nmemb, sizeof(struct dev_probe *));
        if (!dev->probes) {
                efi_error("could not allocate %zd bytes",
                          nmemb * sizeof(struct dev_probe *));
                goto err;
        }

        rc = fstat(fd, &dev->stat);
        if (rc < 0) {
                efi_error("fstat(%d) failed", fd);
                goto err;
        }

        dev->pci_root.pci_domain = 0xffff;
        dev->pci_root.pci_bus = 0xff;

        if (S_ISBLK(dev->stat.st_mode)) {
                dev->major = major(dev->stat.st_rdev);
                dev->minor = minor(dev->stat.st_rdev);
        } else if (S_ISREG(dev->stat.st_mode)) {
                dev->major = major(dev->stat.st_dev);
                dev->minor = minor(dev->stat.st_dev);
        } else {
                efi_error("device is not a block device or regular file");
                goto err;
        }

        rc = sysfs_readlink(&linkbuf, "dev/block/%"PRIu64":%"PRIu32,
                            dev->major, dev->minor);
        if (rc < 0 || !linkbuf) {
                efi_error("readlink of /sys/dev/block/%"PRIu64":%"PRIu32" failed",
                          dev->major, dev->minor);
                goto err;
        }

        dev->link = strdup(linkbuf);
        if (!dev->link) {
                efi_error("strdup(\"%s\") failed", linkbuf);
                goto err;
        }
        debug("dev->link: %s", dev->link);

        if (dev->part == -1) {
                rc = read_sysfs_file(&tmpbuf, "dev/block/%s/partition", dev->link);
                if (rc < 0 || !tmpbuf) {
                        efi_error("device has no /partition node; not a partition");
                } else {
                        rc = sscanf((char *)tmpbuf, "%d\n", &dev->part);
                        if (rc != 1)
                                efi_error("couldn't parse partition number for %s", tmpbuf);
                }
        }

        rc = set_disk_and_part_name(dev);
        if (rc < 0) {
                efi_error("could not set disk and partition names");
                goto err;
        }
        debug("dev->disk_name: %s", dev->disk_name);
        debug("dev->part_name: %s", dev->part_name);

        rc = sysfs_readlink(&tmpbuf, "block/%s/device", dev->disk_name);
        if (rc < 0 || !tmpbuf) {
                debug("readlink of /sys/block/%s/device failed",
                          dev->disk_name);

                dev->device = strdup("");
        } else {
                dev->device = strdup(tmpbuf);
        }

        if (!dev->device) {
                efi_error("strdup(\"%s\") failed", tmpbuf);
                goto err;
        }

        if (dev->device[0] != 0) {
                rc = sysfs_readlink(&tmpbuf, "block/%s/device/driver", dev->disk_name);
                if (rc < 0 || !tmpbuf) {
                        if (errno == ENOENT) {
                                /*
                                 * nvme, for example, will have nvme0n1/device point
                                 * at nvme0, and we need to look for device/driver
                                 * there.
                                 */
                                rc = sysfs_readlink(&tmpbuf,
                                                    "block/%s/device/device/driver",
                                                    dev->disk_name);
                        }
                        if (rc < 0 || !tmpbuf) {
                                efi_error("readlink of /sys/block/%s/device/driver failed",
                                          dev->disk_name);
                                goto err;
                        }
                }

                linkbuf = pathseg(tmpbuf, -1);
                if (!linkbuf) {
                        efi_error("could not get segment -1 of \"%s\"", tmpbuf);
                        goto err;
                }

                dev->driver = strdup(linkbuf);
        } else {
                dev->driver = strdup("");
        }

        if (!dev->driver) {
                efi_error("strdup(\"%s\") failed", linkbuf);
                goto err;
        }

        const char *current = dev->link;
        bool needs_root = true;
        int last_successful_probe = -1;

        debug("searching for device nodes in %s", dev->link);
        for (i = 0;
             dev_probes[i] && dev_probes[i]->parse && *current;
             i++) {
                struct dev_probe *probe = dev_probes[i];
                int pos;

                if (!needs_root &&
                    (probe->flags & DEV_PROVIDES_ROOT)) {
                        debug("not testing %s because flags is 0x%x",
                              probe->name, probe->flags);
                        continue;
                }

                debug("trying %s", probe->name);
                pos = probe->parse(dev, current, dev->link);
                if (pos < 0) {
                        efi_error("parsing %s failed", probe->name);
                        goto err;
                } else if (pos > 0) {
                        debug("%s matched %s", probe->name, current);
                        dev->flags |= probe->flags;

                        if (probe->flags & DEV_PROVIDES_HD ||
                            probe->flags & DEV_PROVIDES_ROOT ||
                            probe->flags & DEV_ABBREV_ONLY)
                                needs_root = false;

                        dev->probes[n++] = dev_probes[i];
                        current += pos;
                        debug("current:%s", current);
                        last_successful_probe = i;

                        if (!*current || !strncmp(current, "block/", 6))
                                break;

                        continue;
                }

                debug("dev_probes[i+1]: %p dev->interface_type: %d\n",
                      dev_probes[i+1], dev->interface_type);
                if (dev_probes[i+1] == NULL && dev->interface_type == unknown) {
                        pos = 0;
                        rc = sscanf(current, "%*[^/]/%n", &pos);
                        if (rc < 0) {
slash_err:
                                efi_error("Cannot parse device link segment \"%s\"", current);
                                goto err;
                        }

                        while (current[pos] == '/')
                                pos += 1;

                        if (!current[pos])
                                goto slash_err;

                        debug("Cannot parse device link segment \"%s\"", current);
                        debug("Skipping to \"%s\"", current + pos);
                        debug("This means we can only create abbreviated paths");
                        dev->flags |= DEV_ABBREV_ONLY;
                        i = last_successful_probe;
                        current += pos;

                        if (!*current || !strncmp(current, "block/", 6))
                                break;
                }
        }

        if (dev->interface_type == unknown &&
            !(dev->flags & DEV_ABBREV_ONLY) &&
            !strcmp(current, "block/")) {
                efi_error("unknown storage interface");
                errno = ENOSYS;
                goto err;
        }

        return dev;
err:
        device_free(dev);
        return NULL;
}

int HIDDEN
make_blockdev_path(uint8_t *buf, ssize_t size, struct device *dev)
{
        ssize_t off = 0;

        debug("entry buf:%p size:%zd", buf, size);

        for (unsigned int i = 0; dev->probes[i] &&
                                 dev->probes[i]->parse; i++) {
                struct dev_probe *probe = dev->probes[i];
                ssize_t sz;

                if (!probe->create)
                        continue;

                sz = probe->create(dev, buf + off, size ? size - off : 0, 0);
                if (sz < 0) {
                        efi_error("could not create %s device path",
                                  probe->name);
                        return sz;
                }
                off += sz;
        }

        debug("= %zd", off);

        return off;
}

ssize_t HIDDEN
make_mac_path(uint8_t *buf, ssize_t size, const char * const ifname)
{
        struct ifreq ifr;
        struct ethtool_drvinfo drvinfo = { 0, };
        int fd = -1, rc;
        ssize_t ret = -1, sz, off = 0;
        char busname[PATH_MAX+1] = "";
        struct device dev;

        memset(&dev, 0, sizeof (dev));
        dev.interface_type = network;
        dev.ifname = strdupa(ifname);
        if (!dev.ifname)
                return -1;

        /*
         * find the device link, which looks like:
         * ../../devices/$PCI_STUFF/net/$IFACE
         */
        rc = sysfs_readlink(&dev.link, "class/net/%s", ifname);
        if (rc < 0 || !dev.link)
                goto err;

        memset(&ifr, 0, sizeof (ifr));
        strncpy(ifr.ifr_name, ifname, IF_NAMESIZE);
        ifr.ifr_name[IF_NAMESIZE-1] = '\0';
        drvinfo.cmd = ETHTOOL_GDRVINFO;
        ifr.ifr_data = (caddr_t)&drvinfo;

        fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0)
                goto err;

        rc = ioctl(fd, SIOCETHTOOL, &ifr);
        if (rc < 0)
                goto err;

        strncpy(busname, drvinfo.bus_info, PATH_MAX);

        rc = ioctl(fd, SIOCGIFHWADDR, &ifr);
        if (rc < 0)
                goto err;

        sz = pci_parser.create(&dev, buf, size, off);
        if (sz < 0)
                goto err;
        off += sz;

        sz = efidp_make_mac_addr(buf+off, size?size-off:0,
                                 ifr.ifr_ifru.ifru_hwaddr.sa_family,
                                 (uint8_t *)ifr.ifr_ifru.ifru_hwaddr.sa_data,
                                 sizeof(ifr.ifr_ifru.ifru_hwaddr.sa_data));
        if (sz < 0)
                goto err;

        off += sz;
        ret = off;
err:
        if (fd >= 0)
                close(fd);
        return ret;
}

/************************************************************
 * get_sector_size
 * Requires:
 *  - filedes is an open file descriptor, suitable for reading
 * Modifies: nothing
 * Returns:
 *  sector size, or 512.
 ************************************************************/
int UNUSED
get_sector_size(int filedes)
{
        int rc, sector_size = 512;

        rc = ioctl(filedes, BLKSSZGET, &sector_size);
        if (rc)
                sector_size = 512;
        return sector_size;
}
