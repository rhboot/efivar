// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012-2013 Red Hat, Inc.
 */

#include "fix_coverity.h"

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "efivar.h"
#include "guid.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"

efi_guid_t const efi_guid_zero = {0};
efi_guid_t const efi_guid_empty = {0};

static int
cmpguidp(const void *p1, const void *p2)
{
	struct efivar_guidname *gn1 = (struct efivar_guidname *)p1;
	struct efivar_guidname *gn2 = (struct efivar_guidname *)p2;

	return efi_guid_cmp_(&gn1->guid, &gn2->guid);
}

static int
cmpnamep(const void *p1, const void *p2)
{
	struct efivar_guidname *gn1 = (struct efivar_guidname *)p1;
	struct efivar_guidname *gn2 = (struct efivar_guidname *)p2;

	return strncmp(gn1->name, gn2->name, sizeof(gn1->name));
}

struct guid_aliases {
	char *name;
	char *alias;
};

static struct guid_aliases guid_aliases[] = {
	{ "efi_guid_empty", "efi_guid_zero" },
	{ "efi_guid_redhat_2", "efi_guid_redhat" },
	{ NULL, NULL }
};

static void make_aliases(FILE *symout, FILE *header,
			 const char *alias, const efi_guid_t *guid)
{
	for (unsigned int i = 0; guid_aliases[i].name != NULL; i++) {
		if (!strcmp(guid_aliases[i].alias, alias)) {
			fprintf(symout,
				"\nconst efi_guid_t\n"
				"\t__attribute__((__visibility__ (\"default\")))\n"
				"\t%s = {cpu_to_le32(0x%08x),cpu_to_le16(0x%04hx),"
					"cpu_to_le16(0x%04hx),cpu_to_be16(0x%02hhx%02hhx),"
					"{0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx}};\n\n",
				guid_aliases[i].name,
				guid->a, guid->b, guid->c,
				(uint8_t)(guid->d & 0xff),
				(uint8_t)((guid->d & 0xff00) >> 8),
				guid->e[0], guid->e[1], guid->e[2],
				guid->e[3], guid->e[4], guid->e[5]);

			fprintf(header,
				"extern const efi_guid_t %s __attribute__((__visibility__ (\"default\")));\n",
				guid_aliases[i].name);
		}
	}
}

static void
write_guidnames(FILE *out, const char *listname,
		struct efivar_guidname *guidnames, size_t n, const char *symver)
{
	size_t i;

	fprintf(out,
		"const struct efivar_guidname\n"
			"\t__attribute__((__visibility__ (\"default\")))\n"
			"\t%s_[%zd]= {\n",
			listname, n);
	for (i = 0; i < n; i++) {
		struct efivar_guidname *gn = &guidnames[i];

		fprintf(out,
			"\t\t{.guid={.a=cpu_to_le32(%#x),\n"
			"\t\t        .b=cpu_to_le16(%#x),\n"
			"\t\t        .c=cpu_to_le16(%#x),\n"
			"\t\t        .d=cpu_to_be16(0x%02hhx%02hhx),\n"
			"\t\t        .e={%#x,%#x,%#x,%#x,%#x,%#x}},\n"
			"\t\t .symbol=\"%s\",\n"
			"\t\t .name=\"%s\",\n"
			"\t\t .description=\"%s\",\n"
			"\t\t},\n",
			gn->guid.a, gn->guid.b, gn->guid.c,
			(uint8_t)(gn->guid.d & 0xff),
			(uint8_t)((gn->guid.d & 0xff00) >> 8),
			gn->guid.e[0], gn->guid.e[1], gn->guid.e[2],
			gn->guid.e[3], gn->guid.e[4], gn->guid.e[5],
			gn->symbol, gn->name, gn->description);
	}
	fprintf(out, "};\n");
        fprintf(out, "const struct efivar_guidname\n"
			"\t__attribute__((__visibility__ (\"default\")))\n"
			"\t* const %s = %s_;\n", listname, listname);
        fprintf(out, "const struct efivar_guidname\n"
			"\t__attribute__((__visibility__ (\"default\")))\n"
                        "\t* const %s_end = %s_\n\t+ %zd;\n",
                        listname, listname, n - 1);
}

int
main(int argc, char *argv[])
{
	int rc;
	FILE *symout, *header;

	if (argc < 4) {
		errx(1, "Not enough arguments.\n");
	} else if (argc > 4) {
		errx(1, "Too many arguments.\n");
	}

	symout = fopen(argv[2], "w");
	if (symout == NULL)
		err(1, "could not open \"%s\"", argv[2]);
	rc = chmod(argv[2], 0644);
	if (rc < 0)
		warn("chmod(%s, 0644)", argv[2]);

	header = fopen(argv[3], "w");
	if (header == NULL)
		err(1, "could not open \"%s\"", argv[3]);
	rc = chmod(argv[3], 0644);
	if (rc < 0)
		warn("chmod(%s, 0644)", argv[3]);

	struct guidname_index *guidnames = NULL;

	rc = read_guids_at(AT_FDCWD, argv[1], &guidnames);
	if (rc < 0)
		err(1, "could not read \"%s\"", argv[1]);

	struct efivar_guidname *outbuf;

	outbuf = calloc(guidnames->nguids, sizeof(struct efivar_guidname));
	if (!outbuf)
		err(1, "could not allocate memory");

	unsigned int line = guidnames->nguids;
	char *strtab = guidnames->strtab;

	fprintf(header, "#ifndef EFIVAR_GUIDS_H\n#define EFIVAR_GUIDS_H 1\n\n");
	fprintf(header, "\
#ifdef __cplusplus\n\
extern \"C\" {\n\
#endif\n");
	fprintf(header, "\n\
struct efivar_guidname {\n\
	efi_guid_t guid;\n\
	char symbol[256];\n\
	char name[256];\n\
	char description[256];\n\
} __attribute__((__aligned__(16)));\n\n");

	fprintf(symout, "#ifndef EFIVAR_BUILD_ENVIRONMENT\n");
	fprintf(symout, "#define EFIVAR_BUILD_ENVIRONMENT\n");
	fprintf(symout, "#endif /* EFIVAR_BUILD_ENVIRONMENT */\n\n");
	fprintf(symout, "#include \"fix_coverity.h\"\n");
	fprintf(symout, "#include <efivar/efivar.h>\n");
	fprintf(symout, "#include <endian.h>\n");
	fprintf(symout, "\n\
#if BYTE_ORDER == BIG_ENDIAN\n\
#define cpu_to_be32(n) (n)\n\
#define cpu_to_be16(n) (n)\n\
#define cpu_to_le32(n) (__builtin_bswap32(n))\n\
#define cpu_to_le16(n) (__builtin_bswap16(n))\n\
#else\n\
#define cpu_to_le32(n) (n)\n\
#define cpu_to_le16(n) (n)\n\
#define cpu_to_be32(n) (__builtin_bswap32(n))\n\
#define cpu_to_be16(n) (__builtin_bswap16(n))\n\
#endif\n\
""");

	unsigned int i;
	for (i = 0; i < line; i++) {
		struct guidname_offset *gno = &guidnames->offsets[i];
		char *sym = &strtab[gno->symoff];
		char *name = &strtab[gno->nameoff];
		char *desc = &strtab[gno->descoff];

		make_aliases(symout, header, sym, &gno->guid);

		size_t sz;

		outbuf[i].guid = gno->guid;

		sz = sizeof(outbuf[i].symbol);
		strncpy(outbuf[i].symbol, sym, sz);
		outbuf[i].symbol[sz - 1] = '\0';

		sz = sizeof(outbuf[i].name);
		strncpy(outbuf[i].name, name, sz);
		outbuf[i].name[sz - 1] = '\0';

		sz = sizeof(outbuf[i].description);
		strncpy(outbuf[i].description, desc, sz);
		outbuf[i].description[sz - 1] = '\0';

		if (!strcmp(sym, "efi_guid_zzignore-this-guid"))
			break;

		fprintf(header, "extern const efi_guid_t %s __attribute__((__visibility__ (\"default\")));\n", sym);

		fprintf(symout, "const efi_guid_t\n"
			"__attribute__((__visibility__ (\"default\")))\n"
			"\t%s = {cpu_to_le32(0x%08x),cpu_to_le16(0x%04hx),"
				"cpu_to_le16(0x%04hx),cpu_to_be16(0x%02hhx%02hhx),"
				"{0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx,0x%02hhx}};\n\n",
			sym,
			gno->guid.a, gno->guid.b, gno->guid.c,
			(uint8_t)(gno->guid.d & 0xff),
			(uint8_t)((gno->guid.d & 0xff00) >> 8),
			gno->guid.e[0], gno->guid.e[1], gno->guid.e[2],
			gno->guid.e[3], gno->guid.e[4], gno->guid.e[5]);
	}

	fprintf(header, "\n");
	fprintf(header, "#ifndef EFIVAR_BUILD_ENVIRONMENT\n\n");
	fprintf(header,
		"extern const struct efivar_guidname\n"
			"\t__attribute__((__visibility__ (\"default\")))\n"
			"\t* const efi_well_known_guids;\n");
	fprintf(header,
		"extern const struct efivar_guidname\n"
			"\t__attribute__((__visibility__ (\"default\")))\n"
			"\t* const efi_well_known_guids_end;\n");
	fprintf(header,
		"extern const uint64_t\n"
			"\t__attribute__((__visibility__ (\"default\")))\n"
			"\tefi_n_well_known_guids;\n\n");
	fprintf(header,
		"extern const struct efivar_guidname\n"
			"\t__attribute__((__visibility__ (\"default\")))\n"
			"\t* const efi_well_known_names;\n");
	fprintf(header,
		"extern const struct efivar_guidname\n"
			"\t__attribute__((__visibility__ (\"default\")))\n"
			"\t* const efi_well_known_names_end;\n");
	fprintf(header,
		"extern const uint64_t\n"
			"\t__attribute__((__visibility__ (\"default\")))\n"
			"\tefi_n_well_known_names;\n\n");
	fprintf(header, "#endif /* EFIVAR_BUILD_ENVIRONMENT */\n");

	/*
	 * These are intentionally off by one, omitting:
	 * ffffffff-ffff-ffff-ffff-ffffffffffff	zzignore-this-guid	zzignore-this-guid
	 */
	fprintf(symout,
		"const uint64_t\n"
			"\t__attribute__((__visibility__ (\"default\")))\n"
			"\tefi_n_well_known_guids = %u;\n",
		i);
	fprintf(symout,
		"const uint64_t\n"
			"\t__attribute__((__visibility__ (\"default\")))\n"
			"\tefi_n_well_known_names = %u;\n\n",
		i);

	/*
	 * Emit the end from here as well.
	 */

	fprintf(header, "\n\
#ifdef __cplusplus\n\
} /* extern \"C\" */\n\
#endif\n");
	fprintf(header, "\n#endif /* EFIVAR_GUIDS_H */\n");
	fclose(header);

	fprintf(symout,
		"struct efivar_guidname {\n"
		"\tefi_guid_t guid;\n"
		"\tchar symbol[256];\n"
		"\tchar name[256];\n"
		"\tchar description[256];\n"
		"} __attribute__((__aligned__(16)));\n\n");

	qsort(outbuf, line, sizeof(struct efivar_guidname), cmpguidp);
	write_guidnames(symout, "efi_well_known_guids", outbuf, line, "libefivar.so.0");

	qsort(outbuf, line, sizeof(struct efivar_guidname), cmpnamep);
	write_guidnames(symout, "efi_well_known_names", outbuf, line, "LIBEFIVAR_1.38");

	fclose(symout);

	free(guidnames->strtab);
	free(guidnames);

	return 0;
}

// vim:fenc=utf-8:tw=75:noet
