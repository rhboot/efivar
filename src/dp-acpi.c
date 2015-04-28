/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012-2015 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <inttypes.h>
#include <stddef.h>

#include <efivar.h>
#include "dp.h"

static ssize_t
_format_acpi_adr(char *buf, size_t size, const_efidp dp)
{
	ssize_t o = 0;
	o += format(buf, size, o, "AcpiAdr(");
	o += format_array(buf, size, o, "%"PRIu32,
			  typeof(dp->acpi_adr.adr[0]),
			  dp->acpi_adr.adr,
			  (efidp_node_size(dp)-4)/sizeof (dp->acpi_adr.adr[0]));
	o += format(buf, size, o, ")");
	return o;
}
#define format_acpi_adr(buf, size, off, dp) \
	_format_acpi_adr(((buf)+(off)), ((size)?((size)-(off)):0), (dp))


ssize_t
__attribute__((__visibility__ ("default")))
_format_acpi_dn(char *buf, size_t size, const_efidp dp)
{
	ssize_t off = 0;
	const char *hidstr = NULL;
	size_t hidlen = 0;
	const char *uidstr = NULL;
	size_t uidlen = 0;
	const char *cidstr = NULL;
	size_t cidlen = 0;

	if (dp->subtype == EFIDP_ACPI_ADR)
		return format_acpi_adr(buf, size, off, dp);

	if (dp->subtype != EFIDP_ACPI_HID_EX && dp->subtype != EFIDP_ACPI_HID) {
		off += format(buf, size, off, "AcpiPath(%d,", dp->subtype);
		off += format_hex(buf, size, off, (uint8_t *)dp+4,
				  (efidp_node_size(dp)-4) / 2);
		off += format(buf, size, off, ")");
		return off;
	}

	if (dp->subtype == EFIDP_ACPI_HID_EX) {
		ssize_t limit = efidp_node_size(dp)
				- offsetof(efidp_acpi_hid_ex, hidstr);

		hidstr = dp->acpi_hid_ex.hidstr;
		hidlen = strnlen(hidstr, limit);
		limit -= hidlen + 1;

		uidstr = hidstr + hidlen + 1;
		uidlen = strnlen(uidstr, limit);
		limit -= uidlen + 1;

		cidstr = uidstr + uidlen + 1;
		cidlen = strnlen(cidstr, limit);
		limit -= cidlen + 1;

		if (uidstr) {
			switch (dp->acpi_hid.hid) {
			case EFIDP_ACPI_PCI_ROOT_HID:
				off += format(buf, size, off, "PciRoot(%s)",
					      uidstr);
				return off;
			case EFIDP_ACPI_PCIE_ROOT_HID:
				off += format(buf, size, off, "PcieRoot(%s)",
					      uidstr);
				return off;
			}
		}
	}

	switch (dp->acpi_hid.hid) {
	case EFIDP_ACPI_PCI_ROOT_HID:
		off += format(buf, size, off, "PciRoot(0x%"PRIx32")",
			      dp->acpi_hid.uid);
		return off;
	case EFIDP_ACPI_PCIE_ROOT_HID:
		off += format(buf, size, off, "PcieRoot(0x%"PRIx32")",
			      dp->acpi_hid.uid);
		return off;
	case EFIDP_ACPI_FLOPPY_HID:
		off += format(buf, size, off, "Floppy(0x%"PRIx32")",
			      dp->acpi_hid.uid);
		return off;
	case EFIDP_ACPI_KEYBOARD_HID:
		off += format(buf, size, off, "Keyboard(0x%"PRIx32")",
			      dp->acpi_hid.uid);
		return off;
	case EFIDP_ACPI_SERIAL_HID:
		off += format(buf, size, off, "Serial(0x%"PRIx32")",
			      dp->acpi_hid.uid);
		return off;
	default:
		switch (dp->subtype) {
		case EFIDP_ACPI_HID_EX:
			if (!hidstr && !cidstr &&
					(uidstr || dp->acpi_hid_ex.uid)){
				off += format(buf, size, off,
					      "AcpiExp(0x%"PRIx32",0x%"PRIx32",",
					      dp->acpi_hid_ex.hid,
					      dp->acpi_hid_ex.cid);
				if (uidstr)
					off += format(buf, size, off, "%s)",
						      uidstr);
				else
					off += format(buf, size, off,
						      "0x%"PRIx32")",
						      dp->acpi_hid.uid);
				return off;
			}
			off += format(buf, size, off, "AcpiEx(");
			if (hidstr)
				off += format(buf, size, off, "%s,", hidstr);
			else
				off += format(buf, size, off, "0x%"PRIx32",",
					      dp->acpi_hid.hid);
			if (cidstr)
				off += format(buf, size, off, "%s,", cidstr);
			else
				off += format(buf, size, off, "0x%"PRIx32",",
					      dp->acpi_hid_ex.cid);

			if (uidstr)
				off += format(buf, size, off, "%s)", uidstr);
			else
				off += format(buf, size, off, "0x%"PRIx32")",
					      dp->acpi_hid.uid);
			return off;
		case EFIDP_ACPI_HID:
			off += format(buf, size, off,
				      "Acpi(0x%"PRIx32",0x%"PRIx32")",
				      dp->acpi_hid.hid, dp->acpi_hid.uid);
			return off;
		}
	}

	return off;
}

ssize_t
__attribute__((__visibility__ ("default")))
efidp_make_acpi_hid(uint8_t *buf, ssize_t size, uint32_t hid, uint32_t uid)
{
	efidp_acpi_hid *acpi_hid = (efidp_acpi_hid *)buf;
	ssize_t req = sizeof (*acpi_hid);
	ssize_t sz;

	sz = efidp_make_generic(buf, size, EFIDP_ACPI_TYPE, EFIDP_ACPI_HID,
				sizeof (*acpi_hid));
	if (size && sz == req) {
		acpi_hid->uid = uid;
		acpi_hid->hid = hid;
	}

	return sz;
}

ssize_t
__attribute__((__visibility__ ("default")))
__attribute__((__nonnull__ (6,7,8)))
efidp_make_acpi_hid_ex(uint8_t *buf, ssize_t size,
		       uint32_t hid, uint32_t uid, uint32_t cid,
		       char *hidstr, char *uidstr, char *cidstr)
{
	efidp_acpi_hid_ex *acpi_hid = (efidp_acpi_hid_ex *)buf;
	ssize_t req;
	ssize_t sz;

	if (!hidstr || !uidstr || !cidstr) {
		errno = EINVAL;
		return -1;
	}

	req = sizeof (*acpi_hid) + 3 +
		strlen(hidstr) + strlen(uidstr) + strlen(cidstr);
	sz = efidp_make_generic(buf, size, EFIDP_ACPI_TYPE, EFIDP_ACPI_HID_EX,
				req);
	if (size && sz == req) {
		acpi_hid->uid = uid;
		acpi_hid->hid = hid;
		acpi_hid->cid = cid;
		char *next = (char *)buf+offsetof(efidp_acpi_hid_ex, hidstr);
		strcpy(next, hidstr);
		next += strlen(hidstr) + 1;
		strcpy(next, uidstr);
		next += strlen(uidstr) + 1;
		strcpy(next, cidstr);
	}

	return sz;
}
