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

#include "efivar.h"
#include "dp.h"

ssize_t
print_hw_dn(char *buf, size_t size, const_efidp dp)
{
	off_t off = 0;
	switch (dp->subtype) {
	case EFIDP_HW_PCI_SUBTYPE:
		off += pbufx(buf, size, off, "Pci(%d,%d)", dp->pci.device,
			     dp->pci.function);
		break;
	case EFIDP_HW_PCCARD_SUBTYPE:
	case EFIDP_HW_MMIO:
	case EFIDP_HW_VENDOR:
	case EFIDP_HW_CONTROLLER:
	default:
		off += pbufx(buf, size, off, "HardwarePath(%d,", dp->subtype);
		for (int i = 4; i < efidp_node_size(dp); i++)
			off += pbufx(buf, size, off, "%02x",
					  *((const char const *)dp+i+4));
		off += pbufx(buf,size,off,")");
		break;
	}
	return off;
}
