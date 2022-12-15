// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2019 Red Hat, Inc.
 * Copyright (C) 2000-2001 Dell Computer Corporation <Matt_Domsch@dell.com>
 */

#include "fix_coverity.h"

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/random.h>
#include <unistd.h>

#include "efiboot.h"

typedef struct {
	int error;
	char *filename;
	char *function;
	int line;
	char *message;
} error_table_entry;

static _Thread_local error_table_entry *error_table;
static _Thread_local unsigned int current;

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
#ifndef ANDROID
static int efi_dbglog_fd = -1;
static intptr_t efi_dbglog_cookie;
#endif
static int log_level;

void PUBLIC
efi_set_loglevel(int level)
{
	log_level = level;
}

#ifndef ANDROID
static ssize_t
dbglog_write(void *cookie, const char *buf, size_t size)
{
	FILE *log = efi_errlog ? efi_errlog : stderr;
	ssize_t ret = 0;

	while (ret < (ssize_t)size) {
		/*
		 * This is limited to 32 characters per write because if
		 * the only place this is going is strace logs, that's the
		 * default character buffer display size.  If it's going
		 * anywhere else, you won't really notice the difference,
		 * since we're not inserting newlines.
		 */
		ssize_t sz = MIN(size - ret, 32);

		if (efi_get_verbose() >= log_level) {
			sz = fwrite(buf + ret, 1, sz, log);
			if (sz < 1 && (ferror(log) || feof(log)))
				break;
			fflush(log);
		} else if (efi_dbglog_fd >= 0 && sz > 0) {
			if ((intptr_t)cookie != 0 &&
			    (intptr_t)cookie == efi_dbglog_cookie &&
			    (ret + sz) < 0 &&
			    buf[ret + sz - 1] == '\n')
				sz -= 1;
			sz = write(efi_dbglog_fd, buf + ret, sz);
			if (sz < 0)
				break;
		}
		ret += sz;
	}
	return ret;
}

static int
dbglog_seek(void *cookie UNUSED, off_t *offset, int whence)
{
	FILE *log = efi_errlog ? efi_errlog : stderr;
	int rc;

	rc = fseek(log, *offset, whence);
	if (rc < 0)
		return rc;
	*offset = ftell(log);
	return 0;
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
#endif

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
#ifndef ANDROID
	ssize_t bytes;
	cookie_io_functions_t io_funcs = {
		.write = dbglog_write,
		.seek = dbglog_seek,
		.close = dbglog_close,
	};

	efi_dbglog_fd = open("/dev/null", O_WRONLY|O_APPEND|O_CLOEXEC);
	if (efi_dbglog_fd < 0)
		return;

	bytes = getrandom(&efi_dbglog_cookie, sizeof(efi_dbglog_cookie), 0);
	if (bytes < (ssize_t)sizeof(efi_dbglog_cookie))
		efi_dbglog_cookie = 0;

	efi_dbglog = fopencookie((void *)efi_dbglog_cookie, "a", io_funcs);
#endif
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
