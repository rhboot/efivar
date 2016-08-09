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

#ifndef EFIVARFS_MAGIC
#  define EFIVARFS_MAGIC 0xde5e81e4
#endif

static char const default_efivarfs_path[] = "/sys/firmware/efi/efivars/";

static char const *
get_efivarfs_path(void)
{
	static const char *path;
	if (path)
		return path;

	path = getenv("EFIVARFS_PATH");
	if (!path)
		path = default_efivarfs_path;
	return path;
}

static int
efivarfs_probe(void)
{
	const char *path = get_efivarfs_path();

	if (!access(path, F_OK)) {
		int rc = 0;
		struct statfs buf;

		memset(&buf, '\0', sizeof (buf));
		rc = statfs(path, &buf);
		if (rc == 0) {
			char *tmp;
			typeof(buf.f_type) magic = EFIVARFS_MAGIC;
			if (!memcmp(&buf.f_type, &magic, sizeof (magic)))
				return 1;
			else
				efi_error("bad fs type for %s", path);

			tmp = getenv("EFIVARFS_PATH");
			if (tmp && !strcmp(tmp, path)) {
				efi_error_clear();
				return 1;
			}
		} else {
			efi_error("statfs(%s) failed", path);
		}
	} else {
		efi_error("access(%s, F_OK) failed", path);
	}

	return 0;
}

#define make_efivarfs_path(str, guid, name) ({				\
		asprintf(str, "%s%s-" GUID_FORMAT, get_efivarfs_path(),	\
			name, (guid).a, (guid).b, (guid).c,		\
			bswap_16((guid).d),				\
			(guid).e[0], (guid).e[1], (guid).e[2],		\
			(guid).e[3], (guid).e[4], (guid).e[5]);		\
	})

static int
efivarfs_set_fd_immutable(int fd, int immutable)
{
	unsigned int flags;
	int rc = 0;

	rc = ioctl(fd, FS_IOC_GETFLAGS, &flags);
	if (rc < 0) {
		if (errno == ENOTTY)
			rc = 0;
		else
			efi_error("ioctl(%d, FS_IOC_GETFLAGS) failed", fd);
	} else if ((immutable && !(flags & FS_IMMUTABLE_FL)) ||
		   (!immutable && (flags & FS_IMMUTABLE_FL))) {
		if (immutable)
			flags |= FS_IMMUTABLE_FL;
		else
			flags &= ~FS_IMMUTABLE_FL;

		rc = ioctl(fd, FS_IOC_SETFLAGS, &flags);
		if (rc < 0)
			efi_error("ioctl(%d, FS_IOC_SETFLAGS) failed", fd);
	}

	return rc;
}

static int
efivarfs_set_immutable(char *path, int immutable)
{
	typeof(errno) error = 0;
	int fd;
	int rc = 0;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		if (errno == ENOTTY) {
			efi_error("open(%s, O_RDONLY) failed", path);
			return 0;
		} else {
			return fd;
		}
	}

	rc = efivarfs_set_fd_immutable(fd, immutable);
	error = errno;
	close(fd);
	errno = error;
	if (rc < 0)
		efi_error("efivarfs_set_fd_immutable(%d, %d) on %s failed",
			  fd, immutable, path);
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
	if (rc < 0) {
		efi_error("make_efivarfs_path failed");
		goto err;
	}

	struct stat statbuf = { 0, };
	rc = stat(path, &statbuf);
	if (rc < 0) {
		efi_error("stat(%s) failed", path);
		goto err;
	}

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
	if (ret < 0) {
		efi_error("efi_get_variable failed");
		return ret;
	}

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
	if (rc < 0) {
		efi_error("make_efivarfs_path failed");
		return -1;
	}

	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		efi_error("open(%s)", path);
		goto err;
	}

	rc = read(fd, &ret_attributes, sizeof (ret_attributes));
	if (rc < 0) {
		efi_error("read failed");
		goto err;
	}

	rc = read_file(fd, &ret_data, &size);
	if (rc < 0) {
		efi_error("read_file failed");
		goto err;
	}

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
	if (rc < 0) {
		efi_error("make_efivarfs_path failed");
		return -1;
	}

	efivarfs_set_immutable(path, 0);
	rc = unlink(path);
	if (rc < 0)
		efi_error("unlink failed");

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
		efi_error("name too long (%zd of 1024)", strlen(name));
		errno = EINVAL;
		return -1;
	}

	char *path;
	int rc = make_efivarfs_path(&path, guid, name);
	if (rc < 0) {
		efi_error("make_efivarfs_path failed");
		return -1;
	}

	int fd = -1;

	if (!access(path, F_OK) && !(attributes & EFI_VARIABLE_APPEND_WRITE)) {
		rc = efi_del_variable(guid, name);
		if (rc < 0) {
			efi_error("efi_del_variable failed");
			goto err;
		}
	}

	fd = open(path, O_WRONLY|O_CREAT, mode);
	if (fd < 0) {
		efi_error("open(%s, O_WRONLY|O_CREAT, mode) failed", path);
		goto err;
	}
	efivarfs_set_fd_immutable(fd, 0);

	memcpy(buf, &attributes, sizeof (attributes));
	memcpy(buf + sizeof (attributes), data, data_size);
#if 0
		errno = ENOSPC;
		rc = -1;
#else
		rc = write(fd, buf, sizeof (attributes) + data_size);
#endif
	if (rc >= 0) {
		ret = 0;
		efivarfs_set_fd_immutable(fd, 1);
	} else {
		efi_error("write failed");
		efivarfs_set_fd_immutable(fd, 0);
		unlink(path);
	}
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
	int rc;
	attributes |= EFI_VARIABLE_APPEND_WRITE;
	rc = efivarfs_set_variable(guid, name, data, data_size, attributes, 0);
	if (rc < 0)
		efi_error("efivarfs_set_variable failed");
	return rc;
}

static int
efivarfs_get_next_variable_name(efi_guid_t **guid, char **name)
{
	int rc;
	rc = generic_get_next_variable_name(get_efivarfs_path(), guid, name);
	if (rc < 0)
		efi_error("generic_get_next_variable_name failed");
	return rc;
}

static int
efivarfs_chmod_variable(efi_guid_t guid, const char *name, mode_t mode)
{
	char *path;
	int rc = make_efivarfs_path(&path, guid, name);
	if (rc < 0) {
		efi_error("make_efivarfs_path failed");
		return -1;
	}

	rc = chmod(path, mode);
	int saved_errno = errno;
	if (rc < 0)
		efi_error("chmod(%s,0%o) failed", path, mode);
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
