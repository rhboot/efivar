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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include "efivar.h"

static const char default_vars_path[] = "/sys/firmware/efi/vars/";

static const char *
get_vars_path(void)
{
	static const char *path;
	if (path)
		return path;

	path = getenv("VARS_PATH");
	if (!path)
		path = default_vars_path;
	return path;
}


typedef struct efi_kernel_variable_32_t {
	uint16_t	VariableName[1024/sizeof(uint16_t)];
	efi_guid_t	VendorGuid;
	uint32_t	DataSize;
	uint8_t		Data[1024];
	uint32_t	Status;
	uint32_t	Attributes;
} PACKED efi_kernel_variable_32_t;

typedef struct efi_kernel_variable_64_t {
	uint16_t	VariableName[1024/sizeof(uint16_t)];
	efi_guid_t	VendorGuid;
	uint64_t	DataSize;
	uint8_t		Data[1024];
	uint64_t	Status;
	uint32_t	Attributes;
} PACKED efi_kernel_variable_64_t;

static ssize_t
get_file_data_size(int dfd, char *name)
{
	char raw_var[NAME_MAX + 9];

	memset(raw_var, '\0', sizeof (raw_var));
	strncpy(raw_var, name, NAME_MAX);
	strcat(raw_var, "/raw_var");

	int fd = openat(dfd, raw_var, O_RDONLY);
	if (fd < 0) {
		efi_error("openat failed");
		return -1;
	}

	char buf[4096];
	ssize_t sz, total = 0;
	int tries = 5;
	while (1) {
		sz = read(fd, buf, 4096);
		if (sz < 0 && (errno == EAGAIN || errno == EINTR)) {
			if (tries--)
				continue;
			total = -1;
			break;
		}

		if (sz < 0) {
			int saved_errno = errno;
			close(fd);
			errno = saved_errno;
			return -1;
		}

		if (sz == 0)
			break;
		total += sz;
	}
	close(fd);
	return total;
}

/*
 * Determine which ABI the kernel has given us.
 *
 * We have two situations - before and after kernel's commit e33655a38.
 * Before hand, the situation is like:
 * 64-on-64 - 64-bit DataSize and status
 * 32-on-32 - 32-bit DataSize and status
 * 32-on-64 - 64-bit DataSize and status
 *
 * After it's like this if CONFIG_COMPAT is enabled:
 * 64-on-64 - 64-bit DataSize and status
 * 32-on-64 - 32-bit DataSize and status
 * 32-on-32 - 32-bit DataSize and status
 *
 * Is there a better way to figure this out?
 * Submit your patch here today!
 */
static int
is_64bit(void)
{
	static int sixtyfour_bit = -1;
	DIR *dir = NULL;
	int dfd = -1;
	int saved_errno;

	if (sixtyfour_bit != -1)
		return sixtyfour_bit;

	dir = opendir(get_vars_path());
	if (!dir)
		goto err;

	dfd = dirfd(dir);
	if (dfd < 0)
		goto err;

	while (1) {
		struct dirent *entry = readdir(dir);
		if (entry == NULL)
			break;

		if (!strcmp(entry->d_name, "..") || !strcmp(entry->d_name, "."))
			continue;

		ssize_t size = get_file_data_size(dfd, entry->d_name);
		if (size < 0) {
			continue;
		} else if (size == 2084) {
			sixtyfour_bit = 1;
		} else {
			sixtyfour_bit = 0;
		}

		errno = 0;
		break;
	}
	if (sixtyfour_bit == -1)
		sixtyfour_bit = __SIZEOF_POINTER__ == 4 ? 0 : 1;
err:
	saved_errno = errno;

	if (dir)
		closedir(dir);

	errno = saved_errno;
	return sixtyfour_bit;
}

static int
get_size_from_file(const char *filename, size_t *retsize)
{
	uint8_t *buf = NULL;
	size_t bufsize = -1;
	int errno_value;
	int ret = -1;
	int fd = open(filename, O_RDONLY);
	if (fd < 0) {
		efi_error("open(%s, O_RDONLY) failed", filename);
		goto err;
	}

	int rc = read_file(fd, &buf, &bufsize);
	if (rc < 0) {
		efi_error("read_file(%s) failed", filename);
		goto err;
	}

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
	char *newvar;

	/* If we can't tell if it's 64bit or not, this interface is no good. */
	if (is_64bit() < 0) {
		efi_error("is_64bit() failed");
		return 0;
	}
	if (asprintfa(&newvar, "%s%s", get_vars_path(), "new_var") < 0) {
		efi_error("asprintfa failed");
		return 0;
	}
	if (!access(newvar, F_OK))
		return 1;
	efi_error("access(%s, F_OK) failed", newvar);
	return 0;
}

static int
vars_get_variable_size(efi_guid_t guid, const char *name, size_t *size)
{
	int errno_value;
	int ret = -1;

	char *path = NULL;
	int rc = asprintf(&path, "%s%s-"GUID_FORMAT"/size", get_vars_path(),
			  name, guid.a, guid.b, guid.c, bswap_16(guid.d),
			  guid.e[0], guid.e[1], guid.e[2], guid.e[3],
			  guid.e[4], guid.e[5]);
	if (rc < 0) {
		efi_error("asprintf failed");
		goto err;
	}

	size_t retsize = 0;
	rc = get_size_from_file(path, &retsize);
	if (rc >= 0) {
		ret = 0;
		*size = retsize;
	} else if (rc < 0) {
		efi_error("get_size_from_file(%s) failed", path);
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
	if (ret < 0) {
		efi_error("efi_get_variable() failed");
		return ret;
	}

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
	char *path = NULL;
	int rc;
	int fd = -1;
	int ratelimit;

	/*
	 * The kernel rate limiter hits us if we go faster than 100 efi
	 * variable reads per second as non-root.  So if we're not root, just
	 * delay this long after each read.  The user is not going to notice.
	 *
	 * 1s / 100 = 10000us.
	 */
	ratelimit = geteuid() == 0 ? 0 : 10000;

	rc = asprintf(&path, "%s%s-" GUID_FORMAT "/raw_var",
			  get_vars_path(),
			  name, guid.a, guid.b, guid.c, bswap_16(guid.d),
			  guid.e[0], guid.e[1], guid.e[2],
			  guid.e[3], guid.e[4], guid.e[5]);
	if (rc < 0) {
		efi_error("asprintf failed");
		goto err;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		efi_error("open(%s, O_RDONLY) failed", path);
		goto err;
	}

	usleep(ratelimit);
	rc = read_file(fd, &buf, &bufsize);
	if (rc < 0) {
		efi_error("read_file(%s) failed", path);
		goto err;
	}

	bufsize -= 1; /* read_file pads out 1 extra byte to NUL it */

	if (is_64bit()) {
		efi_kernel_variable_64_t *var64;

		if (bufsize != sizeof(efi_kernel_variable_64_t)) {
			errno = EFBIG;
			efi_error("file size is wrong for 64-bit variable (%zd of %zd)",
				  bufsize, sizeof(efi_kernel_variable_64_t));
			goto err;
		}

		var64 = (void *)buf;
		*data = malloc(var64->DataSize);
		if (!*data) {
			efi_error("malloc failed");
			goto err;
		}
		memcpy(*data, var64->Data, var64->DataSize);
		*data_size = var64->DataSize;
		*attributes = var64->Attributes;
	} else {
		efi_kernel_variable_32_t *var32;

		if (bufsize != sizeof(efi_kernel_variable_32_t)) {
			efi_error("file size is wrong for 32-bit variable (%zd of %zd)",
				  bufsize, sizeof(efi_kernel_variable_32_t));
			errno = EFBIG;
			goto err;
		}

		var32 = (void *)buf;
		*data = malloc(var32->DataSize);
		if (!*data) {
			efi_error("malloc failed");
			goto err;
		}
		memcpy(*data, var32->Data, var32->DataSize);
		*data_size = var32->DataSize;
		*attributes = var32->Attributes;
	}

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
	char *path = NULL;
	int rc;
	int fd = -1;
	uint8_t *buf = NULL;
	size_t buf_size = 0;
	char *delvar;

	rc = asprintf(&path, "%s%s-" GUID_FORMAT "/raw_var",
			  get_vars_path(),
			  name, guid.a, guid.b, guid.c, bswap_16(guid.d),
			  guid.e[0], guid.e[1], guid.e[2],
			  guid.e[3], guid.e[4], guid.e[5]);
	if (rc < 0) {
		efi_error("asprintf failed");
		goto err;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		efi_error("open(%s, O_RDONLY) failed", path);
		goto err;
	}

	rc = read_file(fd, &buf, &buf_size);
	buf_size -= 1; /* read_file pads out 1 extra byte to NUL it */
	if (rc < 0) {
		efi_error("read_file(%s) failed", path);
		goto err;
	}

	if (buf_size != sizeof(efi_kernel_variable_64_t) &&
		       buf_size != sizeof(efi_kernel_variable_32_t)) {
		efi_error("variable size %zd is not 32-bit (%zd) or 64-bit (%zd)",
			  buf_size, sizeof(efi_kernel_variable_32_t),
			  sizeof(efi_kernel_variable_64_t));

		errno = EFBIG;
		goto err;
	}

	if (asprintfa(&delvar, "%s%s", get_vars_path(), "del_var") < 0) {
		efi_error("asprintfa() failed");
		goto err;
	}

	close(fd);
	fd = open(delvar, O_WRONLY);
	if (fd < 0) {
		efi_error("open(%s, O_WRONLY) failed", delvar);
		goto err;
	}

	rc = write(fd, buf, buf_size);
	if (rc >= 0)
		ret = 0;
	else
		efi_error("write() failed");
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
		} else if (rc < 0) {
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
	int rc = asprintf(&path, "%s%s-" GUID_FORMAT, get_vars_path(),
			  name, guid.a, guid.b, guid.c, bswap_16(guid.d),
			  guid.e[0], guid.e[1], guid.e[2], guid.e[3],
			  guid.e[4], guid.e[5]);
	if (rc < 0) {
		efi_error("asprintf failed");
		return -1;
	}

	rc = _vars_chmod_variable(path, mode);
	int saved_errno = errno;
	efi_error("_vars_chmod_variable() failed");
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
	int fd = -1;

	if (strlen(name) > 1024) {
		efi_error("variable name size is too large (%zd of 1024)",
			  strlen(name));
		errno = EINVAL;
		return -1;
	}
	if (data_size > 1024) {
		efi_error("variable data size is too large (%zd of 1024)",
			  data_size);
		errno = ENOSPC;
		return -1;
	}

	char *path;
	int rc = asprintf(&path, "%s%s-" GUID_FORMAT "/data", get_vars_path(),
			  name, guid.a, guid.b, guid.c, bswap_16(guid.d),
			  guid.e[0], guid.e[1], guid.e[2], guid.e[3],
			  guid.e[4], guid.e[5]);
	if (rc < 0) {
		efi_error("asprintf failed");
		goto err;
	}

	len = rc;

	if (!access(path, F_OK)) {
		rc = efi_del_variable(guid, name);
		if (rc < 0) {
			efi_error("efi_del_variable failed");
			goto err;
		}
	}
	char *newvar;
	if (asprintfa(&newvar, "%s%s", get_vars_path(), "new_var") < 0) {
		efi_error("asprintfa failed");
		goto err;
	}

	if (is_64bit()) {
		efi_kernel_variable_64_t var64 = {
			.VendorGuid = guid,
			.DataSize = data_size,
			.Status = 0,
			.Attributes = attributes
			};

		for (int i = 0; name[i] != '\0'; i++)
			var64.VariableName[i] = name[i];
		memcpy(var64.Data, data, data_size);

		fd = open(newvar, O_WRONLY);
		if (fd < 0) {
			efi_error("open(%s, O_WRONLY) failed", newvar);
			goto err;
		}

		rc = write(fd, &var64, sizeof(var64));
	} else {
		efi_kernel_variable_32_t var32 = {
			.VendorGuid = guid,
			.DataSize = data_size,
			.Status = 0,
			.Attributes = attributes
			};
		for (int i = 0; name[i] != '\0'; i++)
			var32.VariableName[i] = name[i];
		memcpy(var32.Data, data, data_size);

		fd = open(newvar, O_WRONLY);
		if (fd < 0) {
			efi_error("open(%s, O_WRONLY) failed", newvar);
			goto err;
		}

		rc = write(fd, &var32, sizeof(var32));
	}

	if (rc >= 0)
		ret = 0;
	else
		efi_error("write() failed");

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
	int rc;
	const char *vp = get_vars_path();
	rc = generic_get_next_variable_name(vp, guid, name);
	if (rc < 0)
		efi_error("generic_get_next_variable_name(%s,...) failed", vp);
	return rc;
}

struct efi_var_operations vars_ops = {
	.name = "vars",
	.probe = vars_probe,
	.set_variable = vars_set_variable,
	.del_variable = vars_del_variable,
	.get_variable = vars_get_variable,
	.get_variable_attributes = vars_get_variable_attributes,
	.get_variable_size = vars_get_variable_size,
	.get_next_variable_name = vars_get_next_variable_name,
	.chmod_variable = vars_chmod_variable,
};
