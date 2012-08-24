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

#include <efivar.h>

#define TEST_GUID EFI_GUID(0x84be9c3e,0x8a32,0x42c0,0x891c,0x4c,0xd3,0xb0,0x72,0xbe,0xcc)

unsigned int get_random_bytes(size_t size, uint8_t **data)
{
	int ret = -1;
	int errno_saved = 0;

	if (!size) {
		*data = NULL;
		return 0;
	}

	uint8_t *retdata = malloc(size);
	if (!retdata)
		goto fail;

	int fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0)
		goto fail;

	int rc = read(fd, retdata, size);
	if (rc < 0)
		goto fail;

	*data = retdata;
	retdata = NULL;
	ret = 0;
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

static void print_error(struct test *test, int rc, char *fmt, ...)
	__attribute__ ((format (printf, 3, 4)));
static void print_error(struct test *test, int rc, char *fmt, ...)
{
	fprintf(stderr, "FAIL: \"%s\" (%d) ", test->name, rc);
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#define report_error(test, ret, rc, ...) ({			\
		__typeof(errno) __errno_saved = errno;		\
		if (test->result != rc)				\
			print_error(test, rc, __VA_ARGS__);	\
		free(testdata);					\
		ret = -1;					\
		if (data) {					\
			free(data);				\
			data = NULL;				\
		}						\
		errno = __errno_saved;				\
		goto fail;					\
	})

int do_test(struct test *test)
{
	int rc;

	uint8_t *testdata = NULL;

	rc = get_random_bytes(test->size, &testdata);
	if (rc < 0)
		return rc;

	int ret = 0;

	rc = efi_set_variable(TEST_GUID, test->name,
			      testdata, test->size,
			      EFI_VARIABLE_BOOTSERVICE_ACCESS |
			      EFI_VARIABLE_RUNTIME_ACCESS | 
			      EFI_VARIABLE_NON_VOLATILE);
	if (rc < 0) {
		report_error(test, ret, rc, "set test failed: %m\n");
	}

	uint8_t *data = alloca(test->size);
	size_t datasize = 0;
	uint32_t attributes = 0;

	rc = efi_get_variable_size(TEST_GUID, test->name, &datasize);
	if (rc < 0)
		report_error(test, ret, rc, "get size test failed: %m\n");

	if (datasize != test->size)
		report_error(test, ret, rc, "get size test failed: wrong size\n");

	rc = efi_get_variable(TEST_GUID, test->name, &data, &datasize,
			      &attributes);
	if (rc < 0)
		report_error(test, ret, rc, "get test failed: %m\n");

	if (datasize != test->size)
		report_error(test, ret, rc, "get test failed: wrong size\n");

	if (memcmp(data, testdata, test->size))
		report_error(test, ret, rc, "get test failed: bad data\n");

	if (attributes != (EFI_VARIABLE_BOOTSERVICE_ACCESS |
			   EFI_VARIABLE_RUNTIME_ACCESS |
			   EFI_VARIABLE_NON_VOLATILE))
		report_error(test, ret, rc, "get test failed: wrong attributes\n");

	rc = efi_get_variable_attributes(TEST_GUID, test->name, &attributes);
	if (rc < 0)
		report_error(test, ret, rc, "get attributes test failed: %m\n");

	if (attributes != (EFI_VARIABLE_BOOTSERVICE_ACCESS |
			   EFI_VARIABLE_RUNTIME_ACCESS |
			   EFI_VARIABLE_NON_VOLATILE))
		report_error(test, ret, rc, "get attributes test failed: wrong attributes\n");

	rc = efi_del_variable(TEST_GUID, test->name);
	if (rc < 0)
		report_error(test, ret, rc, "del test failed: %m\n");

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
		{.name= "tentwentyfive", .size = 1025, .result= -1},
		{.name= "", .size = 0, .result= 0}
	};

	for (int x = 0; tests[x].name[0] != '\0'; x++) {
		efi_del_variable(TEST_GUID, tests[x].name);
	}

	int ret = 0;

	for (int x = 0; tests[x].name[0] != '\0'; x++) {
		int rc = do_test(&tests[x]);
		if (rc < 0) {
			efi_del_variable(TEST_GUID, tests[x].name);
			ret = 1;
			break;
		}
	}
	return ret;
}
