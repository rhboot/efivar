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
 * /sys/class/net/$IFACE -> ../../devices/$PCI_STUFF/net/$IFACE
 *
 */
static ssize_t
parse_pci_root(struct device *dev, const char *path, const char *root UNUSED)
{
	const char * current = path;
	int rc;
	int pos0 = -1, pos1 = -1;
	uint16_t root_domain;
	uint8_t root_bus;

	debug("entry");

	/*
	 * find the pci root domain and port; they basically look like:
	 * pci0000:00/
	 *    ^d   ^p
	 */
	rc = sscanf(current, "%n../../devices/pci%hx:%hhx/%n", &pos0, &root_domain, &root_bus, &pos1);
	debug("current:'%s' rc:%d pos0:%d pos1:%d", current, rc, pos0, pos1);
	dbgmk("         ", pos0, pos1);
	/*
	 * If we can't find that, it's not a PCI device.
	 */
	if (rc != 2)
	        return 0;
	current += pos1;

	dev->pci_root.pci_domain = root_domain;
	dev->pci_root.pci_bus = root_bus;

	rc = parse_acpi_hid_uid(dev, "devices/pci%04hx:%02hhx",
	                        root_domain, root_bus);
	if (rc < 0)
	        return -1;

	errno = 0;
	debug("current:'%s' sz:%zd\n", current, current - path);
	return current - path;
}

static ssize_t
dp_create_pci_root(struct device *dev UNUSED,
	           uint8_t *buf, ssize_t size, ssize_t off)
{
	ssize_t new = 0, sz = 0;
	debug("entry buf:%p size:%zd off:%zd", buf, size, off);
	debug("returning 0");
	if (dev->acpi_root.acpi_uid_str) {
	        debug("creating acpi_hid_ex dp hid:0x%08x uid:'%s'",
	              dev->acpi_root.acpi_hid,
	              dev->acpi_root.acpi_uid_str);
	        new = efidp_make_acpi_hid_ex(buf + off, size ? size - off : 0,
	                                    dev->acpi_root.acpi_hid,
	                                    0, 0, "",
	                                    dev->acpi_root.acpi_uid_str,
	                                    "");
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

enum interface_type pci_root_iftypes[] = { pci_root, unknown };

struct dev_probe HIDDEN pci_root_parser = {
	.name = "pci_root",
	.iftypes = pci_root_iftypes,
	.flags = DEV_PROVIDES_ROOT,
	.parse = parse_pci_root,
	.create = dp_create_pci_root,
};

// vim:fenc=utf-8:tw=75:noet
