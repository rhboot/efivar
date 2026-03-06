// SPDX-License-Identifier: GPL-v3-or-later
/*
 * authenticode.c - implement the authenticode digest function
 * Copyright Peter Jones <pjones@redhat.com>
 */

#include "sbchooser.h" // IWYU pragma: keep

void
fmt_digest(digest_data_t *dgst, char *buf, size_t bufsz)
{
	size_t pos = 0;
	memset(buf, 0, bufsz);

	if (!dgst) {
		debug("dgst is NULL!");
		return;
	}

	if (!dgst->data || dgst->datasz == 0) {
		debug("dgst->data:%p dgst->datasz:%zu", dgst->data, dgst->datasz);
		return;
	}
	for (size_t i = 0; i < dgst->datasz; i++) {
		size_t rc;

		rc = snprintf(&buf[pos], bufsz - pos, "%02x", dgst->data[i]);
		if (rc == 0) {
			buf[pos] = '\0';
			break;
		}
		pos += rc;
	}
	buf[bufsz - 1] = '\0';
}

/*
 * most of this is just used as intermediates - when we're done computing
 * things, only digest_size and digest are really useful.
 */
struct digest_buffer {
	/*
	 * the openssl short name for the algorithm.  This is used to look
	 * up openssl's NID representation of the algorithm.
	 */
	char algorithm[256];
	int algorithm_nid;	// used to look up the algorithm
	EVP_MD_CTX *mdctx;	// openssl MD context
	digest_data_t dgst;	// output buffer
};
typedef struct digest_buffer digest_buffer_t;

static int
generate_authenticode_begin(digest_buffer_t **dbufs)
{
	digest_buffer_t *dbp = NULL;
	for (size_t i = 0; dbufs[i] != NULL; i++) {
		const EVP_MD *md;

		dbp = dbufs[i];

		dbp->mdctx = EVP_MD_CTX_new();
		if (!dbp->mdctx) {
			warnx("Could not create message digest context");
			goto err;
		}

		dbp->algorithm_nid = OBJ_sn2nid(dbp->algorithm);
		if (!strcmp(dbp->algorithm, "SHA256")) {
			md = EVP_sha256();
		} else if (!strcmp(dbp->algorithm, "SHA384")) {
			md = EVP_sha384();
		} else if (!strcmp(dbp->algorithm, "SHA512")) {
			md = EVP_sha512();
		} else {
			warnx("Unsupported digest \"%s\"", dbp->algorithm);
			continue;
		}
		dbp->dgst.datasz = EVP_MD_get_size(md);
		dbp->dgst.data = calloc(1, dbp->dgst.datasz);
		if (!dbp->dgst.data) {
			warn("Couldn't allocate digest buffer");
			goto err;
		}
		if (!EVP_DigestInit(dbp->mdctx, md)) {
			warnx("Couldn't initialize digest %s", dbp->algorithm);
			goto err;
		}
	}
	return 0;

err:
	for (size_t i = 0; dbufs[i] != NULL; i++) {
		dbp = dbufs[i];

		if (dbp->mdctx) {
			EVP_MD_CTX_free(dbp->mdctx);
			dbp->mdctx = NULL;
		}
		dbp->dgst.datasz = 0;
		if (dbp->dgst.data) {
			free(dbp->dgst.data);
			dbp->dgst.data = NULL;
		}
	}
	return -1;

}

static void
update_all_hashes(digest_buffer_t **dbufs, void *data, size_t size)
{
	for (size_t i = 0; dbufs[i] != NULL; i++) {
		digest_buffer_t *dbp = dbufs[i];

		if (dbp->mdctx == NULL)
			continue;

		EVP_DigestUpdate(dbp->mdctx, data, size);
	}
}

static int
generate_authenticode_digest(pe_file_t *pe, digest_buffer_t **dbufs)
{
	char *hashbase;
	unsigned int hashsize;

	unsigned int sum_of_bytes_hashed;
	unsigned int sum_of_section_bytes;
	unsigned int index;
	unsigned int pos;

	efi_image_section_header_t *section;
	efi_image_section_header_t *section_header = NULL;

	pe_image_context_t *ctx = &pe->ctx;

	errno = EINVAL;

	// hash start to checksum
	hashbase = pe->map;
	hashsize = (char *)&ctx->pe_header->pe32.optional_header.checksum - hashbase;
	update_all_hashes(dbufs, hashbase, hashsize);

	// hash post-checksum to start of cert table
	hashbase = (char *)&ctx->pe_header->pe32.optional_header.checksum + sizeof (int);
	hashsize = (char *)ctx->sec_dir - hashbase;
	update_all_hashes(dbufs, hashbase, hashsize);

	// hash end of cert table to end of image header
	efi_image_data_directory_t *dd = ctx->sec_dir + 1;
	hashbase = (char *)dd;
	hashsize = ctx->size_of_headers - (unsigned long)((char *)dd - (char *)pe->map);
	if (hashsize > pe->mapsz) {
		warnx("Data directory is invalid");
		goto err;
	}
	update_all_hashes(dbufs, hashbase, hashsize);

	// sort sections...
	sum_of_bytes_hashed = ctx->size_of_headers;

	section_header = calloc(ctx->number_of_sections, sizeof (efi_image_section_header_t));
	if (!section_header)
		goto err;

	/*
	 * validate section locations and sizes, and sort the table into
	 * our newly allocated copy
	 */
	sum_of_section_bytes = 0;
	section = ctx->first_section;
	for (index = 0; index < ctx->number_of_sections; index++) {
		efi_image_section_header_t *secp;
		char *base;
		size_t size;
		int rc;

		rc = get_section_vma(pe, index, &base, &size, &secp);
		if (rc < 0) {
			if (errno == ENOENT) {
				break;
			} else {
				warnx("Malformed section header");
				goto err;
			}
		}

		// validate section size is within image
		if (secp->size_of_raw_data >
		    pe->mapsz - sum_of_bytes_hashed - sum_of_section_bytes) {
			warnx("Malformed section %d size", index);
			errno = EINVAL;
			goto err;
		}
		sum_of_section_bytes += secp->size_of_raw_data;

		pos = index;
		while ((pos > 0) &&
		       (section->pointer_to_raw_data < section_header[pos - 1].pointer_to_raw_data)) {
			memcpy(&section_header[pos], &section_header[pos - 1],
			       sizeof(efi_image_section_header_t));
			pos--;
		}
		memcpy(&section_header[pos], section, sizeof (efi_image_section_header_t));
		section += 1;
	}
	errno = EINVAL;

	// hash the sections
	for (index = 0; index < ctx->number_of_sections; index++) {
		section = &section_header[index];
		if (section->size_of_raw_data == 0) {
			continue;
		}

		hashbase = pe->map + section->pointer_to_raw_data;

		if (section->size_of_raw_data >
		    pe->mapsz - section->pointer_to_raw_data) {
			warnx("Malformed section %u raw size", index);
			goto err;
		}
		hashsize = (unsigned int)section->size_of_raw_data;
		update_all_hashes(dbufs, hashbase, hashsize);

		sum_of_bytes_hashed += section->size_of_raw_data;
	}

	// hash all remaining data up to sec_dir if sec_dir->size is not 0
	if (pe->mapsz > sum_of_bytes_hashed && ctx->sec_dir->size) {
		hashbase = pe->map + sum_of_bytes_hashed;
		hashsize = pe->mapsz - ctx->sec_dir->size - sum_of_bytes_hashed;

		update_all_hashes(dbufs, hashbase, hashsize);

		sum_of_bytes_hashed += hashsize;
	}

	/*
	 * Hash all remaining data. If sec_dir->size is > 0 this code should
	 * not be entered.  If it is, there are still things to hash.  For
	 * a file without a sec_dir, we need to hash what remains.
	 */
	if (pe->mapsz > sum_of_bytes_hashed + ctx->sec_dir->size) {
		debug("sobh:%zu mapsz:%zu secdir size:%zu", sum_of_bytes_hashed, pe->mapsz, ctx->sec_dir->size);

		hashbase = pe->map + sum_of_bytes_hashed;
		hashsize = pe->mapsz - sum_of_bytes_hashed - ctx->sec_dir->size;

		update_all_hashes(dbufs, hashbase, hashsize);
		sum_of_bytes_hashed += hashsize;
		if (sum_of_bytes_hashed % 8 != 0) {
			char padbuf[8];

			memset(padbuf, 0, sizeof(padbuf));
			hashsize = ALIGNMENT_PADDING(sum_of_bytes_hashed, 8);
			update_all_hashes(dbufs, padbuf, hashsize);
		}
	}

	free(section_header);
	return 0;
err:
	if (section_header)
		free(section_header);
	return -1;
}

static int
generate_authenticode_final(digest_buffer_t **dbufs)
{
	for (size_t i = 0; dbufs[i] != NULL; i++) {
		digest_buffer_t *dbp = dbufs[i];

		EVP_DigestFinal(dbp->mdctx, dbp->dgst.data, NULL);
		EVP_MD_CTX_free(dbp->mdctx);
		dbp->mdctx = NULL;
	}
	return 0;
}

int
generate_authenticode(pe_file_t *pe)
{
	int rc;

	digest_buffer_t sha256_dbuf = {
		.algorithm = "SHA256",
		.algorithm_nid = -1,
		.dgst = {
			.data = NULL,
			.datasz = 0,
		},
		.mdctx = NULL,
	};
	digest_buffer_t sha384_dbuf = {
		.algorithm = "SHA384",
		.algorithm_nid = -1,
		.dgst = {
			.data = NULL,
			.datasz = 0,
		},
		.mdctx = NULL,
	};
	digest_buffer_t sha512_dbuf = {
		.algorithm = "SHA512",
		.algorithm_nid = -1,
		.dgst = {
			.data = NULL,
			.datasz = 0,
		},
		.mdctx = NULL,
	};
	digest_buffer_t *dbufs[] = {
		&sha256_dbuf,
		&sha384_dbuf,
		&sha512_dbuf,
		NULL
	};
	rc = generate_authenticode_begin(dbufs);
	if (rc < 0)
		goto err;

	rc = generate_authenticode_digest(pe, dbufs);
	if (rc < 0)
		goto err;

	rc = generate_authenticode_final(dbufs);
	if (rc < 0)
		goto err;

	for (size_t i = 0; dbufs[i] != NULL; i++) {
		digest_buffer_t *dbp = dbufs[i];

		if (!strcmp(dbp->algorithm, "SHA256")) {
			pe->sha256.data = dbp->dgst.data;
			pe->sha256.datasz = dbp->dgst.datasz;
		} else if (!strcmp(dbp->algorithm, "SHA384")) {
			pe->sha384.data = dbp->dgst.data;
			pe->sha384.datasz = dbp->dgst.datasz;
		} else if (!strcmp(dbp->algorithm, "SHA512")) {
			pe->sha512.data = dbp->dgst.data;
			pe->sha512.datasz = dbp->dgst.datasz;
		}
		EVP_MD_CTX_free(dbp->mdctx);
	}

	if (efi_get_verbose() >= DEBUG_LEVEL) {
		for (size_t i = 0; dbufs[i] != NULL; i++) {
			char buf[1024];
			digest_buffer_t *dbp = dbufs[i];

			fmt_digest(&dbp->dgst, buf, 1024);
			fprintf(efi_get_logfile(), "%s:%s\n",
				dbp->algorithm, buf);
		}
	}

	return 0;
err:
	for (size_t i = 0; dbufs[i] != NULL; i++) {
		digest_buffer_t *dbp = dbufs[i];

		dbp->dgst.datasz = 0;
		if (dbp->dgst.data != NULL) {
			free(dbp->dgst.data);
			dbp->dgst.data = NULL;
		}
		if (dbp->mdctx != NULL) {
			EVP_MD_CTX_free(dbp->mdctx);
			dbp->mdctx = NULL;
		}
	}

	return -1;
}

// vim:fenc=utf-8:tw=75:noet
