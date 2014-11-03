/*
 * Copyright 2011-2014 Red Hat, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s): Peter Jones <pjones@redhat.com>
 */
#ifndef EFIVAR_UTIL_H
#define EFIVAR_UTIL_H 1

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static inline int
__attribute__((unused))
read_file(int fd, uint8_t **buf, size_t *bufsize)
{
	uint8_t *p;
	size_t size = 4096;
	size_t filesize = 0;
	ssize_t s = 0;

	uint8_t *newbuf;
	if (!(newbuf = calloc(size, sizeof (uint8_t))))
		return -1;
	*buf = newbuf;

	do {
		p = *buf + filesize;
		/* size - filesize shouldn't exceed SSIZE_MAX because we're
		 * only allocating 4096 bytes at a time and we're checking that
		 * before doing so. */
		s = read(fd, p, size - filesize);
		if (s < 0 && errno == EAGAIN) {
			continue;
		} else if (s < 0) {
			int saved_errno = errno;
			free(*buf);
			*buf = NULL;
			*bufsize = 0;
			errno = saved_errno;
			return -1;
		}
		filesize += s;
		/* only exit for empty reads */
		if (s == 0)
			break;
		if (filesize >= size) {
			/* See if we're going to overrun and return an error
			 * instead. */
			if (size > (size_t)-1 - 4096) {
				free(*buf);
				*buf = NULL;
				*bufsize = 0;
				errno = ENOMEM;
				return -1;
			}
			newbuf = realloc(*buf, size + 4096);
			if (newbuf == NULL) {
				int saved_errno = errno;
				free(*buf);
				*buf = NULL;
				*bufsize = 0;
				errno = saved_errno;
				return -1;
			}
			*buf = newbuf;
			memset(*buf + size, '\0', 4096);
			size += 4096;
		}
	} while (1);

	*bufsize = filesize;
	return 0;
}

#endif /* EFIVAR_UTIL_H */
