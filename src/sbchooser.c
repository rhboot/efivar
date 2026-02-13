// SPDX-License-Identifier: GPL-v3-or-later
/*
 * sbchooser.c - utility to choose between secure boot enabled bootloaders
 *		 to install.
 * Copyright Peter Jones <pjones@redhat.com>
 */

#include "sbchooser.h"

extern char *optarg;
extern int optind, opterr, optopt;

static void NORETURN
usage(int status)
{
	fprintf(status == 0 ? stdout : stderr,
		"Usage: %s [OPTION...]\n"
		"  -d, --db=<db file>                UEFI trusted key database\n"
		"  -D, --no-system-db                Do not load the UEFI trusted key database\n"
		"  -s, --system-db                   Load the UEFI trusted key database from\n"
		"                                    this system (default)\n"
		"  -x, --dbx=<dbx file>              UEFI revoked key database\n"
		"  -X, --no-system-dbx               Do not load the UEFI revoked key database\n"
		"  -S, --system-dbx                  Load the UEFI revoked key database from\n"
		"                                    this system (default)\n"
		"Help options:\n"
		"  -?, --help                        Show this help message\n"
		"      --usage                       Display brief usage message\n",
		program_invocation_short_name);
	exit(status);
}

static void
clean_up_context(sbchooser_context_t *ctxp)
{
	free_secdb_info(ctxp);

	memset(ctxp, 0, sizeof (*ctxp));
}

int
main(int argc, char *argv[])
{
	const char sopts[] = ":d:Di:sSx:Xvh";
	const struct option lopts[] = {
		{"db", required_argument, NULL, 'd' },
		{"no-system-db", no_argument, NULL, 'D' },
		{"system-db", no_argument, NULL, 's' },
		{"dbx", required_argument, NULL, 'x' },
		{"no-system-dbx", no_argument, NULL, 'X' },
		{"system-dbx", no_argument, NULL, 'S' },
		{"verbose", no_argument, NULL, 'v' },
		{"usage", no_argument, NULL, 'h' },
		{"help", no_argument, NULL, 'h' },
		{NULL, 0, NULL, '\0' }
	};
	int c;
	int verbose = 0;
	int rc;
	bool needs_db = true;
	bool needs_dbx = true;

	sbchooser_context_t ctx;

	memset(&ctx, 0, sizeof(ctx));

	ctx.db = efi_secdb_new();
	if (!ctx.db)
		err(ERR_SECDB, "could not allocate memory");
	efi_secdb_set_bool(ctx.db, EFI_SECDB_SORT, false);
	efi_secdb_set_bool(ctx.db, EFI_SECDB_SORT_DATA, false);
	efi_secdb_set_bool(ctx.db, EFI_SECDB_SORT_DESCENDING, false);
	ctx.dbx = efi_secdb_new();
	if (!ctx.dbx)
		err(ERR_SECDB, "could not allocate memory");
	efi_secdb_set_bool(ctx.dbx, EFI_SECDB_SORT, false);
	efi_secdb_set_bool(ctx.dbx, EFI_SECDB_SORT_DATA, false);
	efi_secdb_set_bool(ctx.dbx, EFI_SECDB_SORT_DESCENDING, false);

	while (true) {
		int option_index = 0;

		opterr = 0;
		c = getopt_long(argc, argv, sopts, lopts, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case ':':
			errx(ERR_USAGE, "Error: '--%s' requires an argument", lopts[optind].name);
			break;
		case 'D':
			needs_db = false;
			break;
		case 'd':
			rc = load_secdb_from_file(argv[optind-1], &ctx.db);
			if (rc < 0)
				err(ERR_SECDB, "Could not load db from \"%s\"",
				    argv[optind-1]);
			needs_db = false;
			break;
		case 'h':
			usage(ERR_SUCCESS);
			break;
		case 's':
			rc = load_secdb_from_var("db", &efi_guid_security, &ctx.db);
			if (rc < 0 && errno != ENOENT)
				err(ERR_SECDB, "Could not load db from EFI variable");
			needs_db = false;
			break;
		case 'S':
			rc = load_secdb_from_var("dbx", &efi_guid_security, &ctx.dbx);
			if (rc < 0 && errno != ENOENT)
				err(ERR_SECDB, "Could not load dbx from EFI variable");
			needs_dbx = false;
			break;
		case 'X':
			needs_dbx = false;
			break;
		case 'x':
			rc = load_secdb_from_file(argv[optind-1], &ctx.dbx);
			if (rc < 0)
				err(ERR_SECDB, "Could not load dbx from \"%s\"",
				    argv[optind-1]);
			needs_dbx = false;
			break;
		case 'v':
			verbose += 1;
			efi_set_verbose(verbose, stderr);
			if (verbose) {
				setvbuf(stdout, NULL, _IONBF, 0);
			}
			break;
		case '?':
			if (optopt == '?')
				usage(ERR_SUCCESS);
			warnx("Unknown argument:\"%s\"", argv[optind-1]);
			usage(ERR_USAGE);
			break;
		default:
			if (strcmp(argv[optind-1], "-?") == 0)
				usage(ERR_SUCCESS);
			warnx("Unknown argument:\"%s\"", argv[optind-1]);
			usage(ERR_USAGE);
			break;
		}
	}

	if ((c == -1 && argc > 1) && optind < argc) {
		warnx("Unknown argument:\"%s\"", argv[optind]);
		usage(ERR_USAGE);
	}

	if (needs_db) {
		rc = load_secdb_from_var("db", &efi_guid_security, &ctx.db);
		if (rc < 0 && errno != ENOENT)
			err(ERR_SECDB, "Could not load db from EFI variable");
	}

	if (needs_dbx) {
		rc = load_secdb_from_var("dbx", &efi_guid_security, &ctx.dbx);
		if (rc < 0 && errno != ENOENT)
			err(ERR_SECDB, "Could not load db from EFI variable");
	}

	rc = parse_secdb_info(&ctx);
	if (rc < 0) {
		errx(ERR_SECDB, "couldn't parse secdb info");
	}

	clean_up_context(&ctx);

	return 0;
}

// vim:fenc=utf-8:tw=75:noet
