// SPDX-License-Identifier: BSD-2-Clause-Patent
/*
 * fuzz-efidp-format.c - fuzz efidp_format_device_path()
 * Copyright Peter Jones <pjones@redhat.com>
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include "include/efivar/efisec.h" // IWYU pragma: keep

#define UNUSED __attribute__((__unused__))

static int
test_0(uint8_t *data, const size_t size)
{
	int rc = 0;
	size_t sz;
	char *outbuf;

	rc = efidp_format_device_path(NULL, 0, (efidp)data, size);
	if (rc < 0) {
		goto err;
	}
	sz = rc;

	rc = -1;
	outbuf = calloc(1, sz);
	if (!outbuf) {
		goto err;
	}

	rc = efidp_format_device_path((uint8_t *)outbuf, sz, (efidp)data, size);

	free(outbuf);

err:
	return rc;
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	int rc = 0;
	uint8_t *data_copy;

	if (size < 1)
		return -1;

#if defined(FUZZ_VERBOSE)
	efi_set_verbose(2, stderr);
#endif

	data_copy = malloc(size);
	if (!data_copy)
		return -1;

	memcpy(data_copy, data, size);

	rc = test_0(data_copy, size);
	if (rc < 0)
		goto err;

err:
	free(data_copy);

	return 0;
}

#ifdef STANDALONE
int main(int argc, char *argv[]) {
	uint8_t buf[4096];
	FILE *f = NULL;
	size_t bufsz;
	char *filename = "src/fuzz-efidp-format-corpus/crash-adc83b19e793491b1c6ea0fd8b46cd9f32e592fc";

	if (argc == 2)
		filename = argv[1];
	f = fopen(filename, "r");
	if (!f)
		err(1, "Couldn't open \"%s\"", filename);

	bufsz = fread(buf, 1, 1024, f);
	fclose(f);

	return LLVMFuzzerTestOneInput(buf, bufsz);
}
#endif


// vim:fenc=utf-8:tw=75:noet
