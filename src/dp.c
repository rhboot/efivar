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

#include <stdlib.h>
#include <stdio.h>

#include "efivar.h"
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

ssize_t
efidp_create_node(uint8_t type, uint8_t subtype, size_t len,
		  void *buf, size_t bufsize)
{
	efidp dp;

	if (!buf || bufsize == 0)
		return len;
	if (bufsize < len) {
		errno = ENOSPC;
		return -1;
	}

	dp = buf;
	dp->type = type;
	dp->subtype = subtype;
	dp->length = len;

	return len;
}

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
efidp_duplicate_path(const_efidp  dp, efidp *out)
{
	return efidp_duplicate_extra(dp, out, 0);
}

int
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
efidp_append_node(const_efidp dp, const_efidp dn, efidp *out)
{
	ssize_t lsz, rsz;
	int rc;

	if (!dp && !dn)
		return efidp_duplicate_path((const_efidp)(const efidp_header const *)&end_entire, out);

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
		if (le->type == EFIDP_END_TYPE &&
				le->subtype == EFIDP_END_ENTIRE) {
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
efidp_print_device_path(char *buf, size_t size, const_efidp dp)
{
	ssize_t sz;
	ssize_t ret = 0;

	switch (dp->type) {
	case EFIDP_HARDWARE_TYPE:
		sz = print_hw_dn(buf, size, dp);
		if (sz < 0)
			return -1;
		if (!peek_dn_type(dp, EFIDP_END_TYPE, EFIDP_END_ENTIRE))
			sz = pbufx(buf, size, sz, "/");
		break;
	case EFIDP_ACPI_TYPE:
		break;
	case EFIDP_MESSAGE_TYPE:
		break;
	case EFIDP_MEDIA_TYPE:
		break;
	case EFIDP_BIOS_BOOT_TYPE:
		break;
	case EFIDP_END_TYPE:
		break;
	default:
		sz = snprintf(buf ? buf+ret : buf, size ? size-ret : size,
			      "Path(%d,%d,", dp->type, dp->subtype);
		ret += sz;
		for (int i = 4; i < dp->length; i++) {
			sz = snprintf(buf ?buf+ret :buf, size ?size-ret :size,
				 "%02x", *((uint8_t *)dp + i));
			ret += sz;
		}
		sz = snprintf(buf ?buf+ret :buf, size ? size-ret :size, ")");
		ret += sz;
		break;
	}
	return ret;
}

#if 0
ssize_t
efidp_parse_device_node(char *path, efidp out, size_t size)
{

}

ssize_t
efidp_parse_device_path(char *path, efidp out, size_t size)
{

}
#endif
