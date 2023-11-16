// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012-2013 Red Hat, Inc.
 */

#include "fix_coverity.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

#include "efivar.h"

#define EFIVAR_MAGIC 0xf3df1597u

#define ATTRS_UNSET 0xa5a5a5a5a5a5a5a5
#define ATTRS_MASK 0xffffffff

/* The exported structure is:
 * struct {
 *	uint32_t magic;
 *	uint32_t version;
 *	uint64_t attr;
 *	efi_guid_t guid;
 *	uint32_t name_len;
 *	uint32_t data_len;
 *	uint16_t name[];
 *	uint8_t data[];
 *	uint32_t crc32;
 * }
 *
 * Unfortunately the exported structure from dmpstore is:
 * struct {
 *	uint32_t name_size; // in bytes
 *	uint32_t data_size; // in bytes
 *	uint16_t name[];
 *	efi_guid_t guid;
 *	uint32_t attr;
 *	unit8_t data[];
 *	uint32_t crc32;
 * }
 */

#ifdef EFIVAR_BUILD_ENVIRONMENT
#error wtf
#endif

ssize_t NONNULL(1, 3)
efi_variable_import_dmpstore(uint8_t *data, size_t size,
			     efi_variable_t **var_out)
{
	efi_variable_t var;
	uint32_t namesz;
	uint32_t datasz;
	size_t min = sizeof (uint32_t)		/* name size */
		   + sizeof (uint32_t)		/* data size */
		   + sizeof (uint16_t)		/* two bytes of name */
		   + sizeof (efi_guid_t)	/* guid */
		   + sizeof (uint32_t)		/* attr */
		   + 1				/* one byte of data */
		   + sizeof (uint32_t);		/* crc32 */
	size_t sz = sizeof (uint32_t)		/* name size */
		  + sizeof (uint32_t)		/* data size */
		  + sizeof (efi_guid_t)		/* guid */
		  + sizeof (uint32_t)		/* attr */
		  + sizeof (uint32_t);		/* crc32 */
	uint8_t *ptr = data;
	uint32_t crc;
	int saved_errno;

	if (size <= min) {
etoosmall:
		errno = EINVAL;
		efi_error("data size is too small for dmpstore variable (%zu < %zu)",
			  size, min);
		return -1;
	}

	memset(&var, 0, sizeof(var));

	namesz = *(uint32_t *)ptr;
	debug("namesz:%"PRIu32, namesz);
	ptr += sizeof(uint32_t);

	if (namesz <= 2) {
		errno = EINVAL;
		debug("name size (%"PRIu32") must be greater than 2", namesz);
		return -1;
	}

	if (namesz % 2 != 0) {
		errno = EINVAL;
		efi_error("name size (%"PRIu32") cannot be odd", namesz);
		return -1;
	}

	datasz = *(uint32_t *)ptr;
	ptr += sizeof(uint32_t);
	debug("datasz:%"PRIu32, datasz);

	if (datasz == 0) {
		errno = EINVAL;
		efi_error("data size (%"PRIu32") must be nonzero", datasz);
		return -1;
	}

	if (ADD(sz, namesz, &sz)) {
overflow:
		errno = EOVERFLOW;
		efi_error("arithmetic overflow computing allocation size");
		return -1;
	}

	if (ADD(sz, datasz, &min))
		goto overflow;

	if (size < min)
		goto etoosmall;
	size = min;

	if (!(ptr[namesz - 1] == 0 && ptr[namesz -2] == 0)) {
		errno = EINVAL;
		efi_error("variable name is not properly terminated.");
		return -1;
	}

	crc = efi_crc32(data, size - sizeof(uint32_t));
	debug("efi_crc32(%p, %zu) -> 0x%"PRIx32", expected 0x%"PRIx32,
	      data, size - sizeof(uint32_t), crc,
	      *(uint32_t*)(data + size - sizeof(uint32_t)));

	if (memcmp(data + size - sizeof(uint32_t),
		    &crc, sizeof(uint32_t))) {
		errno = EINVAL;
		efi_error("crc32 did not match");
		return -1;
	}

	var.name = ucs2_to_utf8(ptr, -1);
	if (!var.name)
		goto oom;
	ptr += namesz;

	var.guid = malloc(sizeof (efi_guid_t));
	if (!var.guid)
		goto oom;
	memcpy(var.guid, ptr, sizeof (efi_guid_t));
	ptr += sizeof (efi_guid_t);

	var.attrs = *(uint32_t *)ptr;
	ptr += sizeof(uint32_t);

	var.data_size = datasz;
	var.data = malloc(datasz);
	if (!var.data) {
		efi_error("Could not allocate %"PRIu32" bytes", datasz);
		goto oom;
	}
	memcpy(var.data, ptr, datasz);

	if (!*var_out) {
		*var_out =malloc(sizeof (var));
		if (!*var_out)
			goto oom;
		memcpy(*var_out, &var, sizeof (var));
	} else {
		return -1;
	}
	return size;
oom:
	saved_errno = errno;

	if (var.guid)
		free(var.guid);

	if (var.name)
		free(var.name);

	if (var.data)
		free(var.data);

	errno = saved_errno;
	efi_error("Could not allocate memory");
	return -1;
}

ssize_t NONNULL(1, 3)
efi_variable_import_efivar(uint8_t *data, size_t datasz, efi_variable_t **var_out)
{
	efi_variable_t var;
	size_t min = sizeof (uint32_t) * 2	/* magic */
		   + sizeof (uint32_t)		/* version */
		   + sizeof (uint64_t)		/* attr */
		   + sizeof (efi_guid_t)	/* guid */
		   + sizeof (uint32_t) * 2	/* name_len and data_len */
		   + sizeof (uint16_t)		/* two bytes of name */
		   + 1				/* one byte of data */
		   + 4;				/* crc32 */
	uint32_t crc;
	uint8_t *ptr = data;
	uint32_t magic = EFIVAR_MAGIC;
	int test;

	errno = EINVAL;
	if (datasz <= min)
		return -1;

	test = memcmp(data, &magic, sizeof (uint32_t));
	debug("test magic 0: cmp(0x%04x,0x%04x)->%d", *(uint32_t *)data, magic, test);
	if (test) {
		errno = EINVAL;
		efi_error("MAGIC for file format did not match.");
		return -1;
	}

	ptr += sizeof (uint32_t);

	debug("test version");
	if (*(uint32_t *)ptr == 1) {
		ptr += sizeof (uint32_t);
		debug("version 1");

		var.attrs = *(uint64_t *)ptr;
		ptr += sizeof (uint64_t);
		debug("var.attrs:0x%08"PRIx64, var.attrs);

		var.guid = malloc(sizeof (efi_guid_t));
		if (!var.guid)
			return -1;
		*var.guid = *(efi_guid_t *)ptr;
		ptr += sizeof (efi_guid_t);
		debug("var.guid:"GUID_FORMAT, GUID_FORMAT_ARGS(var.guid));

		uint32_t name_len = *(uint32_t *)ptr;
		ptr += sizeof (uint32_t);
		debug("name_len:%"PRIu32, name_len);

		uint32_t data_len = *(uint32_t *)ptr;
		ptr += sizeof (uint32_t);
		debug("data_len:%"PRIu32, data_len);

		min -= 3;
		min += name_len;
		min += data_len;

		if (name_len < 2 ||
		    name_len > (datasz - data_len) ||
		    data_len < 1 ||
		    data_len > (datasz - name_len)) {
			int saved_errno = errno;
			free(var.guid);
			errno = saved_errno;
			return -1;
		}

		crc = efi_crc32(data, datasz - sizeof(uint32_t));
		debug("efi_crc32(%p, %zu) -> 0x%"PRIx32", expected 0x%"PRIx32,
		      data, datasz - sizeof(uint32_t), crc,
		      *(uint32_t*)(data + datasz - sizeof(uint32_t)));

		if (memcmp(data + datasz - sizeof (uint32_t), &crc,
			   sizeof (uint32_t))) {
			free(var.guid);
			errno = EINVAL;
			efi_error("crc32 did not match");
			return -1;
		}

		var.name = calloc(1, name_len + 1);
		if (!var.name) {
			int saved_errno = errno;
			free(var.guid);
			errno = saved_errno;
			return -1;
		}

		uint16_t *wname = (uint16_t *)ptr;
		for (uint32_t i = 0; i < name_len; i++)
			var.name[i] = wname[i] & 0xff;
		ptr += name_len;
		debug("name:%s", var.name);

		var.data_size = data_len;
		var.data = malloc(data_len);
		if (!var.data) {
			int saved_errno = errno;
			free(var.guid);
			free(var.name);
			errno = saved_errno;
			return -1;
		}
		memcpy(var.data, ptr, data_len);

		if (!*var_out) {
			*var_out =malloc(sizeof (var));
			if (!*var_out) {
				int saved_errno = errno;
				free(var.guid);
				free(var.name);
				free(var.data);
				errno = saved_errno;
				return -1;
			}
		}
		memcpy(*var_out, &var, sizeof (var));
	} else {
		return -1;
	}
	return min;
}

ssize_t NONNULL(1, 3) PUBLIC
efi_variable_import(uint8_t *data, size_t size, efi_variable_t **var_out)
{
	ssize_t rc;

	rc = efi_variable_import_efivar(data, size, var_out);
	if (rc >= 0)
		return rc;

	rc = efi_variable_import_dmpstore(data, size, var_out);
	return rc;
}

ssize_t NONNULL(1) PUBLIC
efi_variable_export_dmpstore(efi_variable_t *var, uint8_t *data, size_t datasz)
{
	uint32_t tmpu32;
	ssize_t tmpssz;
	uint32_t namesz;
	uint32_t needed = sizeof (uint32_t)		/* name_size */
			+ sizeof (uint32_t)		/* data_size */
			+ 2				/* name */
			+ sizeof (efi_guid_t)		/* guid */
			+ sizeof (uint32_t)		/* attrs */
			+ 1				/* data */
			+ 4;				/* crc32 */
	uint8_t *ptr;
	uint32_t crc;

	if (!var) {
		errno = EINVAL;
		efi_error("var cannot be NULL");
		return -1;
	}
	if (!var->name) {
		errno = EINVAL;
		efi_error("var->name cannot be NULL");
		return -1;
	}
	if (!var->data) {
		errno = EINVAL;
		efi_error("var->data cannot be NULL");
		return -1;
	}

	debug("data: %p datasz: %zu", data, datasz);

	namesz = utf8size(var->name, -1);
	debug("sizeof(uint16_t):%zd * namesz:%"PRIu32, sizeof(uint16_t), namesz);
	if (MUL(sizeof (uint16_t), namesz, &namesz)) {
overflow:
		errno = EOVERFLOW;
		efi_error("arithmetic overflow computing name size");
		return -1;
	}
	debug("namesz -> %"PRIu32, namesz);

	/*
	 * Remove our stand-ins for name size and data size before we add
	 * them back in.
	 */
	needed -= 3;

	debug("needed:%"PRIu32" + namesz:%"PRIu32, needed, namesz);
	if (ADD(needed, namesz, &needed))
		goto overflow;
	debug("needed -> %"PRIu32, needed);

	debug("needed:%"PRIu32" + var->data_size:%zd", needed, var->data_size);
	if (ADD(needed, var->data_size, &needed))
		goto overflow;
	debug("needed -> %"PRIu32, needed);

	if (!data || datasz == 0) {
		debug("data: %p datasz: %zd -> returning needed size %"PRIu32,
		      data, datasz, needed);
		return needed;
	}

	debug("datasz:%zu needed: %"PRIu32, datasz, needed);
	if (datasz < needed) {
		efi_error("needed: %"PRIu32" datasz: %zd -> returning needed datasz %zu",
			  needed, datasz, needed - datasz);
		return needed - datasz;
	}

	ptr = data;

	tmpssz = utf8_to_ucs2(ptr + 8, datasz - 8, true, var->name);
	if (tmpssz < 0) {
		efi_error("UTF-8 to UCS-2 conversion failed");
		return -1;
	}
	tmpu32 = tmpssz;
	tmpu32 *= sizeof(uint16_t);

	debug("namesz:%"PRIu32" - tmpu32:%"PRIu32, namesz, tmpu32);
	if (SUB(namesz, tmpu32, &tmpu32))
		goto overflow;
	debug("tmpu32 -> %"PRIu32, tmpu32);

	debug("namesz:%"PRIu32" - tmpu32:%"PRIu32, namesz, tmpu32);
	if (SUB(namesz, tmpu32, &namesz))
		goto overflow;
	debug("namesz -> %"PRIu32, namesz);

	debug("needed:%"PRIu32" - tmpu32:%"PRIu32, needed, tmpu32);
	if (SUB(needed, tmpu32, &needed))
		goto overflow;
	debug("needed -> %"PRIu32, needed);

	debug("datasz:%zu needed: %"PRIu32, datasz, needed);
	if (datasz < needed) {
		debug("needed: %"PRIu32" datasz: %zd -> returning needed datasz %"PRIu32,
			  needed, datasz, needed);
		return needed;
	}

	*(uint32_t *)ptr = namesz;
	ptr += sizeof (uint32_t);

	*(uint32_t *)ptr = var->data_size;
	ptr += sizeof (uint32_t);

	ptr += namesz;

	memcpy(ptr, var->guid, sizeof (efi_guid_t));
	ptr += sizeof(efi_guid_t);

	*(uint32_t *)ptr = var->attrs;
	ptr += sizeof (uint32_t);

	memcpy(ptr, var->data, var->data_size);
	ptr += var->data_size;

	crc = efi_crc32(data, needed - sizeof(uint32_t));
	debug("efi_crc32(%p, %zu) -> 0x%"PRIx32,
	      data, needed - sizeof(uint32_t), crc);
	*(uint32_t *)ptr = crc;

	return needed;
}

ssize_t NONNULL(1) PUBLIC
efi_variable_export(efi_variable_t *var, uint8_t *data, size_t datasz)
{
	uint32_t tmpu32;
	ssize_t tmpssz;
	uint32_t namesz;
	uint32_t needed = sizeof (uint32_t)		/* magic */
			+ sizeof (uint32_t)		/* version */
			+ sizeof (uint64_t)		/* attr */
			+ sizeof (efi_guid_t)		/* guid */
			+ sizeof (uint32_t)		/* name_len */
			+ sizeof (uint32_t)		/* data_len */
			+ 2				/* name */
			+ 1				/* data */
			+ 4;				/* crc32 */
	uint8_t *ptr;
	uint32_t crc;

	if (!var) {
		errno = EINVAL;
		efi_error("var cannot be NULL");
		return -1;
	}
	if (!var->name) {
		errno = EINVAL;
		efi_error("var->name cannot be NULL");
		return -1;
	}
	if (!var->data) {
		errno = EINVAL;
		efi_error("var->data cannot be NULL");
		return -1;
	}

	debug("data: %p datasz: %zu", data, datasz);

	namesz = utf8size(var->name, -1);
	debug("sizeof(uint16_t):%zd * namesz:%"PRIu32, sizeof(uint16_t), namesz);
	if (MUL(sizeof (uint16_t), namesz, &namesz)) {
overflow:
		errno = EOVERFLOW;
		efi_error("arithmetic overflow computing name size");
		return -1;
	}
	debug("namesz -> %"PRIu32, namesz);

	/*
	 * Remove our stand-ins for name size and data size before we add
	 * them back in.
	 */
	needed -= 3;

	debug("needed:%"PRIu32" + namesz:%"PRIu32, needed, namesz);
	if (ADD(needed, namesz, &needed))
		goto overflow;
	debug("needed -> %"PRIu32, needed);

	debug("needed:%"PRIu32" + var->data_size:%zd", needed, var->data_size);
	if (ADD(needed, var->data_size, &needed))
		goto overflow;
	debug("needed -> %"PRIu32, needed);

	if (!data || datasz == 0) {
		debug("data: %p datasz: %zd -> returning needed datasz %"PRIu32,
		      data, datasz, needed);
		return needed;
	}

	debug("datasz:%zu needed: %"PRIu32, datasz, needed);
	if (datasz < needed) {
		efi_error("needed: %"PRIu32" datasz: %zd -> returning needed datasz %zd",
			  needed, datasz, needed - datasz);
		return needed - datasz;
	}

	ptr = data;

	*(uint32_t *)ptr = EFIVAR_MAGIC;
	ptr += sizeof (uint32_t);

	*(uint32_t *)ptr = 1;
	ptr += sizeof (uint32_t);

	*(uint64_t *)ptr = var->attrs;
	ptr += sizeof (uint64_t);

	memcpy(ptr, var->guid, sizeof (efi_guid_t));
	ptr += sizeof (efi_guid_t);

	tmpssz = utf8_to_ucs2(ptr + 8, datasz - 8, true, var->name);
	if (tmpssz < 0) {
		efi_error("UTF-8 to UCS-2 conversion failed");
		return -1;
	}
	tmpu32 = tmpssz;
	tmpu32 *= sizeof(uint16_t);

	debug("namesz:%"PRIu32" - tmpu32:%"PRIu32, namesz, tmpu32);
	if (SUB(namesz, tmpu32, &tmpu32))
		goto overflow;
	debug("tmpu32 -> %"PRIu32, tmpu32);

	debug("needed:%"PRIu32" - tmpu32:%"PRIu32, needed, tmpu32);
	if (SUB(needed, tmpu32, &needed))
		goto overflow;
	debug("needed -> %"PRIu32, needed);

	debug("namesz:%"PRIu32" - tmpu32:%"PRIu32, namesz, tmpu32);
	if (SUB(namesz, tmpu32, &namesz))
		goto overflow;
	debug("namesz -> %"PRIu32, namesz);

	debug("datasz:%zu needed: %"PRIu32, datasz, needed);
	if (datasz < needed) {
		efi_error("needed: %"PRIu32" datasz: %zd -> returning needed datasz %zd",
			  needed, datasz, needed - datasz);
		return needed - datasz;
	}

	*(uint32_t *)ptr = namesz;
	ptr += sizeof (uint32_t);

	*(uint32_t *)ptr = var->data_size;
	ptr += sizeof (uint32_t);

	ptr += namesz;

	memcpy(ptr, var->data, var->data_size);
	ptr += var->data_size;

	crc = efi_crc32(data, needed - sizeof(uint32_t));
	debug("efi_crc32(%p, %zu) -> 0x%"PRIx32,
	      data, needed - sizeof(uint32_t), crc);
	*(uint32_t *)ptr = crc;

	return needed;
}

efi_variable_t PUBLIC *
efi_variable_alloc(void)
{
	efi_variable_t *var = calloc(1, sizeof (efi_variable_t));
	if (!var)
		return NULL;

	var->attrs = ATTRS_UNSET;
	return var;
}

void PUBLIC
efi_variable_free(efi_variable_t *var, int free_data)
{
	if (!var)
		return;

	if (free_data) {
		if (var->guid)
			free(var->guid);

		if (var->name)
			free(var->name);

		if (var->data && var->data_size)
			free(var->data);
	}

	memset(var, '\0', sizeof (*var));
	free(var);
}

int NONNULL(1, 2) PUBLIC
efi_variable_set_name(efi_variable_t *var, unsigned char *name)
{
	var->name = name;
	return 0;
}

unsigned char PUBLIC NONNULL(1) *
efi_variable_get_name(efi_variable_t *var)
{
	if (!var->name) {
		errno = ENOENT;
	} else {
		errno = 0;
	}
	return var->name;
}

int NONNULL(1, 2) PUBLIC
efi_variable_set_guid(efi_variable_t *var, efi_guid_t *guid)
{
	var->guid = guid;
	return 0;
}

int NONNULL(1, 2) PUBLIC
efi_variable_get_guid(efi_variable_t *var, efi_guid_t **guid)
{
	if (!var->guid) {
		errno = ENOENT;
		return -1;
	}

	*guid = var->guid;
	return 0;
}

int NONNULL(1, 2) PUBLIC
efi_variable_set_data(efi_variable_t *var, uint8_t *data, size_t size)
{
	if (!size) {
		errno = EINVAL;
		return -1;
	}

	var->data = data;
	var->data_size = size;
	return 0;
}

ssize_t NONNULL(1, 2, 3) PUBLIC
efi_variable_get_data(efi_variable_t *var, uint8_t **data, size_t *size)
{
	if (!var->data || !var->data_size) {
		errno = ENOENT;
		return -1;
	}

	*data = var->data;
	*size = var->data_size;
	return 0;
}

int NONNULL(1) PUBLIC
efi_variable_set_attributes(efi_variable_t *var, uint64_t attrs)
{
	var->attrs = attrs;
	return 0;
}

int NONNULL(1, 2) PUBLIC
efi_variable_get_attributes(efi_variable_t *var, uint64_t *attrs)
{
	if (var->attrs == ATTRS_UNSET) {
		errno = ENOENT;
		return -1;
	}

	*attrs = var->attrs;
	return 0;
}

int NONNULL(1) PUBLIC
efi_variable_realize(efi_variable_t *var)
{
	if (!var->name || !var->data || !var->data_size ||
			var->attrs == ATTRS_UNSET) {
		errno = -EINVAL;
		return -1;
	}
	if (var->attrs & EFI_VARIABLE_HAS_AUTH_HEADER &&
			!(var->attrs & EFI_VARIABLE_HAS_SIGNATURE)) {
		errno = -EPERM;
		return -1;
	}
	uint32_t attrs = var->attrs & ATTRS_MASK;
	if (attrs & EFI_VARIABLE_APPEND_WRITE) {
		return efi_append_variable(*var->guid, (char *)var->name,
					var->data, var->data_size, attrs);
	}
	return efi_set_variable(*var->guid, (char *)var->name, var->data,
				var->data_size, attrs, 0600);
}

// vim:fenc=utf-8:tw=75:noet
