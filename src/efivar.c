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
#include <fcntl.h>
#include <popt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "efivar.h"
#include "guid.h"

#define ACTION_LIST	0x1
#define ACTION_PRINT	0x2
#define ACTION_APPEND	0x4

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
	efi_guid_t *guid = NULL;
	char *name = NULL;
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
parse_name(const char *guid_name, char **name, efi_guid_t *guid)
{
	unsigned int guid_len = strlen("84be9c3e-8a32-42c0-891c-4cd3b072becc");
	char guid_buf[guid_len + 1];
	char *name_buf = NULL;
	int name_len;

	/* it has to be at least the length of the guid, a dash, and one more
	 * character */
	if (strlen(guid_name) < guid_len + 2) {
		errno = -EINVAL;
		fprintf(stderr, "efivar: invalid name \"%s\"\n", guid_name);
		exit(1);
	}

	memset(guid_buf, 0, sizeof(guid_buf));
	strncpy(guid_buf, guid_name, guid_len);

	name_len = (strlen(guid_name) + 1) - (guid_len + 2);
	name_buf = calloc(1, name_len);
	if (!name_buf) {
		fprintf(stderr, "efivar: %m\n");
		exit(1);
	}
	strncpy(name_buf, guid_name + guid_len + 1, name_len);

	int rc = text_to_guid(guid_buf, guid);
	if (rc < 0) {
		errno = EINVAL;
		fprintf(stderr, "efivar: invalid name \"%s\"\n", guid_name);
		exit(1);
	}

	*name = name_buf;
}

static void
show_variable(char *guid_name)
{
	efi_guid_t guid;
	char *name = NULL;
	int rc;

	uint8_t *data = NULL;
	size_t data_size = 0;
	uint32_t attributes;

	parse_name(guid_name, &name, &guid);

	errno = 0;
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

static void
append_variable(const char *guid_name, void *data, size_t data_size, int attrib)
{
	efi_guid_t guid;
	char *name = NULL;
	int rc;
	uint8_t *old_data = NULL;
	size_t old_data_size = 0;
	uint32_t old_attributes = 0;

	parse_name(guid_name, &name, &guid);

	rc = efi_get_variable(guid, name, &old_data, &old_data_size,
				&old_attributes);

	if (attrib != 0)
		old_attributes = attrib;

	rc = efi_append_variable(guid, name, data, data_size,
				old_attributes);
	if (rc < 0) {
		fprintf(stderr, "efivar: %m\n");
		exit(1);
	}
}

static void
validate_name(const char *name)
{
	if (name == NULL) {
		fprintf(stderr, "Invalid variable name\n");
		exit(1);
	}
}

static void
prepare_data(const char *filename, void **data, size_t *data_size)
{
	int fd = -1;
	void *buf;
	size_t buflen = 0;
	struct stat statbuf;
	int rc;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		goto err;

	memset(&statbuf, '\0', sizeof (statbuf));
	rc = fstat(fd, &statbuf);
	if (rc < 0)
		goto err;

	buflen = statbuf.st_size;
	buf = mmap(NULL, buflen, PROT_READ, MAP_PRIVATE|MAP_POPULATE, fd, 0);
	if (!buf)
		goto err;

	*data = buf;
	*data_size = buflen;

	return;
err:
	fprintf(stderr, "Could not use \"%s\": %m\n", filename);
	exit(1);
}

int main(int argc, char *argv[])
{
	int action = 0;
	char *name = NULL;
	char *file = NULL;
	int attributes = 0;
	poptContext optCon;
	struct poptOption options[] = {
		{NULL, '\0', POPT_ARG_INTL_DOMAIN, "efivar" },
		{"list", 'l', POPT_ARG_VAL, &action,
		 ACTION_LIST, "list current variables", NULL },
		{"print", 'p', POPT_ARG_VAL, &action,
		 ACTION_PRINT, "print variable specified by --name", NULL },
		{"name", 'n', POPT_ARG_STRING, &name, 0,
		 "variable to manipulate, in the form 8be4df61-93ca-11d2-aa0d-00e098032b8c-Boot0000", "<guid-name>" },
		{"append", 'a', POPT_ARG_VAL, &action,
		 ACTION_APPEND, "append to variable specified by --name", NULL },
		{"fromfile", 'f', POPT_ARG_STRING, &file, 0,
		 "use data from <file>", "<file>" },
		{"attributes", 't', POPT_ARG_INT, &attributes, 0,
		 "attributes to use on append", "<attributes>" },
		POPT_AUTOALIAS
		POPT_AUTOHELP
		POPT_TABLEEND
	};
	int rc;
	void *data = NULL;
	size_t data_size = 0;

	optCon = poptGetContext("efivar", argc, (const char **)argv, options,0);

	rc = poptReadDefaultConfig(optCon, 0);
	if (rc < 0 && !(rc == POPT_ERROR_ERRNO && errno == ENOENT)) {
		fprintf(stderr, "efivar: poptReadDefaultConfig failed: %s\n",
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
			validate_name(name);
			show_variable(name);
			break;
		case ACTION_APPEND | ACTION_PRINT:
			validate_name(name);
			prepare_data(file, &data, &data_size);
			append_variable(name, data, data_size, attributes);
	};

	return 0;
}
