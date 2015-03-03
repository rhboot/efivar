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

#include "efivar.h"
#include "dp.h"

typedef struct efi_load_option_s {
	uint32_t attributes;
	uint16_t file_path_list_length;
	uint16_t description[];
	// uint8_t file_path_list[];
	// uint8_t optional_data[];
} efi_load_option;

ssize_t
efi_make_load_option(uint8_t *buf, ssize_t size, uint32_t attributes,
		     efidp dp, char *description,
		     uint8_t *optional_data, size_t optional_data_size)
{
	if (!dp || !description ||
			(!optional_data && optional_data_size != 0)) {
		errno = EINVAL;
		return -1;
	}

	uint16_t dp_size = efidp_size(dp);
	ssize_t desc_len = utf8len((uint8_t *)description, 1024) + 1;
	ssize_t sz = sizeof (attributes)
		     + sizeof (uint16_t) + desc_len * 2
		     + dp_size + optional_data_size;
	if (size == 0)
		return sz;
	if (size < sz) {
		errno = ENOSPC;
		return -1;
	}

	uint16_t *desc = utf8_to_ucs2((uint8_t *)description, desc_len);
	desc = onstack(desc, desc_len * 2);

	uint8_t *pos = buf;

	*(uint32_t *)pos = attributes;
	pos += sizeof (attributes);

	*(uint16_t *)pos = dp_size;
	pos += sizeof (dp_size);

	memcpy(pos, desc, desc_len * 2);
	pos += desc_len * 2;

	memcpy(pos, dp, dp_size);
	pos += dp_size;

	if (optional_data && optional_data_size > 0)
		memcpy(pos, optional_data, optional_data_size);

	return sz;
}

efidp
efi_load_option_path(efi_load_option *opt)
{
	char *p = (char *)opt;
	efidp dp = (efidp)(p + sizeof (opt->attributes)
		   + sizeof (opt->file_path_list_length)
		   + ucs2len(opt->description, -1));
	return dp;
}
