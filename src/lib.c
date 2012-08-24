/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012 Red Hat, Inc.
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

#include <efivar.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

typedef unsigned long efi_status_t;

typedef struct efi_variable_t {
	uint16_t	VariableName[1024/sizeof(uint16_t)];
	efi_guid_t	VendorGuid;
	unsigned long	DataSize;
	uint8_t		Data[1024];
	efi_status_t	Status;
	uint32_t	Attributes;
} __attribute__((packed)) efi_variable_t;

#define GUID_FORMAT "%08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x"

int efi_variables_supported(void)
{
	if (!access("/sys/firmware/efi/vars/new_var", F_OK))
		return 1;
	
	return 0;
}

static int
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

static int
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
efi_get_variable_size(efi_guid_t guid, const char *name, size_t *size)
{
	int errno_value;
	int ret = -1;

	char *path = NULL;
	int rc = asprintf(&path, "/sys/firmware/efi/vars/%s-"GUID_FORMAT"/size",
			  name, guid.a, guid.b, guid.c, guid.d, guid.e[0],
			  guid.e[1], guid.e[2], guid.e[3], guid.e[4],
			  guid.e[5]);
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

int
efi_get_variable_attributes(efi_guid_t guid, const char *name,
			    uint32_t *attributes)
{
	int errno_value;
	int ret = -1;

	char *path = NULL;
	int rc = asprintf(&path, "/sys/firmware/efi/vars/%s-"GUID_FORMAT
			  "/attributes",
			  name, guid.a, guid.b, guid.c, guid.d, guid.e[0],
			  guid.e[1], guid.e[2], guid.e[3], guid.e[4],
			  guid.e[5]);
	if (rc < 0)
		return -1;

	int fd = open(path, O_RDONLY);
	if (fd < 0)
		goto err;

	uint8_t *buf = NULL;
	size_t bufsize = 0;
	rc = read_fd(fd, &buf, &bufsize);
	if (rc < 0 || bufsize != sizeof(*attributes))
		goto err;

	memcpy(attributes, buf, sizeof(*attributes));
	
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

int
efi_get_variable(efi_guid_t guid, const char *name, uint8_t **data,
		  size_t *data_size, uint32_t *attributes)
{
	int errno_value;
	int ret = -1;

	char *path;
	int rc = asprintf(&path, "/sys/firmware/efi/vars/%s-"GUID_FORMAT"/data",
			  name, guid.a, guid.b, guid.c, guid.d, guid.e[0],
			  guid.e[1], guid.e[2], guid.e[3], guid.e[4],
			  guid.e[5]);
	if (rc < 0)
		return -1;

	int fd = open(path, O_RDONLY);
	if (fd <= 0)
		goto err;

	uint8_t *buf = NULL;
	size_t bufsize = -1;
	rc = read_fd(fd, &buf, &bufsize);
	if (rc < 0)
		goto err;

	*data = buf;
	*data_size = bufsize;

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

int
efi_del_variable(efi_guid_t guid, const char *name)
{
	int errno_value;
	int ret = -1;
	char *path;
	int rc = asprintf(&path, "/sys/firmware/efi/vars/%s-"GUID_FORMAT
			  "raw_var", name, guid.a, guid.b, guid.c, guid.d,
			  guid.e[0], guid.e[1], guid.e[2], guid.e[3], guid.e[4],
			  guid.e[5]);
	if (rc < 0)
		return -1;

	uint8_t *buf = NULL;
	size_t buf_size = 0;

	int fd = open(path, O_RDONLY);
	if (fd < 0)
		goto err;

	rc = read_fd(fd, &buf, &buf_size);
	if (rc < 0 || buf_size != sizeof(efi_variable_t))
		goto err;

	close(fd);
	fd = open("/sys/firmware/efi/vars/del_var", O_WRONLY);
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

int
efi_set_variable(efi_guid_t guid, const char *name, uint8_t *data,
		 size_t data_size, uint32_t attributes)
{
	int errno_value;
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
	int rc = asprintf(&path, "/sys/firmware/efi/vars/%s-"GUID_FORMAT"/data",
			  name, guid.a, guid.b, guid.c, guid.d, guid.e[0],
			  guid.e[1], guid.e[2], guid.e[3], guid.e[4],
			  guid.e[5]);
	if (rc < 0)
		return -1;

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

	fd = open("/sys/firmware/efi/vars/new_var", O_WRONLY);
	if (fd < 0)
		goto err;

	rc = write(fd, &var, sizeof(var));
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
