// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012 Red Hat, Inc.
 */

#include "fix_coverity.h"

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

extern char *optarg;
extern int optind, opterr, optopt;

#include "efivar.h"
#include "efivar/efivar-guids.h"

#define ACTION_USAGE		0x00
#define ACTION_LIST		0x01
#define ACTION_PRINT		0x02
#define ACTION_APPEND		0x04
#define ACTION_LIST_GUIDS	0x08
#define ACTION_WRITE		0x10
#define ACTION_PRINT_DEC	0x20
#define ACTION_IMPORT		0x40
#define ACTION_EXPORT		0x80

#define EDIT_APPEND	0
#define EDIT_WRITE	1

#define SHOW_VERBOSE	0
#define SHOW_DECIMAL	1

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

static inline void
validate_name(const char *name)
{
	if (name == NULL) {
err:
		warnx("Invalid variable name \"%s\"",
		      (name == NULL) ? "(null)" : name);
		show_errors();
		exit(1);
	}
	if (name[0] == '{') {
		const char *next = strchr(name+1, '}');
		if (!next)
			goto err;
		if (next[1] != '-')
			goto err;
		if (next[2] == '\000')
			goto err;
	} else {
		if (strlen(name) < 38)
			goto err;
		if (name[8] != '-' || name[13] != '-' ||
		    name[18] != '-' || name[23] != '-' ||
		    name[36] != '-')
			goto err;
	}
}

static void
list_all_variables(void)
{
	efi_guid_t *guid = NULL;
	char *name = NULL;
	int rc;
	while ((rc = efi_get_next_variable_name(&guid, &name)) > 0)
		 printf(GUID_FORMAT "-%s\n", GUID_FORMAT_ARGS(guid), name);

	if (rc < 0) {
		fprintf(stderr, "efivar: error listing variables: %m\n");
		show_errors();
		exit(1);
	}
}

static void
parse_name(const char *guid_name, char **name, efi_guid_t *guid)
{
	unsigned int guid_len = sizeof("84be9c3e-8a32-42c0-891c-4cd3b072becc");
	char guid_buf[guid_len + 2];
	int rc;
	off_t name_pos = 0;

	const char *left, *right;

	validate_name(guid_name);

	left = strchr(guid_name, '{');
	right = strchr(guid_name, '}');
	if (left && right) {
		if (right[1] != '-' || right[2] == '\0') {
bad_name:
			errno = -EINVAL;
			fprintf(stderr, "efivar: invalid name \"%s\"\n",
				guid_name);
			show_errors();
			exit(1);
		}
		name_pos = right + 1 - guid_name;

		strncpy(guid_buf, guid_name, name_pos);
		guid_buf[name_pos++] = '\0';

		rc = efi_id_guid_to_guid(guid_buf, guid);
		if (rc < 0)
			goto bad_name;
	} else {
		/* it has to be at least the length of the guid, a dash, and
		 * one more character */
		if (strlen(guid_name) < guid_len + 2)
			goto bad_name;
		name_pos = guid_len - 1;

		if (guid_name[name_pos] != '-' || guid_name[name_pos+1] == '\0')
			goto bad_name;
		name_pos++;

		memset(guid_buf, 0, sizeof(guid_buf));
		strncpy(guid_buf, guid_name, guid_len - 1);

		rc = text_to_guid(guid_buf, guid);
		if (rc < 0)
			goto bad_name;
	}

	char *name_buf = NULL;
	int name_len;
	name_len = strlen(guid_name + name_pos) + 1;
	name_buf = calloc(1, name_len);
	if (!name_buf) {
		fprintf(stderr, "efivar: %m\n");
		exit(1);
	}
	strcpy(name_buf, guid_name + name_pos);
	*name = name_buf;
}

static void
show_variable_data(efi_guid_t guid, const char *name, uint32_t attributes,
		   uint8_t *data, size_t data_size,
		   int display_type)
{
	if (display_type == SHOW_VERBOSE) {
		printf("GUID: "GUID_FORMAT "\n", GUID_FORMAT_ARGS(&guid));
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
				if (safe_to_print(data[index]))
					charbuf[index % 16] = data[index];
				index++;
				if (index % 16 == 0)
					break;
			}

			/* If we're above data_size, finish out the line with
			 * space, and also finish out charbuf with space */
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
	} else if (display_type == SHOW_DECIMAL) {
		uint32_t index = 0;
		while (index < data_size) {
			// print the dec values
			while (index < data_size) {
				printf("%d ", data[index]);
				if (index % 8 == 7)
					printf(" ");
				index++;
				if (index % 16 == 0)
					break;
			}
		}
		printf("\n");
	}
}

static void
show_variable(char *guid_name, int display_type)
{
	efi_guid_t guid = efi_guid_empty;
	char *name = NULL;
	int rc;

	uint8_t *data = NULL;
	size_t data_size = 0;
	uint32_t attributes;

	parse_name(guid_name, &name, &guid);
	if (!name || efi_guid_is_empty(&guid)) {
		fprintf(stderr, "efivar: could not parse variable name.\n");
		show_errors();
		exit(1);
	}

	errno = 0;
	rc = efi_get_variable(guid, name, &data, &data_size, &attributes);
	if (rc < 0) {
		fprintf(stderr, "efivar: show variable: %m\n");
		show_errors();
		exit(1);
	}

	show_variable_data(guid, name, attributes,
			   data, data_size, display_type);

	free(name);
	if (data)
		free(data);
}

static void
save_variable_data(efi_variable_t *var, char *outfile, bool dmpstore)
{
	FILE *out = NULL;
	ssize_t sz;
	uint8_t *data = NULL;
	size_t datasz = 0;
	ssize_t (*export)(efi_variable_t *var, uint8_t *data, size_t size) =
		dmpstore ? efi_variable_export_dmpstore : efi_variable_export;

	out = fopen(outfile, "w");
	if (!out)
		err(1, "Could not open \"%s\" for writing", outfile);

	sz = export(var, data, datasz);
	data = calloc(sz, 1);
	if (!data)
		err(1, "Could not allocate memory");
	datasz = sz;

	sz = export(var, data, datasz);
	if (sz < 0)
		err(1, "Could not format data");
	datasz = sz;

	sz = fwrite(data, 1, datasz, out);
	if (sz < (ssize_t)datasz)
		err(1, "Could not write to \"%s\"", outfile);

	fflush(out);
	fclose(out);
}

static void
save_variable(char *guid_name, char *outfile, bool dmpstore)
{
	efi_guid_t guid = efi_guid_empty;
	char *name = NULL;
	int rc;

	uint8_t *data = NULL;
	size_t data_size = 0;
	uint32_t attributes = 7;
	efi_variable_t *var;

	parse_name(guid_name, &name, &guid);
	if (!name || efi_guid_is_empty(&guid)) {
		fprintf(stderr, "efivar: could not parse variable name.\n");
		show_errors();
		exit(1);
	}

	errno = 0;
	rc = efi_get_variable(guid, name, &data, &data_size, &attributes);
	if (rc < 0) {
		fprintf(stderr, "efivar: show variable: %m\n");
		show_errors();
		exit(1);
	}

	var = efi_variable_alloc();
	if (!var) {
		fprintf(stderr, "efivar: could not allocate variable storage.\n");
		show_errors();
		exit(1);
	}

	efi_variable_set_name(var, (unsigned char *)name);
	efi_variable_set_guid(var, &guid);
	efi_variable_set_attributes(var, attributes);
	efi_variable_set_data(var, data, data_size);

	save_variable_data(var, outfile, dmpstore);

	efi_variable_free(var, false);

	free(name);
	if (data)
		free(data);
}

static void
edit_variable(const char *guid_name, void *data, size_t data_size,
	      uint32_t attrib, int edit_type)
{
	efi_guid_t guid = efi_guid_empty;
	char *name = NULL;
	int rc;
	uint8_t *old_data = NULL;
	size_t old_data_size = 0;
	uint32_t old_attributes = 0;

	parse_name(guid_name, &name, &guid);
	if (!name || efi_guid_is_empty(&guid)) {
		fprintf(stderr, "efivar: could not parse variable name.\n");
		show_errors();
		exit(1);
	}

	rc = efi_get_variable(guid, name, &old_data, &old_data_size, &old_attributes);
	/* Ignore errors, as -a can be used to create a variable */
	if (attrib != 0)
		old_attributes = attrib;

	switch (edit_type){
		case EDIT_APPEND:
			rc = efi_append_variable(guid, name,
						 data, data_size,
						 old_attributes);
			break;
		case EDIT_WRITE:
			rc = efi_set_variable(guid, name,
					      data, data_size,
					      old_attributes, 0644);
			break;
	}

	free(name);
	if (old_data)
		free(old_data);

	if (rc < 0) {
		fprintf(stderr, "efivar: %m\n");
		show_errors();
		exit(1);
	}
}

static void
prepare_data(const char *filename, uint8_t **data, size_t *data_size)
{
	int fd = -1;
	void *buf;
	size_t buflen = 0;
	struct stat statbuf;
	int rc;

	if (filename == NULL) {
		fprintf(stderr, "Input filename must be provided.\n");
		exit(1);
	}

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		goto err;

	memset(&statbuf, '\0', sizeof(statbuf));
	rc = fstat(fd, &statbuf);
	if (rc < 0)
		goto err;

	buflen = statbuf.st_size;
	buf = mmap(NULL, buflen, PROT_READ, MAP_PRIVATE|MAP_POPULATE, fd, 0);
	if (buf == MAP_FAILED)
		goto err;

	*data = buf;
	*data_size = buflen;

	close(fd);
	return;
err:
	if (fd >= 0)
		close(fd);
	fprintf(stderr, "Could not use \"%s\": %m\n", filename);
	exit(1);
}

static void __attribute__((__noreturn__))
usage(int ret)
{
	FILE *out = ret == 0 ? stdout : stderr;
	fprintf(out,
		"Usage: %s [OPTION...]\n"
		"  -A, --attributes=<attributes>     attributes to use on append\n"
		"  -l, --list                        list current variables\n"
		"  -p, --print                       print variable specified by --name\n"
		"  -D, --dmpstore                    use DMPSTORE format when exporting\n"
		"  -d, --print-decimal               print variable in decimal values specified\n"
		"                                    by --name\n"
		"  -n, --name=<guid-name>            variable to manipulate, in the form\n"
		"                                    8be4df61-93ca-11d2-aa0d-00e098032b8c-Boot0000\n"
		"  -a, --append                      append to variable specified by --name\n"
		"  -f, --datafile=<file>             load or save variable contents from <file>\n"
		"  -e, --export=<file>               export variable to <file>\n"
		"  -i, --import=<file>               import variable from <file\n"
		"  -L, --list-guids                  show internal guid list\n"
		"  -w, --write                       write to variable specified by --name\n\n"
		"Help options:\n"
		"  -?, --help                        Show this help message\n"
		"      --usage                       Display brief usage message\n",
		program_invocation_short_name);
	exit(ret);
}

int main(int argc, char *argv[])
{
	int c = 0;
	int i = 0;
	int action = 0;
	uint8_t *data = NULL;
	size_t data_size = 0;
	char *guid_name = NULL;
	char *infile = NULL;
	char *outfile = NULL;
	char *datafile = NULL;
	bool dmpstore = false;
	int verbose = 0;
	uint32_t attributes = EFI_VARIABLE_NON_VOLATILE
			      | EFI_VARIABLE_BOOTSERVICE_ACCESS
			      | EFI_VARIABLE_RUNTIME_ACCESS;
	char *sopts = "aA:Dde:f:i:Llpn:vw?";
	struct option lopts[] = {
		{"append", no_argument, 0, 'a'},
		{"attributes", required_argument, 0, 'A'},
		{"datafile", required_argument, 0, 'f'},
		{"dmpstore", no_argument, 0, 'D'},
		{"export", required_argument, 0, 'e'},
		{"help", no_argument, 0, '?'},
		{"import", required_argument, 0, 'i'},
		{"list", no_argument, 0, 'l'},
		{"list-guids", no_argument, 0, 'L'},
		{"name", required_argument, 0, 'n'},
		{"print", no_argument, 0, 'p'},
		{"print-decimal", no_argument, 0, 'd'},
		{"usage", no_argument, 0, 0},
		{"verbose", no_argument, 0, 'v'},
		{"write", no_argument, 0, 'w'},
		{0, 0, 0, 0}
	};

	while ((c = getopt_long(argc, argv, sopts, lopts, &i)) != -1) {
		switch (c) {
			case 'A':
				attributes = strtoul(optarg, NULL, 0);
				if (errno == ERANGE || errno == EINVAL)
					err(1, "invalid argument for -A: %s",
					    optarg);
				break;
			case 'a':
				action |= ACTION_APPEND;
				break;
			case 'D':
				dmpstore = true;
				break;
			case 'd':
				action |= ACTION_PRINT_DEC;
				break;
			case 'e':
				action |= ACTION_EXPORT;
				outfile = optarg;
				break;
			case 'f':
				datafile = optarg;
				break;
			case 'i':
				action |= ACTION_IMPORT;
				infile = optarg;
				break;
			case 'L':
				action |= ACTION_LIST_GUIDS;
				break;
			case 'l':
				action |= ACTION_LIST;
				break;
			case 'n':
				guid_name = optarg;
				break;
			case 'p':
				action |= ACTION_PRINT;
				break;
			case 'v':
				verbose += 1;
				break;
			case 'w':
				action |= ACTION_WRITE;
				break;
			case '?':
				usage(EXIT_SUCCESS);
				break;
			case 0:
				if (strcmp(lopts[i].name, "usage"))
					usage(EXIT_SUCCESS);
				break;
		}
	}

	efi_set_verbose(verbose, stderr);

	if (guid_name && !outfile)
		action |= ACTION_PRINT;

	switch (action) {
		case ACTION_LIST:
			list_all_variables();
			break;
		case ACTION_PRINT:
			show_variable(guid_name, SHOW_VERBOSE);
			break;
		case ACTION_PRINT_DEC | ACTION_PRINT:
			show_variable(guid_name, SHOW_DECIMAL);
			break;
		case ACTION_APPEND | ACTION_PRINT:
			prepare_data(datafile, &data, &data_size);
			edit_variable(guid_name, data, data_size, attributes,
				      EDIT_APPEND);
			break;
		case ACTION_WRITE | ACTION_PRINT:
			prepare_data(datafile, &data, &data_size);
			edit_variable(guid_name, data, data_size, attributes,
				      EDIT_WRITE);
			break;
		case ACTION_LIST_GUIDS: {
			const struct efivar_guidname *guid = &efi_well_known_guids[0];
			const uint64_t n = efi_n_well_known_guids;
			size_t i;

			debug("&guid[0]:%p n:%lu end:%p", &guid[0], n, &guid[n]);
			for (i = 0; i < n; i++) {
				printf("{"GUID_FORMAT"}\t",
				       GUID_FORMAT_ARGS(&guid[i].guid));
				printf("{%s}\t", guid[i].name);
				printf("%s\t", guid[i].symbol);
				printf("%s\n", guid[i].description);
			}
			break;
					}
		case ACTION_EXPORT:
			if (datafile) {
				char *name = NULL;
				efi_guid_t guid = efi_guid_zero;
				efi_variable_t *var;

				parse_name(guid_name, &name, &guid);
				prepare_data(datafile, &data, &data_size);

				var = efi_variable_alloc();
				if (!var)
					err(1, "Could not allocate memory");

				efi_variable_set_name(var, (unsigned char *)name);
				efi_variable_set_guid(var, &guid);
				efi_variable_set_attributes(var, attributes);
				efi_variable_set_data(var, data, data_size);

				save_variable_data(var, outfile, dmpstore);

				efi_variable_free(var, false);
			} else {
				save_variable(guid_name, outfile, dmpstore);
			}
			break;
		case ACTION_IMPORT:
		case ACTION_IMPORT | ACTION_PRINT:
		case ACTION_IMPORT | ACTION_PRINT | ACTION_PRINT_DEC:
			{
				ssize_t sz;
				efi_variable_t *var = NULL;
				char *name;
				efi_guid_t *guid;
				uint64_t attributes;
				int display_type = (action & ACTION_PRINT_DEC)
					? SHOW_VERBOSE|SHOW_DECIMAL
					: SHOW_VERBOSE;


				prepare_data(infile, &data, &data_size);
				sz = efi_variable_import(data, data_size, &var);
				if (sz < 0)
					err(1, "Could not import data from \"%s\"", infile);

				munmap(data, data_size);
				data = NULL;
				data_size = 0;

				name = (char *)efi_variable_get_name(var);
				efi_variable_get_guid(var, &guid);
				efi_variable_get_attributes(var, &attributes);
				efi_variable_get_data(var, &data, &data_size);

				if (datafile) {
					FILE *out;
					int rc;

					out = fopen(datafile, "w");
					if (!out)
						err(1, "Could not open \"%s\" for writing",
						    datafile);

					rc = fwrite(data, data_size, 1, out);
					if (rc < (long)data_size)
						err(1, "Could not write to \"%s\"",
						    datafile);

					fclose(out);
					free(guid_name);
				}
				if (action & ACTION_PRINT)
					show_variable_data(*guid, name,
						((uint32_t)(attributes & 0xffffffff)),
						 data, data_size, display_type);

				efi_variable_free(var, false);
				break;
			}
		case ACTION_IMPORT | ACTION_EXPORT:
			{
				efi_variable_t *var = NULL;
				ssize_t sz;

				if (datafile)
					errx(1, "--datafile cannot be used with --import and --export");

				prepare_data(infile, &data, &data_size);
				sz = efi_variable_import(data, data_size, &var);
				if (sz < 0)
					err(1, "Could not import data from \"%s\"", infile);

				save_variable_data(var, outfile, dmpstore);

				efi_variable_free(var, false);
				break;
			}
		case ACTION_USAGE:
		default:
			usage(EXIT_FAILURE);
	};

	return 0;
}

// vim:fenc=utf-8:tw=75:noet
