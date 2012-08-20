#include <stdio.h>
#include <efivar.h>

#if 0
static efi_guid_t test_guid = EFI_GUID(0x84be9c3e,0x8a32,0x42c0,0x891c,0x4c,0xd3,0xb0,0x72,0xbe,0xcc);
#endif

int main(void)
{
	if (!efi_variables_supported()) {
		printf("UEFI variables not supported on this machine.\n");
		return 0;
	}


	int ret = -1;
	

#if 0
	ret = 0;
fail:
#endif
	return ret;
}
