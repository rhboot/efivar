// SPDX-License-Identifier: GPL-v3-or-later
/*
 * sbchooser-db.c - handling of the UEFI key database for sbchooser
 * Copyright Peter Jones <pjones@redhat.com>
 */

#include "sbchooser.h" // IWYU pragma: keep

struct db_parse_context {
	/*
	 * true=db
	 * false=dbx
	 */
	bool db;
	sbchooser_context_t *ctx;
};

static int
add_cert(sbchooser_context_t *ctx, bool db,
	 const efi_secdb_data_t * const data, const size_t datasz)
{
	cert_data_t *cert = NULL;
	cert_data_t **new_certs = NULL;
	cert_data_t ***old_certsp = NULL;
	size_t n_certs = 0;
	size_t *n_certsp = NULL;
	int rc;

	if (db) {
		n_certs = ctx->n_db_certs;
		n_certsp = &ctx->n_db_certs;
		old_certsp = &ctx->db_certs;
	} else {
		n_certs = ctx->n_dbx_certs;
		n_certsp = &ctx->n_dbx_certs;
		old_certsp = &ctx->dbx_certs;
	}

	/*
	 * We don't update ctx->n_XXX_certs until the end, so we can
	 * avoid stacked cleanup.
	 */
	new_certs = reallocarray(*old_certsp, n_certs + 1, sizeof (cert_data_t));
	if (!new_certs)
		return -1;

	*old_certsp = new_certs;

	cert = calloc(1, sizeof(*cert));
	if (!cert)
		return -1;

	cert->x509 = d2i_X509(NULL, (const unsigned char **)&data, datasz);
	cert->free_x509 = true;
	debug("alloc cert->x509:%p\n", cert->x509);
	if (!cert->x509) {
		// PJFIX: report errors better
		warnx("couldn't make new X509");
		return -1;
	}

	rc = elaborate_x509_info(cert);
	if (rc < 0) {
		memset(cert, 0, sizeof(*cert));
		free(cert);
		return rc;
	}

	new_certs[n_certs] = cert;
	*n_certsp = n_certs + 1;
	return 0;
}

static int
add_digest(sbchooser_context_t *ctx, bool db,
	   const efi_secdb_data_t * const data, const size_t datasz)
{
	digest_data_t *digest = NULL;
	digest_data_t **new_digests = NULL;
	digest_data_t ***old_digestsp = NULL;
	size_t n_digests = 0;
	size_t *n_digestsp = NULL;

	if (db) {
		n_digests = ctx->n_db_digests;
		n_digestsp = &ctx->n_db_digests;
		old_digestsp = &ctx->db_digests;
	} else {
		n_digests = ctx->n_dbx_digests;
		n_digestsp = &ctx->n_dbx_digests;
		old_digestsp = &ctx->dbx_digests;
	}

	/*
	 * We don't update ctx->n_XXX_digests until the end, so we can
	 * avoid stacked cleanup.
	 */
	debug("old_digestsp:%p *old_digestsp:%p, n_digests:%zu, sizeof(digest_data_t):%zu\n",
	      old_digestsp, *old_digestsp, n_digests, sizeof(digest_data_t));
	new_digests = reallocarray(*old_digestsp, n_digests + 1, sizeof (digest_data_t));
	if (!new_digests)
		return -1;

	*old_digestsp = new_digests;

	digest = calloc(1, sizeof(*digest) + datasz);
	if (!digest)
		return -1;

	digest->data = (uint8_t *)((uintptr_t)digest + sizeof(*digest));
	memcpy(digest->data, data, datasz);
	digest->datasz = datasz;

	new_digests[n_digests] = digest;
	*n_digestsp = n_digests + 1;
	return 0;
}

static efi_secdb_visitor_status_t
parse_one_secdb_cert(unsigned int listnum UNUSED,
		     unsigned int signum UNUSED,
		     const efi_guid_t * const owner UNUSED,
		     const efi_secdb_type_t algorithm,
		     const void * const header UNUSED,
		     const size_t headersz UNUSED,
		     const efi_secdb_data_t * const data,
		     const size_t datasz,
		     void *ctxp)
{
	int rc;
	struct db_parse_context *dbctx = ctxp;
	sbchooser_context_t *ctx = dbctx->ctx;

	switch (algorithm) {
	case EFI_SECDB_TYPE_SHA1:
	case EFI_SECDB_TYPE_SHA224:
	case EFI_SECDB_TYPE_SHA256:
	case EFI_SECDB_TYPE_SHA384:
	case EFI_SECDB_TYPE_SHA512:
		rc = add_digest(ctx, dbctx->db, data, datasz);
		if (rc < 0)
			return EFI_SECDB_VISITOR_ERROR;
		return EFI_SECDB_VISITOR_CONTINUE;
	case EFI_SECDB_TYPE_RSA2048:
	case EFI_SECDB_TYPE_RSA2048_SHA1:
	case EFI_SECDB_TYPE_RSA2048_SHA256:
	case EFI_SECDB_TYPE_X509_SHA256:
	case EFI_SECDB_TYPE_X509_SHA384:
	case EFI_SECDB_TYPE_X509_SHA512:
		return EFI_SECDB_VISITOR_CONTINUE;
	case EFI_SECDB_TYPE_X509_CERT:
		rc = add_cert(ctx, dbctx->db, data, datasz);
		if (rc < 0)
			return EFI_SECDB_VISITOR_ERROR;
		return EFI_SECDB_VISITOR_CONTINUE;
	default:
		debug("unknown algorithm %u\n", algorithm);
		return EFI_SECDB_VISITOR_CONTINUE;
	}

	return EFI_SECDB_VISITOR_CONTINUE;
}

int
parse_secdb_info(sbchooser_context_t *ctx)
{
	int rc;
	struct db_parse_context dbctx = {
		.db = false,
		.ctx = ctx,
	};

	dbctx.db = true;
	rc = efi_secdb_visit_entries(ctx->db, parse_one_secdb_cert, &dbctx);
	if (rc < 0) {
		warnx("couldn't visit them all?");
		return rc;
	}

	dbctx.db = false;
	rc = efi_secdb_visit_entries(ctx->dbx, parse_one_secdb_cert, &dbctx);
	if (rc < 0) {
		warnx("couldn't visit them all?");
		return rc;
	}

	return 0;
}

void
free_secdb_info(sbchooser_context_t *ctx)
{
	for (size_t i = 0; i < ctx->n_db_digests; i++) {
		digest_data_t *dgst = ctx->db_digests[i];

		free(dgst);
		ctx->db_digests[i] = NULL;
	}
	free(ctx->db_digests);
	ctx->db_digests = NULL;
	ctx->n_db_digests = 0;

	for (size_t i = 0; i < ctx->n_db_certs; i++) {
		free_cert(ctx->db_certs[i]);
		ctx->db_certs[i] = NULL;
	}
	free(ctx->db_certs);
	ctx->db_certs = NULL;
	ctx->n_db_certs = 0;

	for (size_t i = 0; i < ctx->n_dbx_digests; i++) {
		digest_data_t *dgst = ctx->dbx_digests[i];

		free(dgst);
		ctx->dbx_digests[i] = NULL;
	}
	free(ctx->dbx_digests);
	ctx->dbx_digests = NULL;
	ctx->n_dbx_digests = 0;

	for (size_t i = 0; i < ctx->n_dbx_certs; i++) {
		free_cert(ctx->dbx_certs[i]);
		ctx->dbx_certs[i] = NULL;
	}

	if (ctx->db) {
		efi_secdb_free(ctx->db);
		ctx->db = NULL;
	}

	if (ctx->dbx) {
		efi_secdb_free(ctx->dbx);
		ctx->dbx = NULL;
	}

	free(ctx->dbx_certs);
	ctx->dbx_certs = NULL;
	ctx->n_dbx_certs = 0;
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
