/*
 * Copyright 2011-2014 Red Hat, Inc.
 * All rights reserved.
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
 * Author(s): Peter Jones <pjones@redhat.com>
 */
#ifndef EFIVAR_UTIL_H
#define EFIVAR_UTIL_H 1

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <tgmath.h>
#include <unistd.h>

/*
 * I'm not actually sure when these appear, but they're present in the
 * version in front of me.
 */
#if defined(__GNUC__) && defined(__GNUC_MINOR__)
#if __GNUC__ >= 5 && __GNUC_MINOR__ >= 1
#define int_add(a, b, c) __builtin_add_overflow(a, b, c)
#define long_add(a, b, c) __builtin_add_overflow(a, b, c)
#define long_mult(a, b, c) __builtin_mul_overflow(a, b, c)
#define ulong_add(a, b, c) __builtin_add_overflow(a, b, c)
#define ulong_mult(a, b, c) __builtin_mul_overflow(a, b, c)
#endif
#endif
#ifndef int_add
#define int_add(a, b, c) ({					\
		const int _limit = INT_MAX;			\
		int _ret;					\
		_ret = _limit - ((unsigned long long)a) >	\
			  ((unsigned long long)b);		\
		if (!_ret)					\
			*(c) = ((a) + (b));			\
		_ret;						\
	})
#endif
#ifndef long_add
#define long_add(a, b, c) ({					\
		const long _limit = LONG_MAX;			\
		int _ret;					\
		_ret = _limit - ((unsigned long long)a) >	\
			   ((unsigned long long)b);		\
		if (!_ret)					\
			*(c) = ((a) + (b));			\
		_ret;						\
	})
#endif
#ifndef long_mult
#define long_mult(a, b, c) ({					\
		const long _limit = LONG_MAX;			\
		int _ret = 1;					\
		if ((a) == 0 || (b) == 0)			\
			_ret = 0;				\
		else						\
			_ret = _limit / (a) < (b);		\
		if (!_ret)					\
			*(c) = ((a) * (b));			\
		_ret;						\
	})
#endif
#ifndef ulong_add
#define ulong_add(a, b, c) ({					\
		const unsigned long _limit = ULONG_MAX;		\
		int _ret;					\
		_ret = _limit - ((unsigned long long)a) >	\
			    ((unsigned long long)b);		\
		if (!_ret)					\
			*(c) = ((a) + (b));			\
		_ret;						\
	})
#endif
#ifndef ulong_mult
#define ulong_mult(a, b, c) ({					\
		const unsigned long _limit = ULONG_MAX;		\
		int _ret = 1;					\
		if ((a) == 0 || (b) == 0)			\
			_ret = 0;				\
		else						\
			_ret = _limit / (a) < (b);		\
		if (!_ret)					\
			*(c) = ((a) * (b));			\
		_ret;						\
	})
#endif

#define add(a, b, c) _Generic((c),					\
			      int *: int_add(a,b,c),			\
			      long *: long_add(a,b,c),			\
			      unsigned long *: ulong_add(a,b,c))

#define mult(a, b, c) _Generic((c),					\
			      long *: long_mult(a,b,c),			\
			      unsigned long *: ulong_mult(a,b,c))

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

	newbuf = realloc(*buf, filesize+1);
	if (!newbuf) {
		free(*buf);
		*buf = NULL;
		return -1;
	}
	newbuf[filesize] = '\0';
	*buf = newbuf;
	*bufsize = filesize+1;
	return 0;
}

static inline uint64_t
__attribute__((unused))
lcm(uint64_t x, uint64_t y)
{
	uint64_t m = x, n = y, o;
	while ((o = m % n)) {
		m = n;
		n = o;
	}
	return (x / n) * y;
}

/************************************************************
 * get_sector_size
 * Requires:
 *  - filedes is an open file descriptor, suitable for reading
 * Modifies: nothing
 * Returns:
 *  sector size, or 512.
 ************************************************************/
static inline int
__attribute__((unused))
get_sector_size(int filedes)
{
	int rc, sector_size = 512;

	rc = ioctl(filedes, BLKSSZGET, &sector_size);
	if (rc)
		sector_size = 512;
	return sector_size;
}

#define asprintfa(str, fmt, args...)					\
	({								\
		char *_tmp = NULL;					\
		int _rc;						\
		*(str) = NULL;						\
		_rc = asprintf((str), (fmt), ## args);			\
		if (_rc > 0) {						\
			_tmp = strdupa(*(str));				\
			if (!_tmp) {					\
				_rc = -1;				\
			} else {					\
				free(*(str));				\
				*(str) = _tmp;				\
			}						\
		} else {						\
			_rc = -1;					\
		}							\
		_rc;							\
	})

#define read_sysfs_file(buf, fmt, args...)				\
	({								\
		int _rc=-1;						\
		char *_pathname;					\
		uint8_t *_buf=NULL;					\
		size_t _bufsize=-1;					\
		int _saved_errno;					\
									\
		*(buf) = NULL;						\
		_rc = asprintfa(&_pathname, (fmt), ## args);		\
		if (_rc >= 0) {						\
			int _fd;					\
			_fd = open(_pathname, O_RDONLY);		\
			_saved_errno = errno;				\
			_rc = -1;					\
			if (_fd >= 0) {					\
				_rc = read_file(_fd, &_buf, &_bufsize);	\
				_saved_errno = errno;			\
				close(_fd);				\
				errno = _saved_errno;			\
			}						\
		}							\
		if (_rc >= 0) {						\
			uint8_t *_buf2 = alloca(_bufsize);		\
			_saved_errno = errno;				\
			if (_buf2) {					\
				memcpy(_buf2, _buf, _bufsize);		\
				_rc = _bufsize;				\
			}						\
			free(_buf);					\
			*((uint8_t **)buf) = _buf2;			\
			errno = _saved_errno;				\
		}							\
		_rc;							\
	})

#define sysfs_readlink(linkbuf, fmt, args...)				\
	({								\
		char *_lb = alloca(PATH_MAX+1);				\
		char *_pn;						\
		int _rc;						\
									\
		*(linkbuf) = NULL;					\
		_rc = asprintfa(&_pn, fmt, ## args);			\
		if (_rc >= 0) {						\
			ssize_t _linksz;				\
			_linksz = readlink(_pn, _lb, PATH_MAX);		\
			_rc = _linksz;					\
			if (_linksz >= 0)				\
				_lb[_linksz] = '\0';			\
			*(linkbuf) = _lb;				\
		}							\
		_rc;							\
	})

#endif /* EFIVAR_UTIL_H */
