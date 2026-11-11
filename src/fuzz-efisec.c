// SPDX-License-Identifier: BSD-2-Clause-Patent
/*
 * fuzz-efisec.c - fuzz libefisec.so
 * Copyright Peter Jones <pjones@redhat.com>
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include "include/efivar/efisec.h" // IWYU pragma: keep

#define UNUSED __attribute__((__unused__))

static efi_secdb_visitor_status_t
visit_one_entry(unsigned int listnum UNUSED,
		unsigned int signum UNUSED,
		const efi_guid_t * const owner UNUSED,
		const efi_secdb_type_t algorithm UNUSED,
		const void * const header UNUSED,
		const size_t headersz UNUSED,
		const efi_secdb_data_t * const data UNUSED,
		const size_t datasz UNUSED,
		void *ctxp UNUSED)
{
	FILE *f;

#if defined(FUZZ_VERBOSE)
	printf("listnum:%u signum:%u\n", listnum, signum);
#endif

	f = fopen("/dev/null", "w");
	if (f == NULL)
		return EFI_SECDB_VISITOR_ERROR;

	fwrite(owner, sizeof(efi_guid_t), 1, f);
	fwrite(header, headersz, 1, f);
	fwrite(data, datasz, 1, f);

	fclose(f);

	return EFI_SECDB_VISITOR_CONTINUE;
}

/*
 * This is:
 *
 * 00000000  26 16 c4 c1 4c 50 92 40  ac a9 41 f9 36 93 43 28  |&...LP.@..A.6.C(|  esl[0].signature_type = {sha256}
 * 00000010  7c 00 00 00                                       ||...|              esl[0].signature_list_size = 124 (0x7c)
 * 00000014              00 00 00 00                               |....|          esl[0].signature_header_size = 0
 * 00000018                           30 00 00 00                      |0...|      esl[0].signature_size = 48
 * 0000001c                                                                        esl[0].signature_header (end:0x0000001c)
 * 0000001c                                       bd 9a fa 77              |...w|  esl[0].signature[0].owner = {microsoft}
 * 00000020  59 03 32 4d bd 60 28 f4  e7 8f 78 4b              |Y.2M.`(...xK|
 * 0000002c                                                                        esl[0].signature[0].data (end:0x0000004c)
 * 0000002c                                       e3 b0 c4 42              |...B|  SHA256:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
 * 00000030  98 fc 1c 14 9a fb f4 c8  99 6f b9 24 27 ae 41 e4  |.........o.$'.A.|
 * 00000040  64 9b 93 4c a4 95 99 1b  78 52 b8 55              |d..L....xR.U|
 * 0000004c                                       bd 9a fa 77              |...w|  esl[0].signature[1].owner = {microsoft}
 * 00000050  59 03 32 4d bd 60 28 f4  e7 8f 78 4b              |Y.2M.`(...xK|
 * 0000005c                                                                        esl[0].signature[1].data (end:0x0000007c)
 * 0000005c                                       01 ba 47 19              |..G.|  SHA256:01ba4719c80b6fe911b091a7c05124b64eeece964e09c058ef8f9805daca546b
 * 00000060  c8 0b 6f e9 11 b0 91 a7  c0 51 24 b6 4e ee ce 96  |..o......Q$.N...|
 * 00000070  4e 09 c0 58 ef 8f 98 05  da ca 54 6b              |N..X......Tk|
 * 0000007c
 */
static unsigned char test_db_0[] = {
  0x26, 0x16, 0xc4, 0xc1, 0x4c, 0x50, 0x92, 0x40, 0xac, 0xa9, 0x41, 0xf9,
  0x36, 0x93, 0x43, 0x28, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x30, 0x00, 0x00, 0x00, 0xbd, 0x9a, 0xfa, 0x77, 0x59, 0x03, 0x32, 0x4d,
  0xbd, 0x60, 0x28, 0xf4, 0xe7, 0x8f, 0x78, 0x4b, 0xe3, 0xb0, 0xc4, 0x42,
  0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
  0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b,
  0x78, 0x52, 0xb8, 0x55, 0xbd, 0x9a, 0xfa, 0x77, 0x59, 0x03, 0x32, 0x4d,
  0xbd, 0x60, 0x28, 0xf4, 0xe7, 0x8f, 0x78, 0x4b, 0x01, 0xba, 0x47, 0x19,
  0xc8, 0x0b, 0x6f, 0xe9, 0x11, 0xb0, 0x91, 0xa7, 0xc0, 0x51, 0x24, 0xb6,
  0x4e, 0xee, 0xce, 0x96, 0x4e, 0x09, 0xc0, 0x58, 0xef, 0x8f, 0x98, 0x05,
  0xda, 0xca, 0x54, 0x6b
};
static unsigned int test_db_0_len = 124;

static void
test_0(uint8_t *data, const size_t size)
{
	efi_secdb_t *secdb = NULL;
	int rc = 0;
	void *out = NULL;
	size_t outsz = 0;

	rc = efi_secdb_parse(data, size, &secdb);
	if (rc < 0)
		return;

	/*
	 * make sure we also merge two databases...
	 */
	rc = efi_secdb_parse(data, size, &secdb);
	if (rc < 0)
		goto err_secdb_free;

	rc = efi_secdb_realize(secdb, &out, &outsz);
	if (rc < 0)
		goto err_secdb_free;
	free(out);

	rc = efi_secdb_visit_entries(secdb, visit_one_entry, NULL);

err_secdb_free:
	efi_secdb_free(secdb);
}

static void
test_1(uint8_t *data, const size_t size)
{
	efi_secdb_t *secdb = NULL;
	int rc;
	void *out = NULL;
	size_t outsz = 0;

	rc = efi_secdb_parse(test_db_0, test_db_0_len, &secdb);
	if (rc < 0)
		return;

	/*
	 * make sure we also merge two databases...
	 */
	rc = efi_secdb_parse(data, size, &secdb);
	if (rc < 0)
		goto err_secdb_free;

	rc = efi_secdb_realize(secdb, &out, &outsz);
	if (rc < 0)
		goto err_secdb_free;
	free(out);

	rc = efi_secdb_visit_entries(secdb, visit_one_entry, NULL);

err_secdb_free:
	efi_secdb_free(secdb);
}

static void
test_2(uint8_t *data, const size_t size)
{
	efi_secdb_t *secdb = NULL;
	int rc = 0;
	void *out = NULL;
	size_t outsz = 0;

	rc = efi_secdb_parse(data, size, &secdb);
	if (rc < 0)
		return;

	/*
	 * make sure we also merge two databases...
	 */
	rc = efi_secdb_parse(test_db_0, test_db_0_len, &secdb);
	if (rc < 0)
		goto err_secdb_free;

	rc = efi_secdb_realize(secdb, &out, &outsz);
	if (rc < 0)
		goto err_secdb_free;
	free(out);

	rc = efi_secdb_visit_entries(secdb, visit_one_entry, NULL);

err_secdb_free:
	efi_secdb_free(secdb);
}

int __lsan_is_turned_off(void)
{
	return 1;
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
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

	test_0(data_copy, size);
	test_1(data_copy, size);
	test_2(data_copy, size);

	free(data_copy);

	return 0;
}

#ifdef STANDALONE
int main(int argc, char *argv[]) {
	uint8_t buf[4096];
	FILE *f = NULL;
	size_t bufsz;
	char *filename = "src/fuzz-efisec-corpus/crash-da39a3ee5e6b4b0d3255bfef95601890afd80709";

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
