// SPDX-License-Identifier: GPL-v3-or-later
/*
 * sbchooser-pe.c - pe support for sbchooser
 * Copyright Peter Jones <pjones@redhat.com>
 */

#include "sbchooser.h" // IWYU pragma: keep

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

// vim:fenc=utf-8:tw=75:noet
