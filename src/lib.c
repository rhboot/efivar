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
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "efivar.h"
#include "lib.h"

static int default_probe(void)
{
	return 1;
}

struct efi_var_operations default_ops = {
		.probe = default_probe,
	};

struct efi_var_operations *ops = NULL;

int
read_fd(int fd, uint8_t **buf, size_t *bufsize)
{
	uint8_t *p;
	size_t size = 4096;
	int s = 0, filesize = 0;

	if (!(*buf = calloc(4096, sizeof (char))))
		return -1;

	do {
		p = *buf + filesize;
		s = read(fd, p, 4096 - s);
		if (s < 0)
			break;
		filesize += s;
		/* only exit for empty reads */
		if (s == 0)
			break;
		else if (s == 4096) {
			*buf = realloc(*buf, size + 4096);
			memset(*buf + size, '\0', 4096);
			size += s;
			s = 0;
		} else {
			size += s;
		}
	} while (1);

	*bufsize = filesize;
	return 0;
}

int
get_size_from_file(const char *filename, size_t *retsize)
{
	int errno_value;
	int ret = -1;
	int fd = open(filename, O_RDONLY);
	if (fd < 0)
		goto err;

	uint8_t *buf = NULL;
	size_t bufsize = -1;
	int rc = read_fd(fd, &buf, &bufsize);
	if (rc < 0)
		goto err;

	*retsize = strtoll((char *)buf, NULL, 0);
	if ((*retsize == LLONG_MIN || *retsize == LLONG_MAX) && errno == ERANGE)
		*retsize = -1;
	else
		ret = 0;
err:
	errno_value = errno;

	if (fd >= 0)
		close(fd);

	if (buf != NULL)
		free(buf);

	errno = errno_value;
	return ret;
}

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

int efi_variables_supported(void)
{
	if (ops == &default_ops)
		return 0;
	
	return 1;
}

static void libefivar_init(void) __attribute__((constructor));

static void libefivar_init(void)
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
