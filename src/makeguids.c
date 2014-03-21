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

struct guidname well_known_guids[] = {
};
char well_known_guids_end;

struct guidname well_known_names[] = {
};
char well_known_names_end;

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
	if (argc != 4)
		exit(1);
	
	int in, guidout, nameout;

	in = open(argv[1], O_RDONLY);
	if (in < 0)
		err(1, "makeguids: could not open \"%s\"", argv[1]);

	guidout = open(argv[2], O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (guidout < 0)
		err(1, "makeguids: could not open \"%s\"", argv[2]);

	nameout = open(argv[3], O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (nameout < 0)
		err(1, "makeguids: could not open \"%s\"", argv[3]);

	char *inbuf = NULL;
	size_t inlen = 0;
	int rc = read_file(in, &inbuf, &inlen);
	if (rc < 0)
		err(1, "makeguids: could not read \"%s\"", argv[1]);
	
	/* strictly speaking, this *has* to be too large. */
	struct guidname *outbuf = calloc(inlen, sizeof (char));
	if (!outbuf)
		err(1, "makeguids");
	
	char *guidstr = inbuf;
	int line;
	for (line = 1; guidstr - inbuf < inlen; line++) {
		char *name = strchr(guidstr, '\t');
		if (name == NULL)
			err(1, "makeguids: \"%s\": invalid data on line %d",
				argv[1], line);
		*name = '\0';
		name += 1;

		char *end = strchr(name, '\n');
		if (end == NULL)
			err(1, "makeguids: \"%s\": invalid data on line %d",
				argv[1], line);
		*end = '\0';

		efi_guid_t guid;
		rc = efi_str_to_guid(guidstr, &guid);
		if (rc < 0)
			err(1, "makeguids: \"%s\": invalid data on line %d",
				argv[1], line);

		memcpy(&outbuf[line-1].guid, &guid, sizeof(guid));
		strncpy(outbuf[line-1].name, name, 39);

		guidstr = end+1;
	}
	printf("%d lines\n", line-1);
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
}
