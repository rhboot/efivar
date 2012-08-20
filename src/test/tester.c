#include <stdio.h>
#include <string.h>

#include <efivar.h>

#define TEST_GUID EFI_GUID(0x84be9c3e,0x8a32,0x42c0,0x891c,0x4c,0xd3,0xb0,0x72,0xbe,0xcc)

static void
clean_test_environment(void)
{
	efi_del_variable(TEST_GUID, "small");
	efi_del_variable(TEST_GUID, "large");
}

#define report_error(str) ({fprintf(stderr, str); goto fail;})

int main(void)
{
	if (!efi_variables_supported()) {
		printf("UEFI variables not supported on this machine.\n");
		return 0;
	}

	clean_test_environment();

	int ret = 1;

	char smallvalue[] = "smallvalue";

	int rc;
	rc = efi_set_variable(TEST_GUID, "small",
			      smallvalue, strlen(smallvalue)+1,
			      EFI_VARIABLE_RUNTIME_ACCESS);
	if (rc < 0)
		report_error("small value test failed: %m\n");

	ret = 0;
fail:
	return ret;
}
