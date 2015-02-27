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
#include "ucs2.h"

ssize_t
format_media_dn(char *buf, size_t size, const_efidp dp)
{
	off_t off = 0;
	size_t sz;
	switch (dp->subtype) {
	case EFIDP_MEDIA_HD:
		off += pbufx(buf, size, off, "HD(%d,", dp->hd.partition_number);
		switch (dp->hd.signature_type) {
		case EFIDP_HD_SIGNATURE_MBR: {
			uint32_t *sig = (uint32_t *)dp->hd.signature;
			off += pbufx(buf, size, off,
				     "MBR,0x%"PRIu32",0x%"PRIx64",0x%"PRIx64")",
				     *sig, dp->hd.start, dp->hd.size);
			break;
					     }
		case EFIDP_HD_SIGNATURE_GUID: {
			char *guidstr = NULL;
			int rc;

			rc = efi_guid_to_str((efi_guid_t *)dp->hd.signature,
					     &guidstr);
			if (rc < 0)
				return rc;

			guidstr = onstack(guidstr, strlen(guidstr)+1);
			off += pbufx(buf, size, off,
				     "GPT,%s,0x%"PRIx64",0x%"PRIx64")",
				     guidstr, dp->hd.start, dp->hd.size);
			break;
					      }
		default:
			off += pbufx(buf, size, off, "%d,",
				     dp->hd.signature_type);
			sz = format_hex(buf+off, size?size-off:0,
				       dp->hd.signature,
				       sizeof(dp->hd.signature));
			if (sz < 0)
				return sz;
			off += sz;
			off += pbufx(buf, size, off,
				     ",0x%"PRIx64",0x%"PRIx64")",
				     dp->hd.start, dp->hd.size);
			break;
		}
		break;
	case EFIDP_MEDIA_CDROM:
		off += pbufx(buf, size, off, "CDROM(%d,0x%"PRIx64",0x%"PRIx64")",
			     dp->cdrom.boot_catalog_entry,
			     dp->cdrom.partition_rba,
			     dp->cdrom.sectors);
		break;
	case EFIDP_MEDIA_VENDOR:
		off += format_vendor(buf+off, size?size-off:0, "VenMedia", dp);
		break;
	case EFIDP_MEDIA_FILE: {
		char *str = ucs2_to_utf8(dp->file.name, efidp_node_size(dp)-4);
		str = onstack(str, strlen(str)+1);

		off += pbufx(buf, size, off, "File(%s)", str);
		break;
			       }
	case EFIDP_MEDIA_PROTOCOL: {
		char *guidstr = NULL;
		int rc;

		rc = efi_guid_to_str(&dp->protocol.protocol_guid,
				     &guidstr);
		if (rc < 0)
			return rc;

		guidstr = onstack(guidstr, strlen(guidstr)+1);
		off = pbufx(buf, size, off, "Media(%s)", guidstr);
		break;
				   }
	case EFIDP_MEDIA_FIRMWARE_FILE: {
		char *guidstr = NULL;
		int rc;

		rc = efi_guid_to_str(&dp->protocol.protocol_guid,
				     &guidstr);
		if (rc < 0)
			return rc;

		guidstr = onstack(guidstr, strlen(guidstr)+1);
		off = pbufx(buf, size, off, "FvFile(%s)", guidstr);
		break;
				   }
	case EFIDP_MEDIA_FIRMWARE_VOLUME: {
		char *guidstr = NULL;
		int rc;

		rc = efi_guid_to_str(&dp->protocol.protocol_guid,
				     &guidstr);
		if (rc < 0)
			return rc;

		guidstr = onstack(guidstr, strlen(guidstr)+1);
		off = pbufx(buf, size, off, "FvVol(%s)", guidstr);
		break;
				   }
	case EFIDP_MEDIA_RELATIVE_OFFSET:
		off = pbufx(buf, size, off, "Offset(0x%"PRIx64",0x%"PRIx64")",
			    dp->relative_offset.first_byte,
			    dp->relative_offset.last_byte);
		break;
	case EFIDP_MEDIA_RAMDISK: {
		struct {
			efi_guid_t guid;
			char label[40];
		} subtypes[] = {
			{ EFIDP_VIRTUAL_DISK_GUID, "VirtualDisk" },
			{ EFIDP_VIRTUAL_CD_GUID, "VirtualCD" },
			{ EFIDP_PERSISTENT_VIRTUAL_DISK_GUID, "PersistentVirtualDisk" },
			{ EFIDP_PERSISTENT_VIRTUAL_CD_GUID, "PersistentVirtualCD" },
			{ efi_guid_empty, "" }
		};
		char *label = NULL;

		for (int i = 0; !efi_guid_is_zero(&subtypes[i].guid); i++) {
			if (efi_guid_cmp(&subtypes[i].guid,
					  &dp->ramdisk.disk_type_guid))
			continue;

			label = subtypes[i].label;
			break;
		}

		if (label) {
			off = pbufx(buf, size, off,
				    "%s(0x%"PRIx64",0x%"PRIx64",%d)",
				    label, dp->ramdisk.start_addr,
				    dp->ramdisk.end_addr,
				    dp->ramdisk.instance_number);
			break;
		}

		char *guidstr = NULL;
		int rc;
		rc = efi_guid_to_str(&dp->ramdisk.disk_type_guid,
				     &guidstr);
		if (rc < 0)
			return rc;

		guidstr = onstack(guidstr, strlen(guidstr)+1);

		off = pbufx(buf, size, off, "Ramdisk(0x%"PRIx64",0x%"PRIx64",%d,%s)",
			    dp->ramdisk.start_addr,
			    dp->ramdisk.end_addr,
			    dp->ramdisk.instance_number,
			    guidstr);
		break;
					   }
	default:
		off += pbufx(buf, size, off, "MediaPath(%d,", dp->subtype);
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
