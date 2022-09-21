// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Copyright 2011-2014 Red Hat, Inc.
 * All rights reserved.
 *
 * Author(s): Peter Jones <pjones@redhat.com>
 */
#ifndef EFIVAR_UTIL_H
#define EFIVAR_UTIL_H 1

#include <alloca.h>
#include <ctype.h>
#include <endian.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <tgmath.h>
#include <unistd.h>

#include "include/efivar/efivar.h"

#include "compiler.h"
#include "list.h"

static inline int UNUSED
read_file(int fd, uint8_t **result, size_t *bufsize)
{
	uint8_t *p;
	size_t size = 4096;
	size_t filesize = 0;
	ssize_t s = 0;
	uint8_t *buf, *newbuf;

	if (!(newbuf = calloc(size, sizeof (uint8_t)))) {
		efi_error("could not allocate memory");
		*result = buf = NULL;
		*bufsize = 0;
		return -1;
	}
	buf = newbuf;

	do {
		p = buf + filesize;
		/* size - filesize shouldn't exceed SSIZE_MAX because we're
		 * only allocating 4096 bytes at a time and we're checking that
		 * before doing so. */
		s = read(fd, p, size - filesize);
		if (s < 0 && errno == EAGAIN) {
			/*
			 * if we got EAGAIN, there's a good chance we've hit
			 * the kernel rate limiter.  Doing more reads is just
			 * going to make it worse, so instead, give it a rest.
			 */
			sched_yield();
			continue;
		} else if (s < 0) {
			int saved_errno = errno;
			free(buf);
			*result = buf = NULL;
			*bufsize = 0;
			errno = saved_errno;
			efi_error("could not read from file");
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
				free(buf);
				*result = buf = NULL;
				*bufsize = 0;
				errno = ENOMEM;
				efi_error("could not read from file");
				return -1;
			}
			newbuf = realloc(buf, size + 4096);
			if (newbuf == NULL) {
				int saved_errno = errno;
				free(buf);
				*result = buf = NULL;
				*bufsize = 0;
				errno = saved_errno;
				efi_error("could not allocate memory");
				return -1;
			}
			buf = newbuf;
			memset(buf + size, '\0', 4096);
			size += 4096;
		}
	} while (1);

	newbuf = realloc(buf, filesize+1);
	if (!newbuf) {
		free(buf);
		*result = buf = NULL;
		efi_error("could not allocate memory");
		return -1;
	}
	newbuf[filesize] = '\0';
	*result = newbuf;
	*bufsize = filesize+1;
	return 0;
}

static inline uint64_t UNUSED
lcm(uint64_t x, uint64_t y)
{
	uint64_t m = x, n = y, o;
	while ((o = m % n)) {
		m = n;
		n = o;
	}
	return (x / n) * y;
}

#define xfree(x) ({ if (x) { free(x); x = NULL; } })

#ifndef strdupa
#define strdupa(s)						\
       (__extension__ ({					\
		const char *__in = (s);				\
		size_t __len;					\
		char *__out = NULL;				\
		if (__in) {					\
			__len = strlen (__in);			\
			__out = (char *) alloca (__len + 1);	\
			strcpy(__out, __in);			\
		}						\
		__out;						\
	}))
#endif

#ifndef strndupa
#define strndupa(s, l)						\
       (__extension__ ({					\
		const char *__in = (s);				\
		size_t __len;					\
		char *__out = NULL;				\
		if (__in) {					\
			__len = strnlen (__in, (l));		\
			__out = (char *) alloca (__len + 1);	\
			strncpy(__out, __in, __len);		\
			__out[__len] = '\0';			\
		}						\
		__out;						\
	}))
#endif

#define asprintfa(str, fmt, args...)				\
	({							\
		char *_tmp = NULL;				\
		int _rc;					\
		*(str) = NULL;					\
		_rc = asprintf((str), (fmt), ## args);		\
		if (_rc > 0) {					\
			_tmp = strdupa(*(str));			\
			if (!_tmp) {				\
				_rc = -1;			\
			} else {				\
				free(*(str));			\
				*(str) = _tmp;			\
			}					\
		} else {					\
			_rc = -1;				\
		}						\
		_rc;						\
	})

#define vasprintfa(str, fmt, ap)				\
	({							\
		char *_tmp = NULL;				\
		int _rc;					\
		*(str) = NULL;					\
		_rc = vasprintf((str), (fmt), (ap));		\
		if (_rc > 0) {					\
			_tmp = strdupa(*(str));			\
			if (!_tmp) {				\
				_rc = -1;			\
			} else {				\
				free(*(str));			\
				*(str) = _tmp;			\
			}					\
		} else {					\
			_rc = -1;				\
		}						\
		_rc;						\
	})

extern size_t HIDDEN page_size;

static inline ssize_t UNUSED
get_file(uint8_t **result, const char * const fmt, ...)
{
	char *path;
	uint8_t *buf = NULL;
	size_t bufsize = 0;
	ssize_t rc;
	va_list ap;
	int error;
	int fd;

	if (result == NULL) {
		efi_error("invalid parameter 'result'");
		return -1;
	}

	va_start(ap, fmt);
	rc = vasprintfa(&path, fmt, ap);
	va_end(ap);
	if (rc < 0) {
		efi_error("could not allocate memory");
		return -1;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		efi_error("could not open file \"%s\" for reading",
			  path);
		return -1;
	}

	rc = read_file(fd, &buf, &bufsize);
	error = errno;
	close(fd);
	errno = error;

	if (rc < 0 || bufsize < 1) {
		/*
		 * I don't think this can happen, but I can't convince
		 * cov-scan
		 */
		if (buf)
			free(buf);
		*result = NULL;
		efi_error("could not read file \"%s\"", path);
		return -1;
	}

	*result = buf;
	return bufsize;
}

static inline void UNUSED
swizzle_guid_to_uuid(efi_guid_t *guid)
{
	uint32_t *u32;
	uint16_t *u16;

	u32 = (uint32_t *)guid;
	u32[0] = __builtin_bswap32(u32[0]);
	u16 = (uint16_t *)&u32[1];
	u16[0] = __builtin_bswap16(u16[0]);
	u16[1] = __builtin_bswap16(u16[1]);
}

static inline void UNUSED
debug_markers_(const char * const file, int line,
	       const char * const func, int level,
	       const char * const prefix, ...)
{
	FILE *logfile;
	va_list ap;
	int pos;
	int n = 0;
	bool on = false;

	va_start(ap, prefix);
	for (n = 0, pos = va_arg(ap, int); pos >= 0; pos = va_arg(ap, int), n++)
		;
	va_end(ap);
	if (n < 2)
		return;
	n = 0;

	efi_set_loglevel(level);
	logfile = efi_get_logfile();
	if (!logfile)
		return;
	fprintf(logfile, "%s:%d %s(): %s", file, line, func, prefix ? prefix : "");
	va_start(ap, prefix);
	while ((pos = va_arg(ap, int)) >= 0) {
		for (; n <= pos; n++) {
			if (n == pos)
				on = !on;
			fprintf(logfile, "%c", on ? '^' : ' ');
		}
	}
	fprintf(logfile, "\n");
	va_end(ap);
}

static inline int UNUSED
log_(char *file, int line, const char *func, int level, char *fmt, ...)
{
	efi_set_loglevel(level);
	FILE *logfile = efi_get_logfile();
	size_t len = strlen(fmt);
	va_list ap;
	int rc = 0;
	int sz;

	if (!logfile)
		return 0;

	sz = fprintf(logfile, "%s:%d %s(): ", file, line, func);
	if (sz < 0)
		return sz;
	rc += sz;

	va_start(ap, fmt);
	sz = vfprintf(logfile, fmt, ap);
	va_end(ap);
	if (sz < 0) {
		return sz;
	}
	rc += sz;

	if (!len || fmt[len - 1] != '\n') {
		sz = fprintf(logfile, "\n");
		if (sz < 0)
			return sz;
		rc += sz;
	}

	fflush(logfile);
	return rc;
}

#define LOG_VERBOSE 0
#define LOG_DEBUG 1
#define LOG_DEBUG_DUMPER 2

#define DEBUG_LEVEL 1

/*
 * makeguids includes util.h, which means any declarations that reference
 * something we're not linking in to makeguids needs to be avoided here.
 *
 * log_() uses efi_set_loglevel()/efi_get_loglevel(), which are provided by
 * libefivar, and we're not actually using it meaningfully in makeguids at
 * all, but the compiler gets to choose whether to include it in the output
 * for a compilation unit, and removing it is an optimization that is
 * optional.  So wrapping this here keeps from makeguids getting a linker
 * error on a symbol it isn't actually using anyway.
 */
#ifdef log
#undef log
#endif
#ifdef EFIVAR_BUILD_ENVIRONMENT
#define log(level, fmt, args...)
#define debug(fmt, args...)
#else
#define log(level, fmt, args...) log_(__FILE__, __LINE__, __func__, level, fmt, ## args)
#define debug(fmt, args...) log(DEBUG_LEVEL, fmt, ## args)
#endif
#define log_hex_(file, line, func, level, buf, size)			\
	({								\
		efi_set_loglevel(level);				\
		fhexdumpf(efi_get_logfile(), "%s:%d %s(): ",		\
			  (uint8_t *)buf, size,				\
			  file, line, func);				\
	})
#define log_hex(level, buf, size) log_hex_(__FILE__, __LINE__, __func__, level, buf, size)
#define debug_hex(buf, size) log_hex(LOG_DEBUG, buf, size)
#define dbgmk(prefix, args...) debug_markers_(__FILE__, __LINE__, __func__, LOG_DEBUG, prefix, ## args, -1)

static inline UNUSED void
show_errors(void)
{
	int rc = 1;

	if (efi_get_verbose() < 1)
		return;

	printf("Error trace:\n");
	for (int i = 0; rc > 0; i++) {
		char *filename = NULL;
		char *function = NULL;
		int line = 0;
		char *message = NULL;
		int error = 0;
		size_t len;

		rc = efi_error_get(i, &filename, &function, &line, &message,
				   &error);
		if (rc < 0)
			err(1, "error fetching trace value");
		if (rc == 0)
			break;
		printf(" %s:%d %s(): %s: %s", filename, line, function,
		       strerror(error), message);

		len = strlen(message);
		if (len >= 1 && message[len-1] != '\n')
			printf("\n");
	}
	fflush(stdout);
	efi_error_clear();
}

typedef struct {
	list_t list;
	void *ptr;
} ptrlist_t;

static inline UNUSED void
ptrlist_add(list_t *list, char *ptr)
{
	ptrlist_t *pl = calloc(1, sizeof(ptrlist_t));
	if (!pl)
		err(1, "could not allocate memory");
	pl->ptr = ptr;
	list_add_tail(&pl->list, list);
}
#define for_each_ptr(pos, head) list_for_each(pos, head)
#define for_each_ptr_safe(pos, tmp, head) list_for_each_safe(pos, tmp, head)

static inline UNUSED
int16_t hexchar_to_bin(char hex)
{
	if (hex >= '0' && hex <= '9')
		return hex - '0';
	if (hex >= 'A' && hex <= 'F')
		return hex - 'A' + 10;
	if (hex >= 'a' && hex <= 'f')
		return hex - 'a' + 10;
	return -1;
}

static inline UNUSED uint8_t *
hex_to_bin(char *hex, size_t size)
{
	uint8_t *ret = calloc(1, size+1);
	if (!ret)
		return NULL;

	for (unsigned int i = 0, j = 0; i < size*2; i+= 2, j++) {
		int16_t val;

		val = hexchar_to_bin(hex[i]);
		if (val > 15 || val < 0)
			goto out_of_range;
		ret[j] = (val & 0xf) << 4;
		val = hexchar_to_bin(hex[i+1]);
		if (val > 15 || val < 0)
			goto out_of_range;
		ret[j] |= val & 0xf;
	};
	return ret;
out_of_range:
	free(ret);
	errno = ERANGE;
	return NULL;
}

#define LEG(x, y)                                           \
	({                                                  \
		intmax_t x_ = (intmax_t)(x);                \
		intmax_t y_ = (intmax_t)(y);                \
		((x_ > y_) ? '>' : ((x_ < y_) ? '<' : '='));\
	})
#define LEGR(r) (((r) > 0) ? '>' : (((r) < 0) ? '<' : '='))

/*
 * this is isprint() but it doesn't consider characters that move the
 * cursor or have other side effects as printable
 */
static inline bool UNUSED
safe_to_print(const int c)
{
	if (!isprint(c))
		return false;

	switch (c) {
	case '\a':
	case '\b':
	case '\t':
	case '\n':
	case '\v':
	case '\f':
	case '\r':
		return false;
	default:
		break;
	}

	return true;
}

#endif /* EFIVAR_UTIL_H */

// vim:fenc=utf-8:tw=75:noet
