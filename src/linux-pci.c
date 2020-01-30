// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2019 Red Hat, Inc.
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
 * /sys/class/net/$IFACE -> ../../devices/$PCI_DEVICES/net/$IFACE
 *
 * In both cases our "current" pointer should be at $PCI_DEVICES.
 *
 */
static ssize_t
parse_pci(struct device *dev, const char *path, const char *root)
{
	const char *current = path;
	int rc;

	debug("entry");

	/* find the pci domain/bus/device/function:
	 * 0000:00:01.0/0000:01:00.0/
	 *              ^d   ^b ^d ^f (of the last one in the series)
	 */
	while (*current) {
	        uint16_t domain;
	        uint8_t bus, device, function;
	        struct pci_dev_info *pci_dev;
	        unsigned int i = dev->n_pci_devs;
	        struct stat statbuf;
		int pos0 = -1, pos1 = -1;

	        debug("searching for 0000:00:00.0/");
	        rc = sscanf(current, "%n%hx:%hhx:%hhx.%hhx/%n",
	                    &pos0, &domain, &bus, &device, &function, &pos1);
	        debug("current:'%s' rc:%d pos0:%d pos1:%d", current, rc, pos0, pos1);
		dbgmk("         ", pos0, pos1);
	        if (rc != 4)
	                break;
	        current += pos1;

	        debug("found pci domain %04hx:%02hhx:%02hhx.%02hhx",
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
	        char *tmp = strndup(root, current-root+1);
	        char *linkbuf = NULL;
	        if (!tmp) {
	                efi_error("could not allocate memory");
	                return -1;
	        }
	        tmp[current - root] = '\0';
	        rc = sysfs_stat(&statbuf, "class/block/%s/driver", tmp);
	        if (rc < 0 && errno == ENOENT) {
	                debug("No driver link for /sys/class/block/%s", tmp);
	                debug("Assuming this is just a buggy platform core driver");
	                dev->pci_dev[i].driverlink = NULL;
	        } else {
	                rc = sysfs_readlink(&linkbuf, "class/block/%s/driver", tmp);
	                if (rc < 0 || !linkbuf) {
	                        efi_error("Could not find driver for pci device %s", tmp);
	                        free(tmp);
	                        return -1;
	                } else {
	                        dev->pci_dev[i].driverlink = strdup(linkbuf);
	                        debug("driver:%s\n", linkbuf);
	                }
	        }
	        free(tmp);
	        dev->n_pci_devs += 1;
	}

	debug("current:'%s' sz:%zd\n", current, current - path);
	return current - path;
}

static ssize_t
dp_create_pci(struct device *dev,
	      uint8_t *buf, ssize_t size, ssize_t off)
{
	ssize_t sz = 0, new = 0;

	debug("entry buf:%p size:%zd off:%zd", buf, size, off);

	debug("creating PCI device path nodes");
	for (unsigned int i = 0; i < dev->n_pci_devs; i++) {
	        debug("creating PCI device path node %u", i);
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

	debug("returning %zd", sz);
	return sz;
}

enum interface_type pci_iftypes[] = { pci, unknown };

struct dev_probe HIDDEN pci_parser = {
	.name = "pci",
	.iftypes = pci_iftypes,
	.parse = parse_pci,
	.create = dp_create_pci,
};

// vim:fenc=utf-8:tw=75:noet
