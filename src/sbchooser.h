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

#define OPENSSL_NO_DEPRECATED
#include <openssl/x509.h>

#include "efivar/efisec.h" // IWYU pragma: export

/*
 * exit status codes from sbchooser
 */
enum {
	ERR_SUCCESS = 0,
	ERR_USAGE,
	ERR_SECDB,
	ERR_BAD_PE,
	ERR_INPUT,
};

typedef struct sbchooser_context sbchooser_context_t;
typedef struct cert_data cert_data_t;

struct digest_data {
	uint8_t *data;
	size_t datasz;
};
typedef struct digest_data digest_data_t;

typedef struct pe_file pe_file_t;

#include "compiler.h" // IWYU pragma: export
#include "util.h" // IWYU pragma: export
#include "sbchooser-pe.h" // IWYU pragma: export
#include "sbchooser-db.h" // IWYU pragma: export
#include "sbchooser-x509.h" // IWYU pragma: export

/*
 * sbchooser's main context
 */
struct sbchooser_context {
	/*
	 * our input pe files
	 */
	size_t n_files;
	pe_file_t **files;

	efi_secdb_t *db;
	size_t n_db_digests;
	digest_data_t **db_digests;
	size_t n_db_certs;
	cert_data_t **db_certs;

	efi_secdb_t *dbx;
	size_t n_dbx_digests;
	digest_data_t **dbx_digests;
	size_t n_dbx_certs;
	cert_data_t **dbx_certs;
	// XXX PJFIX: support cert TBS hash revocations

	bool first_sig_only;	// should only the first signature be scored?
};

// vim:fenc=utf-8:tw=75:noet
