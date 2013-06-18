#ifndef LIBEFIVAR_GUID_H
#define LIBEFIVAR_GUID_H 1

#include <errno.h>
#include <limits.h>
#include <string.h>

static int
text_to_guid(const char *text, efi_guid_t *guid)
{
	char eightbytes[9] = "";
	char fourbytes[5] = "";
	char twobytes[3] = "";
	uint32_t value;

	if (strlen(text) != strlen("84be9c3e-8a32-42c0-891c-4cd3b072becc")) {
		errno = EINVAL;
		return -1;
	}

	/* 84be9c3e-8a32-42c0-891c-4cd3b072becc
	 * ^ */
	strncpy(eightbytes, text, 8);
	value = strtol(eightbytes, NULL, 16);
	if ((value == LONG_MAX || value == LONG_MIN) && errno == ERANGE)
		return -1;
	guid->a = value;

	/* 84be9c3e-8a32-42c0-891c-4cd3b072becc
	 *          ^ */
	strncpy(fourbytes, text+9, 4);
	value = strtol(fourbytes, NULL, 16);
	if ((value == LONG_MAX || value == LONG_MIN) && errno == ERANGE)
		return -1;
	guid->b = value & 0xffffUL;

	/* 84be9c3e-8a32-42c0-891c-4cd3b072becc
	 *               ^ */
	strncpy(fourbytes, text+14, 4);
	value = strtol(fourbytes, NULL, 16);
	if ((value == LONG_MAX || value == LONG_MIN) && errno == ERANGE)
		return -1;
	guid->c = value & 0xffffUL;

	/* 84be9c3e-8a32-42c0-891c-4cd3b072becc
	 *                    ^ */
	strncpy(fourbytes, text+19, 4);
	value = strtol(fourbytes, NULL, 16);
	if ((value == LONG_MAX || value == LONG_MIN) && errno == ERANGE)
		return -1;
	guid->d = value & 0xffffUL;
	guid->d = bswap_16(guid->d);

	/* 84be9c3e-8a32-42c0-891c-4cd3b072becc
	 *                         ^ */
	strncpy(twobytes, text+24, 2);
	value = strtol(twobytes, NULL, 16);
	if ((value == LONG_MAX || value == LONG_MIN) && errno == ERANGE)
		return -1;
	guid->e[0] = value & 0xffUL;

	/* 84be9c3e-8a32-42c0-891c-4cd3b072becc
	 *                           ^ */
	strncpy(twobytes, text+26, 2);
	value = strtol(twobytes, NULL, 16);
	if ((value == LONG_MAX || value == LONG_MIN) && errno == ERANGE)
		return -1;
	guid->e[1] = value & 0xffUL;

	/* 84be9c3e-8a32-42c0-891c-4cd3b072becc
	 *                             ^ */
	strncpy(twobytes, text+28, 2);
	value = strtol(twobytes, NULL, 16);
	if ((value == LONG_MAX || value == LONG_MIN) && errno == ERANGE)
		return -1;
	guid->e[2] = value & 0xffUL;

	/* 84be9c3e-8a32-42c0-891c-4cd3b072becc
	 *                               ^ */
	strncpy(twobytes, text+30, 2);
	value = strtol(twobytes, NULL, 16);
	if ((value == LONG_MAX || value == LONG_MIN) && errno == ERANGE)
		return -1;
	guid->e[3] = value & 0xffUL;

	/* 84be9c3e-8a32-42c0-891c-4cd3b072becc
	 *                                 ^ */
	strncpy(twobytes, text+32, 2);
	value = strtol(twobytes, NULL, 16);
	if ((value == LONG_MAX || value == LONG_MIN) && errno == ERANGE)
		return -1;
	guid->e[4] = value & 0xffUL;

	/* 84be9c3e-8a32-42c0-891c-4cd3b072becc
	 *                                   ^ */
	strncpy(twobytes, text+34, 2);
	value = strtol(twobytes, NULL, 16);
	if ((value == LONG_MAX || value == LONG_MIN) && errno == ERANGE)
		return -1;
	guid->e[5] = value & 0xffUL;

	return 0;
}

#endif /* LIBEFIVAR_GUID */
