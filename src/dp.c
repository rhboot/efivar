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

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#include <efivar.h>
#include "dp.h"

static const efidp_header end_entire = {
	.type = EFIDP_END_TYPE,
	.subtype = EFIDP_END_ENTIRE,
	.length = 4
};
static const efidp_header end_instance = {
	.type = EFIDP_END_TYPE,
	.subtype = EFIDP_END_INSTANCE,
	.length = 4
};

static inline void *
efidp_data_address(const_efidp dp)
{
	if (dp->length <= 4) {
		errno = ENOSPC;
		return NULL;
	}
	return (void *)((uint8_t *)dp + sizeof (dp));
}

int
__attribute__((__visibility__ ("default")))
efidp_set_node_data(const_efidp dn, void *buf, size_t bufsize)
{
	if (dn->length < 4 || bufsize > (size_t)dn->length - 4) {
		errno = ENOSPC;
		return -1;
	}
	void *data = efidp_data_address(dn);
	if (!data)
		return -1;
	memcpy(data, buf, bufsize);
	return 0;
}

static inline int
efidp_duplicate_extra(const_efidp dp, efidp *out, size_t extra)
{
	ssize_t sz;
	size_t plus;

	efidp new;

	sz = efidp_size(dp);
	if (sz < 0)
		return sz;

	plus = (size_t)sz + extra;
	if (plus < (size_t)sz || plus < extra) {
		errno = ENOSPC;
		return -1;
	}

	new = calloc(1, plus);
	if (!new)
		return -1;

	memcpy(new, dp, sz);
	*out = new;
	return 0;
}

int
__attribute__((__visibility__ ("default")))
efidp_duplicate_path(const_efidp  dp, efidp *out)
{
	return efidp_duplicate_extra(dp, out, 0);
}

int
__attribute__((__visibility__ ("default")))
efidp_append_path(const_efidp dp0, const_efidp dp1, efidp *out)
{
	ssize_t lsz, rsz;
	const_efidp le;
	int rc;

	if (!dp0 && !dp1)
		return efidp_duplicate_path((const_efidp)&end_entire, out);

	if (dp0 && !dp1)
		return efidp_duplicate_path(dp0, out);

	if (!dp0 && dp1)
		return efidp_duplicate_path(dp1, out);

	lsz = efidp_size(dp0);
	if (lsz < 0)
		return -1;

	rsz = efidp_size(dp1);
	if (rsz < 0)
		return -1;

	le = dp0;
	while (1) {
		if (efidp_type(le) == EFIDP_END_TYPE &&
				efidp_subtype(le) == EFIDP_END_ENTIRE) {
			ssize_t lesz = efidp_size(le);
			if (lesz < 0)
				return -1;
			lsz -= lesz;
			break;
		}

		rc = efidp_get_next_end(le, &le);
		if (rc < 0)
			return -1;
	}

	efidp new;
	new = malloc(lsz + rsz);
	if (!new)
		return -1;

	*out = new;

	memcpy(new, dp0, lsz);
	memcpy((uint8_t *)new + lsz, dp1, rsz);

	return 0;
}

int
__attribute__((__visibility__ ("default")))
efidp_append_node(const_efidp dp, const_efidp dn, efidp *out)
{
	ssize_t lsz, rsz;
	int rc;

	if (!dp && !dn)
		return efidp_duplicate_path((const_efidp)(const efidp_header * const)&end_entire, out);

	if (dp && !dn)
		return efidp_duplicate_path(dp, out);

	if (!dp && dn) {
		efidp new = malloc(efidp_node_size(dn) + sizeof (end_entire));
		if (!new)
			return -1;

		memcpy(new, dn, dn->length);
		memcpy((uint8_t *)new + dn->length, &end_entire,
		       sizeof (end_entire));
		*out = new;
		return 0;
	}

	lsz = efidp_size(dp);
	if (lsz < 0)
		return -1;

	rsz = efidp_node_size(dn);
	if (rsz < 0)
		return -1;

	const_efidp le;
	le = dp;
	while (1) {
		if (efidp_type(le) == EFIDP_END_TYPE &&
				efidp_subtype(le) == EFIDP_END_ENTIRE) {
			ssize_t lesz = efidp_size(le);
			if (lesz < 0)
				return -1;
			lsz -= lesz;
			break;
		}

		rc = efidp_get_next_end(le, &le);
		if (rc < 0)
			return -1;
	}

	efidp new = malloc(lsz + rsz + sizeof (end_entire));
	if (!new)
		return -1;

	*out = new;
	memcpy(new, dp, lsz);
	memcpy((uint8_t *)new + lsz, dn, rsz);
	memcpy((uint8_t *)new + lsz + rsz, &end_entire, sizeof (end_entire));

	return 0;
}

int
__attribute__((__visibility__ ("default")))
efidp_append_instance(const_efidp dp, const_efidp dpi, efidp *out)
{
	ssize_t lsz, rsz;
	int rc;

	if (!dp && !dpi) {
		errno = EINVAL;
		return -1;
	}

	if (!dp && dpi)
		return efidp_duplicate_path(dpi, out);

	lsz = efidp_size(dp);
	if (lsz < 0)
		return -1;

	rsz = efidp_node_size(dpi);
	if (rsz < 0)
		return -1;

	efidp le;
	le = (efidp)dp;
	while (1) {
		if (le->type == EFIDP_END_TYPE &&
				le->subtype == EFIDP_END_ENTIRE)
			break;

		rc = efidp_get_next_end(le, (const_efidp *)&le);
		if (rc < 0)
			return -1;
	}
	le->subtype = EFIDP_END_INSTANCE;

	efidp new = malloc(lsz + rsz + sizeof (end_entire));
	if (!new)
		return -1;

	*out = new;
	memcpy(new, dp, lsz);
	memcpy((uint8_t *)new + lsz, dpi, rsz);

	return 0;

}

ssize_t
__attribute__((__visibility__ ("default")))
efidp_format_device_path(char *buf, size_t size, const_efidp dp, ssize_t limit)
{
	ssize_t sz;
	ssize_t off = 0;
	int first = 1;

	while (1) {
		if (limit && (limit < 4 || efidp_node_size(dp) > limit))
			return off+1;

		if (first) {
			first = 0;
		} else {
			if (dp->type == EFIDP_END_TYPE) {
				if (dp->type == EFIDP_END_INSTANCE)
					off += format(buf, size, off, ",");
				else
					return off+1;
			} else {
				off += format(buf, size, off, "/");
			}
		}

		switch (dp->type) {
		case EFIDP_HARDWARE_TYPE:
			sz = format_hw_dn(buf, size, off, dp);
			if (sz < 0)
				return -1;
			off += sz;
			break;
		case EFIDP_ACPI_TYPE:
			sz = format_acpi_dn(buf, size, off, dp);
			if (sz < 0)
				return -1;
			off += sz;
			break;
		case EFIDP_MESSAGE_TYPE:
			sz = format_message_dn(buf, size, off, dp);
			if (sz < 0)
				return -1;
			off += sz;
			break;
		case EFIDP_MEDIA_TYPE:
			sz = format_media_dn(buf, size, off, dp);
			if (sz < 0)
				return -1;
			off += sz;
			break;
		case EFIDP_BIOS_BOOT_TYPE: {
			char *types[] = {"", "Floppy", "HD", "CDROM", "PCMCIA",
					 "USB", "Network", "" };

			if (dp->subtype != EFIDP_BIOS_BOOT) {
				off += format(buf, size, off, "BbsPath(%d,",
					      dp->subtype);
				off += format_hex(buf, size, off,
						  (uint8_t *)dp+4,
						  efidp_node_size(dp)-4);
				off += format(buf,size,off,")");
				break;
			}

			if (dp->bios_boot.device_type > 0 &&
					dp->bios_boot.device_type < 7) {
				off += format(buf, size, off,
					      "BBS(%s,%s,0x%"PRIx32")",
					      types[dp->bios_boot.device_type],
					      dp->bios_boot.description,
					      dp->bios_boot.status);
			} else {
				off += format(buf, size, off,
					      "BBS(%d,%s,0x%"PRIx32")",
					      dp->bios_boot.device_type,
					      dp->bios_boot.description,
					      dp->bios_boot.status);
			}
			break;
					   }
		case EFIDP_END_TYPE:
			if (dp->subtype == EFIDP_END_INSTANCE) {
				off += format(buf, size, off, ",");
				break;
			}
			return off;
		default:
			off += format(buf, size, off, "Path(%d,%d,", dp->type,
				      dp->subtype);
			off += format_hex(buf, size, off,
					  (uint8_t *)dp + 4,
					  efidp_node_size(dp) - 4);
			off += format(buf, size, off, ")");
			break;
		}

		if (limit)
			limit -= efidp_node_size(dp);

		int rc = efidp_next_node(dp, &dp);
		if (rc < 0)
			return rc;
	}
	return off+1;
}

ssize_t
__attribute__((__visibility__ ("default")))
efidp_parse_device_node(char *path, efidp out, size_t size)
{
	errno = -ENOSYS;
	return -1;
}

ssize_t
__attribute__((__visibility__ ("default")))
efidp_parse_device_path(char *path, efidp out, size_t size)
{
	errno = -ENOSYS;
	return -1;
}

ssize_t
__attribute__((__visibility__ ("default")))
efidp_make_vendor(uint8_t *buf, ssize_t size, uint8_t type, uint8_t subtype,
		  efi_guid_t vendor_guid, void *data, size_t data_size)
{
	efidp_hw_vendor *vend = (efidp_hw_vendor *)buf;
	ssize_t sz;
	ssize_t req = sizeof (*vend) + data_size;
	sz = efidp_make_generic(buf, size, type, subtype, req);
	if (size && sz == req) {
		vend->vendor_guid = vendor_guid;
		memcpy(vend->vendor_data, data, data_size);
	}

	return sz;
}

ssize_t
__attribute__((__visibility__ ("default")))
efidp_make_generic(uint8_t *buf, ssize_t size, uint8_t type, uint8_t subtype,
		   ssize_t total_size)
{
	efidp_header *head = (efidp_header *)buf;

	if (!size)
		return total_size;
	if (size < total_size) {
		errno = ENOSPC;
		return -1;
	}

	head->type = type;
	head->subtype = subtype;
	head->length = total_size;
	return head->length;
}
