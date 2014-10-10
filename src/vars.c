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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "lib.h"
#include "generics.h"
#include "util.h"

#define VARS_PATH "/sys/firmware/efi/vars/"

typedef struct efi_variable_t {
	uint16_t	VariableName[1024/sizeof(uint16_t)];
	efi_guid_t	VendorGuid;
	uint64_t	DataSize;
	uint8_t		Data[1024];
	efi_status_t	Status;
	uint32_t	Attributes;
} __attribute__((packed)) efi_variable_t;

static int
get_size_from_file(const char *filename, size_t *retsize)
{
	uint8_t *buf = NULL;
	size_t bufsize = -1;
	int errno_value;
	int ret = -1;
	int fd = open(filename, O_RDONLY);
	if (fd < 0)
		goto err;

	int rc = read_file(fd, &buf, &bufsize);
	if (rc < 0)
		goto err;

	long long size = strtoll((char *)buf, NULL, 0);
	if ((size == LLONG_MIN || size == LLONG_MAX) && errno == ERANGE) {
		*retsize = -1;
	} else if (size < 0) {
		*retsize = -1;
	} else {
		*retsize = (size_t)size;
		ret = 0;
	}
err:
	errno_value = errno;

	if (fd >= 0)
		close(fd);

	if (buf != NULL)
		free(buf);

	errno = errno_value;
	return ret;
}


static int
vars_probe(void)
{
	if (!access(VARS_PATH "new_var", F_OK))
		return 1;
	return 0;
}

static int
vars_get_variable_size(efi_guid_t guid, const char *name, size_t *size)
{
	int errno_value;
	int ret = -1;

	char *path = NULL;
	int rc = asprintf(&path, VARS_PATH "%s-"GUID_FORMAT"/size",
			  name, guid.a, guid.b, guid.c, bswap_16(guid.d),
			  guid.e[0], guid.e[1], guid.e[2], guid.e[3],
			  guid.e[4], guid.e[5]);
	if (rc < 0)
		goto err;

	size_t retsize = 0;
	rc = get_size_from_file(path, &retsize);
	if (rc >= 0) {
		ret = 0;
		*size = retsize;
	}
err:
	errno_value = errno;

	if (path)
		free(path);

	errno = errno_value;
	return ret;
}

static int
vars_get_variable_attributes(efi_guid_t guid, const char *name,
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
vars_get_variable(efi_guid_t guid, const char *name, uint8_t **data,
		  size_t *data_size, uint32_t *attributes)
{
	int errno_value;
	int ret = -1;
	uint8_t *buf = NULL;
	size_t bufsize = -1;
	char *path;
	int rc = asprintf(&path, VARS_PATH "%s-" GUID_FORMAT "/raw_var",
			  name, guid.a, guid.b, guid.c, bswap_16(guid.d),
			  guid.e[0], guid.e[1], guid.e[2],
			  guid.e[3], guid.e[4], guid.e[5]);
	if (rc < 0)
		return -1;

	int fd = open(path, O_RDONLY);
	if (fd < 0)
		goto err;

	rc = read_file(fd, &buf, &bufsize);
	if (rc < 0)
		goto err;

	efi_variable_t *var = (void *)buf;

	*data = malloc(var->DataSize);
	if (!*data)
		goto err;
	memcpy(*data, var->Data, var->DataSize);
	*data_size = var->DataSize;
	*attributes = var->Attributes;

	ret = 0;
err:
	errno_value = errno;

	if (buf)
		free(buf);

	if (fd >= 0)
		close(fd);

	if (path)
		free(path);

	errno = errno_value;
	return ret;
}

static int
vars_del_variable(efi_guid_t guid, const char *name)
{
	int errno_value;
	int ret = -1;
	char *path;
	int rc = asprintf(&path, VARS_PATH "%s-" GUID_FORMAT "/raw_var",
			  name, guid.a, guid.b, guid.c, bswap_16(guid.d),
			  guid.e[0], guid.e[1], guid.e[2],
			  guid.e[3], guid.e[4], guid.e[5]);
	if (rc < 0)
		return -1;

	uint8_t *buf = NULL;
	size_t buf_size = 0;

	int fd = open(path, O_RDONLY);
	if (fd < 0)
		goto err;

	rc = read_file(fd, &buf, &buf_size);
	if (rc < 0 || buf_size != sizeof(efi_variable_t))
		goto err;

	close(fd);
	fd = open(VARS_PATH "del_var", O_WRONLY);
	if (fd < 0)
		goto err;

	rc = write(fd, buf, buf_size);
	if (rc >= 0)
		ret = 0;
err:
	errno_value = errno;

	if (buf)
		free(buf);

	if (fd >= 0)
		close(fd);

	if (path)
		free(path);

	errno = errno_value;
	return ret;
}

static int
_vars_chmod_variable(char *path, mode_t mode)
{
	mode_t mask = umask(umask(0));
	size_t len = strlen(path);
	char c = path[len - 5];
	path[len - 5] = '\0';

	char *files[] = {
		"", "attributes", "data", "guid", "raw_var", "size", NULL
		};

	int saved_errno = 0;
	int ret = 0;
	for (int i = 0; files[i] != NULL; i++) {
		char *new_path = NULL;
		int rc = asprintf(&new_path, "%s/%s", path, files[i]);
		if (rc > 0) {
			rc = chmod(new_path, mode & ~mask);
			if (rc < 0) {
				if (saved_errno == 0)
					saved_errno = errno;
				ret = -1;
			}
			free(new_path);
		} else if (ret < 0) {
			if (saved_errno == 0)
				saved_errno = errno;
			ret = -1;
		}
	}
	path[len - 5] = c;
	errno = saved_errno;
	return ret;
}

static int
vars_chmod_variable(efi_guid_t guid, const char *name, mode_t mode)
{
	if (strlen(name) > 1024) {
		errno = EINVAL;
		return -1;
	}

	char *path;
	int rc = asprintf(&path, VARS_PATH "%s-" GUID_FORMAT,
			  name, guid.a, guid.b, guid.c, bswap_16(guid.d),
			  guid.e[0], guid.e[1], guid.e[2], guid.e[3],
			  guid.e[4], guid.e[5]);
	if (rc < 0)
		return -1;

	rc = _vars_chmod_variable(path, mode);
	int saved_errno = errno;
	free(path);
	errno = saved_errno;
	return rc;
}

static int
vars_set_variable(efi_guid_t guid, const char *name, uint8_t *data,
		 size_t data_size, uint32_t attributes, mode_t mode)
{
	int errno_value;
	size_t len;
	int ret = -1;

	if (strlen(name) > 1024) {
		errno = EINVAL;
		return -1;
	}
	if (data_size > 1024) {
		errno = ENOSPC;
		return -1;
	}

	char *path;
	int rc = asprintf(&path, VARS_PATH "%s-" GUID_FORMAT "/data",
			  name, guid.a, guid.b, guid.c, bswap_16(guid.d),
			  guid.e[0], guid.e[1], guid.e[2], guid.e[3],
			  guid.e[4], guid.e[5]);
	if (rc < 0)
		return -1;

	len = rc;
	int fd = -1;

	if (!access(path, F_OK)) {
		rc = efi_del_variable(guid, name);
		if (rc < 0)
			goto err;
	}

	efi_variable_t var = {
		.VendorGuid = guid,
		.DataSize = data_size,
		.Status = 0,
		.Attributes = attributes
		};
	for (int i = 0; name[i] != '\0'; i++)
		var.VariableName[i] = name[i];
	memcpy(var.Data, data, data_size);

	fd = open(VARS_PATH "new_var", O_WRONLY);
	if (fd < 0)
		goto err;
	rc = write(fd, &var, sizeof(var));
	if (rc >= 0)
		ret = 0;

	/* this is inherently racy, but there's no way to do it correctly with
	 * this kernel API.  Fortunately, all directory contents get created
	 * with root.root ownership and an effective umask of 177 */
	path[len-5] = '\0';
	_vars_chmod_variable(path, mode);
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
vars_get_next_variable_name(efi_guid_t **guid, char **name)
{
	return generic_get_next_variable_name(VARS_PATH, guid, name);
}

struct efi_var_operations vars_ops = {
	.probe = vars_probe,
	.set_variable = vars_set_variable,
	.del_variable = vars_del_variable,
	.get_variable = vars_get_variable,
	.get_variable_attributes = vars_get_variable_attributes,
	.get_variable_size = vars_get_variable_size,
	.get_next_variable_name = vars_get_next_variable_name,
	.chmod_variable = vars_chmod_variable,
};
