#include <stdio.h>

#include "efivar.h"
#include "guid.h"

#define GUID_FORMAT "%08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x"

int efi_str_to_guid(const char *s, efi_guid_t *guid)
{
	return text_to_guid(s, guid);
}

int efi_guid_to_str(const efi_guid_t *guid, char **sp)
{
	char *ret = NULL;
	int rc;

	rc = asprintf(&ret, GUID_FORMAT,
		 	guid->a, guid->b, guid->c, bswap_16(guid->d),
			guid->e[0], guid->e[1], guid->e[2], guid->e[3],
			guid->e[4], guid->e[5]);
	if (rc >= 0)
		*sp = ret;
	return rc;
}
