// SPDX-License-Identifier: GPL-v3-or-later
/*
 * sbchooser-db.h - includes for sbchooser's db
 * Copyright Peter Jones <pjones@redhat.com>
 */

#pragma once

#include "sbchooser.h" // IWYU pragma: keep

int load_secdb_from_file(const char * const filename, efi_secdb_t **secdbp);
int load_secdb_from_var(const char * const name,
			const efi_guid_t * const guidp, efi_secdb_t **secdbp);
int parse_secdb_info(sbchooser_context_t *ctx);
void free_secdb_info(sbchooser_context_t *ctx);

// vim:fenc=utf-8:tw=75:noet
