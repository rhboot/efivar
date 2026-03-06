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
		"\nOptions:\n"
		"  -d, --db=<db file>                UEFI trusted key database\n"
		"  -D, --no-system-db                Do not load the UEFI trusted key database\n"
		"  -s, --system-db                   Load the UEFI trusted key database from\n"
		"                                    this system (default)\n"
		"  -e, --explain                     Instead of acting as a sorter, explain choices\n"
		"  -f, --first-sig-only              Only consider the first signature on an input\n"
		"  -x, --dbx=<dbx file>              UEFI revoked key database\n"
		"  -X, --no-system-dbx               Do not load the UEFI revoked key database\n"
		"  -S, --system-dbx                  Load the UEFI revoked key database from\n"
		"                                    this system (default)\n"
		"  -i, --input=<efi file>            EFI binary for sorting\n"
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

	for (size_t i = 0; i < ctxp->n_files; i++) {
		if (ctxp->files[i] != NULL)
			free_pe(&ctxp->files[i]);
	}
	free(ctxp->files);
	ctxp->n_files = 0;
	ctxp->files = NULL;

	memset(ctxp, 0, sizeof (*ctxp));
}

static int
add_file_to_ctx(sbchooser_context_t *ctxp, pe_file_t *pe)
{
	size_t n_files = ctxp->n_files + 1;
	pe_file_t **files;

	files = reallocarray(ctxp->files, n_files, sizeof (*files));
	if (!files)
		return -1;

	files[ctxp->n_files] = pe;

	ctxp->files = files;
	ctxp->n_files = n_files;

	return 0;
}

static void
add_one_pe_to_ctx(sbchooser_context_t *ctx, const char *filename)
{
	int rc;
	pe_file_t *pe = NULL;

	rc = load_pe(ctx, filename, &pe);
	if (rc < 0) {
		if (filename[0] == '-') {
			warnx("Unknown argument:\"%s\"", filename);
			usage(ERR_USAGE);
		}
		err(ERR_BAD_PE, "Could not open \"%s\"", filename);
	}
	rc = add_file_to_ctx(ctx, pe);
	if (rc < 0)
		err(ERR_BAD_PE, "Could not add \"%s\" to context", filename);
}

int
main(int argc, char *argv[])
{
	const char sopts[] = ":d:Defi:sSx:Xvh";
	const struct option lopts[] = {
		{"db", required_argument, NULL, 'd' },
		{"no-system-db", no_argument, NULL, 'D' },
		{"system-db", no_argument, NULL, 's' },
		{"dbx", required_argument, NULL, 'x' },
		{"no-system-dbx", no_argument, NULL, 'X' },
		{"system-dbx", no_argument, NULL, 'S' },
		{"first-sig-only", no_argument, NULL, 'f' },
		{"explain", no_argument, NULL, 'e' },
		{"in", required_argument, NULL, 'i' },
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
	bool read_inputs_from_stdin = false;
	bool explain = false;

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
		case 'e':
			explain = true;
			break;
		case 'f':
			ctx.first_sig_only = true;
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
		case 'i':
			if (strcmp(argv[optind-1], "-") == 0) {
				read_inputs_from_stdin = true;
				break;
			}
			add_one_pe_to_ctx(&ctx, argv[optind-1]);
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

			add_one_pe_to_ctx(&ctx, argv[optind-1]);
			break;
		}
	}

	if (optind > 0 && !strcmp(argv[optind-1], "--")) {
		while (optind < argc) {
			add_one_pe_to_ctx(&ctx, argv[optind]);
			optind += 1;
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

	if (ctx.n_files == 0 && !isatty(STDIN_FILENO)) {
		read_inputs_from_stdin = true;
	}

	if (read_inputs_from_stdin) {
		while (true) {
			char filename[PATH_MAX+1];
			char *fret;

			memset(filename, 0, sizeof(filename));
			fret = fgets(filename, PATH_MAX, stdin);
			if (fret == NULL) {
				if (feof(stdin))
					break;
				err(ERR_INPUT, "Could not read from stdin");
			}
			for (size_t i = 0; filename[i] != 0; i++) {
				switch (filename[i]) {
				case '\r':
				case '\n':
					filename[i] = '\0';
					break;
				default:
					continue;
				}
			}
			add_one_pe_to_ctx(&ctx, filename);
		}
	}

	if (ctx.n_files == 0) {
		warnx("no input files!");
		exit(ERR_USAGE);
	}

	for (size_t i = 0; i < ctx.n_files; i++) {
		update_pe_security(&ctx, ctx.files[i]);
	}

	qsort(ctx.files, ctx.n_files, sizeof(ctx.files[0]), pe_cmp);

	for (size_t i = 0; i < ctx.n_files; i++) {
		pe_file_t *pe = ctx.files[i];
		digest_data_t *dgst = NULL;
		char buf[1024];

		/*
		 * UEFI spec 2.12ish says:
		 *   – C. Any entry with SignatureListType of EFI_CERT_X509_GUID,
		 *     with SignatureData which contains a certificate with the
		 *     same Issuer, Serial Number, and To-Be-Signed hash included
		 *     in any certificate in the signing chain of the signature
		 *     being verified.
		 *
		 *     Multiple signatures are allowed to exist in the binary's
		 *     certificate table (as per the "Attribute Certificate Table"
		 *     section of the Microsoft PE/COFF Specification). The
		 *     firmware must do the validation according to the following:
		 *
		 *     - If the hash of the binary is in dbx, then the image shall
		 *       fail the validation.
		 *     - Else if the hash of the binary is in db, then the image
		 *       shall pass the validation.
		 *     - Else if one of signatures is in db and is not in dbx, then
		 *       the image shall pass the validation.
		 *     - Else the image shall fail the validation.
		 *
		 * And so we check dbx hashes first, then db.
		 */
		if (is_revoked_by_hash(pe, &dgst)) {
			fmt_digest(dgst, buf, sizeof(buf));
			debug("PE \"%s\" is revoked by hash %s", pe->filename, buf);
			if (explain) {
				printf("%s is revoked by hash %s in dbx\n", pe->filename, buf);
			}
			continue;
		}
		dgst = NULL;
		if (is_trusted_by_hash(pe, &dgst)) {
			fmt_digest(dgst, buf, sizeof(buf));
			debug("PE \"%s\" is trusted by hash %s", pe->filename, buf);
			if (explain) {
				printf("%s is trusted by hash %s in db\n", pe->filename, buf);
			} else {
				printf("%s\n", pe->filename);
			}
			continue;
		} else {
			debug("PE \"%s\" is not trusted by hash", pe->filename);
		}

		debug("PE \"%s\" score 0x%"PRIx32, pe->filename, pe->secbits);
		if (pe->secbits == 0) {
			if (explain) {
				if (!pe->rationale) {
					printf("%s is not trusted because no certs or hashes trust it\n", pe->filename);
				} else {
					printf("%s is not trusted because %s\n", pe->filename, pe->rationale);
				}
			}
		} else {
			if (explain) {
				printf("%s is trusted because %s\n", pe->filename, pe->rationale);
			} else {
				printf("%s\n", pe->filename);
			}
		}
	}

	clean_up_context(&ctx);
	OPENSSL_cleanup();

	return 0;
}

// vim:fenc=utf-8:tw=75:noet
