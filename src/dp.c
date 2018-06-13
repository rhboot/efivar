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

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#include "efivar.h"

static const efidp_header end_entire = {
	.type = EFIDP_END_TYPE,
	.subtype = EFIDP_END_ENTIRE,
	.length = 4
};

static inline void *
efidp_data_address(const_efidp dp)
{
	if (dp->length <= 4) {
		errno = ENOSPC;
		efi_error("DP was smaller than DP header");
		return NULL;
	}
	return (void *)((uint8_t *)dp + sizeof (dp));
}

int PUBLIC
efidp_set_node_data(const_efidp dn, void *buf, size_t bufsize)
{
	if (dn->length < 4 || bufsize > (size_t)dn->length - 4) {
		errno = ENOSPC;
		efi_error("DP was smaller than DP header");
		return -1;
	}
	void *data = efidp_data_address(dn);
	if (!data) {
		efi_error("efidp_data_address failed");
		return -1;
	}
	memcpy(data, buf, bufsize);
	return 0;
}

static inline int
efidp_duplicate_extra(const_efidp dp, efidp *out, size_t extra)
{
	ssize_t sz;
	ssize_t plus;

	efidp new;

	sz = efidp_size(dp);
	if (sz < 0) {
		efi_error("efidp_size(dp) returned error");
		return sz;
	}

	if (add(sz, extra, &plus)) {
		errno = EOVERFLOW;
		efi_error("arithmetic overflow computing allocation size");
		return -1;
	}

	if (plus < (ssize_t)sizeof(efidp_header)) {
		errno = EINVAL;
		efi_error("allocation for new device path is smaller than device path header.");
		return -1;
	}

	new = calloc(1, plus);
	if (!new) {
		efi_error("allocation failed");
		return -1;
	}

	memcpy(new, dp, sz);
	*out = new;
	return 0;
}

int PUBLIC
efidp_duplicate_path(const_efidp  dp, efidp *out)
{
	int rc;
	rc = efidp_duplicate_extra(dp, out, 0);
	if (rc < 0)
		efi_error("efi_duplicate_extra(dp, out, 0) returned error");
	return rc;
}

int PUBLIC
efidp_append_path(const_efidp dp0, const_efidp dp1, efidp *out)
{
	ssize_t lsz, rsz, newsz = 0;
	const_efidp le;
	int rc;

	if (!dp0 && !dp1) {
		rc = efidp_duplicate_path((const_efidp)&end_entire, out);
		if (rc < 0)
			efi_error("efidp_duplicate_path failed");
		return rc;
	}

	if (dp0 && !dp1) {
		rc = efidp_duplicate_path(dp0, out);
		if (rc < 0)
			efi_error("efidp_duplicate_path failed");
		return rc;
	}

	if (!dp0 && dp1) {
		rc = efidp_duplicate_path(dp1, out);
		if (rc < 0)
			efi_error("efidp_duplicate_path failed");
		return rc;
	}

	lsz = efidp_size(dp0);
	if (lsz < 0) {
		efi_error("efidp_size(dp0) returned error");
		return -1;
	}

	rsz = efidp_size(dp1);
	if (rsz < 0) {
		efi_error("efidp_size(dp1) returned error");
		return -1;
	}

	le = dp0;
	while (1) {
		if (efidp_type(le) == EFIDP_END_TYPE &&
				efidp_subtype(le) == EFIDP_END_ENTIRE) {
			ssize_t lesz = efidp_size(le);
			lsz -= lesz;
			break;
		}

		rc = efidp_get_next_end(le, &le);
		if (rc < 0) {
			efi_error("efidp_get_next_end() returned error");
			return -1;
		}
	}

	efidp new;
	if (add(lsz, rsz, &newsz)) {
		errno = EOVERFLOW;
		efi_error("arithmetic overflow computing allocation size");
		return -1;
	}

	if (newsz < (ssize_t)sizeof(efidp_header)) {
		errno = EINVAL;
		efi_error("allocation for new device path is smaller than device path header.");
		return -1;
	}

	new = malloc(newsz);
	if (!new) {
		efi_error("allocation failed");
		return -1;
	}
	*out = new;

	memcpy(new, dp0, lsz);
	memcpy((uint8_t *)new + lsz, dp1, rsz);

	return 0;
}

int PUBLIC
efidp_append_node(const_efidp dp, const_efidp dn, efidp *out)
{
	ssize_t lsz = 0, rsz = 0, newsz;
	int rc;

	if (dp) {
		lsz = efidp_size(dp);
		if (lsz < 0) {
			efi_error("efidp_size(dp) returned error");
			return -1;
		}

		const_efidp le;
		le = dp;
		while (1) {
			if (efidp_type(le) == EFIDP_END_TYPE &&
			    efidp_subtype(le) == EFIDP_END_ENTIRE) {
				ssize_t lesz = efidp_size(le);
				lsz -= lesz;
				break;
			}

			rc = efidp_get_next_end(le, &le);
			if (rc < 0) {
				efi_error("efidp_get_next_end() returned error");
				return -1;
			}
		}
	}

	if (dn) {
		rsz = efidp_node_size(dn);
		if (rsz < 0) {
			efi_error("efidp_size(dn) returned error");
			return -1;
		}
	}

	if (add(lsz, rsz, &newsz) || add(newsz, sizeof(end_entire), &newsz)) {
		errno = EOVERFLOW;
		efi_error("arithmetic overflow computing allocation size");
		return -1;
	}

	efidp new = malloc(newsz);
	if (!new) {
		efi_error("allocation failed");
		return -1;
	}

	*out = new;
	if (dp)
		memcpy(new, dp, lsz);
	if (dn)
		memcpy((uint8_t *)new + lsz, dn, rsz);
	memcpy((uint8_t *)new + lsz + rsz, &end_entire, sizeof (end_entire));

	return 0;
}

int PUBLIC
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

ssize_t PUBLIC
efidp_format_device_path(char *buf, size_t size, const_efidp dp, ssize_t limit)
{
	ssize_t off = 0;
	int first = 1;

	if (!dp)
		return -1;

	if (buf && size)
		memset(buf, 0, size);

	while (limit) {
		if (limit >= 0 && (limit < 4 || efidp_node_size(dp) > limit)) {
			if (off)
				return off;
			else
				return -1;
		}

		if (first) {
			first = 0;
		} else {
			if (dp->type == EFIDP_END_TYPE) {
				if (dp->type == EFIDP_END_INSTANCE) {
					format(buf, size, off, "\b", ",");
				} else {
					return off+1;
				}
			} else {
				format(buf, size, off, "\b", "/");
			}
		}

		switch (dp->type) {
		case EFIDP_HARDWARE_TYPE:
			format_hw_dn(buf, size, off, dp);
			break;
		case EFIDP_ACPI_TYPE:
			format_acpi_dn(buf, size, off, dp);
			break;
		case EFIDP_MESSAGE_TYPE:
			format_message_dn(buf, size, off, dp);
			break;
		case EFIDP_MEDIA_TYPE:
			format_media_dn(buf, size, off, dp);
			break;
		case EFIDP_BIOS_BOOT_TYPE: {
			char *types[] = {"", "Floppy", "HD", "CDROM", "PCMCIA",
					 "USB", "Network", "" };

			if (dp->subtype != EFIDP_BIOS_BOOT) {
				format(buf, size, off, "BbsPath",
				       "BbsPath(%d,", dp->subtype);
				format_hex(buf, size, off, "BbsPath",
					   (uint8_t *)dp+4,
					   efidp_node_size(dp)-4);
				format(buf, size, off, "BbsPath", ")");
				break;
			}

			if (dp->bios_boot.device_type > 0 &&
					dp->bios_boot.device_type < 7) {
				format(buf, size, off, "BBS",
				       "BBS(%s,%s,0x%"PRIx32")",
				       types[dp->bios_boot.device_type],
				       dp->bios_boot.description,
				       dp->bios_boot.status);
			} else {
				format(buf, size, off, "BBS",
				       "BBS(%d,%s,0x%"PRIx32")",
				       dp->bios_boot.device_type,
				       dp->bios_boot.description,
				       dp->bios_boot.status);
			}
			break;
					   }
		case EFIDP_END_TYPE:
			if (dp->subtype == EFIDP_END_INSTANCE) {
				format(buf, size, off, "End", ",");
				break;
			}
			break;
		default:
			format(buf, size, off, "Path",
				    "Path(%d,%d,", dp->type, dp->subtype);
			format_hex(buf, size, off, "Path", (uint8_t *)dp + 4,
				   efidp_node_size(dp) - 4);
			format(buf, size, off, "Path", ")");
			break;
		}

		if (limit)
			limit -= efidp_node_size(dp);

		int rc = efidp_next_node(dp, &dp);
		if (rc < 0) {
			efi_error("could not format DP");
			return rc;
		}
	}
	return off+1;
}

ssize_t PUBLIC
efidp_parse_device_node(char *path UNUSED, efidp out UNUSED,
                        size_t size UNUSED)
{
	efi_error("not implented");
	errno = -ENOSYS;
	return -1;
}

ssize_t PUBLIC
efidp_parse_device_path(char *path UNUSED, efidp out UNUSED,
			size_t size UNUSED)
{
	efi_error("not implented");
	errno = -ENOSYS;
	return -1;
}

ssize_t PUBLIC
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

ssize_t PUBLIC
efidp_make_generic(uint8_t *buf, ssize_t size, uint8_t type, uint8_t subtype,
		   ssize_t total_size)
{
	efidp_header *head = (efidp_header *)buf;

	if (!size)
		return total_size;

	if (!buf) {
		errno = EINVAL;
		efi_error("%s was called with nonzero size and NULL buffer",
			  __func__);
		return -1;
	}

	if (size < total_size) {
		errno = ENOSPC;
		efi_error("total size is bigger than size limit");
		return -1;
	}

	head->type = type;
	head->subtype = subtype;
	head->length = total_size;
	return head->length;
}
