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
#include <linux/magic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <unistd.h>

#include "lib.h"
#include "generics.h"

#define EFIVARS_PATH "/sys/firmware/efi/efivars/"

#ifndef EFIVARFS_MAGIC
#  define EFIVARFS_MAGIC 0xde5e81e4
#endif

typedef struct efi_variable_t {
	uint32_t	Attributes;
	uint8_t		Data[];
} __attribute__((packed)) efi_variable_t;

static int
efivarfs_probe(void)
{
	if (!access(EFIVARS_PATH, F_OK)) {
		int rc = 0;
		struct statfs buf;

		memset(&buf, '\0', sizeof (buf));
		rc = statfs(EFIVARS_PATH, &buf);
		if (rc == 0) {
			typeof(buf.f_type) magic = EFIVARFS_MAGIC;
			if (!memcmp(&buf.f_type, &magic, sizeof (magic)))
				return 1;
		}
	}
	
	return 0;
}

#define make_efivarfs_path(str, guid, name) ({				\
		asprintf(str, EFIVARS_PATH "%s-" GUID_FORMAT,		\
			name, (guid).a, (guid).b, (guid).c,		\
			bswap_16((guid).d),				\
			(guid).e[0], (guid).e[1], (guid).e[2],		\
			(guid).e[3], (guid).e[4], (guid).e[5]);		\
	})

static int
efivarfs_get_variable_size(efi_guid_t guid, const char *name, size_t *size)
{
	char *path = NULL;
	int rc = 0;
	int ret = -1;
	typeof(errno) errno_value;
	
	rc = make_efivarfs_path(&path, guid, name);
	if (rc < 0)
		goto err;
	
	struct stat statbuf;
	rc = stat(path, &statbuf);
	if (rc < 0)
		goto err;

	ret = 0;
	/* Compensate for the size of the Attributes field. */
	*size = statbuf.st_size - sizeof (uint32_t);
err:
	errno_value = errno;

	if (path)
		free(path);

	errno = errno_value;
	return ret;
}

static int
efivarfs_get_variable_attributes(efi_guid_t guid, const char *name,
			    uint32_t *attributes)
{
	int ret = -1;

	uint8_t *data;
	size_t data_size;
	uint32_t attribs;

	ret = efi_get_variable(guid, name, &data, &data_size, &attribs);
	if (ret < 0)
		return ret;

	*attributes = attribs;
	if (data)
		free(data);
	return ret;
}

static int
efivarfs_get_variable(efi_guid_t guid, const char *name, uint8_t **data,
		  size_t *data_size, uint32_t *attributes)
{
	typeof(errno) errno_value;
	int ret = -1;
	struct stat statbuf;
	off_t size = 0;
	uint32_t ret_attributes = 0;
	uint8_t *ret_data;

	char *path;
	int rc = make_efivarfs_path(&path, guid, name);
	if (rc < 0)
		return -1;

	int fd = open(path, O_RDONLY);
	if (fd < 0)
		goto err;

	rc = stat(path, &statbuf);
	if (rc < 0)
		goto err;

	size = statbuf.st_size - sizeof (*attributes);
	ret_data = malloc(size);
	if (!ret_data)
		goto err;

	rc = read(fd, &ret_attributes, sizeof (ret_attributes));
	if (rc < 0) {
read_err:
		errno_value = errno;
		free(ret_data);
		errno = errno_value;
		goto err;
	}

	rc = read(fd, ret_data, size);
	if (rc < 0)
		goto read_err;

	*attributes = ret_attributes;
	*data = ret_data;
	*data_size = size;

	ret = 0;
err:
	errno_value = errno;

	if (fd >= 0)
		close(fd);

	if (path)
		free(path);

	errno = errno_value;
	return ret;
}

static int
efivarfs_del_variable(efi_guid_t guid, const char *name)
{
	char *path;
	int rc = make_efivarfs_path(&path, guid, name);
	if (rc < 0)
		return -1;

	rc = unlink(path);

	typeof(errno) errno_value = errno;
	free(path);
	errno = errno_value;

	return rc;
}

static int
efivarfs_set_variable(efi_guid_t guid, const char *name, uint8_t *data,
		 size_t data_size, uint32_t attributes)
{
	uint8_t buf[sizeof (attributes) + data_size];
	typeof(errno) errno_value;
	int ret = -1;

	if (strlen(name) > 1024) {
		errno = EINVAL;
		return -1;
	}

	char *path;
	int rc = make_efivarfs_path(&path, guid, name);
	if (rc < 0)
		return -1;

	int fd = -1;

	if (!access(path, F_OK) && !(attributes | EFI_VARIABLE_APPEND_WRITE)) {
		rc = efi_del_variable(guid, name);
		if (rc < 0)
			goto err;
	}

	fd = open(path, O_WRONLY|O_CREAT, 0600);
	if (fd < 0)
		goto err;
	
	rc = ftruncate(fd, data_size + sizeof (attributes));
	if (rc < 0)
		goto err;

#if 1
	memcpy(buf, &attributes, sizeof (attributes));
	memcpy(buf + sizeof (attributes), data, data_size);
	rc = write(fd, buf, sizeof (attributes) + data_size);
#else
	struct iovec iovs[] = {
		{ &attributes, sizeof (attributes) },
		{ data, data_size }
	};

	rc = writev(fd, &iovs[0], 2);
#endif
	if (rc >= 0)
		ret = 0;
err:
	errno_value = errno;

	if (path)
		free(path);

	if (fd >= 0)
		close(fd);

	errno = errno_value;
	return ret;
}

static int
efivarfs_append_variable(efi_guid_t guid, const char *name, uint8_t *data,
	size_t data_size, uint32_t attributes)
{
	attributes |= EFI_VARIABLE_APPEND_WRITE;
	return efivarfs_set_variable(guid, name, data, data_size, attributes);
}

static int
efivarfs_get_next_variable_name(efi_guid_t **guid, char **name)
{
	return generic_get_next_variable_name(EFIVARS_PATH, guid, name);
}

struct efi_var_operations efivarfs_ops = {
	.probe = efivarfs_probe,
	.set_variable = efivarfs_set_variable,
	.append_variable = efivarfs_append_variable,
	.del_variable = efivarfs_del_variable,
	.get_variable = efivarfs_get_variable,
	.get_variable_attributes = efivarfs_get_variable_attributes,
	.get_variable_size = efivarfs_get_variable_size,
	.get_next_variable_name = efivarfs_get_next_variable_name,
};


