// SPDX-License-Identifier: GPL-v3-or-later
/*
 * sbchooser.h - includes for sbchooser
 * Copyright Peter Jones <pjones@redhat.com>
 */

#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif /* defined(_GNU_SOURCE) */

#include <err.h>
#include <errno.h> // IWYU pragma: keep
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h> // IWYU pragma: keep
#include <stdlib.h> // IWYU pragma: keep
#include <string.h> // IWYU pragma: keep
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * exit status codes from sbchooser
 */
enum {
	ERR_SUCCESS = 0,
	ERR_USAGE,
	ERR_BAD_PE,
};

typedef struct pe_file pe_file_t;

#include "compiler.h" // IWYU pragma: export
#include "util.h" // IWYU pragma: export
#include "sbchooser-pe.h" // IWYU pragma: export
#include "sbchooser-db.h" // IWYU pragma: export
#include "sbchooser-x509.h" // IWYU pragma: export

// vim:fenc=utf-8:tw=75:noet
