/*
* libefivar - library for the manipulation of EFI variables
* Copyright 2015 Red Hat, Inc.
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

#include <inttypes.h>
#include <limits.h>
#include <mntent.h>
#include <stdlib.h>
#include <stdio.h>

#include "efivar.h"
#include "dp.h"

static int
__attribute__((__nonnull__ (1,2,3)))
find_file(const char const *filepath, char **devicep, char **relpathp)
{
	struct stat fsb = { 0, };
	int rc;
	int ret = -1;
	FILE *mounts = NULL;
	char linkbuf[PATH_MAX+1] = "";
	ssize_t linklen = 0;

	if (!filepath || !devicep || !relpathp) {
		errno = EINVAL;
		return -1;
	}

	linklen = strlen(filepath);
	if (linklen > PATH_MAX) {
		errno = ENAMETOOLONG;
		return -1;
	}
	strcpy(linkbuf, filepath);

	do {
		rc = stat(linkbuf, &fsb);
		if (rc < 0)
			return rc;

		if (S_ISLNK(fsb.st_mode)) {
			char tmp[PATH_MAX+1] = "";
			ssize_t l;

			l = readlink(linkbuf, tmp, PATH_MAX);
			if (l < 0)
				return -1;
			tmp[l] = '\0';
			linklen = l;
			strcpy(linkbuf, tmp);
		} else {
			break;
		}
	} while (1);

	mounts = fopen("/proc/self/mounts", "r");
	if (mounts == NULL)
		return rc;

	struct mntent *me;
	while (1) {
		struct stat dsb = { 0, };

		errno = 0;
		me = getmntent(mounts);
		if (!me) {
			if (feof(mounts))
				errno = ENOENT;
			goto err;
		}

		if (me->mnt_fsname[0] != '/')
			continue;

		rc = stat(me->mnt_fsname, &dsb);
		if (rc < 0) {
			if (errno == ENOENT)
				continue;
			goto err;
		}

		if (!S_ISBLK(dsb.st_mode))
			continue;

		if (dsb.st_rdev == fsb.st_dev) {
			ssize_t mntlen = strlen(me->mnt_dir);
			if (mntlen >= linklen) {
				errno = ENAMETOOLONG;
				goto err;
			}
			*devicep = strdup(me->mnt_fsname);
			if (!*devicep)
				goto err;
			*relpathp = strdup(linkbuf + mntlen);
			if (!*relpathp) {
				free(*devicep);
				*devicep = NULL;
				goto err;
			}
			ret = 0;
			break;
		}
	}
err:
	if (mounts)
		endmntent(mounts);
	return ret;
}

ssize_t
__attribute__((__nonnull__ (3)))
__attribute__((__visibility__ ("default")))
efi_generate_file_device_path(uint8_t *buf, ssize_t size,
			    const char const *filepath,
			    uint32_t abbrev, int ignore_fs_err)
{
	int rc;
	char *device = NULL;
	char *relpath = NULL;

	rc = find_file(filepath, &device, &relpath);
	if (rc < 0)
		return -1;

	printf("device: %s\nrelpath: %s\n", device, relpath);

	return 0;
}

ssize_t
__attribute__((__nonnull__ (3)))
__attribute__((__visibility__ ("default")))
efi_generate_network_device_path(uint8_t *buf, ssize_t size,
			       const char const *ifname,
			       uint32_t abbrev)
{
	errno = ENOSYS;
	return -1;
}
