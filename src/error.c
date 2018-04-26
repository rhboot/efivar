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

void PUBLIC DESTRUCTOR
efi_error_clear(void)
{
	if (error_table) {
		for (unsigned int i = 0; i < current; i++) {
			error_table_entry *et = &error_table[i];

			if (et->filename)
				free(et->filename);
			if (et->function)
				free(et->function);
			if (et->message)
				free(et->message);

			memset(et, '\0', sizeof(*et));
		}
		free(error_table);
	}
	error_table = NULL;
	current = 0;
}

static int efi_verbose;
static FILE *efi_errlog;

FILE PUBLIC *
efi_get_logfile(void)
{
	if (efi_errlog)
		return efi_errlog;
	return stderr;
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
