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
	for (size_t i = 0; i < ctxp->n_files; i++) {
		if (ctxp->files[i] != NULL)
			free_pe(&ctxp->files[i]);
	}
	free(ctxp->files);
	ctxp->n_files = 0;
	ctxp->files = NULL;
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

	rc = load_pe(filename, &pe);
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
	const char sopts[] = ":i:vh";
	const struct option lopts[] = {
		{"in", required_argument, NULL, 'i' },
		{"verbose", no_argument, NULL, 'v' },
		{"usage", no_argument, NULL, 'h' },
		{"help", no_argument, NULL, 'h' },
		{NULL, 0, NULL, '\0' }
	};
	int c;
	int verbose = 0;
	bool read_inputs_from_stdin = false;

	sbchooser_context_t ctx;

	memset(&ctx, 0, sizeof(ctx));

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
		case 'h':
			usage(ERR_SUCCESS);
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
		pe_file_t *pe = ctx.files[i];
		printf("%s\n", pe->filename);
	}

	clean_up_context(&ctx);

	return 0;
}

// vim:fenc=utf-8:tw=75:noet
