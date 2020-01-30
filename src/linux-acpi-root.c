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
 * support for ACPI-like platform root hub and devices
 *
 * various devices /sys/dev/block/$major:$minor start with:
 * maj:min -> ../../devices/ACPI0000:00/$PCI_DEVICES/$BLOCKDEV_STUFF/block/$DISK/$PART
 * i.e.:		    APMC0D0D:00/ata1/host0/target0:0:0/0:0:0:0/block/sda
 *			    ^ root hub ^blockdev stuff
 * or:
 * maj:min -> ../../devices/ACPI0000:00/$PCI_DEVICES/$BLOCKDEV_STUFF/block/$DISK/$PART
 * i.e.:		    APMC0D0D:00/0000:00:1d.0/0000:05:00.0/ata1/host0/target0:0:0/0:0:0:0/block/sda
 *			    ^ root hub ^pci dev      ^pci dev     ^ blockdev stuff
 */
static ssize_t
parse_acpi_root(struct device *dev, const char *path, const char *root UNUSED)
{
	const char *current = path;
	int rc;
	int pos0 = -1, pos1 = -1, pos2 = -1;
	uint16_t pad0;
	uint8_t pad1;
	char *acpi_header = NULL;
	char *colon;

	debug("entry");

	/*
	 * find the ACPI root dunno0 and dunno1; they basically look like:
	 * ABCD0000:00/
	 *     ^d0  ^d1
	 * This is annoying because "/%04ms%h:%hhx/" won't bind from the right
	 * side in sscanf.
	 */
	rc = sscanf(current, "../../devices/%nplatform/%n", &pos0, &pos1);
	debug("current:'%s' rc:%d pos0:%d pos1:%d", current, rc, pos0, pos1);
	dbgmk("         ", pos0, pos1);
	if (rc != 0 || pos0 == -1 || pos1 == -1)
		return 0;
	current += pos1;

	debug("searching for an ACPI string like A0000:00 or ACPI0000:00");
	pos0 = 0;
	/*
	 * If it's too short to be A0000:00, it's not an ACPI string
	 */
	if (strlen(current) < 8)
		return 0;

	colon = strchr(current, ':');
	if (!colon)
		return 0;
	pos1 = colon - current;

	/*
	 * If colon doesn't point at something between one of these:
	 * A0000:00 ACPI0000:00
	 *	^ 5	    ^ 8
	 * Then it's not an ACPI string.
	 */
	if (pos1 < 5 || pos1 > 8)
		return 0;

	debug("current:'%s' rc:%d pos0:%d pos1:%d", current, rc, pos0, pos1);
	dbgmk("         ", pos0, pos1);

	dev->acpi_root.acpi_hid_str = strndup(current, pos1 + 1);
	if (!dev->acpi_root.acpi_hid_str) {
		efi_error("Could not allocate memory");
		return -1;
	}
	dev->acpi_root.acpi_hid_str[pos1] = 0;
	debug("acpi_hid_str:'%s'", dev->acpi_root.acpi_hid_str);

	/*
	 * The string is like ACPI0000:00.
	 *                    ^^^^
	 * Here we're saying only this bit has been parsed, though we've
	 * partially parsed up to the colon.
	 */
	pos1 -= 4;
	acpi_header = strndupa(current, pos1);
	if (!acpi_header)
		return 0;
	acpi_header[pos1] = 0;
	debug("acpi_header:'%s'", acpi_header);

	/*
	 * If we can't find these numbers, it's not an ACPI string
	 */
	rc = sscanf(current+pos1, "%hx:%hhx/%n", &pad0, &pad1, &pos2);
	if (rc != 2) {
		efi_error("Could not parse ACPI path \"%s\"", current);
		return 0;
	}
	debug("current:'%s' rc:%d pos0:%d pos1:%d pos2:%d",
	      current, rc, pos0, pos1, pos2);
	dbgmk("         ", pos0, pos2);
	current += pos2;

	const char * const formats[] = {
		"devices/platform/%s%04hX:%02hhX",
		"devices/platform/%s%04hx:%02hhx",
		NULL
	};

	for (unsigned int i = 0; formats[i]; i++) {
		rc = parse_acpi_hid_uid(dev, formats[i],
					acpi_header, pad0, pad1);
		debug("rc:%d acpi_header:%s pad0:%04hx pad1:%02hhx",
		      rc, acpi_header, pad0, pad1);
		if (rc >= 0)
			break;
		if (errno != ENOENT) {
			efi_error("Could not parse hid/uid");
			return rc;
		}
	}
	debug("Parsed HID:0x%08x UID:0x%"PRIx64" uidstr:'%s' path:'%s'",
	      dev->acpi_root.acpi_hid, dev->acpi_root.acpi_uid,
	      dev->acpi_root.acpi_uid_str,
	      dev->acpi_root.acpi_cid_str);

	debug("current:'%s' sz:%zd", current, current - path);
	return current - path;
}

static ssize_t
dp_create_acpi_root(struct device *dev,
		    uint8_t *buf, ssize_t size, ssize_t off)
{
	ssize_t sz = 0, new = 0;

	debug("entry buf:%p size:%zd off:%zd", buf, size, off);

	if (dev->acpi_root.acpi_uid_str || dev->acpi_root.acpi_cid_str) {
		debug("creating acpi_hid_ex dp hid:0x%08x uid:0x%"PRIx64" uidstr:'%s' cidstr:'%s'",
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

// vim:fenc=utf-8:tw=75:noet
