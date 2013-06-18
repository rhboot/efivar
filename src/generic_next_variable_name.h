#ifndef LIBEFIVAR_GENERIC_NEXT_VARIABLE_NAME_H
#define LIBEFIVAR_GENERIC_NEXT_VARIABLE_NAME_H 1

#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

#include "guid.h"

static DIR *dir;

static inline int
generic_get_next_variable_name(char *path, efi_guid_t **guid, char **name)
{
	static char ret_name[NAME_MAX+1];
	static efi_guid_t ret_guid;

	/* if only one of guid and name are null, there's no "next" variable,
	 * because the current variable is invalid. */
	if ((*guid == NULL && *name != NULL) ||
			(*guid != NULL && *name == NULL)) {
		errno = EINVAL;
		return -1;
	}

	/* if they're both NULL, we're starting over */
	if (guid == NULL && dir != NULL) {
		closedir(dir);
		dir = NULL;
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
		flags |= FD_CLOEXEC;
		fcntl(fd, F_SETFD, flags);

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

#endif /* LIBEFIVAR_GENERIC_NEXT_VARIABLE_NAME_H */
