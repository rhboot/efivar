// SPDX-License-Identifier: GPL-v3-or-later
/*
 * sbchooser-db.c - handling of the UEFI key database for sbchooser
 * Copyright Peter Jones <pjones@redhat.com>
 */

#include "sbchooser.h" // IWYU pragma: keep

void
free_secdb_info(sbchooser_context_t *ctx)
{
	if (ctx->db) {
		efi_secdb_free(ctx->db);
		ctx->db = NULL;
	}

	if (ctx->dbx) {
		efi_secdb_free(ctx->dbx);
		ctx->dbx = NULL;
	}
}

int
load_secdb_from_file(const char * const filename, efi_secdb_t **secdbp)
{
	int rc;
	uint8_t *data = NULL;
	size_t data_size = 0;
	int fd;

	fd = open(filename, O_RDONLY|O_CLOEXEC);
	if (fd < 0) {
		efi_error("Could not open file \"%s\": %m", filename);
		return fd;
	}

	rc = read_file(fd, &data, &data_size);
	close(fd);
	if (rc < 0) {
		efi_error("Could not read file \"%s\": %m", filename);
		return fd;
	}
	data_size -= 1;

	rc = efi_secdb_parse(data, data_size, secdbp);
	free(data);
	if (rc < 0) {
		efi_error("Could not parse security database \"%s\"", filename);
		return rc;
	}

	return 0;
}

int
load_secdb_from_var(const char * const name, const efi_guid_t * const guidp,
		    efi_secdb_t **secdbp)
{
	uint8_t *data;
	size_t data_size;
	uint32_t attrs;
	int rc;

	if (!efi_variables_supported())
		return -1;

	rc = efi_get_variable(*guidp, name, &data, &data_size, &attrs);
	if (rc < 0) {
		efi_error("Could not get variable \"%s\"", name);
		return rc;
	}

	rc = efi_secdb_parse(data, data_size, secdbp);
	free(data);
	if (rc < 0) {
		efi_error("Could not parse security database \"%s\"", name);
		return rc;
	}

	return 0;
}

// vim:fenc=utf-8:tw=75:noet
