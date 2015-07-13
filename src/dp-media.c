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

ssize_t
__attribute__((__visibility__ ("default")))
_format_media_dn(char *buf, size_t size, const_efidp dp)
{
	ssize_t off = 0;
	switch (dp->subtype) {
	case EFIDP_MEDIA_HD:
		off += format(buf, size, off, "HD(%d,",
			      dp->hd.partition_number);
		switch (dp->hd.signature_type) {
		case EFIDP_HD_SIGNATURE_MBR:
			off += format(buf, size, off,
				      "MBR,0x%"PRIu32",0x%"PRIx64",0x%"PRIx64")",
				      *(char *)dp->hd.signature,
				      dp->hd.start, dp->hd.size);
			break;
		case EFIDP_HD_SIGNATURE_GUID:
			off += format(buf, size, off, "GPT,");
			off += format_guid(buf, size, off,
					   (efi_guid_t *)dp->hd.signature);
			off += format(buf, size, off,
				      ",0x%"PRIx64",0x%"PRIx64")",
				      dp->hd.start, dp->hd.size);
			break;
		default:
			off += format(buf, size, off, "%d,",
				      dp->hd.signature_type);
			off += format_hex(buf, size, off,
					  dp->hd.signature,
					  sizeof(dp->hd.signature));
			off += format(buf, size, off,
				      ",0x%"PRIx64",0x%"PRIx64")",
				      dp->hd.start, dp->hd.size);
			break;
		}
		break;
	case EFIDP_MEDIA_CDROM:
		off += format(buf, size, off,
			      "CDROM(%d,0x%"PRIx64",0x%"PRIx64")",
			      dp->cdrom.boot_catalog_entry,
			      dp->cdrom.partition_rba, dp->cdrom.sectors);
		break;
	case EFIDP_MEDIA_VENDOR:
		off += format_vendor(buf, size, off, "VenMedia", dp);
		break;
	case EFIDP_MEDIA_FILE:
		off += format(buf, size, off, "File(");
		off += format_ucs2(buf, size, off, dp->file.name,
				   (efidp_node_size(dp)
				   - offsetof(efidp_file, name)) / 2);
		off += format(buf, size, off, ")");
		break;
	case EFIDP_MEDIA_PROTOCOL:
		off += format(buf, size, off, "Media(");
		off += format_guid(buf, size, off, &dp->protocol.protocol_guid);
		off += format(buf, size, off, ")");
		break;
	case EFIDP_MEDIA_FIRMWARE_FILE:
		off += format(buf, size, off, "FvFile(");
		off += format_guid(buf, size, off, &dp->protocol.protocol_guid);
		off += format(buf, size, off, ")");
		break;
	case EFIDP_MEDIA_FIRMWARE_VOLUME:
		off += format(buf, size, off, "FvVol(");
		off += format_guid(buf, size, off, &dp->protocol.protocol_guid);
		off += format(buf, size, off, ")");
		break;
	case EFIDP_MEDIA_RELATIVE_OFFSET:
		off = format(buf, size, off, "Offset(0x%"PRIx64",0x%"PRIx64")",
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
			off += format(buf, size, off,
				     "%s(0x%"PRIx64",0x%"PRIx64",%d)", label,
				     dp->ramdisk.start_addr,
				     dp->ramdisk.end_addr,
				     dp->ramdisk.instance_number);
			break;
		}
		off += format(buf, size, off,
			     "Ramdisk(0x%"PRIx64",0x%"PRIx64",%d,",
			     dp->ramdisk.start_addr, dp->ramdisk.end_addr,
			     dp->ramdisk.instance_number);
		off += format_guid(buf, size, off, &dp->ramdisk.disk_type_guid);
		off += format(buf, size, off, ")");
		break;
					   }
	default:
		off += format(buf, size, off, "MediaPath(%d,", dp->subtype);
		off += format_hex(buf, size, off,
				  (uint8_t *)dp+4,
				  (efidp_node_size(dp)-4) / 2);
		off += format(buf,size,off,")");
		break;
	}
	return off;
}

ssize_t
__attribute__((__visibility__ ("default")))
efidp_make_file(uint8_t *buf, ssize_t size, char *filepath)
{
	efidp_file *file = (efidp_file *)buf;
	unsigned char *lf = (unsigned char *)filepath;
	ssize_t sz;
	ssize_t len = utf8len(lf, -1) + 1;
	ssize_t req = sizeof (*file) + len * sizeof (uint16_t);
	sz = efidp_make_generic(buf, size, EFIDP_MEDIA_TYPE, EFIDP_MEDIA_FILE,
				req);
	if (size && sz == req) {
		memset(buf+4, 0, req-4);
		utf8_to_ucs2(file->name, req-4, 1, lf);
	}
	return sz;
}

ssize_t
__attribute__((__visibility__ ("default")))
efidp_make_hd(uint8_t *buf, ssize_t size, uint32_t num, uint64_t part_start,
	      uint64_t part_size, uint8_t *signature, uint8_t format,
	      uint8_t signature_type)
{
	efidp_hd *hd = (efidp_hd *)buf;
	ssize_t sz;
	ssize_t req = sizeof (*hd);
	sz = efidp_make_generic(buf, size, EFIDP_MEDIA_TYPE, EFIDP_MEDIA_HD,
				req);
	if (size && sz == req) {
		hd->partition_number = num;
		hd->start = part_start;
		hd->size = part_size;
		if (signature)
			memcpy(hd->signature, signature,
			       sizeof (hd->signature));
		hd->format = format;
		hd->signature_type = signature_type;
	}
	return sz;
}
