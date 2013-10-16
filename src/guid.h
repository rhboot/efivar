#ifndef LIBEFIVAR_GUID_H
#define LIBEFIVAR_GUID_H 1

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int
check_sanity(const char *text, size_t len)
{
	errno = EINVAL;
	if (len != strlen("84be9c3e-8a32-42c0-891c-4cd3b072becc"))
		return -1;

	if (text[8] != '-' || text[13] != '-' || text[18] != '-' ||
			text[23] != '-')
		return -1;

	errno = 0;
	return 0;
}

static int
check_segment_sanity(const char *text, size_t len)
{
	for(int i = 0; i < len; i++) {
		if (text[i] >= '0' && text[i] <= '9')
			continue;
		/* "| 0x20" is tolower() without having to worry about
		 * locale concerns, since we know everything here must
		 * be within traditional ascii space. */
		if ((text[i] | 0x20) >= 'a' && (text[i] | 0x20) <= 'f')
			continue;
		errno = EINVAL;
		return -1;
	}
	return 0;
}

static int
text_to_guid(const char *text, efi_guid_t *guid)
{
	/* these variables represent the length of the /string/ they hold,
	 * not the interpreted length of the value from them.  Mostly the
	 * names make it more obvious to verify that my bounds checking is
	 * correct. */
	char eightbytes[9] = "";
	char fourbytes[5] = "";
	char twobytes[3] = "";

	if (check_sanity(text, strlen(text)) < 0)
		return -1;

	/* 84be9c3e-8a32-42c0-891c-4cd3b072becc
	 * ^ */
	strncpy(eightbytes, text, 8);
	if (check_segment_sanity(eightbytes, 8) < 0)
		return -1;
	guid->a = (uint32_t)strtoul(eightbytes, NULL, 16);

	/* 84be9c3e-8a32-42c0-891c-4cd3b072becc
	 *          ^ */
	strncpy(fourbytes, text+9, 4);
	if (check_segment_sanity(fourbytes, 4) < 0)
		return -1;
	guid->b = (uint16_t)strtoul(fourbytes, NULL, 16);

	/* 84be9c3e-8a32-42c0-891c-4cd3b072becc
	 *               ^ */
	strncpy(fourbytes, text+14, 4);
	if (check_segment_sanity(fourbytes, 4) < 0)
		return -1;
	guid->c = (uint16_t)strtoul(fourbytes, NULL, 16);

	/* 84be9c3e-8a32-42c0-891c-4cd3b072becc
	 *                    ^ */
	strncpy(fourbytes, text+19, 4);
	if (check_segment_sanity(fourbytes, 4) < 0)
		return -1;
	guid->d = bswap_16((uint16_t)strtoul(fourbytes, NULL, 16));

	/* 84be9c3e-8a32-42c0-891c-4cd3b072becc
	 *                         ^ */
	strncpy(twobytes, text+24, 2);
	if (check_segment_sanity(twobytes, 2) < 0)
		return -1;
	guid->e[0] = (uint8_t)strtoul(twobytes, NULL, 16);

	/* 84be9c3e-8a32-42c0-891c-4cd3b072becc
	 *                           ^ */
	strncpy(twobytes, text+26, 2);
	if (check_segment_sanity(twobytes, 2) < 0)
		return -1;
	guid->e[1] = (uint8_t)strtoul(twobytes, NULL, 16);

	/* 84be9c3e-8a32-42c0-891c-4cd3b072becc
	 *                             ^ */
	strncpy(twobytes, text+28, 2);
	if (check_segment_sanity(twobytes, 2) < 0)
		return -1;
	guid->e[2] = (uint8_t)strtoul(twobytes, NULL, 16);

	/* 84be9c3e-8a32-42c0-891c-4cd3b072becc
	 *                               ^ */
	strncpy(twobytes, text+30, 2);
	if (check_segment_sanity(twobytes, 2) < 0)
		return -1;
	guid->e[3] = (uint8_t)strtoul(twobytes, NULL, 16);

	/* 84be9c3e-8a32-42c0-891c-4cd3b072becc
	 *                                 ^ */
	strncpy(twobytes, text+32, 2);
	if (check_segment_sanity(twobytes, 2) < 0)
		return -1;
	guid->e[4] = (uint8_t)strtoul(twobytes, NULL, 16);

	/* 84be9c3e-8a32-42c0-891c-4cd3b072becc
	 *                                   ^ */
	strncpy(twobytes, text+34, 2);
	if (check_segment_sanity(twobytes, 2) < 0)
		return -1;
	guid->e[5] = (uint8_t)strtoul(twobytes, NULL, 16);

	return 0;
}

#endif /* LIBEFIVAR_GUID */
