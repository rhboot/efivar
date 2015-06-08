#ifndef LIBEFIVAR_GENERIC_NEXT_VARIABLE_NAME_H
#define LIBEFIVAR_GENERIC_NEXT_VARIABLE_NAME_H 1

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "guid.h"

static DIR *dir;

static inline int
__attribute__((unused))
generic_get_next_variable_name(char *path, efi_guid_t **guid, char **name)
{
	static char ret_name[NAME_MAX+1];
	static efi_guid_t ret_guid;

	if (!guid || !name) {
		errno = EINVAL;
		return -1;
	}

	/* if only one of guid and name are null, there's no "next" variable,
	 * because the current variable is invalid. */
	if ((*guid == NULL && *name != NULL) ||
			(*guid != NULL && *name == NULL)) {
		errno = EINVAL;
		return -1;
	}

	/* if dir is NULL, we're also starting over */
	if (!dir) {
		dir = opendir(path);
		if (!dir)
			return -1;

		int fd = dirfd(dir);
		if (fd < 0) {
			typeof(errno) errno_value = errno;
			closedir(dir);
			errno = errno_value;
			return -1;
		}
		int flags = fcntl(fd, F_GETFD);
		if (flags < 0) {
			warn("fcntl(fd, F_GETFD) failed");
		} else {
			flags |= FD_CLOEXEC;
			if (fcntl(fd, F_SETFD, flags) < 0)
				warn("fcntl(fd, F_SETFD, "
					"flags | FD_CLOEXEC) failed");
		}

		*guid = NULL;
		*name = NULL;
	}

	struct dirent *de = NULL;
	char *guidtext = "8be4df61-93ca-11d2-aa0d-00e098032b8c";
	size_t guidlen = strlen(guidtext);

	while (1) {
		de = readdir(dir);
		if (de == NULL) {
			closedir(dir);
			dir = NULL;
			return 0;
		}
		/* a proper entry must have space for a guid, a dash, and
		 * the variable name */
		size_t namelen = strlen(de->d_name);
		if (namelen < guidlen + 2)
			continue;

		int rc = text_to_guid(de->d_name +namelen -guidlen, &ret_guid);
		if (rc < 0) {
			closedir(dir);
			dir = NULL;
			errno = EINVAL;
			return -1;
		}

		strncpy(ret_name, de->d_name, sizeof(ret_name));
		ret_name[namelen - guidlen - 1] = '\0';

		*guid = &ret_guid;
		*name = ret_name;
		break;
	}

	return 1;
}

static void __attribute__((destructor)) close_dir(void);
static void
close_dir(void)
{
	if (dir != NULL) {
		closedir(dir);
		dir = NULL;
	}
}

/* this is a simple read/delete/write implementation of "update".  Good luck.
 * -- pjones */
static int
__attribute__((__unused__))
__attribute__((__flatten__))
generic_append_variable(efi_guid_t guid, const char *name,
		       uint8_t *new_data, size_t new_data_size,
		       uint32_t new_attributes)
{
	int rc;
	uint8_t *data = NULL;
	size_t data_size = 0;
	uint32_t attributes = 0;

	rc = efi_get_variable(guid, name, &data, &data_size, &attributes);
	if (rc >= 0) {
		if ((attributes | EFI_VARIABLE_APPEND_WRITE) !=
				(new_attributes | EFI_VARIABLE_APPEND_WRITE)) {
			free(data);
			errno = EINVAL;
			return -1;
		}
		uint8_t *d = malloc(data_size + new_data_size);
		size_t ds = data_size + new_data_size;
		memcpy(d, data, data_size);
		memcpy(d + data_size, new_data, new_data_size);
		attributes &= ~EFI_VARIABLE_APPEND_WRITE;
		rc = efi_del_variable(guid, name);
		if (rc < 0) {
			free(data);
			free(d);
			return rc;
		}
		/* if this doesn't work, we accidentally deleted.  There's
		 * really not much to do about it, so return the error and
		 * let our caller attempt to clean up :/
		 */
		rc = _efi_set_variable(guid, name, d, ds, attributes, 0);
		free(d);
		free(data);
		return rc;
	} else if (errno == ENOENT) {
		data = new_data;
		data_size = new_data_size;
		attributes = new_attributes & ~EFI_VARIABLE_APPEND_WRITE;
		rc = _efi_set_variable(guid, name, data, data_size, attributes, 0);
		return rc;
	}
	return rc;
}

#endif /* LIBEFIVAR_GENERIC_NEXT_VARIABLE_NAME_H */
