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

static void
free_sig(sig_data_t *sig)
{
	if (!sig)
		return;

	for (size_t i = 0; i < sig->n_certs; i++) {
		free_cert(sig->certs[i]);
		sig->certs[i] = NULL;
	}
	if (sig->certs) {
		sig->n_certs = 0;
		free(sig->certs);
		sig->certs = NULL;
	}

	if (sig->p7) {
		PKCS7_free(sig->p7);
		sig->p7 = NULL;
	}

	if (sig->x509s) {
		sk_X509_free(sig->x509s);
		sig->x509s = NULL;
	}

	free(sig);
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

	if (pe->sigs) {
		for (size_t i = 0; i < pe->n_sigs; i++) {
			free_sig(pe->sigs[i]);
			pe->sigs[i] = NULL;
		}
		free(pe->sigs);
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

static bool
get_revocation(sbchooser_context_t *ctx, cert_data_t *sigcert)
{
	debug("looking for subject or issuer in %d dbx certs", ctx->n_dbx_certs);

	for (size_t i = 0; i < ctx->n_dbx_certs; i++) {
		cert_data_t *dbxcert = ctx->dbx_certs[i];

		if (is_same_cert(sigcert, dbxcert)) {
			if (!sigcert->revoked_cert)
				sigcert->revoked_cert = dbxcert;
			debug("found");
			return true;
		}

		if (is_issuing_cert(sigcert, dbxcert)) {
			if (!sigcert->revoked_cert)
				sigcert->revoked_cert = dbxcert;
			debug("found");
			return true;
		}
		/*
		 * XXX PJFIX: right now we don't check cert revocations by
		 * TBS hash.  I think we could solve this with
		 * X509_digest() and looking them up, but I don't have any
		 * dbx examples handy.
		 */
	}
	debug("none found");
	return false;
}

static bool
get_authorization(sbchooser_context_t *ctx, cert_data_t *sigcert)
{
	debug("looking for subject or issuer in %d db certs", ctx->n_db_certs);

	for (size_t i = 0; i < ctx->n_db_certs; i++) {
		cert_data_t *dbcert = ctx->db_certs[i];

		if (is_same_cert(sigcert, dbcert)) {
			if (!sigcert->trust_anchor_cert)
				sigcert->trust_anchor_cert = dbcert;
			debug("found");
			return true;
		}

		if (is_issuing_cert(sigcert, dbcert)) {
			if (!sigcert->trust_anchor_cert)
				sigcert->trust_anchor_cert = dbcert;
			debug("found");
			return true;
		}
		/*
		 * XXX PJFIX: right now we don't check cert authorizations
		 * by TBS hash.  I think we could solve this with
		 * X509_digest() and looking them up, but I don't have any
		 * db examples handy.
		 */
	}
	debug("none found");
	return false;
}

static void
update_cert_trust(sbchooser_context_t *ctx, cert_data_t *cert)
{
	char subject[4096];
	memset(subject, 0, sizeof(subject));
	X509_NAME_oneline(cert->subject, subject, 4095);

	if (get_revocation(ctx, cert)) {
		char revoker[4096];

		memset(revoker, 0, sizeof(revoker));

		X509_NAME_oneline(cert->revoked_cert->subject, revoker, 4095);

		if (cert->rationale) {
			free(cert->rationale);
			cert->rationale = NULL;
			debug("updating cert rationale to revoked");
		}
		asprintf(&cert->rationale, "cert \"%s\" is revoked by \"%s\" in dbx",
			 subject, revoker);
		debug("cert \"%s\" revoked by \"%s\"", subject, revoker);
		cert->revoked = true;
	} else {
		debug("no revocations for \"%s\"", subject);
	}

	if (get_authorization(ctx, cert)) {
		char trust_anchor[4096];

		memset(trust_anchor, 0, sizeof(trust_anchor));

		X509_NAME_oneline(cert->trust_anchor_cert->subject,
				  trust_anchor, 4095);

		if (!cert->rationale) {
			debug("updating cert rationale to trusted");
			asprintf(&cert->rationale, "cert \"%s\" is trusted by \"%s\" in db",
				 subject, trust_anchor);
		}
		debug("cert \"%s\" trusted by \"%s\"", subject, trust_anchor);
		cert->trusted = true;
	} else {
		debug("no trust for \"%s\"", subject);
	}
	debug("cert \"%s\" trust is: trusted:%s revoked:%s", subject,
	      cert->trusted ? "true" : "false",
	      cert->revoked ? "true" : "false");
}

static int
add_one_cert(sig_data_t *sig, X509 *x509, cert_data_t **worst_cert)
{
	cert_data_t *cert = NULL;
	cert_data_t **new_certs = NULL;
	size_t n_certs = sig->n_certs;
	int rc;
	char buf0[1024];

	memset(buf0, 0, sizeof(buf0));

	new_certs = reallocarray(sig->certs, n_certs + 1, sizeof(cert_data_t));
	if (!new_certs)
		return -1;

	sig->certs = new_certs;

	cert = calloc(1, sizeof(*cert));
	if (!cert)
		return -1;

	cert->free_x509 = false;
	cert->x509 = x509;

	rc = elaborate_x509_info(cert);
	if (rc < 0) {
		memset(cert, 0, sizeof(*cert));
		free(cert);
		return rc;
	}

	if (!worst_cert || !*worst_cert ||
	    cert_sec_cmp(cert, *worst_cert) < 0) {
		*worst_cert = cert;
	}

	if (!sig->earliest_not_before ||
	    time_cmp(cert->not_before, sig->earliest_not_before) < 0) {
		fmt_time(cert->not_before, buf0);
		debug("setting sig->earliest_not_before to %s", buf0);
		sig->earliest_not_before = cert->not_before;
	}

	if (!sig->latest_not_after ||
	    time_cmp(cert->not_after, sig->latest_not_after) > 0) {
		fmt_time(cert->not_after, buf0);
		debug("setting sig->latest_not_after to %s", buf0);
		sig->latest_not_after = cert->not_after;
	}

	new_certs[n_certs] = cert;
	sig->n_certs += 1;
	return 0;
}

static int
parse_pkcs7(PKCS7 *p7, sig_data_t *sig, cert_data_t **worst_cert)
{
	STACK_OF(X509) *certs;
	int rc = 0;

	sig->p7 = p7;

	certs = PKCS7_get0_signers(p7, NULL, 0);
	if (!certs) {
		warnx("failed to parse X509 certs");
		debug_print_openssl_errors();
		errno = EINVAL;
		goto err;
	}
	sig->x509s = certs;

	for (int i = 0; i < sk_X509_num(certs); i++) {
		X509 *x = sk_X509_value(certs, i);

		rc = add_one_cert(sig, x, worst_cert);
		if (rc < 0)
			goto err;
	}

	return rc;
err:
	return -1;
}

static void
update_sig_trust(sbchooser_context_t *ctx, sig_data_t *sig)
{
	for (size_t j = 0; j < sig->n_certs; j++) {
		cert_data_t *sigcert = sig->certs[j];

		update_cert_trust(ctx, sigcert);

		if (sigcert->trusted) {
			if (!sig->revoked) {
				debug("updating sig rationale to trusted");
				sig->rationale = sigcert->rationale;
			}
			sig->trusted = true;
		}

		if (sigcert->revoked) {
			debug("updating sig rationale to revoked");
			sig->rationale = sigcert->rationale;
			sig->revoked = true;
		}
	}
	if (sig->revoked)
		sig->trusted = false;
}

static int
add_one_sig(pe_file_t *pe, uint8_t *data, size_t datasz)
{
	sig_data_t *sig = NULL;
	const unsigned char *ppin = (const unsigned char *)data;
	sig_data_t **sigs = NULL;
	size_t n_sigs = pe->n_sigs + 1;
	int rc;
	cert_data_t *worst_cert = NULL;
	char buf0[1024];

	memset(buf0, 0, sizeof(buf0));

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

	rc = parse_pkcs7(p7, sig, &worst_cert);
	if (rc < 0) {
		debug("parsing pkcs7 data failed");
		goto err;
	}

	if (worst_cert) {
		sig->lowest_md_secbits = worst_cert->md_secbits;
		sig->lowest_pk_secbits = worst_cert->pk_secbits;
	}

	if (!pe->earliest_not_before ||
	    time_cmp(pe->earliest_not_before, sig->earliest_not_before) > 0) {
		pe->earliest_not_before = sig->earliest_not_before;
	}
	fmt_time(pe->earliest_not_before, buf0);
	debug("set pe->earliest_not_before to %s", buf0);

	if (!pe->latest_not_after ||
	    time_cmp(pe->latest_not_after, sig->latest_not_after) < 0) {
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

	debug("security directory is at %p + 0x%p (%p)", pe->map,
	      (void *)(uintptr_t)pe->ctx.sec_dir->virtual_address, (void *)dd);
	while (pos < pe->ctx.sec_dir->size) {
		win_certificate_header_t *wincert = (win_certificate_header_t *)(dd + pos);
		win_certificate_pkcs_signed_data_t *pkcs7 = NULL;
		size_t data_len;

		if (pe->ctx.sec_dir->size - pos < sizeof(*wincert)) {
			debug("secdir is %d bytes long, ignoring",
			      pe->ctx.sec_dir->size - pos);
			break;
		}

		debug("win_certificate_t at pos:0x%"PRIx32" (0x%"PRIx32") ",
		      pos, pe->ctx.sec_dir->virtual_address + pos);
		debug("length:%"PRIu32" (0x%08"PRIx32") revision:0x%04"PRIx16" type:0x%04"PRIx16,
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
		if (wincert->length == 0)
			break;
		pos += wincert->length;
	}

	return rc;
}

int
load_pe(sbchooser_context_t *ctx,
	const char * const filename,
	pe_file_t **pe_p)
{
	int ret = -1;
	pe_file_t *pe = NULL;
	int fd = -1;
	int rc;
	struct stat statbuf;
	efi_image_dos_header_t *doshdr = NULL;
	efi_image_optional_header_union_t *pehdr = NULL;
	pe_image_context_t *pe_ctx = NULL;
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

	pe_ctx = &pe->ctx;
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

	pe_ctx->pe_header = pehdr;

	if (image_is_64_bit(pehdr)) {
		pe_ctx->size_of_image =
			pehdr->pe32plus.optional_header.size_of_image;
		pe_ctx->size_of_headers =
			pehdr->pe32plus.optional_header.size_of_headers;
		pe_ctx->number_of_sections =
			pehdr->pe32plus.file_header.number_of_sections;
		pe_ctx->section_alignment =
			pehdr->pe32plus.optional_header.section_alignment;
		pe_ctx->number_of_rva_and_sizes =
			pehdr->pe32plus.optional_header.number_of_rva_and_sizes;
		pe_ctx->dll_characteristics =
			pehdr->pe32plus.optional_header.dll_characteristics;
		file_alignment = pehdr->pe32plus.optional_header.file_alignment;
		opt_header_size = sizeof(efi_image_optional_header64_t);
	} else {
		pe_ctx->size_of_image =
			(uint64_t)pehdr->pe32.optional_header.size_of_image;
		pe_ctx->size_of_headers =
			pehdr->pe32.optional_header.size_of_headers;
		pe_ctx->number_of_sections =
			pehdr->pe32.file_header.number_of_sections;
		pe_ctx->section_alignment =
			pehdr->pe32.optional_header.section_alignment;
		pe_ctx->number_of_rva_and_sizes =
			pehdr->pe32.optional_header.number_of_rva_and_sizes;
		pe_ctx->dll_characteristics =
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
	if (pe_ctx->section_alignment == 0)
		pe_ctx->section_alignment = page_size;
	if (pe_ctx->section_alignment < file_alignment)
		pe_ctx->section_alignment = file_alignment;

	/*
	 * Check and make sure the space for data directory entries is as
	 * large as we expect.
	 *
	 * In truth we could set this number smaller if we needed to -
	 * currently it's 16 but we only care about #4 and #5 (the fifth
	 * and sixth ones) - but it hasn't been a problem.  If it's too
	 * weird we'll fail trying to allocate it.
	 */
	if (EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES < pe_ctx->number_of_rva_and_sizes) {
		debug("Invalid number of RVAs (%lu)", pe_ctx->number_of_rva_and_sizes);
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
	    checked_mul((size_t)pe_ctx->number_of_rva_and_sizes, sizeof (efi_image_data_directory_t), &tmpsz1) ||
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
	if (checked_sub((size_t)pe_ctx->size_of_image, section_header_offset, &tmpsz0) ||
	    (tmpsz0 / EFI_IMAGE_SIZEOF_SECTION_HEADER <= pe_ctx->number_of_sections)) {
		debug("Image sections overflow image size");
		goto err;
	}

	/*
	 * Check that the section headers fit within the total headers
	 */
	if (checked_sub((size_t)pe_ctx->size_of_headers, section_header_offset, &tmpsz0) ||
	    (tmpsz0 / EFI_IMAGE_SIZEOF_SECTION_HEADER < (uint32_t)pe_ctx->number_of_sections)) {
		debug("Image sections overflow section headers");
		goto err;
	}

	/*
	 * Check that the section headers are actually within the data
	 * we've read.  Might be duplicative of the size_of_image one, but
	 * it won't hurt.
	 */
	if (checked_mul((size_t)pe_ctx->number_of_sections, sizeof(efi_image_section_header_t), &tmpsz0) ||
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
		pe_ctx->sec_dir = &pehdr->pe32plus.optional_header.data_directory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY];
	} else {
		pe_ctx->sec_dir = &pehdr->pe32.optional_header.data_directory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY];
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
	pe_ctx->first_section = (efi_image_section_header_t *)(uintptr_t)tmpsz0;
	if ((uint64_t)(uintptr_t)(pe_ctx->first_section) > (uint64_t)(uintptr_t)pe->map + pe->mapsz) {
		debug("Invalid image");
		goto err;
	}

	/*
	 * Check that the headers fit within the image.
	 */
	if (pe_ctx->size_of_image < pe_ctx->size_of_headers) {
		debug("Invalid image");
		goto err;
	}

	/*
	 * check that the data directory fits within the image.
	 */
	if (checked_sub((size_t)(uintptr_t)pe_ctx->sec_dir, (size_t)(uintptr_t)pe->map, &tmpsz0) ||
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
	if (pe_ctx->sec_dir->virtual_address > pe->mapsz ||
	    (pe_ctx->sec_dir->virtual_address == pe->mapsz && pe_ctx->sec_dir->size > 0)) {
		debug("pe_ctx->sec_dir->virtual_address:0x%llx pe_ctx->sec_dir->size:0x%llx datasize:0x%llx\n",
		      pe_ctx->sec_dir->virtual_address, pe_ctx->sec_dir->size, pe->mapsz);
		debug("Malformed security header");
		goto err;
	}

	pe->first_sig_only = ctx->first_sig_only;

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

static void
check_secdb_hash(char *dbname, digest_data_t **digests, size_t n_digests,
		 char *dgstname, digest_data_t *candidate, bool *found)
{
	char buf[1024];

	fmt_digest(candidate, buf, sizeof(buf));
	debug("candidate:%s", buf);

	for (size_t i = 0; i < n_digests; i++) {
		digest_data_t *dgst = digests[i];

		if (candidate->datasz != dgst->datasz)
			continue;

		fmt_digest(dgst, buf, sizeof(buf));
		log(LOG_DEBUG_DUMPER, "%s:%s", dbname, buf);

		if (memcmp(dgst->data, candidate->data, dgst->datasz) == 0) {
			debug("%s hash is in %s", dgstname, dbname);
			*found = true;
		}
	}
}

static void
check_dbx_hashes(sbchooser_context_t *ctx, pe_file_t *pe)
{
	debug("%zu digests in dbx\n", ctx->n_dbx_digests);

	check_secdb_hash("dbx", ctx->dbx_digests, ctx->n_dbx_digests,
			 "sha512", &pe->sha512, &pe->sha512_revoked);
	check_secdb_hash("dbx", ctx->dbx_digests, ctx->n_dbx_digests,
			 "sha384", &pe->sha384, &pe->sha384_revoked);
	check_secdb_hash("dbx", ctx->dbx_digests, ctx->n_dbx_digests,
			 "sha256", &pe->sha256, &pe->sha256_revoked);
}

static void
check_db_hashes(sbchooser_context_t *ctx, pe_file_t *pe)
{
	debug("%zu digests in db\n", ctx->n_db_digests);

	check_secdb_hash("db", ctx->db_digests, ctx->n_db_digests,
			 "sha512", &pe->sha512, &pe->sha512_trusted);
	check_secdb_hash("db", ctx->db_digests, ctx->n_db_digests,
			 "sha384", &pe->sha384, &pe->sha384_trusted);
	check_secdb_hash("db", ctx->db_digests, ctx->n_db_digests,
			 "sha256", &pe->sha256, &pe->sha256_trusted);
}

bool
is_revoked_by_hash(pe_file_t *pe, digest_data_t **revoking_digest)
{
	if (pe->sha512_revoked) {
		if (revoking_digest)
			*revoking_digest = &pe->sha512;
		return true;
	}

	if (pe->sha384_revoked) {
		if (revoking_digest)
			*revoking_digest = &pe->sha384;
		return true;
	}

	if (pe->sha256_revoked) {
		if (revoking_digest)
			*revoking_digest = &pe->sha256;
		return true;
	}

	return false;
}

bool
is_trusted_by_hash(pe_file_t *pe, digest_data_t **trusting_digest)
{
	if (pe->sha512_trusted) {
		if (trusting_digest)
			*trusting_digest = &pe->sha512;
		return true;
	}

	if (pe->sha384_trusted) {
		if (trusting_digest)
			*trusting_digest = &pe->sha384;
		return true;
	}

	if (pe->sha256_trusted) {
		if (trusting_digest)
			*trusting_digest = &pe->sha256;
		return true;
	}

	return false;
}

static uint32_t
get_highest_hash_secbits(pe_file_t *pe)
{
	if (is_revoked_by_hash(pe, NULL))
		return 0;

	if (pe->sha512_trusted)
		return 256;
	if (pe->sha384_trusted)
		return 192;
	if (pe->sha256_trusted)
		return 128;

	return 0;
}

void
update_pe_security(sbchooser_context_t *ctx, pe_file_t *pe)
{
	debug("scoring \"%s\"", pe->filename);

	check_dbx_hashes(ctx, pe);
	check_db_hashes(ctx, pe);

	uint32_t lowest_pk_secbits = 0xffffffffull;
	uint32_t lowest_md_secbits = 0xffffffffull;

	bool found_trusted_sig = false;
	for (size_t i = 0; i < pe->n_sigs; i++) {
		sig_data_t *sig = pe->sigs[i];

		update_sig_trust(ctx, sig);

		if (sig->rationale && !pe->rationale) {
			debug("updating pe rationale to %s", sig->revoked ? "revoked" : (sig->trusted ? "trusted" : ""));
			pe->rationale = sig->rationale;
		}

		if (sig->trusted) {
			found_trusted_sig = true;
			if (!pe->has_trusted_signature && pe->rationale) {
				debug("updating pe rationale to %s", sig->revoked ? "revoked" : (sig->trusted ? "trusted" : ""));
				pe->rationale = sig->rationale;
			}
			pe->has_trusted_signature = true;

			if (sig->lowest_md_secbits < lowest_md_secbits) {
				lowest_md_secbits = sig->lowest_md_secbits;
			}
			if (sig->lowest_pk_secbits < lowest_pk_secbits) {
				lowest_pk_secbits = sig->lowest_pk_secbits;
			}

			if (!pe->earliest_not_before ||
			    time_cmp(sig->earliest_not_before, pe->earliest_not_before) < 0) {
				pe->earliest_not_before = sig->earliest_not_before;
			}

			if (!pe->latest_not_after ||
			    time_cmp(sig->latest_not_after, pe->latest_not_after) > 0) {
				pe->latest_not_after = sig->latest_not_after;
			}
		}

		if (pe->first_sig_only)
			break;
	}
	if (!found_trusted_sig) {
		pe->secbits = 0;
		return;
	}
	if (lowest_md_secbits == 0xffffffffull) {
		lowest_md_secbits = 0;
	}
	if (lowest_pk_secbits == 0xffffffffull) {
		lowest_pk_secbits = 0;
	}
	pe->secbits = lowest_md_secbits < lowest_pk_secbits ? lowest_md_secbits : lowest_pk_secbits;
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
	int score;

	bool pe0_revoked = is_revoked_by_hash(pe0, NULL);
	bool pe1_revoked = is_revoked_by_hash(pe1, NULL);
	if (!pe1_revoked && pe0_revoked)
		return -1;
	if (pe1_revoked && !pe0_revoked)
		return 1;

	bool pe0_trusted = is_trusted_by_hash(pe0, NULL);
	bool pe1_trusted = is_trusted_by_hash(pe1, NULL);
	if (pe1_trusted && !pe0_trusted)
		return 1;
	if (!pe1_trusted && pe0_trusted)
		return -1;

	uint32_t pe0_hash_secbits = get_highest_hash_secbits(pe0);
	uint32_t pe1_hash_secbits = get_highest_hash_secbits(pe1);

	score = pe1_hash_secbits - pe0_hash_secbits;
	if (score != 0)
		return score;

	score = pe1->secbits - pe0->secbits;
	debug("\"%s\" (secbits %"PRIu32") vs \"%s\" (secbits %"PRIu32"): %d",
	      pe0->filename, pe0->secbits, pe1->filename, pe1->secbits, score);
	if (score != 0)
		return score;

	debug("security strength is equal; comparing validities");
	score = compare_validities(pe0, pe1);
	if (score < 0) {
		debug("prefer \"%s\"", pe1->filename);
	} else if (score > 0) {
		debug("prefer \"%s\"", pe0->filename);
	} else {
		debug("no preference");
		return strcmp(pe0->filename, pe1->filename);
	}
	return score;
}

// vim:fenc=utf-8:tw=75:noet
