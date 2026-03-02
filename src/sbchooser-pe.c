// SPDX-License-Identifier: GPL-v3-or-later
/*
 * sbchooser-pe.c - pe support for sbchooser
 * Copyright Peter Jones <pjones@redhat.com>
 */

#include "sbchooser.h" // IWYU pragma: keep
#include <openssl/sha.h>

#include <openssl/err.h>

static void
debug_print_openssl_errors(void)
{
	while (true) {
		unsigned long err;
		const char *file = NULL, *func = NULL, *data = NULL;
		int line = 0;
		int flags = 0;

		err = ERR_get_error_all(&file, &line, &func, &data, &flags);
		if (err == 0)
			break;
		debug("openssl error 0x%016llx %s:%d:%s flags:%d data:\"%s\"",
		      err, file, line, func, flags, data);
	}
}

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

	for (size_t i = 0; i < pe->n_sigs; i++) {
		free(pe->sigs[i]);
		pe->sigs[i] = NULL;
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

void
fmt_time(const ASN1_TIME *asn1, char buf[1024])
{
	struct tm tm;

	memset(buf, 0, 1024);
	if (!asn1) {
		strcpy(buf, "(null)");
		return;
	}
	ASN1_TIME_to_tm(asn1, &tm);
	asctime_r(&tm, buf);

	buf[1023] = '\0';
	for (size_t i = 0; i < 1024; i++) {
		if (buf[i] == '\r' || buf[i] == 'n' || !isprint(buf[i]))
			buf[i] = '\0';
	}
}

static int
time_cmp(const ASN1_TIME *t0, const ASN1_TIME *t1)
{
	int rc = 0;
	char str0[1024];
	char str1[1024];

	memset(str0, 0, 1024);
	memset(str1, 0, 1024);
	fmt_time(t0, str0);
	fmt_time(t1, str1);

	if (t0 && t1)
		rc = ASN1_TIME_compare(t0, t1);
	if (t0 && !t1)
		rc = -1;
	if (t1 && !t0)
		rc = 1;
	return rc;
}

static int
add_one_cert(sig_data_t *sig, X509 *x509)
{
	cert_data_t *cert = NULL;
	cert_data_t **new_certs = NULL;
	size_t n_certs = sig->n_certs;
	int rc;
	char buf0[1024];
	char buf1[1024];

	memset(buf0, 0, sizeof(buf0));
	memset(buf1, 0, sizeof(buf1));

	new_certs = reallocarray(sig->certs, n_certs + 1, sizeof(cert_data_t));
	if (!new_certs)
		return -1;

	sig->certs = new_certs;

	cert = calloc(1, sizeof(*cert));
	if (!cert)
		return -1;

	cert->x509 = x509;

	rc = elaborate_x509_info(cert);
	if (rc < 0) {
		memset(cert, 0, sizeof(*cert));
		free(cert);
		return rc;
	}

	if (sig->earliest_not_before) {
		fmt_time(sig->earliest_not_before, buf0);
		fmt_time(cert->not_before, buf1);
		debug("comparing \"%s\" to \"%s\"", buf0, buf1);
	}
	if (!sig->earliest_not_before) {
		sig->earliest_not_before = cert->not_before;
	} else if (time_cmp(sig->earliest_not_before, cert->not_before) > 0) {
		sig->earliest_not_before = cert->not_before;
	}
	fmt_time(sig->earliest_not_before, buf0);
	debug("set sig->earliest_not_before to %s", buf0);

	if (sig->latest_not_after) {
		fmt_time(sig->latest_not_after, buf0);
		fmt_time(cert->not_after, buf1);
		debug("finding latest of \"%s\" to \"%s\"", buf0, buf1);
	}

	if (!sig->latest_not_after) {
		sig->latest_not_after = cert->not_after;
	} else if (time_cmp(sig->latest_not_after, cert->not_after) < 0) {
		sig->latest_not_after = cert->not_after;
	}
	fmt_time(sig->latest_not_after, buf0);
	debug("set sig->latest_not_after to %s", buf0);

	new_certs[n_certs] = cert;
	sig->n_certs += 1;
	return 0;
}

static int
parse_pkcs7(PKCS7 *p7, sig_data_t *sig)
{
	STACK_OF(X509) *certs;
	int rc = 0;

	certs = PKCS7_get0_signers(p7, NULL, 0);
	if (!certs) {
		warnx("failed to parse X509 certs");
		debug_print_openssl_errors();
		errno = EINVAL;
		goto err;
	}

	for (int i = 0; i < sk_X509_num(certs); i++) {
		X509 *x = sk_X509_value(certs, i);

		rc = add_one_cert(sig, x);
		if (rc < 0)
			goto err;
	}

	return rc;
err:
	return -1;
}

static int
add_one_sig(pe_file_t *pe, uint8_t *data, size_t datasz)
{
	sig_data_t *sig = NULL;
	const unsigned char *ppin = (const unsigned char *)data;
	sig_data_t **sigs = NULL;
	size_t n_sigs = pe->n_sigs + 1;
	int rc;
	char buf0[1024];
	char buf1[1024];

	memset(buf0, 0, sizeof(buf0));
	memset(buf1, 0, sizeof(buf1));

	sigs = reallocarray(pe->sigs, n_sigs, sizeof(*sigs));
	if (!sigs)
		return -1;
	pe->sigs = sigs;

	sig = calloc(1, sizeof(*sig));
	if (!sig)
		return -1;

	PKCS7 *p7;
	p7 = d2i_PKCS7(NULL, &ppin, datasz);
	if (!p7) {
		warnx("failed to parse X509 signature");
		debug_print_openssl_errors();
		errno = EINVAL;
		goto err;
	}

	rc = parse_pkcs7(p7, sig);
	if (rc < 0) {
		debug("parsing pkcs7 data failed");
		goto err;
	}

	if (pe->earliest_not_before) {
		fmt_time(pe->earliest_not_before, buf0);
		fmt_time(sig->earliest_not_before, buf1);
		debug("finding earliest of \"%s\" to \"%s\"", buf0, buf1);
	}

	if (!pe->earliest_not_before) {
		pe->earliest_not_before = sig->earliest_not_before;
	} else if (time_cmp(pe->earliest_not_before, sig->earliest_not_before) > 0) {
		pe->earliest_not_before = sig->earliest_not_before;
	}
	fmt_time(pe->earliest_not_before, buf0);
	debug("set pe->earliest_not_before to %s", buf0);

	if (pe->latest_not_after) {
		fmt_time(pe->latest_not_after, buf0);
		fmt_time(sig->latest_not_after, buf1);
		debug("finding latest of \"%s\" to \"%s\"", buf0, buf1);
	}

	if (!pe->latest_not_after) {
		pe->latest_not_after = sig->latest_not_after;
	} else if (time_cmp(pe->latest_not_after, sig->latest_not_after) < 0) {
		pe->latest_not_after = sig->latest_not_after;
	}
	fmt_time(pe->latest_not_after, buf0);
	debug("set pe->latest_not_after to %s", buf0);

	sigs[pe->n_sigs] = sig;
	pe->n_sigs = n_sigs;

	return 0;
err:
	if (p7) {
		debug("Freeing PKCS7_SIGNED %p", p7);
		PKCS7_free(p7);
		p7 = NULL;
	}

	free(sig);
	return -1;
}

static int
parse_sigs(pe_file_t *pe)
{
	int rc = 0;
	uintptr_t pos = 0;
	uintptr_t dd = (uintptr_t)(pe->map) + pe->ctx.sec_dir->virtual_address;

	while (pos < pe->ctx.sec_dir->size) {
		win_certificate_header_t *wincert = (win_certificate_header_t *)(dd + pos);
		win_certificate_pkcs_signed_data_t *pkcs7 = NULL;
		size_t data_len;

		debug("win_certificate_t length:%"PRIu32" (0x%08"PRIx32") revision:0x%04"PRIx16" type:0x%04"PRIx16,
		      wincert->length, wincert->length, wincert->revision, wincert->cert_type);
		if (wincert->revision != WIN_CERT_REVISION_2_0) {
			debug("weird win_cert revision 0x%04"PRIx16, wincert->revision);
			goto next;
		}

		if (wincert->cert_type != WIN_CERT_TYPE_PKCS_SIGNED_DATA) {
			debug("weird win_cert type 0x%04"PRIx16, wincert->cert_type);
			goto next;
		}

		data_len = wincert->length - sizeof(*wincert);
		pkcs7 = (win_certificate_pkcs_signed_data_t *)wincert;

		rc = add_one_sig(pe, pkcs7->data, data_len);
		if (rc < 0) {
			warn("adding signature failed");
			break;
		}
next:
		pos += wincert->length;
	}

	return rc;
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

	rc = parse_sigs(pe);
	if (rc < 0)
		goto err;

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

#define PE_SCORE_CERT_TBS_SHA512	0x00000004ull
#define PE_SCORE_CERT_TBS_SHA384	0x00000002ull
#define PE_SCORE_CERT_TBS_SHA256	0x00000001ull
#define PE_SCORE_CERT_TBS_MASK		0x00000007ull
#define PE_SCORE_CERT_TBS_SHIFT		24ull

/*
 * PQC could stand to be elaborated here...
 */
#define PE_SCORE_CERT_PK_PQC		0x00000020ull

// these are defined by security strength, not key size.
#define PE_SCORE_CERT_PK_RSA_256	0x00000010ull // RSA15360
#define PE_SCORE_CERT_PK_RSA_192	0x00000008ull // RSA7690
#define PE_SCORE_CERT_PK_RSA_128	0x00000004ull // RSA3072
#define PE_SCORE_CERT_PK_RSA_112	0x00000002ull // RSA2048
#define PE_SCORE_CERT_PK_RSA_80		0x00000001ull // RSA1024
#define PE_SCORE_CERT_PK_MASK		0x0000003full
#define PE_SCORE_CERT_PK_SHIFT		20ull

static void
debug_print_digest(char *label, uint8_t *data, size_t datasz)
{
	if (efi_get_verbose() >= DEBUG_LEVEL) {
		FILE *dbglog = efi_get_logfile();
		fprintf(dbglog, "%s:", label);
		for (size_t i = 0; i < datasz; i++) {
			fprintf(dbglog, "%02x", data[i]);
		}
		fprintf(dbglog, "\n");
	}
}

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

		debug_print_digest("  dgst", dgst->data, dgst->datasz);
		debug_print_digest("sha512", pe->sha512.data, pe->sha512.datasz);
		debug_print_digest("sha384", pe->sha384.data, pe->sha384.datasz);
		debug_print_digest("sha256", pe->sha256.data, pe->sha256.datasz);

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

static void
_minimize_cert_pk_score_shifted(uint32_t *score)
{
	uint32_t pk_score = *score;

	if (pk_score & PE_SCORE_CERT_PK_RSA_80) {
		pk_score = PE_SCORE_CERT_PK_RSA_80;
	}
	if (pk_score & PE_SCORE_CERT_PK_RSA_112) {
		pk_score = PE_SCORE_CERT_PK_RSA_112;
	}
	if (pk_score & PE_SCORE_CERT_PK_RSA_128) {
		pk_score = PE_SCORE_CERT_PK_RSA_128;
	}
	if (pk_score & PE_SCORE_CERT_PK_RSA_192) {
		pk_score = PE_SCORE_CERT_PK_RSA_192;
	}
	if (pk_score & PE_SCORE_CERT_PK_RSA_256) {
		pk_score = PE_SCORE_CERT_PK_RSA_256;
	}
	*score = pk_score;
}

static void
update_pk_score(uint32_t *score, uint32_t value)
{
	uint32_t orig_pk_score = (*score >> PE_SCORE_CERT_PK_SHIFT) & PE_SCORE_CERT_PK_MASK;
	uint32_t pk_score = orig_pk_score;

	pk_score |= value;
	_minimize_cert_pk_score_shifted(&pk_score);
	*score &= ~(PE_SCORE_CERT_PK_MASK << PE_SCORE_CERT_PK_SHIFT);
	*score |= pk_score << PE_SCORE_CERT_PK_SHIFT;
	debug("pk_score: 0x%08"PRIx32"->0x%08"PRIx32, orig_pk_score, pk_score);
}

static void
_minimize_cert_md_score_shifted(uint32_t *score)
{
	uint32_t md_score = *score;

	if (md_score & PE_SCORE_CERT_TBS_SHA256) {
		md_score = PE_SCORE_CERT_TBS_SHA256;
	}
	if (md_score & PE_SCORE_CERT_TBS_SHA384) {
		md_score = PE_SCORE_CERT_TBS_SHA384;
	}
	if (md_score & PE_SCORE_CERT_TBS_SHA512) {
		md_score = PE_SCORE_CERT_TBS_SHA512;
	}

	*score = md_score;
}

static void
update_md_score(uint32_t *score, uint32_t value)
{
	uint32_t orig_md_score = (*score >> PE_SCORE_CERT_TBS_SHIFT) & PE_SCORE_CERT_TBS_MASK;
	uint32_t md_score = orig_md_score;

	md_score |= value;
	_minimize_cert_md_score_shifted(&md_score);

	*score &= ~(PE_SCORE_CERT_TBS_MASK << PE_SCORE_CERT_TBS_SHIFT);
	*score |= md_score << PE_SCORE_CERT_TBS_SHIFT;
	debug("md_score: 0x%08"PRIx32"->0x%08"PRIx32, orig_md_score, md_score);
}

static void
debug_print_trusted_cert(cert_data_t *anchor, cert_data_t *signer)
{
	if (efi_get_verbose() < DEBUG_LEVEL)
		return;

	char buf0[4096];
	char buf1[4096];

	X509_NAME_oneline(anchor->subject, buf0, 4096);

	debug("anchor subject:\"%s\"", buf0);

	X509_NAME_oneline(signer->issuer, buf0, 4096);
	X509_NAME_oneline(signer->subject, buf1, 4096);

	debug("signer issuer:\"%s\" subject:\"%s\"", buf0, buf1);
}

static void
assign_sig_score(sbchooser_context_t *ctx, sig_data_t *sig)
{
	bool only_trusted = true;
	bool found_trust_anchor = false;

	if (sig->revoked) {
		sig->score = 0;
		return;
	}

	for (size_t i = 0; i < sig->n_certs; i++) {
		cert_data_t *sigcert = sig->certs[i];

		for (size_t j = 0; j < ctx->n_db_certs; j++) {
			cert_data_t *dbcert = ctx->db_certs[j];

			if (is_same_cert(sigcert, dbcert)) {
				debug("signature cert %zu is trust anchor %zu", i, j);
				sigcert->trust_anchor_cert = dbcert;
				debug_print_trusted_cert(dbcert, sigcert);
				found_trust_anchor = true;
			} else if (is_issuing_cert(sigcert, dbcert)) {
				debug("signature cert %zu is issued by anchor %zu", i, j);
				sigcert->trust_anchor_cert = dbcert;
				debug_print_trusted_cert(dbcert, sigcert);
				found_trust_anchor = true;
			}
		}
		if (found_trust_anchor) {
			sigcert->trusted = true;
		} else {
			only_trusted = false;
		}
	}

	if (only_trusted && found_trust_anchor) {
		debug("only trusted certs found");
		sig->trusted = true;
	}

	for (size_t i = 0; i < sig->n_certs; i++) {
		cert_data_t *sigcert = sig->certs[i];

		const char *mdsn = OBJ_nid2sn(sigcert->mdnid);
		debug("mdsn:\"%s\"", mdsn);


		if (!strcmp(mdsn, "SHA512")) {
			update_md_score(&sig->score, PE_SCORE_CERT_TBS_SHA512);
		} else if (!strcmp(mdsn, "SHA384")) {
			update_md_score(&sig->score, PE_SCORE_CERT_TBS_SHA384);
		} else if (!strcmp(mdsn, "SHA256")) {
			update_md_score(&sig->score, PE_SCORE_CERT_TBS_SHA256);
		}

		const char *pksn = OBJ_nid2ln(sigcert->pknid);
		debug("pksn:\"%s\"", pksn);

		/*
		 * XXX:PJFIX PQC isn't implemented here yet
		 */
		if (!strcmp(pksn, "rsaEncryption")) {
			X509_PUBKEY *xpk;
			ASN1_OBJECT *ppkalg = NULL;
			const unsigned char *pk = NULL;
			int pklen = 0;
			X509_ALGOR *pa = NULL;
			int rc = 0;

			xpk = X509_get_X509_PUBKEY(sigcert->x509);
			if (!xpk) {
				debug("This certificate has no public key.  Not scoring.");
				return;
			}

			rc = X509_PUBKEY_get0_param(&ppkalg, &pk, &pklen, &pa, xpk);
			if (!rc) {
				debug("This public key has no algorithm parameters.  Not scoring.");
				return;
			}
			/*
			 * PK here is the ASN1 encoded modulus and
			 * exponent, so pklen (in bytes) includes a few
			 * extra bytes here and there, but these values are
			 * far enough apart it shouldn't matter...
			 */
			pklen *= 8;
			if (pklen >= 15360) {
				update_pk_score(&sig->score, PE_SCORE_CERT_PK_RSA_256);
			} else if (pklen >= 7690) {
				update_pk_score(&sig->score, PE_SCORE_CERT_PK_RSA_192);
			} else if (pklen >= 3072) {
				update_pk_score(&sig->score, PE_SCORE_CERT_PK_RSA_128);
			} else if (pklen >= 2048) {
				update_pk_score(&sig->score, PE_SCORE_CERT_PK_RSA_112);
			} else if (pklen >= 1024) {
				update_pk_score(&sig->score, PE_SCORE_CERT_PK_RSA_80);
			}
		}
	}
}

static bool
is_signing_cert_revoked(sbchooser_context_t *ctx, cert_data_t *sigcert)
{
	for (size_t i = 0; i < ctx->n_dbx_certs; i++) {
		cert_data_t *dbxcert = ctx->dbx_certs[i];

		if (is_same_cert(sigcert, dbxcert)) {
			return true;
		}

		if (is_issuing_cert(sigcert, dbxcert)) {
			return true;
		}
		/*
		 * XXX PJFIX: right now we don't check cert revocations by
		 * TBS hash.  I think we could solve this with
		 * X509_digest() and looking them up, but I don't have any
		 * dbx examples handy.
		 */
	}
	return false;
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

	for (size_t i = 0; i < pe->n_sigs; i++) {
		bool found = false;
		sig_data_t *sig = pe->sigs[i];

		for (size_t j = 0; j < sig->n_certs; j++) {
			cert_data_t *sigcert = sig->certs[j];

			if (is_signing_cert_revoked(ctx, sigcert)) {
				debug("PE \"%s\" signature %zu is revoked by cert", pe->filename, i);
				found = true;
				sig->revoked = true;
				break;
			}
		}
		if (found)
			continue;

		assign_sig_score(ctx, sig);
	}
	debug("merging pe sig scores before:0x%08"PRIx32, pe->score);
	for (size_t i = 0; i < pe->n_sigs; i++) {
		sig_data_t *sig = pe->sigs[i];
		uint32_t tmpscore;

		if (sig->trusted) {
			debug("signature %zu is trusted, merging", i);
		} else {
			debug("signature %zu is not trusted, not merging", i);
			continue;
		}

		tmpscore = sig->score & (PE_SCORE_CERT_TBS_MASK << PE_SCORE_CERT_TBS_SHIFT);
		tmpscore >>= PE_SCORE_CERT_TBS_SHIFT;
		debug("tmpscore:0x%08"PRIx32, tmpscore);
		update_md_score(&pe->score, tmpscore);

		tmpscore = sig->score & (PE_SCORE_CERT_PK_MASK << PE_SCORE_CERT_PK_SHIFT);
		tmpscore >>= PE_SCORE_CERT_PK_SHIFT;
		debug("tmpscore:0x%08"PRIx32, tmpscore);
		update_pk_score(&pe->score, tmpscore);
	}
	debug("merging pe sig scores after:0x%08"PRIx32, pe->score);
}

static uint32_t
compute_minimum_security_strength(pe_file_t *pe)
{
	uint32_t secbits = 0xffffffffull;

	uint32_t pk_score = (pe->score >> PE_SCORE_CERT_PK_SHIFT) & PE_SCORE_CERT_PK_MASK;
	if (pk_score & PE_SCORE_CERT_PK_RSA_256) {
		secbits = 256;
	}
	if (pk_score & PE_SCORE_CERT_PK_RSA_192) {
		secbits = 192;
	}
	if (pk_score & PE_SCORE_CERT_PK_RSA_128) {
		secbits = 128;
	}
	if (pk_score & PE_SCORE_CERT_PK_RSA_112) {
		secbits = 112;
	}
	if (pk_score & PE_SCORE_CERT_PK_RSA_80) {
		secbits = 80;
	}

	uint32_t md_score = (pe->score >> PE_SCORE_CERT_TBS_SHIFT) & PE_SCORE_CERT_TBS_MASK;
	if ((md_score & PE_SCORE_CERT_TBS_SHA512) && secbits >= 256) {
		secbits = 256;
	}
	if ((md_score & PE_SCORE_CERT_TBS_SHA384) && secbits >= 192) {
		secbits = 192;
	}
	if ((md_score & PE_SCORE_CERT_TBS_SHA256) && secbits >= 128) {
		secbits = 128;
	}

	/*
	 * If the hash is in db, we let that override signature security
	 * strength
	 */
	uint32_t db_score = (pe->score >> PE_SCORE_HASH_IN_DB_SHIFT) & PE_SCORE_HASH_IN_DB_MASK;
	if (db_score & PE_SCORE_SHA512_IN_DB) {
		secbits = 256;
	} else if (db_score & PE_SCORE_SHA384_IN_DB) {
		secbits = 192;
	} else if (db_score & PE_SCORE_SHA256_IN_DB) {
		secbits = 128;
	}

	if (secbits == 0xffffffffull)
		secbits = 0;

	//debug("%s pe->score:0x%08"PRIx32" secbits:%"PRIu32, pe->filename, pe->score, secbits);
	return secbits;
}

static int
compare_validities(pe_file_t *pe0, pe_file_t *pe1)
{
	int rc = 0;
	char buf0[1024];
	char buf1[1024];
	char default_pref[] = "no preference";
	char *pref;

	/*
	 * Dunno when this can happen, but always prefer the one that's
	 * signed if we get this far...
	 */
	if (pe0->latest_not_after && !pe1->latest_not_after)
		return -1;
	if (!pe0->latest_not_after && pe1->latest_not_after)
		return 1;

	/*
	 * prefer the one that has certs expiring the latest
	 */
	fmt_time(pe0->latest_not_after, buf0);
	fmt_time(pe1->latest_not_after, buf1);
	rc = time_cmp(pe0->latest_not_after, pe1->latest_not_after);
	if (rc < 0)
		pref = buf1;
	else if (rc > 0)
		pref = buf0;
	else
		pref = default_pref;

	debug("finding latest of \"%s\" and \"%s\": %s", buf0, buf1, pref);
	if (rc < 0)
		return 1;
	if (rc > 0)
		return -1;

	/*
	 * Dunno when this can happen, but always prefer the one that's
	 * signed if we get this far...
	 */
	if (pe0->earliest_not_before && !pe1->earliest_not_before)
		return -1;
	if (!pe0->earliest_not_before && pe1->earliest_not_before)
		return 1;

	/*
	 * prefer the one that has certs starting the earliest
	 */
	fmt_time(pe0->earliest_not_before, buf0);
	fmt_time(pe1->earliest_not_before, buf1);
	rc = time_cmp(pe1->earliest_not_before, pe0->earliest_not_before);
	if (rc < 0)
		pref = buf1;
	else if (rc > 0)
		pref = buf0;
	else
		pref = default_pref;

	debug("finding earliest of \"%s\" and \"%s\": %s", buf0, buf1, pref);
	if (rc < 0)
		return 1;
	if (rc > 0)
		return -1;
	return 0;
}

int
pe_cmp(const void *p0, const void *p1)
{
	pe_file_t *pe0 = *(pe_file_t **)p0;
	pe_file_t *pe1 = *(pe_file_t **)p1;
	int rc;

	uint32_t score = 0;

	uint32_t strength0 = compute_minimum_security_strength(pe0);
	uint32_t strength1 = compute_minimum_security_strength(pe1);

	score = strength1 - strength0;
	debug("\"%s\" (secbits %"PRIu32") vs \"%s\" (secbits %"PRIu32"): %d",
	      pe0->filename, strength0, pe1->filename, strength1, score);
	if (score != 0)
		return pe1->score - pe0->score;

	debug("security strength is equal; comparing validities");
	rc = compare_validities(pe0, pe1);
	if (rc < 0) {
		debug("prefer \"%s\"", pe1->filename);
	} else if (rc > 0) {
		debug("prefer \"%s\"", pe0->filename);
	} else {
		debug("no preference");
		return strcmp(pe0->filename, pe1->filename);
	}
	return rc;
}

// vim:fenc=utf-8:tw=75:noet
