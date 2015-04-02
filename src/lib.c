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
		.name = "default",
		.probe = default_probe,
	};

struct efi_var_operations *ops = NULL;

int
__attribute__((__nonnull__ (2, 3)))
__attribute__((__visibility__ ("default")))
_efi_set_variable(efi_guid_t guid, const char *name, uint8_t *data,
		 size_t data_size, uint32_t attributes, mode_t mode)
{
	return ops->set_variable(guid, name, data, data_size, attributes, mode);
}

int
__attribute__((__nonnull__ (2, 3)))
__attribute__((__visibility__ ("default")))
_efi_set_variable_variadic(efi_guid_t guid, const char *name, uint8_t *data,
                 size_t data_size, uint32_t attributes, ...)
{
	va_list ap;
	va_start(ap, attributes);
	mode_t mode = va_arg(ap, mode_t);
	va_end(ap);
	return ops->set_variable(guid, name, data, data_size, attributes, mode);
}
extern typeof(_efi_set_variable_variadic) efi_set_variable
	__attribute__ ((alias ("_efi_set_variable_variadic")));

int
__attribute__((__nonnull__ (2, 3)))
__attribute__((__visibility__ ("default")))
efi_append_variable(efi_guid_t guid, const char *name, uint8_t *data,
			size_t data_size, uint32_t attributes)
{
	if (!ops->append_variable)
		return generic_append_variable(guid, name, data, data_size,
						attributes);
	return ops->append_variable(guid, name, data, data_size, attributes);
}

int
__attribute__((__nonnull__ (2)))
__attribute__((__visibility__ ("default")))
efi_del_variable(efi_guid_t guid, const char *name)
{
	if (!ops->del_variable) {
		errno = ENOSYS;
		return -1;
	}
	return ops->del_variable(guid, name);
}

int
__attribute__((__nonnull__ (2, 3, 4, 5)))
__attribute__((__visibility__ ("default")))
efi_get_variable(efi_guid_t guid, const char *name, uint8_t **data,
		  size_t *data_size, uint32_t *attributes)
{
	if (!ops->get_variable) {
		errno = ENOSYS;
		return -1;
	}
	return ops->get_variable(guid, name, data, data_size, attributes);
}

int
__attribute__((__nonnull__ (2, 3)))
__attribute__((__visibility__ ("default")))
efi_get_variable_attributes(efi_guid_t guid, const char *name,
			    uint32_t *attributes)
{
	if (!ops->get_variable_attributes) {
		errno = ENOSYS;
		return -1;
	}
	return ops->get_variable_attributes(guid, name, attributes);
}

int
__attribute__((__nonnull__ (2, 3)))
__attribute__((__visibility__ ("default")))
efi_get_variable_size(efi_guid_t guid, const char *name, size_t *size)
{
	if (!ops->get_variable_size) {
		errno = ENOSYS;
		return -1;
	}
	return ops->get_variable_size(guid, name, size);
}

int
__attribute__((__nonnull__ (1, 2)))
__attribute__((__visibility__ ("default")))
efi_get_next_variable_name(efi_guid_t **guid, char **name)
{
	if (!ops->get_next_variable_name) {
		errno = ENOSYS;
		return -1;
	}
	return ops->get_next_variable_name(guid, name);
}

int
__attribute__((__nonnull__ (2)))
__attribute__((__visibility__ ("default")))
efi_chmod_variable(efi_guid_t guid, const char *name, mode_t mode)
{
	if (!ops->chmod_variable) {
		errno = ENOSYS;
		return -1;
	}
	return ops->chmod_variable(guid, name, mode);
}

int
__attribute__((__visibility__ ("default")))
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
	char *ops_name = getenv("LIBEFIVAR_OPS");
	for (int i = 0; ops_list[i] != NULL; i++)
	{
		if (ops_name != NULL) {
			if (!strcmp(ops_list[i]->name, ops_name) ||
					!strcmp(ops_list[i]->name, "default")) {
				ops = ops_list[i];
				break;
			}
		} else if (ops_list[i]->probe()) {
			ops = ops_list[i];
			break;
		}
	}
}
