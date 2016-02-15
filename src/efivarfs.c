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
#include "util.h"

#include <linux/fs.h>

#define EFIVARS_PATH "/sys/firmware/efi/efivars/"

#ifndef EFIVARFS_MAGIC
#  define EFIVARFS_MAGIC 0xde5e81e4
#endif

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
efivarfs_set_immutable(char *path, int immutable)
{
	unsigned int flags;
	typeof(errno) error = 0;
	int fd;
	int rc = 0;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		if (errno == ENOTTY)
			return 0;
		else
			return fd;
	}

	rc = ioctl(fd, FS_IOC_GETFLAGS, &flags);
	if (rc < 0) {
		if (errno == ENOTTY) {
			rc = 0;
		} else {
			error = errno;
		}
	} else if ((immutable && !(flags & FS_IMMUTABLE_FL)) ||
		   (!immutable && (flags & FS_IMMUTABLE_FL))) {
		if (immutable)
			flags |= FS_IMMUTABLE_FL;
		else
			flags &= ~FS_IMMUTABLE_FL;

		rc = ioctl(fd, FS_IOC_SETFLAGS, &flags);
		if (rc < 0)
			error = errno;
	}

	close(fd);
	errno = error;
	return rc;
}

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

	struct stat statbuf = { 0, };
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
	size_t size = 0;
	uint32_t ret_attributes = 0;
	uint8_t *ret_data;

	char *path;
	int rc = make_efivarfs_path(&path, guid, name);
	if (rc < 0)
		return -1;

	int fd = open(path, O_RDONLY);
	if (fd < 0)
		goto err;

	rc = read(fd, &ret_attributes, sizeof (ret_attributes));
	if (rc < 0)
		goto err;

	rc = read_file(fd, &ret_data, &size);
	if (rc < 0)
		goto err;

	*attributes = ret_attributes;
	*data = ret_data;
	*data_size = size - 1; // read_file pads out 1 extra byte to NUL it */

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

	rc = efivarfs_set_immutable(path, 0);
	if (rc >= 0)
		rc = unlink(path);

	typeof(errno) errno_value = errno;
	free(path);
	errno = errno_value;

	return rc;
}

static int
efivarfs_set_variable(efi_guid_t guid, const char *name, uint8_t *data,
		      size_t data_size, uint32_t attributes, mode_t mode)
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

	if (!access(path, F_OK) && !(attributes & EFI_VARIABLE_APPEND_WRITE)) {
		rc = efi_del_variable(guid, name);
		if (rc < 0)
			goto err;
	}

	fd = open(path, O_WRONLY|O_CREAT, mode);
	if (fd < 0)
		goto err;

	memcpy(buf, &attributes, sizeof (attributes));
	memcpy(buf + sizeof (attributes), data, data_size);
	rc = write(fd, buf, sizeof (attributes) + data_size);
	if (rc >= 0) {
		ret = 0;
	} else {
		unlink(path);
	}
	efivarfs_set_immutable(path, 1);
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
	return efivarfs_set_variable(guid, name, data, data_size, attributes, 0);
}

static int
efivarfs_get_next_variable_name(efi_guid_t **guid, char **name)
{
	return generic_get_next_variable_name(EFIVARS_PATH, guid, name);
}

static int
efivarfs_chmod_variable(efi_guid_t guid, const char *name, mode_t mode)
{
	char *path;
	int rc = make_efivarfs_path(&path, guid, name);
	if (rc < 0)
		return -1;

	rc = chmod(path, mode);
	int saved_errno = errno;
	free(path);
	errno = saved_errno;
	return -1;
}

struct efi_var_operations efivarfs_ops = {
	.name = "efivarfs",
	.probe = efivarfs_probe,
	.set_variable = efivarfs_set_variable,
	.append_variable = efivarfs_append_variable,
	.del_variable = efivarfs_del_variable,
	.get_variable = efivarfs_get_variable,
	.get_variable_attributes = efivarfs_get_variable_attributes,
	.get_variable_size = efivarfs_get_variable_size,
	.get_next_variable_name = efivarfs_get_next_variable_name,
	.chmod_variable = efivarfs_chmod_variable,
};
