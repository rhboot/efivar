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
	size_t n_certs;
	cert_data_t **certs;

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

	bool revoked;
	bool trusted;

	uint32_t score;
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
	digest_data_t sha384;
	digest_data_t sha512;

	/*
	 * each authenticode signature founds on this binary
	 */
	size_t n_sigs;
	sig_data_t **sigs;

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

	bool first_sig_only;	// should only the first signature be scored?

	uint32_t score;		// score for sorting
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
int load_pe(const char * const filename, pe_file_t **pe_file,
	    bool first_sig_only);

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

int generate_authenticode(pe_file_t *pe);

void score_pe(sbchooser_context_t *ctx, pe_file_t *pe);
int pe_cmp(const void *p0, const void *p1);

// vim:fenc=utf-8:tw=75:noet
