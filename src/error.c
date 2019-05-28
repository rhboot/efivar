/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2015 Red Hat, Inc.
 * Copyright (C) 2000-2001 Dell Computer Corporation <Matt_Domsch@dell.com>
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

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "efiboot.h"

/*
 * GCC/Clang complains that we check for null if we have a nonnull attribute,
 * even though older or other compilers might just ignore that attribute if
 * they don't support it.  Ugh.
 */
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wpointer-bool-conversion"
#elif defined(__GNUC__) && __GNUC__ >= 6
#pragma GCC diagnostic ignored "-Wnonnull-compare"
#endif

typedef struct {
	int error;
	char *filename;
	char *function;
	int line;
	char *message;
} error_table_entry;

static error_table_entry *error_table;
static unsigned int current;

int PUBLIC NONNULL(2, 3, 4, 5, 6)
efi_error_get(unsigned int n,
	      char ** const filename,
	      char ** const function,
	      int *line,
	      char ** const message,
	      int *error
	      )
{
	if (!filename || !function || !line || !message || !error) {
		errno = EINVAL;
		return -1;
	}

	if (n >= current)
		return 0;

	*filename = error_table[n].filename;
	*function = error_table[n].function;
	*line = error_table[n].line;
	*message = error_table[n].message;
	*error = error_table[n].error;

	return 1;
}

static inline UNUSED void
clear_error_entry(error_table_entry *et)
{
	if (!et)
		return;

	if (et->filename)
		free(et->filename);
	if (et->function)
		free(et->function);
	if (et->message)
		free(et->message);

	memset(et, '\0', sizeof(*et));
}

int PUBLIC NONNULL(1, 2, 5) PRINTF(5, 6)
efi_error_set(const char *filename,
	      const char *function,
	      int line,
	      int error,
	      const char *fmt, ...)
{
	error_table_entry et = { 0, };
	error_table_entry *table;
	char *tmp;

	table = realloc(error_table, sizeof(et) * (current +1));
	if (!table)
		goto err;
	error_table = table;

	et.error = error;
	et.line = line;
	tmp = filename ? strdup(filename) : NULL;
	if (!tmp)
		goto err;
	et.filename = tmp;

	tmp = function ? strdup(function) : NULL;
	if (!tmp)
		goto err;
	et.function = tmp;

	if (fmt) {
		int rc;
		int saved_errno;
		va_list ap;

		tmp = NULL;
		va_start(ap, fmt);
		rc = vasprintf(&tmp, fmt, ap);
		saved_errno = errno;
		va_end(ap);
		errno = saved_errno;
		if (rc < 0)
			goto err;
		et.message = tmp;
	}

	memcpy(&error_table[current], &et, sizeof(et));
	current += 1;
	return current;
err:
	if (et.filename)
		free(et.filename);
	if (et.function)
		free(et.function);
	if (et.message)
		free(et.message);
	errno = ENOMEM;
	return -1;
}

void PUBLIC
efi_error_pop(void)
{
	if (current <= 0)
		return;

	current -= 1;
	clear_error_entry(&error_table[current]);
}

static int efi_verbose;
static FILE *efi_errlog, *efi_dbglog;
static int efi_dbglog_fd = -1;
static int stashed_log_level;
static char efi_dbglog_buf[4096];

void PUBLIC
efi_stash_loglevel_(int level)
{
	stashed_log_level = level;
}

static ssize_t
dbglog_write(void *cookie UNUSED, const char *buf, size_t size)
{
	FILE *log = efi_errlog ? efi_errlog : stderr;
	ssize_t ret = size;

	if (efi_get_verbose() >= stashed_log_level) {
		ret = fwrite(buf, 1, size, log);
	} else if (efi_dbglog_fd >= 0) {
		lseek(efi_dbglog_fd, 0, SEEK_SET);
		write(efi_dbglog_fd, buf, size);
	}
	return ret;
}

static int
dbglog_seek(void *cookie UNUSED, off64_t *offset, int whence)
{
	FILE *log = efi_errlog ? efi_errlog : stderr;
	return fseek(log, *offset, whence);
}

static int
dbglog_close(void *cookie UNUSED)
{
	if (efi_dbglog_fd >= 0) {
		close(efi_dbglog_fd);
		efi_dbglog_fd = -1;
	}
	if (efi_errlog) {
		int ret = fclose(efi_errlog);
		efi_errlog = NULL;
		return ret;
	}

	errno = EBADF;
	return -1;
}

void PUBLIC
efi_error_clear(void)
{
	if (error_table) {
		for (unsigned int i = 0; i < current; i++) {
			error_table_entry *et = &error_table[i];

			clear_error_entry(et);
		}
		free(error_table);
	}
	error_table = NULL;
	current = 0;
}

void DESTRUCTOR
efi_error_fini(void)
{
	efi_error_clear();
	if (efi_dbglog) {
		fclose(efi_dbglog);
		efi_dbglog = NULL;
	}
}

static void CONSTRUCTOR
efi_error_init(void)
{
	cookie_io_functions_t io_funcs = {
		.write = dbglog_write,
		.seek = dbglog_seek,
		.close = dbglog_close,
	};

	efi_dbglog_fd = memfd_create("efivar-debug.log", MFD_CLOEXEC);
	if (efi_dbglog_fd == -1)
		return;

	efi_dbglog = fopencookie(NULL, "a", io_funcs);
	if (efi_dbglog)
		setvbuf(efi_dbglog, efi_dbglog_buf, _IOLBF,
			sizeof(efi_dbglog_buf));
}

FILE PUBLIC *
efi_get_logfile(void)
{
	return efi_dbglog;
}

void PUBLIC
efi_set_verbose(int verbosity, FILE *errlog)
{
	efi_verbose = verbosity;
	if (errlog)
		efi_errlog = errlog;
}

int PUBLIC
efi_get_verbose(void)
{
	return efi_verbose;
}

// vim:fenc=utf-8:tw=75:noet
