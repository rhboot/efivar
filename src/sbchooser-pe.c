// SPDX-License-Identifier: GPL-v3-or-later
/*
 * sbchooser-pe.c - pe support for sbchooser
 * Copyright Peter Jones <pjones@redhat.com>
 */

#include "sbchooser.h" // IWYU pragma: keep
#include <openssl/sha.h>

void
free_pe(pe_file_t **pe_p)
{
	pe_file_t *pe;
	int rc;

	if (!pe_p)
		return;
	pe = *pe_p;

	if (pe->filename)
		free(pe->filename);

	if (pe->map && pe->mapsz) {
		rc = munmap(pe->map, pe->mapsz);
		if (rc < 0)
			warn("munmap(%p, %zu) failed", pe->map, pe->mapsz);
	}

	if (pe->sha256.data) {
		free(pe->sha256.data);
		pe->sha256.data = NULL;
		pe->sha256.datasz = 0;
	}
	if (pe->sha384.data) {
		free(pe->sha384.data);
		pe->sha384.data = NULL;
		pe->sha384.datasz = 0;
	}
	if (pe->sha512.data) {
		free(pe->sha512.data);
		pe->sha512.data = NULL;
		pe->sha512.datasz = 0;
	}

	memset(pe, 0, sizeof(*pe));
	free(pe);
	*pe_p = NULL;
}

static bool
image_is_64_bit(efi_image_optional_header_union_t *pehdr)
{
	/* .Magic is the same offset in all cases */
	if (pehdr->pe32plus.optional_header.magic ==
	    EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC)
		return true;
	return false;
}

int
load_pe(const char * const filename, pe_file_t **pe_p)
{
	int ret = -1;
	pe_file_t *pe = NULL;
	int fd = -1;
	int rc;
	struct stat statbuf;
	efi_image_dos_header_t *doshdr = NULL;
	efi_image_optional_header_union_t *pehdr = NULL;
	pe_image_context_t *ctx = NULL;
	unsigned long opt_header_size;
	unsigned long file_alignment = 0;
	size_t page_size = sysconf(_SC_PAGE_SIZE);

	if (!filename || !pe_p) {
		errno = EINVAL;
		goto err;
	}

	debug("loading PE from \"%s\"", filename);

	fd = open(filename, O_RDONLY|O_CLOEXEC);
	if (fd < 0)
		goto err;

	rc = fstat(fd, &statbuf);
	if (rc < 0)
		goto err;

	pe = calloc(1, sizeof (*pe));
	if (!pe)
		goto err;

	ctx = &pe->ctx;
	pe->mapsz = statbuf.st_size;
	pe->map = mmap(NULL, pe->mapsz, PROT_READ, MAP_SHARED, fd, 0);
	if (pe->map == MAP_FAILED)
		err(ERR_BAD_PE, "Could not map \"%s\"", filename);
	close(fd);
	fd = -1;

	pe->filename = strdup(filename);
	if (!pe->filename)
		goto err;

	if (pe->mapsz < sizeof(*pehdr) + sizeof(*doshdr) + sizeof(pehdr->pe32)) {
		debug("PE file is too small (%zu bytes < %zu bytes", pe->mapsz,
		      sizeof(*pehdr) + sizeof(*doshdr) + sizeof(pehdr->pe32));
		goto err;
	}

	doshdr = (efi_image_dos_header_t *) pe->map;
	if (doshdr->e_magic != EFI_IMAGE_DOS_SIGNATURE) {
		debug("PE Image has bad DOS signature %02x%02x",
		      (doshdr->e_magic & 0xff00) >> 8,
		      (doshdr->e_magic & 0x00ff));
		goto err;
	}

	if (doshdr->e_lfanew >= pe->mapsz) {
		debug("PE Image has invalid PE header location 0x%x",
		      doshdr->e_lfanew);
		goto err;
	}

	pehdr = (efi_image_optional_header_union_t *)(void *)((uint8_t *)pe->map + doshdr->e_lfanew);
	if (pehdr->pe32.signature != EFI_IMAGE_NT_SIGNATURE) {
		debug("PE Image has invalid PE header signature 0x%08x",
		      pehdr->pe32.signature);
		goto err;
	}

	ctx->pe_header = pehdr;

	if (image_is_64_bit(pehdr)) {
		ctx->size_of_image =
			pehdr->pe32plus.optional_header.size_of_image;
		ctx->size_of_headers =
			pehdr->pe32plus.optional_header.size_of_headers;
		ctx->number_of_sections =
			pehdr->pe32plus.file_header.number_of_sections;
		ctx->section_alignment =
			pehdr->pe32plus.optional_header.section_alignment;
		ctx->number_of_rva_and_sizes =
			pehdr->pe32plus.optional_header.number_of_rva_and_sizes;
		ctx->dll_characteristics =
			pehdr->pe32plus.optional_header.dll_characteristics;
		file_alignment = pehdr->pe32plus.optional_header.file_alignment;
		opt_header_size = sizeof(efi_image_optional_header64_t);
	} else {
		ctx->size_of_image =
			(uint64_t)pehdr->pe32.optional_header.size_of_image;
		ctx->size_of_headers =
			pehdr->pe32.optional_header.size_of_headers;
		ctx->number_of_sections =
			pehdr->pe32.file_header.number_of_sections;
		ctx->section_alignment =
			pehdr->pe32.optional_header.section_alignment;
		ctx->number_of_rva_and_sizes =
			pehdr->pe32.optional_header.number_of_rva_and_sizes;
		ctx->dll_characteristics =
			pehdr->pe32.optional_header.dll_characteristics;
		file_alignment = pehdr->pe32.optional_header.file_alignment;
		opt_header_size = sizeof(efi_image_optional_header32_t);
	}

	errno = EINVAL;

	/*
	 * Set up our file alignment and section alignment expectations to
	 * be mostly sane.
	 *
	 * This probably should have a check for /power/ of two not just
	 * multiple, but in practice it hasn't been an issue.
	 */
	if (file_alignment % 2 != 0) {
		debug("File alignment is invalid (%lu)\n", file_alignment);
		goto err;
	}
	if (file_alignment == 0)
		file_alignment = 0x200;
	if (ctx->section_alignment == 0)
		ctx->section_alignment = page_size;
	if (ctx->section_alignment < file_alignment)
		ctx->section_alignment = file_alignment;

	/*
	 * Check and make sure the space for data directory entries is as
	 * large as we expect.
	 *
	 * In truth we could set this number smaller if we needed to -
	 * currently it's 16 but we only care about #4 and #5 (the fifth
	 * and sixth ones) - but it hasn't been a problem.  If it's too
	 * weird we'll fail trying to allocate it.
	 */
	if (EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES < ctx->number_of_rva_and_sizes) {
		debug("Invalid number of RVAs (%lu)", ctx->number_of_rva_and_sizes);
		goto err;
	}

	size_t tmpsz0 = 0;
	size_t tmpsz1 = 0;

	/*
	 * Check that the optional_headersize and the end of the Data
	 * Directory match up sanely
	 */
	unsigned long header_without_datadir = 0;
	unsigned long rva_sizes = 0;
	if (checked_mul(sizeof(efi_image_data_directory_t), EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES, &tmpsz0) ||
	    checked_sub(opt_header_size, tmpsz0, &header_without_datadir) ||
	    checked_sub((size_t)pehdr->pe32.file_header.size_of_optional_header, header_without_datadir, &tmpsz0) ||
	    checked_mul((size_t)ctx->number_of_rva_and_sizes, sizeof (efi_image_data_directory_t), &tmpsz1) ||
	    (tmpsz0 != tmpsz1)) {
		debug("RVA space isn't equal to leftover header space (%lu != %lu)",
		      rva_sizes, header_without_datadir);
		goto err;
	}

	/*
	 * Check that the SectionHeaderOffset field is within the image.
	 */
	unsigned long section_header_offset;
	if (checked_add((size_t)doshdr->e_lfanew, sizeof(uint32_t), &tmpsz0) ||
	    checked_add(tmpsz0, sizeof(efi_image_file_header_t), &tmpsz0) ||
	    checked_add(tmpsz0, pehdr->pe32.file_header.size_of_optional_header, &section_header_offset)) {
		debug("Image sections overflow image size");
		goto err;
	}

	/*
	 * Check that the sections headers themselves are within the image
	 */
	if (checked_sub((size_t)ctx->size_of_image, section_header_offset, &tmpsz0) ||
	    (tmpsz0 / EFI_IMAGE_SIZEOF_SECTION_HEADER <= ctx->number_of_sections)) {
		debug("Image sections overflow image size");
		goto err;
	}

	/*
	 * Check that the section headers fit within the total headers
	 */
	if (checked_sub((size_t)ctx->size_of_headers, section_header_offset, &tmpsz0) ||
	    (tmpsz0 / EFI_IMAGE_SIZEOF_SECTION_HEADER < (uint32_t)ctx->number_of_sections)) {
		debug("Image sections overflow section headers");
		goto err;
	}

	/*
	 * Check that the section headers are actually within the data
	 * we've read.  Might be duplicative of the size_of_image one, but
	 * it won't hurt.
	 */
	if (checked_mul((size_t)ctx->number_of_sections, sizeof(efi_image_section_header_t), &tmpsz0) ||
	    checked_add(tmpsz0, section_header_offset, &tmpsz0) ||
	    (tmpsz0 > pe->mapsz)) {
		debug("Image sections overflow section headers");
		goto err;
	}

	/*
	 * Check that the optional header fits in the image.
	 */
	if (checked_sub((size_t)(uintptr_t)pehdr, (size_t)(uintptr_t)pe->map, &tmpsz0) ||
	    checked_add(tmpsz0, sizeof(efi_image_optional_header_union_t), &tmpsz0) ||
	    (tmpsz0 > pe->mapsz)) {
		debug("Invalid image");
		goto err;
	}

	/*
	 * Check that this claims to be a PE binary
	 */
	if (pehdr->te.signature != EFI_IMAGE_NT_SIGNATURE) {
		debug("Unsupported image type");
		goto err;
	}

	/*
	 * Check that relocations aren't stripped, because that won't work.
	 */
	if (pehdr->pe32.file_header.characteristics & EFI_IMAGE_FILE_RELOCS_STRIPPED) {
		debug("Unsupported image - Relocations have been stripped");
		goto err;
	}

	/*
	 * We didn't load these earlier because we hadn't verified the size
	 * yet.
	 */
	if (image_is_64_bit(pehdr)) {
		ctx->sec_dir = &pehdr->pe32plus.optional_header.data_directory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY];
	} else {
		ctx->sec_dir = &pehdr->pe32.optional_header.data_directory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY];
	}

	/*
	 * Check that the file header fits within the image.
	 */
	if (checked_add((size_t)(uintptr_t)pehdr, pehdr->pe32.file_header.size_of_optional_header, &tmpsz0) ||
	    checked_add(tmpsz0, sizeof(uint32_t), &tmpsz0) ||
	    checked_add(tmpsz0, sizeof(efi_image_file_header_t), &tmpsz0)) {
		debug("Invalid image");
		goto err;
	}

	/*
	 * Check that the first section header is within the image data
	 */
	ctx->first_section = (efi_image_section_header_t *)(uintptr_t)tmpsz0;
	if ((uint64_t)(uintptr_t)(ctx->first_section) > (uint64_t)(uintptr_t)pe->map + pe->mapsz) {
		debug("Invalid image");
		goto err;
	}

	/*
	 * Check that the headers fit within the image.
	 */
	if (ctx->size_of_image < ctx->size_of_headers) {
		debug("Invalid image");
		goto err;
	}

	/*
	 * check that the data directory fits within the image.
	 */
	if (checked_sub((size_t)(uintptr_t)ctx->sec_dir, (size_t)(uintptr_t)pe->map, &tmpsz0) ||
	    (tmpsz0 > pe->mapsz - sizeof(efi_image_data_directory_t))) {
		debug("Invalid image");
		goto err;
	}

	/*
	 * Check that the certificate table is within the binary -
	 * "virtual_address" is a misnomer here, it's a relative offset to the
	 * image's load address, so compared to datasize it should be
	 * absolute.
	 */
	if (ctx->sec_dir->virtual_address > pe->mapsz ||
	    (ctx->sec_dir->virtual_address == pe->mapsz && ctx->sec_dir->size > 0)) {
		debug("ctx->sec_dir->virtual_address:0x%llx ctx->sec_dir->size:0x%llx datasize:0x%llx\n",
		      ctx->sec_dir->virtual_address, ctx->sec_dir->size, pe->mapsz);
		debug("Malformed security header");
		goto err;
	}

	rc = generate_authenticode(pe);
	if (rc < 0)
		goto err;

	*pe_p = pe;
	return 0;
err:
	{
		typeof(errno) error = errno;
		if (pe)
			free_pe(&pe);
		if (fd >= 0)
			close(fd);
		errno = error;
	}
	*pe_p = NULL;
	return ret;
}

int
get_section_vma (pe_file_t *pe, unsigned int section_num,
		 char **basep, size_t *sizep,
		 efi_image_section_header_t **sectionp)
{
	efi_image_section_header_t *sections = pe->ctx.first_section;
	efi_image_section_header_t *section;
	char *base = NULL, *end = NULL;

	if (section_num >= pe->ctx.number_of_sections) {
		errno = ENOENT;
		return -1;
	}

	errno = EINVAL;

	if (pe->ctx.first_section == NULL) {
		debug("Invalid section %u requested", section_num);
		goto err;
	}

	section = &sections[section_num];

	base = pe->map + section->virtual_address;
	end = pe->map + (section->virtual_address + section->misc.virtual_size - 1);

	if (!(section->characteristics & EFI_IMAGE_SCN_MEM_DISCARDABLE)) {
		if (!base) {
			debug("Section %u has invalid base address", section_num);
			goto err;
		}
		if (!end) {
			debug("Section %u has zero size", section_num);
			goto err;
		}
	}

	if (!(section->characteristics & EFI_IMAGE_SCN_CNT_UNINITIALIZED_DATA) &&
	    (section->virtual_address < pe->ctx.size_of_headers ||
	     section->pointer_to_raw_data < pe->ctx.size_of_headers)) {
		debug("Section %u is inside image headers", section_num);
		goto err;
	}

	if (end < base) {
		debug("Section %u has negative size", section_num);
		goto err;
	}

	*basep = base;
	*sizep = end - base;
	*sectionp = section;
	return 0;
err:
	return -1;
}

static bool
is_revoked_by_hash(sbchooser_context_t *ctx, pe_file_t *pe)
{
	debug("%zu digests in dbx\n", ctx->n_dbx_digests);
	for (size_t i = 0; i < ctx->n_dbx_digests; i++) {
		digest_data_t *dgst = ctx->dbx_digests[i];

		//debug("digest size %d", dgst->datasz);

		if (dgst->datasz == pe->sha512.datasz &&
		    memcmp(dgst->data, pe->sha512.data, dgst->datasz) == 0)
			return true;

		if (dgst->datasz == pe->sha384.datasz &&
		    memcmp(dgst->data, pe->sha384.data, dgst->datasz) == 0)
			return true;

		if (dgst->datasz == pe->sha256.datasz &&
		    memcmp(dgst->data, pe->sha256.data, dgst->datasz) == 0)
			return true;
	}

	return false;
}

#define PE_SCORE_SHA512_IN_DB		0x00000004ull
#define PE_SCORE_SHA384_IN_DB		0x00000002ull
#define PE_SCORE_SHA256_IN_DB		0x00000001ull
#define PE_SCORE_HASH_IN_DB_MASK	0x00000007ull
#define PE_SCORE_HASH_IN_DB_SHIFT	28ull

static void
assign_db_hash_score(sbchooser_context_t *ctx, pe_file_t *pe)
{
	for (size_t i = 0; i < ctx->n_db_digests; i++) {
		digest_data_t *dgst = ctx->db_digests[i];

		if ((dgst->datasz == SHA256_DIGEST_LENGTH &&
		     (pe->score & (PE_SCORE_SHA256_IN_DB << PE_SCORE_HASH_IN_DB_MASK))) ||
		    (dgst->datasz == SHA384_DIGEST_LENGTH &&
		     (pe->score & (PE_SCORE_SHA384_IN_DB << PE_SCORE_HASH_IN_DB_MASK))) ||
		    (dgst->datasz == SHA512_DIGEST_LENGTH &&
		     (pe->score & (PE_SCORE_SHA512_IN_DB << PE_SCORE_HASH_IN_DB_MASK))))
			continue;

		if (efi_get_verbose() >= DEBUG_LEVEL) {
			fprintf(stderr, "  dgst:");
			for (size_t j = 0; j < dgst->datasz; j++)
				fprintf(stderr, "%02x", dgst->data[j]);
			fprintf(stderr, "\n");
		}

		if (efi_get_verbose() >= DEBUG_LEVEL &&
		    dgst->datasz == pe->sha512.datasz) {
			fprintf(stderr, "sha512:");
			for (size_t j = 0; j < pe->sha512.datasz; j++)
				fprintf(stderr, "%02x", pe->sha512.data[j]);
			fprintf(stderr, "\n");
		}

		if (efi_get_verbose() >= DEBUG_LEVEL &&
		    dgst->datasz == pe->sha384.datasz) {
			fprintf(stderr, "sha384:");
			for (size_t j = 0; j < pe->sha384.datasz; j++)
				fprintf(stderr, "%02x", pe->sha384.data[j]);
			fprintf(stderr, "\n");
		}

		if (efi_get_verbose() >= DEBUG_LEVEL &&
		    dgst->datasz == pe->sha256.datasz) {
			fprintf(stderr, "sha256:");
			for (size_t j = 0; j < pe->sha256.datasz; j++)
				fprintf(stderr, "%02x", pe->sha256.data[j]);
			fprintf(stderr, "\n");
		}

		if (dgst->datasz == pe->sha512.datasz &&
		    !(pe->score & PE_SCORE_SHA512_IN_DB) &&
		    memcmp(dgst->data, pe->sha512.data, dgst->datasz) == 0) {
			debug("sha512 hash is in db");
			pe->score |= PE_SCORE_SHA512_IN_DB << PE_SCORE_HASH_IN_DB_SHIFT;
		}

		if (dgst->datasz == pe->sha384.datasz &&
		    !(pe->score & PE_SCORE_SHA384_IN_DB) &&
		    memcmp(dgst->data, pe->sha384.data, dgst->datasz) == 0) {
			debug("sha384 hash is in db");
			pe->score |= PE_SCORE_SHA384_IN_DB << PE_SCORE_HASH_IN_DB_SHIFT;
		}

		if (dgst->datasz == pe->sha256.datasz &&
		    !(pe->score & PE_SCORE_SHA256_IN_DB) &&
		    memcmp(dgst->data, pe->sha256.data, dgst->datasz) == 0) {
			debug("sha256 hash is in db");
			pe->score |= PE_SCORE_SHA256_IN_DB << PE_SCORE_HASH_IN_DB_SHIFT;
		}
	}
}

void
score_pe(sbchooser_context_t *ctx, pe_file_t *pe)
{
	debug("scoring \"%s\"", pe->filename);
	/*
	 * UEFI spec 2.12ish says:
	 *   – C. Any entry with SignatureListType of EFI_CERT_X509_GUID,
	 *     with SignatureData which contains a certificate with the
	 *     same Issuer, Serial Number, and To-Be-Signed hash included
	 *     in any certificate in the signing chain of the signature
	 *     being verified.
	 *
	 *     Multiple signatures are allowed to exist in the binary's
	 *     certificate table (as per the "Attribute Certificate Table"
	 *     section of the Microsoft PE/COFF Specification). The
	 *     firmware must do the validation according to the following:
	 *
	 *     - If the hash of the binary is in dbx, then the image shall
	 *       fail the validation.
	 *     - Else if the hash of the binary is in db, then the image
	 *       shall pass the validation.
	 *     - Else if one of signatures is in db and is not in dbx, then
	 *       the image shall pass the validation.
	 *     - Else the image shall fail the validation.
	 *
	 * And so we check dbx hashes first, then db.
	 */
	if (is_revoked_by_hash(ctx, pe)) {
		debug("PE \"%s\" is revoked by hash", pe->filename);
		pe->score = 0;
		return;
	}

	assign_db_hash_score(ctx, pe);

	if (pe->score != 0) {
		debug("PE \"%s\" is validated by hash", pe->filename);
		return;
	}
}

int
pe_cmp(const void *p0, const void *p1)
{
	pe_file_t *pe0 = *(pe_file_t **)p0;
	pe_file_t *pe1 = *(pe_file_t **)p1;

	return pe1->score - pe0->score;
}

// vim:fenc=utf-8:tw=75:noet
