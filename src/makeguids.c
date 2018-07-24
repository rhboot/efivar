/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012-2013 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#include "fix_coverity.h"

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "efivar.h"

efi_guid_t const efi_guid_zero = {0};
efi_guid_t const efi_guid_empty = {0};

struct guidname efi_well_known_guids;
struct guidname efi_well_known_guids_end;
struct guidname efi_well_known_names;
struct guidname efi_well_known_names_end;

static int
cmpguidp(const void *p1, const void *p2)
{
	struct guidname *gn1 = (struct guidname *)p1;
	struct guidname *gn2 = (struct guidname *)p2;

	return memcmp(&gn1->guid, &gn2->guid, sizeof (gn1->guid));
}

static int
cmpnamep(const void *p1, const void *p2)
{
	struct guidname *gn1 = (struct guidname *)p1;
	struct guidname *gn2 = (struct guidname *)p2;

	return memcmp(gn1->name, gn2->name, sizeof (gn1->name));
}

struct guid_aliases {
	char *name;
	char *alias;
};

static struct guid_aliases guid_aliases[] = {
	{ "efi_guid_empty", "efi_guid_zero" },
	{ "efi_guid_redhat", "efi_guid_fwupdate" },
	{ NULL, NULL }
};

static void make_aliases(FILE *symout, FILE *header,
			 const char *alias, const uint8_t *guid_data)
{
	for (unsigned int i = 0; guid_aliases[i].name != NULL; i++) {
		if (!strcmp(guid_aliases[i].alias, alias)) {
			fprintf(symout,
				"\nconst efi_guid_t\n"
				"\t__attribute__((__visibility__ (\"default\")))\n"
				"\t%s = {cpu_to_le32(0x%02x%02x%02x%02x),cpu_to_le16(0x%02x%02x),cpu_to_le16(0x%02x%02x),cpu_to_be16(0x%02x%02x),{0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x}};\n\n",
				guid_aliases[i].name,
				guid_data[3], guid_data[2],
				guid_data[1], guid_data[0],
				guid_data[5], guid_data[4],
				guid_data[7], guid_data[6],
				guid_data[8], guid_data[9],
				guid_data[10], guid_data[11],
				guid_data[12], guid_data[13],
				guid_data[14], guid_data[15]);

			fprintf(header,
				"extern const efi_guid_t %s __attribute__((__visibility__ (\"default\")));\n",
				guid_aliases[i].name);
		}
	}
}

int
main(int argc, char *argv[])
{
	if (argc != 6)
		exit(1);

	int in, guidout, nameout;
	int rc;

	FILE *symout, *header;

	in = open(argv[1], O_RDONLY);
	if (in < 0)
		err(1, "makeguids: could not open \"%s\"", argv[1]);

	guidout = open(argv[2], O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (guidout < 0)
		err(1, "makeguids: could not open \"%s\"", argv[2]);

	nameout = open(argv[3], O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (nameout < 0)
		err(1, "makeguids: could not open \"%s\"", argv[3]);

	symout = fopen(argv[4], "w");
	if (symout == NULL)
		err(1, "makeguids: could not open \"%s\"", argv[4]);
	rc = chmod(argv[4], 0644);
	if (rc < 0)
		warn("makeguids: chmod(%s, 0644)", argv[4]);

	header = fopen(argv[5], "w");
	if (header == NULL)
		err(1, "makeguids: could not open \"%s\"", argv[5]);
	rc = chmod(argv[5], 0644);
	if (rc < 0)
		warn("makeguids: chmod(%s, 0644)", argv[5]);

	char *inbuf = NULL;
	size_t inlen = 0;
	rc = read_file(in, (uint8_t **)&inbuf, &inlen);
	if (rc < 0)
		err(1, "makeguids: could not read \"%s\"", argv[1]);

	struct guidname *outbuf = malloc(1);
	if (!outbuf)
		err(1, "makeguids");

	char *guidstr = inbuf;
	unsigned int line;
	for (line = 1; (uintptr_t)guidstr - (uintptr_t)inbuf < inlen; line++) {
		if (guidstr && guidstr[0] == '\0')
			break;
		outbuf = realloc(outbuf, line * sizeof (struct guidname));
		if (!outbuf)
			err(1, "makeguids");
		memset(outbuf + line - 1, 0, sizeof(struct guidname));

		char *symbol = strchr(guidstr, '\t');
		if (symbol == NULL)
			err(1, "makeguids: \"%s\": 1 invalid data on line %d",
				argv[1], line);
		*symbol = '\0';
		symbol += 1;

		char *name = strchr(symbol, '\t');
		if (name == NULL)
			err(1, "makeguids: \"%s\": 2 invalid data on line %d",
				argv[1], line);
		*name = '\0';
		name += 1;

		char *end = strchr(name, '\n');
		if (end == NULL)
			err(1, "makeguids: \"%s\": 3 invalid data on line %d",
				argv[1], line);
		*end = '\0';

		efi_guid_t guid;
		rc = efi_str_to_guid(guidstr, &guid);
		if (rc < 0)
			err(1, "makeguids: \"%s\": 4 invalid data on line %d",
				argv[1], line);

		memcpy(&outbuf[line-1].guid, &guid, sizeof(guid));
		strcpy(outbuf[line-1].symbol, "efi_guid_");
		strncat(outbuf[line-1].symbol, symbol,
					255 - strlen("efi_guid_"));
		strncpy(outbuf[line-1].name, name, 255);

		guidstr = end+1;
	}
	printf("%d lines\n", line-1);

	fprintf(header, "#ifndef EFIVAR_GUIDS_H\n#define EFIVAR_GUIDS_H 1\n\n");

	fprintf(symout, "#include <efivar/efivar.h>\n");
	fprintf(symout, "#include <endian.h>\n");
	fprintf(symout, """\n\
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

	for (unsigned int i = 0; i < line-1; i++) {
		uint8_t *guid_data = (uint8_t *) &outbuf[i].guid;

		if (!strcmp(outbuf[i].symbol, "efi_guid_zzignore-this-guid"))
			break;

		make_aliases(symout, header, outbuf[i].symbol, guid_data);

		fprintf(header, "extern const efi_guid_t %s __attribute__((__visibility__ (\"default\")));\n", outbuf[i].symbol);

		fprintf(symout, "const\n"
			"__attribute__((__visibility__ (\"default\")))\n"
			"efi_guid_t %s = {cpu_to_le32(0x%02x%02x%02x%02x),cpu_to_le16(0x%02x%02x),cpu_to_le16(0x%02x%02x),cpu_to_be16(0x%02x%02x),{0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x}};\n\n",
			outbuf[i].symbol,
			guid_data[3], guid_data[2],
			guid_data[1], guid_data[0],
			guid_data[5], guid_data[4],
			guid_data[7], guid_data[6],
			guid_data[8], guid_data[9],
			guid_data[10], guid_data[11],
			guid_data[12], guid_data[13],
			guid_data[14], guid_data[15]);
	}

	fprintf(header, "\n#endif /* EFIVAR_GUIDS_H */\n");
	fclose(header);
	fclose(symout);

	qsort(outbuf, line-1, sizeof (struct guidname), cmpguidp);
	rc = write(guidout, outbuf, sizeof (struct guidname) * (line - 1));
	if (rc < 0)
		err(1, "makeguids");

	qsort(outbuf, line-1, sizeof (struct guidname), cmpnamep);
	rc = write(nameout, outbuf, sizeof (struct guidname) * (line - 1));
	if (rc < 0)
		err(1, "makeguids");
	close(in);
	close(guidout);
	close(nameout);
	free(inbuf);

	return 0;
}
