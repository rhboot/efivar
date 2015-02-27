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
format_hw_dn(char *buf, size_t size, const_efidp dp)
{
	off_t off = 0;
	size_t sz;
	switch (dp->subtype) {
	case EFIDP_HW_PCI_SUBTYPE:
		off += pbufx(buf, size, off, "Pci(%d,%d)",
			     dp->pci.device, dp->pci.function);
		break;
	case EFIDP_HW_PCCARD_SUBTYPE:
		off += pbufx(buf, size, off, "PcCard(%d)",
			     dp->pccard.function);
		break;
	case EFIDP_HW_MMIO:
		off += pbufx(buf, size, off,
			     "MemoryMapped(0x%"PRIx32",0x%"PRIx64",0x%"PRIx64")",
			     dp->mmio.memory_type, dp->mmio.starting_address,
			     dp->mmio.ending_address);
		break;
	case EFIDP_HW_VENDOR:
		off += format_vendor(buf+off, size?size-off:0, "VenHw", dp);
		break;
	case EFIDP_HW_CONTROLLER:
		off += pbufx(buf, size, off, "Ctrl(0x%"PRIx32")",
			     dp->controller.controller);
		break;
	case EFIDP_HW_BMC:
		off += pbufx(buf, size, off, "BMC(%d,0x%"PRIx64")",
			     dp->bmc.interface_type,
			     dp->bmc.base_addr);
		break;
	default:
		off += pbufx(buf, size, off, "HardwarePath(%d,", dp->subtype);
		sz = format_hex(buf+off, size?size-off:0, (uint8_t *)dp+4,
			       efidp_node_size(dp)-4);
		if (sz < 0)
			return sz;
		off += sz;
		off += pbufx(buf,size,off,")");
		break;
	}
	return off;
}
