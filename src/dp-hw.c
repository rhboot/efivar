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

#include <efivar.h>
#include "dp.h"

ssize_t
__attribute__((__visibility__ ("default")))
format_edd10_guid(char *buf, size_t size, const_efidp dp)
{
	size_t off = 0;
	efidp_edd10 const *edd_dp = (efidp_edd10 *)dp;
	off = format(buf, size, off, "EDD10(0x%"PRIx32")",
		     edd_dp->hardware_device);
	return off;
}

ssize_t
__attribute__((__visibility__ ("default")))
_format_hw_dn(char *buf, size_t size, const_efidp dp)
{
	efi_guid_t edd10_guid = EDD10_HARDWARE_VENDOR_PATH_GUID;
	off_t off = 0;
	switch (dp->subtype) {
	case EFIDP_HW_PCI:
		off += format(buf, size, off, "Pci(0x%"PRIx32",0x%"PRIx32")",
			      dp->pci.device, dp->pci.function);
		break;
	case EFIDP_HW_PCCARD:
		off += format(buf, size, off, "PcCard(0x%"PRIx32")",
			      dp->pccard.function);
		break;
	case EFIDP_HW_MMIO:
		off += format(buf, size, off,
			      "MemoryMapped(%"PRIu32",0x%"PRIx64",0x%"PRIx64")",
			      dp->mmio.memory_type, dp->mmio.starting_address,
			      dp->mmio.ending_address);
		break;
	case EFIDP_HW_VENDOR:
		if (!efi_guid_cmp(&dp->hw_vendor.vendor_guid, &edd10_guid)) {
			off += format_helper(format_edd10_guid, buf, size,
					     off, dp);
		} else {
			off += format_vendor(buf, size, off, "VenHw", dp);
		}
		break;
	case EFIDP_HW_CONTROLLER:
		off += format(buf, size, off, "Ctrl(0x%"PRIx32")",
			      dp->controller.controller);
		break;
	case EFIDP_HW_BMC:
		off += format(buf, size, off, "BMC(%d,0x%"PRIx64")",
			      dp->bmc.interface_type, dp->bmc.base_addr);
		break;
	default:
		off += format(buf, size, off, "HardwarePath(%d,", dp->subtype);
		off += format_hex(buf, size, off, (uint8_t *)dp+4,
				  efidp_node_size(dp)-4);
		off += format(buf,size,off,")");
		break;
	}
	return off;
}

ssize_t
__attribute__((__visibility__ ("default")))
efidp_make_pci(uint8_t *buf, ssize_t size, uint8_t device, uint8_t function)
{
	efidp_pci *pci = (efidp_pci *)buf;
	ssize_t sz;
	ssize_t req = sizeof (*pci);
	sz = efidp_make_generic(buf, size, EFIDP_HARDWARE_TYPE, EFIDP_HW_PCI,
				req);
	if (size && sz == req) {
		pci->device = device;
		pci->function = function;
	}
	return sz;
}

ssize_t
__attribute__((__visibility__ ("default")))
efidp_make_edd10(uint8_t *buf, ssize_t size, uint32_t hardware_device)
{
	efi_guid_t edd10_guid = EDD10_HARDWARE_VENDOR_PATH_GUID;
	efidp_edd10 *edd_dp = (efidp_edd10 *)buf;
	ssize_t sz;
	ssize_t req = sizeof (*edd_dp);
	sz = efidp_make_generic(buf, size, EFIDP_HARDWARE_TYPE, EFIDP_HW_VENDOR,
				req);
	if (size && sz == req) {
		memcpy(&edd_dp->vendor_guid, &edd10_guid, sizeof (edd10_guid));
		edd_dp->hardware_device = hardware_device;
	}
	return sz;
}
