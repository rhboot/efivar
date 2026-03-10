// SPDX-License-Identifier: GPL-v3-or-later
/*
 * sbchooser-pe.h - includes for sbchooser's pe support
 * Copyright Peter Jones <pjones@redhat.com>
 */

#pragma once

#include "sbchooser.h" // IWYU pragma: keep
#include "peimage.h" // IWYU pragma: keep

/*
 * This is a cache of things from the image headers, so we don't have to
 * write this all over the place:
 *   if (is_64_bit_image(...) {
 *     x = pehdr->pe32plus.optional_header.x;
 *   } else {
 *     x = pehdr->pe32.optional_header.x;
 *  }
 */
typedef struct pe_image_context {
	efi_image_optional_header_union_t *pe_header; // the main pe header
	uint64_t size_of_image;		// optional_header.size_of_image
	unsigned long size_of_headers;	// optional_header.size_of_headers
	uint16_t number_of_sections;	// file_header.number_of_sections
	uint32_t section_alignment;	// optional_header.section_alignment
	uint64_t number_of_rva_and_sizes; // optional_header.number_of_rva_and_sizes
	uint16_t dll_characteristics;	// optional_header.dll_characteristics
	efi_image_section_header_t *first_section; // first section header
	efi_image_data_directory_t *sec_dir; // security directory
} pe_image_context_t;

struct sig_data {
	/*
	 * Stuff from OpenSSL that we need to free later.
	 */
	PKCS7 *p7;
	/*
	 * This is procured with PKCS7_get0_signers(), which
	 * ossl-guide-libraries-introduction(7ossl) claims means the p7
	 * object still owns it, and it should get freed when we free that.
	 *
	 * This doesn't actually happen - the contents get freed, but
	 * there's still 128 bytes of overhead that got allocated.
	 *
	 * openssl/apps/smime.c frees these with sk_X509_free().  That
	 * seems to work, and valgrind like it just fine, but I can't find
	 * any documentation of that at all.
	 */
	STACK_OF(X509) *x509s;

	size_t n_certs;
	cert_data_t **certs;

	bool trusted;		// sig is in db
	bool revoked;		// sig is in dbx

	/*
	 * validity info from this signature's signing certs
	 */
	uint32_t lowest_md_secbits;
	uint32_t lowest_pk_secbits;

	/*
	 * the earliest not_before and latest not_after validation date
	 * from our signature's issuers.
	 *
	 * Strictly this isn't necessary, but if everything has the same
	 * security strength, we'd prefer the "newest" binary, so we need
	 * some heuristic for that.
	 */
	const ASN1_TIME *earliest_not_before;
	const ASN1_TIME *latest_not_after;

	/*
	 * why was this revoked or trusted?  Borrowed from certs
	 */
	char *rationale;
};

typedef struct sig_data sig_data_t;

struct pe_file {
	char *filename;		// for display later
	void *map;		// where the file is mapped
	size_t mapsz;		// how big the map is
	pe_image_context_t ctx;	// context built from "loading" it.

	/*
	 * authenticode hashes of this binary, using different digest
	 * functions.
	 */
	digest_data_t sha256;
	bool sha256_revoked;
	bool sha256_trusted;

	digest_data_t sha384;
	bool sha384_revoked;
	bool sha384_trusted;

	digest_data_t sha512;
	bool sha512_revoked;
	bool sha512_trusted;

	/*
	 * each authenticode signature found on this binary
	 */
	size_t n_sigs;
	sig_data_t **sigs;

	/*
	 * information for sorting and reporting
	 */
	bool has_trusted_signature;
	uint32_t secbits;

	/*
	 * the earliest not_before and latest not_after validation date
	 * from our signature's issuers.
	 *
	 * Strictly this isn't necessary, but if everything has the same
	 * security strength, we'd prefer the "newest" binary, so we need
	 * some heuristic for that.
	 */
	const ASN1_TIME *earliest_not_before;
	const ASN1_TIME *latest_not_after;

	/*
	 * why was this revoked or trusted?  Borrowed from sigs
	 */
	char *rationale;

	bool first_sig_only;	// should only the first signature be scored?
};

/*
 * load_pe() allocates a pe_file_t, maps it,  and "loads" it, validating
 * the input as well as filling out the "context" structure.
 *
 * If "first_sig_only" is true, only the first signature will be scored and
 * used for sorting.
 *
 * returns 0 on success, negative on error
 */
int load_pe(sbchooser_context_t *ctx,
	    const char * const filename, pe_file_t **pe_file);

/*
 * free_pe() cleans up the pe_file_t, unmapping and freeing all associated
 * memory.
 */
void free_pe(pe_file_t **pe_file);

/*
 * finds the given numbered section...
 * inputs:
 *   pe - the pe_file_t that's been loaded
 *   section_num - the section number, 0-indexed
 * outputs:
 *   *basep - pointer to the section data
 *   *sizep - size of the section data
 *   *sectionp - pointer to the section header
 *
 * returns 0 on success, negative on failure
 */
int get_section_vma (pe_file_t *pe, unsigned int section_num,
		     char **basep, size_t *sizep,
		     efi_image_section_header_t **sectionp);

void fmt_digest(digest_data_t *dgst, char *buf, size_t bufsz);
int generate_authenticode(pe_file_t *pe);

/*
 * evaluate the security posture of the PE file provided, in the context of
 * the security databases in ctx.
 */
void update_pe_security(sbchooser_context_t *ctx, pe_file_t *pe);

bool is_revoked_by_hash(pe_file_t *pe, digest_data_t **revoking_digest);
bool is_trusted_by_hash(pe_file_t *pe, digest_data_t **trusting_digest);

/*
 * PE comparison function, suitable for use with qsort(3)
 * Lower return value is most desirable.
 */
int pe_cmp(const void *p0, const void *p1);

// vim:fenc=utf-8:tw=75:noet
