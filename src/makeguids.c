/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012-2013 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "efivar.h"
#include "util.h"
#include "guid.h"

efi_guid_t const efi_guid_zero = {0};
efi_guid_t const efi_guid_empty = {0};

struct guidname efi_well_known_guids[] = {
};
char efi_well_known_guids_end;

struct guidname efi_well_known_names[] = {
};
char efi_well_known_names_end;

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

	for (unsigned int i = 0; i < line-1; i++) {
		if (!strcmp(outbuf[i].symbol, "efi_guid_zero"))
			fprintf(symout, "\t.globl %s\n"
					"\t.data\n"
					"\t.balign 1\n"
					"\t.type %s, %%object\n"
					"\t.size %s, %s_end - %s\n",
				"efi_guid_empty", "efi_guid_empty",
				"efi_guid_empty", "efi_guid_empty",
				"efi_guid_empty");
		fprintf(symout, "\t.globl %s\n"
				"\t.data\n"
				"\t.balign 1\n"
				"\t.type %s, %%object\n"
				"\t.size %s, %s_end - %s\n"
				"%s:\n",
			outbuf[i].symbol,
			outbuf[i].symbol,
			outbuf[i].symbol,
			outbuf[i].symbol,
			outbuf[i].symbol,
			outbuf[i].symbol);
		if (!strcmp(outbuf[i].symbol, "efi_guid_zero"))
			fprintf(symout, "efi_guid_empty:\n");

		uint8_t *guid_data = (uint8_t *) &outbuf[i].guid;
		for (unsigned int j = 0; j < sizeof (efi_guid_t); j++)
			fprintf(symout,"\t.byte 0x%02x\n", guid_data[j]);

		fprintf(symout, "%s_end:\n", outbuf[i].symbol);
		if (!strcmp(outbuf[i].symbol, "efi_guid_zero")) {
			fprintf(symout, "efi_guid_empty_end:\n");
			fprintf(header, "extern const efi_guid_t efi_guid_empty;\n");
		}

		fprintf(header, "extern const efi_guid_t %s;\n", outbuf[i].symbol);
	}

	fprintf(header, "\n#endif /* EFIVAR_GUIDS_H */\n");
	fprintf(symout, "#if defined(__linux__) && defined(__ELF__)\n.section .note.GNU-stack,\"\",%%progbits\n#endif");
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

	return 0;
}
