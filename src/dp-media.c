/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012-2015 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#include "fix_coverity.h"

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stddef.h>

#include "efivar.h"

ssize_t
_format_media_dn(char *buf, size_t size, const_efidp dp)
{
	ssize_t off = 0;
	switch (dp->subtype) {
	case EFIDP_MEDIA_HD:
		format(buf, size, off, "HD", "HD(%d,", dp->hd.partition_number);
		switch (dp->hd.signature_type) {
		case EFIDP_HD_SIGNATURE_MBR:
			format(buf, size, off, "HD",
			       "MBR,0x%"PRIx32",0x%"PRIx64",0x%"PRIx64")",
			       (uint32_t)dp->hd.signature[0] |
			       ((uint32_t)dp->hd.signature[1] << 8) |
			       ((uint32_t)dp->hd.signature[2] << 16) |
			       ((uint32_t)dp->hd.signature[3] << 24),
			       dp->hd.start, dp->hd.size);
			break;
		case EFIDP_HD_SIGNATURE_GUID:
			format(buf, size, off, "HD", "GPT,");
			format_guid(buf, size, off, "HD",
				    (efi_guid_t *)dp->hd.signature);
			format(buf, size, off, "HD",
			       ",0x%"PRIx64",0x%"PRIx64")",
			       dp->hd.start, dp->hd.size);
			break;
		default:
			format(buf, size, off, "HD", "%d,",
			       dp->hd.signature_type);
			format_hex(buf, size, off, "HD", dp->hd.signature,
				   sizeof(dp->hd.signature));
			format(buf, size, off, "HD",
			       ",0x%"PRIx64",0x%"PRIx64")",
			       dp->hd.start, dp->hd.size);
			break;
		}
		break;
	case EFIDP_MEDIA_CDROM:
		format(buf, size, off, "CDROM",
		       "CDROM(%d,0x%"PRIx64",0x%"PRIx64")",
		       dp->cdrom.boot_catalog_entry,
		       dp->cdrom.partition_rba, dp->cdrom.sectors);
		break;
	case EFIDP_MEDIA_VENDOR:
		format_vendor(buf, size, off, "VenMedia", dp);
		break;
	case EFIDP_MEDIA_FILE: {
		size_t limit = (efidp_node_size(dp)
				- offsetof(efidp_file, name)) / 2;
		format(buf, size, off, "File", "File(");
		format_ucs2(buf, size, off, "File", dp->file.name, limit);
		format(buf, size, off, "File", ")");
		break;
			       }
	case EFIDP_MEDIA_PROTOCOL:
		format(buf, size, off, "Media", "Media(");
		format_guid(buf, size, off, "Media",
			    &dp->protocol.protocol_guid);
		format(buf, size, off, "Media", ")");
		break;
	case EFIDP_MEDIA_FIRMWARE_FILE:
		format(buf, size, off, "FvFile", "FvFile(");
		format_guid(buf, size, off, "FvFile",
			    &dp->protocol.protocol_guid);
		format(buf, size, off, "FvFile", ")");
		break;
	case EFIDP_MEDIA_FIRMWARE_VOLUME:
		format(buf, size, off, "FvVol", "FvVol(");
		format_guid(buf, size, off, "FvVol",
			    &dp->protocol.protocol_guid);
		format(buf, size, off, "FvVol", ")");
		break;
	case EFIDP_MEDIA_RELATIVE_OFFSET:
		format(buf, size, off, "Offset",
		       "Offset(0x%"PRIx64",0x%"PRIx64")",
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
			format(buf, size, off, label,
			       "%s(0x%"PRIx64",0x%"PRIx64",%d)", label,
			       dp->ramdisk.start_addr,
			       dp->ramdisk.end_addr,
			       dp->ramdisk.instance_number);
			break;
		}
		format(buf, size, off, "Ramdisk",
		       "Ramdisk(0x%"PRIx64",0x%"PRIx64",%d,",
		       dp->ramdisk.start_addr, dp->ramdisk.end_addr,
		       dp->ramdisk.instance_number);
		format_guid(buf, size, off, "Ramdisk",
			    &dp->ramdisk.disk_type_guid);
		format(buf, size, off, "Ramdisk", ")");
		break;
					   }
	default:
		format(buf, size, off, "Media", "MediaPath(%d,", dp->subtype);
		format_hex(buf, size, off, "Media", (uint8_t *)dp+4,
				(efidp_node_size(dp)-4) / 2);
		format(buf,size,off, "Media",")");
		break;
	}
	return off;
}

ssize_t PUBLIC
efidp_make_file(uint8_t *buf, ssize_t size, char *filepath)
{
	efidp_file *file = (efidp_file *)buf;
	unsigned char *lf = (unsigned char *)filepath;
	ssize_t sz;
	ssize_t len = utf8len(lf, -1) + 1;
	ssize_t req = sizeof (*file) + len * sizeof (uint16_t);

	if (len == 0) {
		errno = EINVAL;
		efi_error("%s() called with %s file path", __func__,
			  filepath == NULL ? "NULL" : "empty");
		return -1;
	}
	sz = efidp_make_generic(buf, size, EFIDP_MEDIA_TYPE, EFIDP_MEDIA_FILE,
				req);
	if (size && sz == req) {
		memset(buf+4, 0, req-4);
		utf8_to_ucs2(file->name, req-4, 1, lf);
	}

	if (sz < 0)
		efi_error("efidp_make_generic failed");

	return sz;
}

ssize_t PUBLIC
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

	if (sz < 0)
		efi_error("efidp_make_generic failed");

	return sz;
}
