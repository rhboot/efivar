// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * efisecdb.c - efi signature list management tool
 * Copyright Peter Jones <pjones@redhat.com>
 * Copyright Red Hat, Inc.
 */
#include "fix_coverity.h"

#include "linux.h"
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "efisec.h"

extern char *optarg;
extern int optind, opterr, optopt;

static efi_secdb_t *secdb = NULL;
static list_t infiles;
static list_t actions;

struct hash_param {
	char *name;
	efi_secdb_type_t algorithm;
	ssize_t size;
	bool def;
};

static struct hash_param hash_params[] = {
	{.name = "sha512",
	 .algorithm = SHA512,
	 .size = 64,
	 .def = false,
	},
	{.name = "sha256",
	 .algorithm = SHA256,
	 .size = 32,
	 .def = true,
	},
	{.name = "sha1",
	 .algorithm = SHA1,
	 .size = 20,
	 .def = false,
	},
};
static int n_hash_params = sizeof(hash_params) / sizeof(hash_params[0]);

static void
set_hash_parameters(char *name, int *hash_number)
{
	FILE *out;
	int def = -1;

	if (strcmp(name, "help")) {
		out = stderr;
		for (int i = 0; i < n_hash_params; i++) {
			if (!strcmp(name, hash_params[i].name)) {
				*hash_number = i;
				return;
			}
		}
		warnx("Invalid hash type \"%s\"", name);
	} else {
		out = stdout;
	}

	fprintf(out, "Supported hashes:");
	for (int i = 0; i < n_hash_params; i++) {
		fprintf(out, " %s", hash_params[i].name);
		if (hash_params[i].def)
			def = i;
	}
	fprintf(out, "\n");
	if (def >= 0)
		fprintf(out, "Default hash is %s\n", hash_params[def].name);
	exit(out == stderr);
}

static void
secdb_warnx(const char * const fmt, ...)
{
	va_list ap;
	int errnum = errno;

	fflush(stdout);
	fprintf(stderr, "%s: ", program_invocation_short_name);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	errno = errnum;
	fprintf(stderr, "\n");
	show_errors();
}

static void NORETURN
secdb_err(int status, const char * const fmt, ...)
{
	va_list ap;
	int errnum = errno;

	fflush(stdout);
	fprintf(stderr, "%s: ", program_invocation_short_name);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	errno = errnum;
	fprintf(stderr, ": %m\n");
	show_errors();
	exit(status);
}

static void NORETURN
secdb_errx(int status, const char * const fmt, ...)
{
	va_list ap;
	int errnum = errno;

	fflush(stdout);
	fprintf(stderr, "%s: ", program_invocation_short_name);
	va_start(ap, fmt);
	errno = errnum;
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	show_errors();
	exit(status);
}

static void NORETURN
usage(int status)
{
	fprintf(status == 0 ? stdout : stderr,
		"Usage: %s [OPTION...]\n"
		"  -i, --infile=<file>       input database\n"
		"  -o, --outfile=<file>      output database\n"
		"  -a, --add                 following hashes or certs are to be added (default)\n"
		"  -r, --remove              following hashes or certs are to be removed\n"
		"  -g, --owner-guid=<GUID>   following added entries use GUID as the owner\n"
		"  -h, --hash=<hash>         hash value to add (\n"
		"  -t, --type=<hash-type>    hash type to add (\"help\" lists options)\n"
		"  -c, --certificate=<file>  certificate file to add\n"
		"  -L, --list-guids          list well known guids\n",
		program_invocation_short_name);
	exit(status);
}

typedef enum {
	ADD,
	REMOVE
} action_type_t;

typedef struct {
	list_t list;

	action_type_t action;
	efi_guid_t owner;
	efi_secdb_type_t algorithm;
	uint8_t *data;
	size_t datasz;
} action_t;
#define for_each_action(pos, head) list_for_each(pos, head)
#define for_each_action_safe(pos, n, head) list_for_each_safe(pos, n, head)

static void
add_action(list_t *list, action_type_t action_type, const efi_guid_t *owner,
	   efi_secdb_type_t algorithm, uint8_t *data, size_t datasz)
{
	action_t *action;

	if (action_type == ADD && efi_guid_is_empty(owner))
		errx(1, "no owner spefified for --add");

	action = calloc(1, sizeof(action_t));
	if (!action)
		err(1, "could not allocate memory");
	action->action = action_type;
	action->owner = *owner;
	action->algorithm = algorithm;
	action->data = data;
	action->datasz = datasz;
	list_add_tail(&action->list, list);
}

static void
free_actions(void)
{
	list_t *pos, *tmp;

	for_each_action_safe(pos, tmp, &actions) {
		action_t *action = list_entry(pos, action_t, list);

		list_del(&action->list);
		xfree(action->data);
		free(action);
	}
}

static void
free_infiles(void)
{
	list_t *pos, *tmp;

	for_each_ptr_safe(pos, tmp, &infiles) {
		ptrlist_t *entry = list_entry(pos, ptrlist_t, list);

		list_del(&entry->list);
		free(entry);
	}
}

static void
maybe_free_secdb(void)
{
	if (secdb == NULL)
		return;

	efi_secdb_free(secdb);
}

static void
check_hash_index(int hash_index)
{
	if (hash_index < 0 || hash_index >= n_hash_params)
		errx(1, "hash type is not set");
}

static void
list_guids(void)
{
	const struct efivar_guidname *guid = &efi_well_known_guids[0];
	const uint64_t n = efi_n_well_known_guids;
	unsigned int i;

	debug("&guid[0]:%p n:%lu", &guid[0], n);
	for (i = 0; i < n; i++) {
		printf("{"GUID_FORMAT"}\t",
		       GUID_FORMAT_ARGS(&guid[i].guid));
		printf("{%s}\t", guid[i].name);
		printf("%s\t", guid[i].symbol);
		printf("%s\n", guid[i].description);
	}
}

/*
 * The return value here is the UNIX shell convention, 0 is success, > 0 is
 * failure.
 */
static int
parse_input_files(list_t *infiles, efi_secdb_t **secdb, bool dump)
{
	int status = 0;
	list_t *pos, *tmp;
	int rc;

	for_each_ptr_safe(pos, tmp, infiles) {
		int infd = -1;
		uint8_t *siglist = NULL;
		size_t siglistsz = 0;
		char *infile;
		ptrlist_t *entry = list_entry(pos, ptrlist_t, list);

		infile = entry->ptr;

		debug("adding input file %s entry:%p", infile, entry);
		infd = open(infile, O_RDONLY);
		if (infd < 0)
			err(1, "could not open \"%s\"", infile);

		rc = read_file(infd, &siglist, &siglistsz);
		if (rc < 0)
			err(1, "could not read \"%s\"", infile);
		siglistsz -= 1;
		close(infd);

		rc = efi_secdb_parse(siglist, siglistsz, secdb);
		efi_error_clear();
		if (rc < 0) {
			/* haaaack city */
			debug("*****************************");
			debug(" starting over with offset 4");
			debug("*****************************");
			if (siglistsz > 4 && !(*(uint32_t *)siglist & ~0x7ffu))
				rc = efi_secdb_parse(&siglist[4], siglistsz-4,
						     secdb);
			if (rc < 0) {
				secdb_warnx("could not parse input file \"%s\"", infile);
				if (!dump)
					exit(1);
				status = 1;
				break;
			}
		}
		xfree(siglist);
		list_del(&entry->list);
		free(entry);
	}

	return status;
}

int
main(int argc, char *argv[])
{
	efi_guid_t owner = efi_guid_empty;
	int rc;
	action_type_t mode = ADD;
	list_t *pos, *tmp;
	int c, i;
	int hash_index = -1;
	bool force = false;
	int verbose = 0;
	bool dump = false;
	bool annotate = false;
	bool wants_add_actions = false;
	bool did_list_guids = false;
	bool do_sort = true;
	bool do_sort_data = false;
	bool sort_descending = false;
	int status = 0;
	char *outfile = NULL;

	const char sopts[] = ":aAc:dfg:h:i:Lo:rs:t:v?";
	const struct option lopts[] = {
		{"add", no_argument, NULL, 'a' },
		{"annotate", no_argument, NULL, 'A' },
		{"certificate", required_argument, NULL, 'c' },
		{"dump", no_argument, NULL, 'd' },
		{"force", no_argument, NULL, 'f' },
		{"owner-guid", required_argument, NULL, 'g' },
		{"hash", required_argument, NULL, 'h' },
		{"infile", required_argument, NULL, 'i' },
		{"list-guids", no_argument, NULL, 'L' },
		{"outfile", required_argument, NULL, 'o' },
		{"remove", no_argument, NULL, 'r' },
		{"sort", required_argument, NULL, 's' },
		{"type", required_argument, NULL, 't' },
		{"verbose", no_argument, NULL, 'v' },
		{"usage", no_argument, NULL, '?' },
		{"help", no_argument, NULL, '?' },
		{NULL, 0, NULL, '\0' }
	};

	INIT_LIST_HEAD(&infiles);
	INIT_LIST_HEAD(&actions);

	atexit(free_actions);
	atexit(free_infiles);
	atexit(maybe_free_secdb);

	/*
	 * parse the command line.
	 *
	 * note that we don't really process the security database inputs
	 * here, and the cert and hash add/remove must be kept in order as
	 * supplied.
	 */
	opterr = 0;
	while ((c = getopt_long(argc, argv, sopts, lopts, &i)) != -1) {
		uint8_t *data;
		ssize_t datasz;

		switch (c) {
		case 'A':
			dump = true;
			annotate = true;
			break;
		case 'a':
			mode = ADD;
			break;
		case 'c':
			if (optarg == NULL)
				secdb_errx(1, "--certificate requires a value");
			datasz = get_file(&data, "%s", optarg);
			if (datasz < 0)
				secdb_err(1, "could not read certificate \"%s\"",
					  optarg);
			datasz -= 1;

			// this is arbitrary but still much too small
			if (datasz < 16)
				secdb_err(1, "certificate \"%s\" is invalid",
					  optarg);

			debug("%s certificate of %zd bytes",
			      mode == ADD ? "adding" : "removing", datasz);
			if (mode == ADD)
				wants_add_actions = true;
			add_action(&actions, mode, &owner, X509_CERT, data, datasz);
			break;
		case 'd':
			dump = true;
			break;
		case 'f':
			force = true;
			break;
		case 'g':
			if (optarg == NULL)
				secdb_errx(1, "--owner-guid requires a value");
			rc = efi_id_guid_to_guid(optarg, &owner);
			if (rc < 0)
				secdb_errx(1, "could not parse guid \"%s\"", optarg);
			break;
		case 'h':
			if (optarg == NULL)
				secdb_errx(1, "--hash requires a value");
			check_hash_index(hash_index);
			datasz = strlen(optarg);
			if (datasz != hash_params[hash_index].size * 2)
				secdb_errx(1,
					   "hash \"%s\" requires a %zd-bit value, but supplied value is %zd bits",
					   hash_params[hash_index].name,
					   hash_params[hash_index].size * 8,
					   datasz * 4);
			datasz >>= 1;
			data = hex_to_bin(optarg, datasz);
			debug("%s hash %s", mode == ADD ? "adding" : "removing", optarg);
			if (mode == ADD)
				wants_add_actions = true;
			add_action(&actions, mode, &owner,
				   hash_params[hash_index].algorithm,
				   data, datasz);
			break;
		case 'i':
			if (optarg == NULL)
				secdb_errx(1, "--infile requires a value");
			ptrlist_add(&infiles, optarg);
			break;
		case 'L':
			list_guids();
			did_list_guids = true;
			break;
		case 'o':
			if (outfile)
				secdb_errx(1, "--outfile cannot be used multiple times.");
			if (optarg == NULL)
				secdb_errx(1, "--outfile requires a value");
			outfile = optarg;
			break;
		case 'r':
			mode = REMOVE;
			break;
		case 's':
			if (optarg == NULL) {
sort_err:
				secdb_errx(1, "--sort requires one of \"type\", \"data\", \"all\", or \"none\", \"ascending\", \"descending\"");
			}
			if (!strcmp(optarg, "type")) {
				do_sort = true;
				do_sort_data = false;
			} else if (!strcmp(optarg, "data")) {
				do_sort = false;
				do_sort_data = true;
			} else if (!strcmp(optarg, "all")) {
				do_sort = true;
				do_sort_data = true;
			} else if (!strcmp(optarg, "none")) {
				do_sort = false;
				do_sort_data = false;
			} else if (!strcmp(optarg, "ascending")) {
				sort_descending = false;
			} else if (!strcmp(optarg, "descending")) {
				sort_descending = true;
			} else if (!strcmp(optarg, "help")) {
				printf("sort options are: type data all none ascending descending");
				exit(0);
			} else {
				goto sort_err;
			}
			break;
		case 't':
			if (optarg == NULL)
				secdb_errx(1, "--type requires a value");
			set_hash_parameters(optarg, &hash_index);
			break;
		case 'v':
			if (optarg) {
				long v;

				errno = 0;
				v = strtol(optarg, NULL, 0);
				verbose = (errno == ERANGE) ? verbose + 1 : v;
			} else {
				verbose += 1;
			}
			break;
		case '?':
			usage(0);
			break;
		case ':':
			if (optarg != NULL)
				errx(1, "option '%c' does not take an argument (\"%s\")",
				     optopt, optarg);
		}
	}

	setenv("NSS_DEFAULT_DB_TYPE", "sql", 0);
	efi_set_verbose(verbose, stderr);
	if (verbose) {
		setvbuf(stdout, NULL, _IONBF, 0);
	}

	if (!outfile && !dump) {
		if (did_list_guids)
			return 0;
		errx(1, "no output file specified");
	}
	if (list_empty(&infiles) && !wants_add_actions)
		errx(1, "no input files or database additions");

	secdb = efi_secdb_new();
	if (!secdb)
		err(1, "could not allocate memory");
	debug("top secdb:%p", secdb);

	efi_secdb_set_bool(secdb, EFI_SECDB_SORT, do_sort);
	efi_secdb_set_bool(secdb, EFI_SECDB_SORT_DATA, do_sort_data);
	efi_secdb_set_bool(secdb, EFI_SECDB_SORT_DESCENDING, sort_descending);

	status = parse_input_files(&infiles, &secdb, dump);
	if (status == 0) {
		for_each_action_safe(pos, tmp, &actions) {
			action_t *action = list_entry(pos, action_t, list);

			if (action->action == ADD) {
				debug("adding %d entry", action->algorithm);
				efi_secdb_add_entry(secdb, &action->owner,
						    action->algorithm,
						    (efi_secdb_data_t *)action->data,
						    action->datasz);
			} else {
				debug("removing %d entry", action->algorithm);
				efi_secdb_del_entry(secdb, &action->owner,
						    action->algorithm,
						    (efi_secdb_data_t *)action->data,
						    action->datasz);
			}
			list_del(&action->list);
			free(action->data);
			free(action);
		}
	}

	if (dump)
		secdb_dump(secdb, annotate);

	if (!outfile)
		exit(status);

	int outfd = -1;
	int flags = O_WRONLY | O_CREAT | (force ? 0 : O_EXCL);

	debug("adding output file %s", outfile);
	outfd = open(outfile, flags, 0600);
	if (outfd < 0) {
		char *tmpoutfile = outfile;
		if (errno != EEXIST)
			unlink(outfile);
		err(1, "could not open \"%s\"", tmpoutfile);
	}

	rc = ftruncate(outfd, 0);
	if (rc < 0) {
		unlink(outfile);
		err(1, "could not truncate output file \"%s\"", outfile);
	}

	void *output;
	size_t size = 0;
	rc = efi_secdb_realize(secdb, &output, &size);
	if (rc < 0) {
		unlink(outfile);
		secdb_err(1, "could not realize signature list");
	}

	rc = write(outfd, output, size);
	if (rc < 0) {
		unlink(outfile);
		err(1, "could not write signature list");
	}

	close(outfd);
	xfree(output);

	return 0;
}
