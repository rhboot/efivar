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
	int s = 0, filesize = 0;

	uint8_t *newbuf;
	if (!(newbuf = calloc(size, sizeof (uint8_t))))
		return -1;
	*buf = newbuf;

	do {
		p = *buf + filesize;
		s = read(fd, p, 4096 - s);
		if (s < 0 && errno == EAGAIN) {
			continue;
		} else if (s < 0) {
			free(*buf);
			*buf = NULL;
			*bufsize = 0;
			break;
		}
		filesize += s;
		/* only exit for empty reads */
		if (s == 0) {
			break;
		} else if (s == 4096) {
			newbuf = realloc(*buf, size + 4096);
			if (newbuf == NULL) {
				free(*buf);
				*buf = NULL;
				*bufsize = 0;
				return -1;
			}
			*buf = newbuf;
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


#endif /* EFIVAR_UTIL_H */
