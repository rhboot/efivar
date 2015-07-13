/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2015 Red Hat, Inc.
 * Copyright (C) 2001 Dell Computer Corporation <Matt_Domsch@dell.com>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <efivar.h>
#include "dp.h"

typedef struct efi_load_option_s {
	uint32_t attributes;
	uint16_t file_path_list_length;
	uint16_t description[];
	// uint8_t file_path_list[];
	// uint8_t optional_data[];
}
__attribute__((packed))
efi_load_option;

ssize_t
__attribute__((__nonnull__ (6)))
__attribute__((__visibility__ ("default")))
efi_loadopt_create(uint8_t *buf, ssize_t size, uint32_t attributes,
		   efidp dp, ssize_t dp_size, unsigned char *description,
		   uint8_t *optional_data, size_t optional_data_size)
{
	if (!description) {
		errno = EINVAL;
		return -1;
	}

	ssize_t desc_len = utf8len((uint8_t *)description, 1024) * 2 + 2;
	ssize_t sz = sizeof (attributes)
		     + sizeof (uint16_t) + desc_len
		     + dp_size + optional_data_size;
	if (size == 0)
		return sz;
	if (size < sz) {
		errno = ENOSPC;
		return -1;
	}

	if (!optional_data && optional_data_size != 0) {
		errno = EINVAL;
		return -1;
	}

	if (!dp && dp_size == 0) {
		errno = EINVAL;
		return -1;
	}

	uint8_t *pos = buf;

	*(uint32_t *)pos = attributes;
	pos += sizeof (attributes);

	*(uint16_t *)pos = dp_size;
	pos += sizeof (uint16_t);

	utf8_to_ucs2((uint16_t *)pos, desc_len, 1, (uint8_t *)description);
	pos += desc_len;

	memcpy(pos, dp, dp_size);
	pos += dp_size;

	if (optional_data && optional_data_size > 0)
		memcpy(pos, optional_data, optional_data_size);

	return sz;
}

ssize_t
__attribute__((__nonnull__ (1)))
__attribute__((__visibility__ ("default")))
efi_loadopt_optional_data_size(efi_load_option *opt, size_t size)
{
	size_t sz;
	uint8_t *p;

	if (!opt)
		return -1;

	if (size < sizeof(*opt))
		return -1;
	size -= sizeof(*opt);
	if (size < opt->file_path_list_length)
		return -1;
	sz = ucs2size(opt->description, size);
	if (sz >= size) // since there's no room for a file path...
		return -1;
	p = (uint8_t *)(opt->description) + sz;
	size -= sz;

	if (!efidp_is_valid((const_efidp)p, size))
		return -1;
	sz = efidp_size((const_efidp)p);
	p += sz;
	size -= sz;

	return size;
}

int
__attribute__((__nonnull__ (1)))
__attribute__((__visibility__ ("default")))
efi_loadopt_is_valid(efi_load_option *opt, size_t size)
{
	ssize_t rc;

	rc = efi_loadopt_optional_data_size(opt, size);
	return (rc >= 0);
}

uint32_t
__attribute__((__nonnull__ (1)))
__attribute__((__visibility__ ("default")))
efi_loadopt_attrs(efi_load_option *opt)
{
	return opt->attributes;
}

void
__attribute__((__nonnull__ (1)))
__attribute__((__visibility__ ("default")))
efi_loadopt_attr_set(efi_load_option *opt, uint16_t attr)
{
	opt->attributes |= attr;
}

void
__attribute__((__nonnull__ (1)))
__attribute__((__visibility__ ("default")))
efi_loadopt_attr_clear(efi_load_option *opt, uint16_t attr)
{
	opt->attributes &= ~attr;
}

uint16_t
__attribute__((__nonnull__ (1)))
__attribute__((__visibility__ ("default")))
efi_loadopt_pathlen(efi_load_option *opt)
{
	return opt->file_path_list_length;
}

efidp
__attribute__((__nonnull__ (1)))
__attribute__((__visibility__ ("default")))
efi_loadopt_path(efi_load_option *opt)
{
	char *p = (char *)opt;
	if (!opt) {
		errno = EINVAL;
		return NULL;
	}
	efidp dp = (efidp)(p + sizeof (opt->attributes)
		   + sizeof (opt->file_path_list_length)
		   + ucs2size(opt->description, -1));
	return dp;
}

int
__attribute__((__nonnull__ (1,3)))
__attribute__((__visibility__ ("default")))
efi_loadopt_optional_data(efi_load_option *opt, size_t opt_size,
			  unsigned char **datap, size_t *len)
{
	unsigned char *p = (unsigned char *)opt;
	if (!opt || !datap) {
		errno = EINVAL;
		return -1;
	}
	*datap = (unsigned char *)(p + sizeof (opt->attributes)
		   + sizeof (opt->file_path_list_length)
		   + ucs2size(opt->description, -1)
		   + opt->file_path_list_length);
	if (len && opt_size > 0)
		*len = (p+opt_size) - *datap;
	return 0;
}

ssize_t
__attribute__((__nonnull__ (3)))
__attribute__((__visibility__ ("default")))
efi_loadopt_args_from_file(uint8_t *buf, ssize_t size, char *filename)
{
	int rc;
	ssize_t ret = -1;
	struct stat statbuf = { 0, };
	int saved_errno;
	FILE *f;

	if (!filename || (!buf && size > 0)) {
		errno = -EINVAL;
		return -1;
	}

	f = fopen(filename, "r");
	if (!f)
		return -1;

	rc = fstat(fileno(f), &statbuf);
	if (rc < 0)
		goto err;

	if (size == 0)
		return statbuf.st_size;

	if (size < statbuf.st_size) {
		errno = ENOSPC;
		goto err;
	}

	ret = fread(buf, 1, statbuf.st_size, f);
	if (ret < statbuf.st_size)
		ret = -1;
err:
	saved_errno = errno;
	if (f)
		fclose(f);
	errno = saved_errno;
	return ret;
}

ssize_t
__attribute__((__nonnull__ (3)))
__attribute__((__visibility__ ("default")))
efi_loadopt_args_as_utf8(uint8_t *buf, ssize_t size, char *utf8)
{
	ssize_t req;
	if (!utf8 || (!buf && size > 0)) {
		errno = EINVAL;
		return -1;
	}

	/* we specifically want the storage size without NUL here,
	 * not the size with NUL or the number of utf8 characters */
	req = strlen(utf8);

	if (size == 0)
		return req;

	if (size < req) {
		errno = ENOSPC;
		return -1;
	}

	memcpy(buf, utf8, req);
	return req;
}

ssize_t
__attribute__((__nonnull__ (3)))
__attribute__((__visibility__ ("default")))
efi_loadopt_args_as_ucs2(uint16_t *buf, ssize_t size, uint8_t *utf8)
{
	ssize_t req;
	if (!utf8 || (!buf && size > 0)) {
		errno = EINVAL;
		return -1;
	}

	req = utf8len(utf8, -1) * sizeof(uint16_t);
	if (size == 0)
		return req;

	if (size < req) {
		errno = ENOSPC;
		return -1;
	}

	return utf8_to_ucs2(buf, size, 0, utf8);
}

static unsigned char *last_desc;

static void
__attribute__((destructor))
teardown(void)
{
	if (last_desc)
		free(last_desc);
	last_desc = NULL;
}

__attribute__((__nonnull__ (1)))
__attribute__((__visibility__ ("default")))
const unsigned char * const
efi_loadopt_desc(efi_load_option *opt)
{
	if (last_desc) {
		free(last_desc);
		last_desc = NULL;
	}

	last_desc = ucs2_to_utf8(opt->description, -1);
	return last_desc;
}
