/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012-2013 Red Hat, Inc.
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
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "efivar.h"
#include "lib.h"
#include "generics.h"

static int default_probe(void)
{
	return 1;
}

struct efi_var_operations default_ops = {
		.probe = default_probe,
	};

struct efi_var_operations *ops = NULL;

int
efi_set_variable(efi_guid_t guid, const char *name, uint8_t *data,
		 size_t data_size, uint32_t attributes)
{
	if (!ops->set_variable)
		return -ENOSYS;
	return ops->set_variable(guid, name, data, data_size,
						attributes);
}

int
efi_append_variable(efi_guid_t guid, const char *name, uint8_t *data,
			size_t data_size, uint32_t attributes)
{
	if (!ops->append_variable)
		return generic_append_variable(guid, name, data, data_size,
						attributes);
	return ops->append_variable(guid, name, data, data_size, attributes);
}

int
efi_del_variable(efi_guid_t guid, const char *name)
{
	if (!ops->del_variable)
		return -ENOSYS;
	return ops->del_variable(guid, name);
}

int
efi_get_variable(efi_guid_t guid, const char *name, uint8_t **data,
		  size_t *data_size, uint32_t *attributes)
{
	if (!ops->get_variable)
		return -ENOSYS;
	return ops->get_variable(guid, name, data, data_size, attributes);
}

int
efi_get_variable_attributes(efi_guid_t guid, const char *name,
			    uint32_t *attributes)
{
	if (!ops->get_variable_attributes)
		return -ENOSYS;
	return ops->get_variable_attributes(guid, name, attributes);
}

int
efi_get_variable_size(efi_guid_t guid, const char *name, size_t *size)
{
	if (!ops->get_variable_size)
		return -ENOSYS;
	return ops->get_variable_size(guid, name, size);
}

int
efi_get_next_variable_name(efi_guid_t **guid, char **name)
{
	if (!ops->get_next_variable_name)
		return -ENOSYS;
	return ops->get_next_variable_name(guid, name);
}

int
efi_variables_supported(void)
{
	if (ops == &default_ops)
		return 0;
	
	return 1;
}

static void libefivar_init(void) __attribute__((constructor));

static void
libefivar_init(void)
{
	struct efi_var_operations *ops_list[] = {
		&efivarfs_ops,
		&vars_ops,
		&default_ops,
		NULL
	};
	for (int i = 0; ops_list[i] != NULL; i++)
	{
		if (ops_list[i]->probe()) {
			ops = ops_list[i];
			break;
		}
	}
}
