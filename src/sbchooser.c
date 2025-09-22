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
		"Help options:\n"
		"  -?, --help                        Show this help message\n"
		"      --usage                       Display brief usage message\n",
		program_invocation_short_name);
	exit(status);
}

static void
clean_up_context(sbchooser_context_t *ctxp)
{
	memset(ctxp, 0, sizeof (*ctxp));
}

int
main(int argc, char *argv[])
{
	const char sopts[] = ":i:vh";
	const struct option lopts[] = {
		{"verbose", no_argument, NULL, 'v' },
		{"usage", no_argument, NULL, 'h' },
		{"help", no_argument, NULL, 'h' },
		{NULL, 0, NULL, '\0' }
	};
	int c;
	int verbose = 0;
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

	clean_up_context(&ctx);

	return 0;
}

// vim:fenc=utf-8:tw=75:noet
