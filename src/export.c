/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012-2013 Red Hat, Inc.
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
#include <stdint.h>
#include <stdlib.h>
#include <uchar.h>

#include "efivar.h"

#define EFIVAR_MAGIC 0xf3df1597

#define ATTRS_UNSET 0xa5a5a5a5a5a5a5a5
#define ATTRS_MASK 0xffffffff

struct efi_variable {
	uint64_t attrs;
	efi_guid_t *guid;
	char *name;
	uint8_t *data;
	size_t data_size;
};

/* The exported structure is:
 * struct {
 *	uint32_t magic;
 *	uint32_t version;
 *	uint64_t attr;
 *	efi_guid_t guid;
 *	uint32_t name_len;
 *	uint32_t data_len;
 *	char16_t name[];
 *	uint8_t data[];
 *	uint32_t magic;
 * }
 */

ssize_t NONNULL(1, 3) PUBLIC
efi_variable_import(uint8_t *data, size_t size, efi_variable_t **var_out)
{
	efi_variable_t var;
	size_t min = sizeof (uint32_t) * 2	/* magic */
		   + sizeof (uint32_t)		/* version */
		   + sizeof (uint64_t)		/* attr */
		   + sizeof (efi_guid_t)	/* guid */
		   + sizeof (uint32_t) * 2	/* name_len and data_len */
		   + sizeof (char16_t)	/* two bytes of name */
		   + 1;				/* one byte of data */
	errno = EINVAL;
	if (size <= min)
		return -1;

	uint8_t *ptr = data;
	uint32_t magic = EFIVAR_MAGIC;
	if (memcmp(data, &magic, sizeof (uint32_t)) ||
			memcmp(data + size - sizeof (uint32_t), &magic,
				sizeof (uint32_t)))
		return -1;
	size -= sizeof (uint32_t);
	ptr += sizeof (uint32_t);

	if (*(uint32_t *)ptr == 1) {
		ptr += sizeof (uint32_t);
		var.attrs = *(uint64_t *)ptr;
		ptr += sizeof (uint32_t);

		var.guid = malloc(sizeof (efi_guid_t));
		if (!var.guid)
			return -1;
		*var.guid = *(efi_guid_t *)ptr;
		ptr += sizeof (efi_guid_t);

		uint32_t name_len = *(uint32_t *)ptr;
		ptr += sizeof (uint32_t);
		uint32_t data_len = *(uint32_t *)ptr;
		ptr += sizeof (uint32_t);

		if (name_len < 1 ||
		    name_len != ((data + size) - ptr - data_len) ||
		    data_len < 1 ||
		    data_len != ((data + size) - ptr - name_len)) {
			int saved_errno = errno;
			free(var.guid);
			errno = saved_errno;
			return -1;
		}

		var.name = calloc(1, name_len + 1);
		if (!var.name) {
			int saved_errno = errno;
			free(var.guid);
			errno = saved_errno;
			return -1;
		}

		char16_t *wname = (char16_t *)ptr;
		for (uint32_t i = 0; i < name_len; i++)
			var.name[i] = wname[i] & 0xff;
		ptr += name_len * 2;

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
	return size;
}

ssize_t NONNULL(1) PUBLIC
efi_variable_export(efi_variable_t *var, uint8_t *data, size_t size)
{
	size_t name_len = strlen(var->name);

	size_t needed = sizeof (uint32_t)		/* magic */
		      + sizeof (uint32_t)		/* version */
		      + sizeof (uint64_t)		/* attr */
		      + sizeof (efi_guid_t)		/* guid */
		      + sizeof (uint32_t)		/* name_len */
		      + sizeof (uint32_t)		/* data_len */
		      + sizeof (char16_t) * name_len	/* name */
		      + var->data_size			/* data */
		      + sizeof (uint32_t);		/* magic again */

	if (!data || size == 0) {
		return needed;
	} else if (size < needed) {
		return needed - size;
	}

	uint8_t *ptr = data;

	*(uint32_t *)ptr = EFIVAR_MAGIC;
	ptr += sizeof (uint32_t);

	*(uint32_t *)ptr = 1;
	ptr += sizeof (uint32_t);

	*(uint64_t *)ptr = var->attrs;
	ptr += sizeof (uint64_t);

	memcpy(ptr, var->guid, sizeof (efi_guid_t));
	ptr += sizeof (efi_guid_t);

	*(uint32_t *)ptr = (uint32_t)(sizeof (char16_t) * name_len);
	ptr += sizeof (uint32_t);

	*(uint32_t *)ptr = var->data_size;
	ptr += sizeof (uint32_t);

	for (uint32_t i = 0; i < name_len; i++) {
		*(char16_t *)ptr = var->name[i];
		ptr += sizeof (char16_t);
	}

	memcpy(ptr, var->data, var->data_size);
	ptr += var->data_size;

	*(uint32_t *)ptr = EFIVAR_MAGIC;

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
efi_variable_set_name(efi_variable_t *var, char *name)
{
	var->name = name;
	return 0;
}

char PUBLIC NONNULL(1) *
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
	if (var->data || !var->data_size) {
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
		return efi_append_variable(*var->guid, var->name,
					var->data, var->data_size, attrs);
	}
	return efi_set_variable(*var->guid, var->name, var->data,
				var->data_size, attrs, 0600);
}
