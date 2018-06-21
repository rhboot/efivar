/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2015 Red Hat, Inc.
 * Copyright (C) 2001 Dell Computer Corporation <Matt_Domsch@dell.com>
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

#include <stddef.h>

#include "efiboot.h"
#include "include/efivar/efiboot-loadopt.h"
#include "util.h"
#include "hexdump.h"

typedef struct efi_load_option_s {
	uint32_t attributes;
	uint16_t file_path_list_length;
	uint16_t description[];
	// uint8_t file_path_list[];
	// uint8_t optional_data[];
} PACKED efi_load_option;

ssize_t NONNULL(6) PUBLIC
efi_loadopt_create(uint8_t *buf, ssize_t size, uint32_t attributes,
		   efidp dp, ssize_t dp_size, unsigned char *description,
		   uint8_t *optional_data, size_t optional_data_size)
{
	ssize_t desc_len = utf8len((uint8_t *)description, 1024) * 2 + 2;
	ssize_t sz = sizeof (attributes)
		     + sizeof (uint16_t) + desc_len
		     + dp_size + optional_data_size;

	debug("entry buf:%p size:%zd dp:%p dp_size:%zd",
	      buf, size, dp, dp_size);

	if (size == 0)
		return sz;

	if (size < sz) {
		errno = ENOSPC;
		return -1;
	}

	debug("testing buf");
	if (!buf) {
invalid:
		errno = EINVAL;
		return -1;
	}

	debug("testing optional data presence");
	if (!optional_data && optional_data_size != 0)
		goto invalid;

	debug("testing dp presence");
	if ((!dp && dp_size == 0) || dp_size < 0)
		goto invalid;

	if (dp) {
		debug("testing dp validity");
		if (!efidp_is_valid(dp, dp_size)) {
			if (efi_get_verbose() >= 1)
				hexdump((void *)dp, dp_size);
			goto invalid;
		}

		debug("testing dp size: dp_size:%zd efidp_size(dp):%zd",
		      dp_size, efidp_size(dp));
		if (efidp_size(dp) != dp_size) {
			if (efi_get_verbose() >= 1)
				hexdump((void *)dp, dp_size);
			goto invalid;
		}
	}

	if (buf) {
		uint8_t *pos = buf;
		*(uint32_t *)pos = attributes;
		pos += sizeof (attributes);

		*(uint16_t *)pos = dp_size;
		pos += sizeof (uint16_t);

		utf8_to_ucs2((uint16_t *)pos, desc_len, 1,
			     (uint8_t *)description);
		pos += desc_len;

		if (dp)
			memcpy(pos, dp, dp_size);
		pos += dp_size;

		if (optional_data && optional_data_size > 0)
			memcpy(pos, optional_data, optional_data_size);
	}

	return sz;
}

ssize_t NONNULL(1) PUBLIC
efi_loadopt_optional_data_size(efi_load_option *opt, size_t size)
{
	ssize_t sz;
	ssize_t ret;
	uint8_t *p;

	ret = size;
	if (ret < (ssize_t)sizeof(*opt)) {
		efi_error("load option size is too small for header (%zd/%zd)",
			  ret, sizeof(*opt));
		return -1;
	}
	ret -= sizeof(*opt);
	if (ret < opt->file_path_list_length) {
		efi_error("load option size is too small for path (%zd/%d)",
			  size, opt->file_path_list_length);
		return -1;
	}
	ret -= opt->file_path_list_length;
	if (ret < 0) {
		efi_error("leftover size is negative (%zd)", ret);
		return -1;
	}
	/* "size" as the limit means sz will be size or less in all cases; no
	 * need to test it.  if it /is/ size, there's no optional data. */
	sz = ucs2size(opt->description, ret);
	p = (uint8_t *)(opt->description) + sz;
	ret -= sz;
	if (ret < 0) {
		efi_error("leftover size is negative (%zd)", ret);
		return -1;
	}

	if (!efidp_is_valid((const_efidp)p, opt->file_path_list_length)) {
		efi_error("efi device path is not valid");
		return -1;
	}
	sz = efidp_size((const_efidp)p);
	if (sz != opt->file_path_list_length) {
		efi_error("size does not match file path size (%zd/%d)",
			  sz, opt->file_path_list_length);
		return -1;
	}

	return ret;
}

int NONNULL(1) PUBLIC
efi_loadopt_is_valid(efi_load_option *opt, size_t size)
{
	ssize_t rc;

	rc = efi_loadopt_optional_data_size(opt, size);
	return (rc >= 0);
}

uint32_t NONNULL(1) PUBLIC
efi_loadopt_attrs(efi_load_option *opt)
{
	return opt->attributes;
}

void NONNULL(1) PUBLIC
efi_loadopt_attr_set(efi_load_option *opt, uint16_t attr)
{
	opt->attributes |= attr;
}

void NONNULL(1) PUBLIC
efi_loadopt_attr_clear(efi_load_option *opt, uint16_t attr)
{
	opt->attributes &= ~attr;
}

uint16_t NONNULL(1) PUBLIC
/* limit here is the /whole/ load option */
efi_loadopt_pathlen(efi_load_option *opt, ssize_t limit)
{
	uint16_t len = opt->file_path_list_length;
	if (limit >= 0) {
		if (len > limit)
			return 0;
		if (limit - offsetof(efi_load_option, file_path_list_length) < len)
			return 0;
	}
	return len;
}

efidp NONNULL(1) PUBLIC
/* limit here is the /whole/ load option */
efi_loadopt_path(efi_load_option *opt, ssize_t limit)
{
	char *p = (char *)opt;
	size_t sz;
	size_t left;
	efidp dp;

	left = (size_t)limit;
	if (left <= offsetof(efi_load_option, description))
		return NULL;
	left -= offsetof(efi_load_option, description);
	p += offsetof(efi_load_option, description);

	sz = ucs2size(opt->description, left);
	if (sz >= left) // since there's no room for a file path...
		return NULL;
	p += sz;
	left -= sz;

	if (left < opt->file_path_list_length)
		return NULL;

	dp = (efidp)p;
	if (!efidp_is_valid(dp, opt->file_path_list_length))
		return NULL;
	return dp;
}

int NONNULL(1,3) PUBLIC
efi_loadopt_optional_data(efi_load_option *opt, size_t opt_size,
			  unsigned char **datap, size_t *len)
{
	unsigned char *p = (unsigned char *)opt;
	size_t l = sizeof (opt->attributes)
		    + sizeof (opt->file_path_list_length);
	size_t ul;

	if (l > opt_size) {
over_limit:
		*len = 0;
		errno = EINVAL;
		return -1;
	}

	ul = ucs2size(opt->description, opt_size - l);
	if (opt->file_path_list_length > opt_size)
		goto over_limit;
	if (ul > opt_size)
		goto over_limit;
	if (opt_size - ul < opt->file_path_list_length)
		goto over_limit;
	l += ul + opt->file_path_list_length;
	if (l > opt_size)
		goto over_limit;

	*datap = (unsigned char *)(p + l);
	if (len && opt_size > 0)
		*len = (p+opt_size) - *datap;
	return 0;
}

ssize_t NONNULL(3) PUBLIC
efi_loadopt_args_from_file(uint8_t *buf, ssize_t size, char *filename)
{
	int rc;
	ssize_t ret = -1;
	struct stat statbuf = { 0, };
	int saved_errno;
	FILE *f;

	if (!buf && size != 0) {
		errno = -EINVAL;
		return -1;
	}

	f = fopen(filename, "r");
	if (!f)
		return -1;

	rc = fstat(fileno(f), &statbuf);
	if (rc < 0)
		goto err;

	if (size == 0) {
		fclose(f);
		return statbuf.st_size;
	}

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

ssize_t NONNULL(3) PUBLIC
efi_loadopt_args_as_utf8(uint8_t *buf, ssize_t size, uint8_t *utf8)
{
	ssize_t req;
	if (!buf && size != 0) {
		errno = EINVAL;
		return -1;
	}

	/* we specifically want the storage size without NUL here,
	 * not the size with NUL or the number of utf8 characters */
	req = strlen((char *)utf8);

	if (size == 0)
		return req;

	if (size < req) {
		errno = ENOSPC;
		return -1;
	}

	memcpy(buf, utf8, req);
	return req;
}

ssize_t NONNULL(3) PUBLIC
efi_loadopt_args_as_ucs2(uint16_t *buf, ssize_t size, uint8_t *utf8)
{
	ssize_t req;
	if (!buf && size > 0) {
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

static void DESTRUCTOR
teardown(void)
{
	if (last_desc)
		free(last_desc);
	last_desc = NULL;
}

const unsigned char NONNULL(1) PUBLIC *
efi_loadopt_desc(efi_load_option *opt, ssize_t limit)
{
	if (last_desc) {
		free(last_desc);
		last_desc = NULL;
	}

	last_desc = ucs2_to_utf8(opt->description, limit);
	return last_desc;
}
