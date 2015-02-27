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

#include "efivar.h"
#include "dp.h"

ssize_t
format_acpi_dn(char *buf, size_t size, const_efidp dp)
{
	off_t off = 0;
	size_t sz;
	switch (dp->subtype) {
	case EFIDP_ACPI_HID:
		off += pbufx(buf, size, off, "ACPI(0x%"PRIx32",0x%"PRIx32")",
			     dp->acpi_hid.hid, dp->acpi_hid.uid);
		break;

	default:
		off += pbufx(buf, size, off, "AcpiPath(%d,", dp->subtype);
		sz = format_hex(buf+off, size?size-off:0, (uint8_t *)dp+4,
			       (efidp_node_size(dp)-4) / 2);
		if (sz < 0)
			return sz;
		off += sz;
		off += pbufx(buf,size,off,")");
		break;
	}
	return off;
}
