/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012 Red Hat, Inc.
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

#include <ctype.h>
#include <popt.h>
#include <stdio.h>
#include <stdlib.h>

#include "efivar.h"
#include "guid.h"

#define ACTION_LIST	0x1
#define ACTION_PRINT	0x2

#define GUID_FORMAT "%08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x"

static const char *attribute_names[] = {
	"Non-Volatile",
	"Boot Service Access",
	"Runtime Service Access",
	"Hardware Error Record",
	"Authenticated Write Access",
	"Time-Based Authenticated Write Access",
	"Append Write",
	""
};

static void
list_all_variables(void)
{
	efi_guid_t *guid;
	char *name;
	int rc;
	while ((rc = efi_get_next_variable_name(&guid, &name)) > 0)
		 printf(GUID_FORMAT "-%s\n",
		 	guid->a, guid->b, guid->c, bswap_16(guid->d),
			guid->e[0], guid->e[1], guid->e[2], guid->e[3],
			guid->e[4], guid->e[5], name);

	if (rc < 0) {
		fprintf(stderr, "efivar: error listing variables: %m\n");
		exit(1);
	}
}

static void
show_variable(char *guid_name)
{
	efi_guid_t guid;
	char *name;

	int guid_len = strlen("84be9c3e-8a32-42c0-891c-4cd3b072becc");
	/* it has to be at least the length of the guid, a dash, and one more
	 * character */
	if (strlen(guid_name) < guid_len + 2) {
		errno = -EINVAL;
		fprintf(stderr, "efivar: show variable: %m\n");
		exit(1);
	}

	char c = guid_name[guid_len];
	guid_name[guid_len] = '\0';

	int rc = text_to_guid(guid_name, &guid);
	guid_name[guid_len] = c;
	if (rc < 0) {
		errno = EINVAL;
		fprintf(stderr, "efivar: show variable: %m\n");
		exit(1);
	}

	name = guid_name + guid_len + 1;

	uint8_t *data = NULL;
	size_t data_size = 0;
	uint32_t attributes;

	rc = efi_get_variable(guid, name, &data, &data_size, &attributes);
	if (rc < 0) {
		fprintf(stderr, "efivar: show variable: %m\n");
		exit(1);
	}

	printf("GUID: "GUID_FORMAT "\n",
		 	guid.a, guid.b, guid.c, bswap_16(guid.d),
			guid.e[0], guid.e[1], guid.e[2], guid.e[3],
			guid.e[4], guid.e[5]);
	printf("Name: \"%s\"\n", name);
	printf("Attributes:\n");
	for (int i = 0; attribute_names[i][0] != '\0'; i++) {
		if(attributes & (1 << i))
			printf("\t%s\n", attribute_names[i]);
	}
	printf("Value:\n");

	uint32_t index = 0;
	while (index < data_size) {
		char charbuf[] = "................";
		printf("%08x  ", index);
		/* print the hex values, and render the ascii bits into
		 * charbuf */
		while (index < data_size) {
			printf("%02x ", data[index]);
			if (index % 8 == 7)
				printf(" ");
			if (isprint(data[index]))
				charbuf[index % 16] = data[index];
			index++;
			if (index % 16 == 0)
				break;
		}

		/* If we're above data_size, finish out the line with space,
		 * and also finish out charbuf with space */
		while (index >= data_size && index % 16 != 0) {
			if (index % 8 == 7)
				printf(" ");
			printf("   ");
			charbuf[index % 16] = ' ';

			index++;
			if (index % 16 == 0)
				break;
		}
		printf("|%s|\n", charbuf);
	}
}

int main(int argc, char *argv[])
{
	int action = 0;
	char *name = NULL;
	poptContext optCon;
	struct poptOption options[] = {
		{NULL, '\0', POPT_ARG_INTL_DOMAIN, "efivar" },
		{"list", 'l', POPT_ARG_VAL, &action,
		 ACTION_LIST, "list current variables", NULL },
		{"print", 'p', POPT_ARG_STRING, &name, 0,
		 "variable to print, in the form 8be4df61-93ca-11d2-aa0d-00e098032b8c-Boot0000", "<guid-name>" },
		POPT_AUTOALIAS
		POPT_AUTOHELP
		POPT_TABLEEND
	};
	int rc;

	optCon = poptGetContext("efivar", argc, (const char **)argv, options,0);
	
	rc = poptReadDefaultConfig(optCon, 0);
	if (rc < 0) {
		fprintf(stderr, "efivar: poprReadDefaultConfig failed: %s\n",
			poptStrerror(rc));
		exit(1);
	}

	while ((rc = poptGetNextOpt(optCon)) > 0)
		;

	if (rc < -1) {
		fprintf(stderr, "efivar: Invalid argument: %s: %s\n",
			poptBadOption(optCon, 0), poptStrerror(rc));
		exit(1);
	}

	if (poptPeekArg(optCon)) {
		fprintf(stderr, "efivar: Invalid Argument: \"%s\"\n",
			poptPeekArg(optCon));
		exit(1);
	}

	poptFreeContext(optCon);

	if (name)
		action |= ACTION_PRINT;

	switch (action) {
		case ACTION_LIST:
			list_all_variables();
			break;
		case ACTION_PRINT:
			show_variable(name);
			break;
	};

	return 0;
}
