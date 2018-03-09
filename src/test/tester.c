/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <efivar/efivar.h>

#define TEST_GUID EFI_GUID(0x84be9c3e,0x8a32,0x42c0,0x891c,0x4c,0xd3,0xb0,0x72,0xbe,0xcc)

uint8_t *
__attribute__((malloc))
__attribute__((alloc_size(1)))
get_random_bytes(size_t size)
{
	uint8_t *ret = NULL;
	int errno_saved = 0;
	int fd = -1;

	if (!size)
		return ret;

	uint8_t *retdata = malloc(size);
	if (!retdata)
		goto fail;

	fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0)
		goto fail;

	int rc = read(fd, retdata, size);
	if (rc < 0)
		goto fail;

	ret = retdata;
	retdata = NULL;
fail:
	errno_saved = errno;

	if (fd >= 0)
		close(fd);

	if (retdata)
		free(retdata);

	errno = errno_saved;
	return ret;
}

struct test {
	const char *name;
	size_t size;
	int result;
};

static void print_error(int line, struct test *test, int rc, char *fmt, ...)
	__attribute__ ((format (printf, 4, 5)));
static void print_error(int line, struct test *test, int rc, char *fmt, ...)
{
	fprintf(stderr, "FAIL: \"%s\"(line %d) (%d) ", test->name, line, rc);
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#define report_error(test, ret, rc, ...) ({			\
		__typeof(errno) __errno_saved = errno;		\
		free(testdata);					\
		if (data) {					\
			free(data);				\
			data = NULL;				\
		}						\
		if (test->result != rc) {				\
			print_error(__LINE__, test, rc, __VA_ARGS__);	\
			ret = -1;					\
			errno = __errno_saved;				\
			goto fail;					\
		}							\
		errno = __errno_saved;				\
	})

int do_test(struct test *test)
{
	int rc = -1;
	errno = 0;

	uint8_t *testdata = NULL;
	uint8_t *data = NULL;

	testdata = get_random_bytes(test->size);
	if (testdata == NULL && errno != 0) {
		perror(test->name);
		return -1;
	}

	int ret = 0;

	printf("testing efi_set_variable()\n");
	rc = efi_set_variable(TEST_GUID, test->name,
			      testdata, test->size,
			      EFI_VARIABLE_BOOTSERVICE_ACCESS |
			      EFI_VARIABLE_RUNTIME_ACCESS |
			      EFI_VARIABLE_NON_VOLATILE, 0600);
	if (rc < 0) {
		report_error(test, ret, rc, "set test failed: %m\n");
	}

	size_t datasize = 0;
	uint32_t attributes = 0;

	printf("testing efi_get_variable_size()\n");
	rc = efi_get_variable_size(TEST_GUID, test->name, &datasize);
	if (rc < 0)
		report_error(test, ret, rc, "get size test failed: %m\n");

	printf("testing efi_get_variable_exists()\n");
	rc = efi_get_variable_exists(TEST_GUID, test->name);
	if (rc < 0)
		report_error(test, ret, rc, "get exists test failed: %m\n");

	if (datasize != test->size)
		report_error(test, ret, -1, "get size test failed: wrong size: %zd should be %zd\n", datasize, test->size);

	printf("testing efi_get_variable()\n");
	rc = efi_get_variable(TEST_GUID, test->name, &data, &datasize,
			      &attributes);
	if (rc < 0)
		report_error(test, ret, rc, "get test failed: %m\n");

	if (datasize != test->size)
		report_error(test, ret, -1, "get size test failed: wrong size: %zd should be %zd\n", datasize, test->size);

	if (testdata != NULL && test->size > 0)
		if (memcmp(data, testdata, test->size))
			report_error(test, ret, rc,
					"get test failed: bad data\n");

	free(data);
	data = NULL;

	if (attributes != (EFI_VARIABLE_BOOTSERVICE_ACCESS |
			   EFI_VARIABLE_RUNTIME_ACCESS |
			   EFI_VARIABLE_NON_VOLATILE))
		report_error(test, ret, rc, "get test failed: wrong attributes\n");

	printf("testing efi_get_variable_attributes()\n");
	rc = efi_get_variable_attributes(TEST_GUID, test->name, &attributes);
	if (rc < 0)
		report_error(test, ret, rc, "get attributes test failed: %m\n");

	if (attributes != (EFI_VARIABLE_BOOTSERVICE_ACCESS |
			   EFI_VARIABLE_RUNTIME_ACCESS |
			   EFI_VARIABLE_NON_VOLATILE))
		report_error(test, ret, rc, "get attributes test failed: wrong attributes\n");

	printf("testing efi_del_variable()\n");
	rc = efi_del_variable(TEST_GUID, test->name);
	if (rc < 0)
		report_error(test, ret, rc, "del test failed: %m\n");

	rc = efi_set_variable(TEST_GUID, test->name,
			      testdata, test->size,
			      EFI_VARIABLE_BOOTSERVICE_ACCESS |
			      EFI_VARIABLE_RUNTIME_ACCESS |
			      EFI_VARIABLE_NON_VOLATILE,
			      0600);
	if (rc < 0) {
		report_error(test, ret, rc, "set test failed: %m\n");
	}

	printf("testing efi_append_variable()\n");
	rc = efi_append_variable(TEST_GUID, test->name,
				testdata, test->size,
				EFI_VARIABLE_APPEND_WRITE |
				EFI_VARIABLE_BOOTSERVICE_ACCESS |
				EFI_VARIABLE_RUNTIME_ACCESS |
				EFI_VARIABLE_NON_VOLATILE);
	if (rc < 0) {
		report_error(test, ret, rc, "append test failed: %m\n");
	}

	printf("testing efi_get_variable()\n");
	rc = efi_get_variable(TEST_GUID, test->name, &data, &datasize,
			      &attributes);
	if (rc < 0)
		report_error(test, ret, rc, "get test failed: %m\n");

	if (datasize != test->size * 2)
		report_error(test, ret, -1, "get size test failed: wrong size: %zd should be %zd (append may be at fault)\n", datasize, test->size * 2);

	if (memcmp(data, testdata, test->size))
		report_error(test, ret, rc, "get test failed: bad data\n");
	if (memcmp(data + test->size, testdata, test->size))
		report_error(test, ret, rc, "get test failed: bad data\n");

	printf("testing efi_del_variable()\n");
	rc = efi_del_variable(TEST_GUID, test->name);
	if (rc < 0 && test->size != 0)
		report_error(test, ret, rc, "del test failed: %m\n");
	else
		ret = test->result;

	free(data);
	free(testdata);
fail:
	if (ret != test->result)
		return -1;
	return 0;
}

int main(void)
{
	if (!efi_variables_supported()) {
		printf("UEFI variables not supported on this machine.\n");
		return 0;
	}

	struct test tests[] = {
		{.name=	"empty", .size = 0, .result= -1},
		{.name= "one", .size = 1, .result= 0 },
		{.name= "two", .size = 2, .result= 0 },
		{.name= "three", .size = 3, .result= 0 },
		{.name= "four", .size = 4, .result= 0 },
		{.name= "five", .size = 5, .result= 0 },
		{.name= "seven", .size = 7, .result= 0 },
		{.name= "eight", .size = 8, .result= 0 },
		{.name= "nine", .size = 9, .result= 0 },
		{.name= "fifteen", .size = 15, .result= 0 },
		{.name= "sixteen", .size = 16, .result= 0 },
		{.name= "seventeen", .size = 17, .result= 0 },
		{.name= "thirtyone", .size = 31, .result= 0 },
		{.name= "thirtytwo", .size = 32, .result= 0 },
		{.name= "thirtythree", .size = 33, .result= 0 },
		{.name= "tentwentyfour", .size = 1024, .result= 0},
		{.name= "tentwentyfive", .size = 1025, .result= 0},
		{.name= "", .size = 0, .result= 0}
	};

	for (int x = 0; tests[x].name[0] != '\0'; x++) {
		efi_del_variable(TEST_GUID, tests[x].name);
	}

	int ret = 0;

	for (int x = 0; tests[x].name[0] != '\0'; x++) {
		printf("About to test %s\n", tests[x].name);
		int rc = do_test(&tests[x]);
		if (rc < 0) {
			efi_del_variable(TEST_GUID, tests[x].name);
			ret = 1;
			break;
		}
	}
	return ret;
}
