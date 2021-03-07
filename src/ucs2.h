// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Copyright 2012-2016 Red Hat, Inc.
 */
#ifndef _EFIVAR_UCS2_H
#define _EFIVAR_UCS2_H

#define ev_bits(val, mask, shift) \
	(((val) & ((mask) << (shift))) >> (shift))

/*
 * ucs2len(): Count the number of characters in a UCS-2 string.
 * s: a UCS-2 string
 * limit: the maximum number of uint16_t bytepairs to examine
 *
 * returns the number of characters before NUL is found (i.e., excluding
 * the NUL character).  If limit is non-negative, no character index above
 * limit will be accessed, and the maximum return value is limit.
 */
static inline size_t UNUSED
ucs2len(const void *s, ssize_t limit)
{
	ssize_t i;
	const uint8_t *s8 = s;

	for (i = 0;
	     i < (limit >= 0 ? limit : i+1) && !(s8[0] == 0 && s8[1] == 0);
	     i++, s8 += 2)
		;
	return i;
}

/*
 * ucs2size(): count the number of bytes in use by a UCS-2 string.
 * s: a UCS-2 string
 * limit: the maximum number of uint16_t bytepairs to examine
 *
 * returns the number of bytes, including NUL, in the UCS-2 string s.  If
 * limit is non-negative, no character index above limit will be accessed,
 * and the maximum return value is limit.
 */
static inline size_t UNUSED
ucs2size(const void *s, ssize_t limit)
{
	size_t rc = ucs2len(s, limit);
	rc *= sizeof (uint16_t);
	rc += sizeof (uint16_t);
	if (limit > 0 && rc > (size_t)limit)
		return limit;
	return rc;
}

/*
 * utf8len(): Count the number of characters in a UTF-8 string.
 * s: a UTF-8 string
 * limit: the maximum number of bytes to examine
 *
 * returns the number of UTF-8 charters before NUL is found (i.e.,
 * excluding the NUL character).  If limit is non-negative, no character
 * index above limit will be accessed, and the maximum return value is
 * limit.
 *
 * Caveat: only good up to 3-byte sequences.
 */
static inline size_t UNUSED NONNULL(1)
utf8len(const unsigned char *s, ssize_t limit)
{
	ssize_t i, j;
	for (i = 0, j = 0; i < (limit >= 0 ? limit : i+1) && s[i] != '\0';
	     j++, i++) {
		if (!(s[i] & 0x80)) {
			;
		} else if ((s[i] & 0xc0) == 0xc0 && !(s[i] & 0x20)) {
			i += 1;
		} else if ((s[i] & 0xe0) == 0xe0 && !(s[i] & 0x10)) {
			i += 2;
		}
	}
	return j;
}

/*
 * utf8size(): count the number of bytes in use by a UTF-8 string.
 * s: a UTF-8 string
 * limit: the maximum number of bytes to examine
 *
 * returns the number of bytes, including NUL, in the UTF-8 string s.
 * If limit is non-negative, no character index above limit will be
 * accessed, and the maximum return value is limit.
 */
static inline size_t UNUSED NONNULL(1)
utf8size(const unsigned char *s, ssize_t limit)
{
	size_t ret = utf8len(s,limit);
	if (ret < (limit >= 0 ? (size_t)limit : ret+1))
		ret++;
	return ret;
}

/*
 * ucs2_to_utf8(): convert UCS-2 to UTF-8
 * s: the UCS-2 string
 * limit: the maximum number of characters to copy from s, including the
 *	  NUL terminator, or -1 for no limit.
 *
 * returns an allocated string, into which at most limit - 1 characters of
 * UTF-8 are translated from UCS-2.  The return value is *always*
 * NUL-terminated.
 */
static inline unsigned char * UNUSED
ucs2_to_utf8(const void * const s, ssize_t limit)
{
	ssize_t i, j;
	unsigned char *out, *ret;
	const uint16_t * const chars = s;

	if (limit < 0)
		limit = ucs2len(chars, -1);
	out = malloc(limit * 6 + 1);
	if (!out)
		return NULL;
	memset(out, 0, limit * 6 +1);

	for (i=0, j=0; chars[i] && i < (limit >= 0 ? limit : i+1); i++,j++) {
		if (chars[i] <= 0x7f) {
			out[j] = chars[i];
		} else if (chars[i] > 0x7f && chars[i] <= 0x7ff) {
			out[j++] = 0xc0 | ev_bits(chars[i], 0x1f, 6);
			out[j]   = 0x80 | ev_bits(chars[i], 0x3f, 0);
#if 1
		} else if (chars[i] > 0x7ff) {
			out[j++] = 0xe0 | ev_bits(chars[i], 0xf, 12);
			out[j++] = 0x80 | ev_bits(chars[i], 0x3f, 6);
			out[j]   = 0x80| ev_bits(chars[i], 0x3f, 0);
		}
#else
		} else if (chars[i] > 0x7ff && chars[i] < 0x10000) {
			out[j++] = 0xe0 | ev_bits(chars[i], 0xf, 12);
			out[j++] = 0x80 | ev_bits(chars[i], 0x3f, 6);
			out[j]   = 0x80| ev_bits(chars[i], 0x3f, 0);
		} else if (chars[i] > 0xffff && chars[i] < 0x200000) {
			out[j++] = 0xf0 | ev_bits(chars[i], 0x7, 18);
			out[j++] = 0x80 | ev_bits(chars[i], 0x3f, 12);
			out[j++] = 0x80 | ev_bits(chars[i], 0x3f, 6);
			out[j]   = 0x80| ev_bits(chars[i], 0x3f, 0);
		} else if (chars[i] > 0x1fffff && chars[i] < 0x4000000) {
			out[j++] = 0xf8 | ev_bits(chars[i], 0x3, 24);
			out[j++] = 0x80 | ev_bits(chars[i], 0x3f, 18);
			out[j++] = 0x80 | ev_bits(chars[i], 0x3f, 12);
			out[j++] = 0x80 | ev_bits(chars[i], 0x3f, 6);
			out[j]   = 0x80 | ev_bits(chars[i], 0x3f, 0);
		} else if (chars[i] > 0x3ffffff) {
			out[j++] = 0xfc | ev_bits(chars[i], 0x1, 30);
			out[j++] = 0x80 | ev_bits(chars[i], 0x3f, 24);
			out[j++] = 0x80 | ev_bits(chars[i], 0x3f, 18);
			out[j++] = 0x80 | ev_bits(chars[i], 0x3f, 12);
			out[j++] = 0x80 | ev_bits(chars[i], 0x3f, 6);
			out[j]   = 0x80 | ev_bits(chars[i], 0x3f, 0);
		}
#endif
	}
	out[j++] = '\0';
	ret = realloc(out, j);
	if (!ret) {
		free(out);
		return NULL;
	}
	return ret;
}

/*
 * utf8_to_ucs2(): convert UTF-8 to UCS-2
 * s: the destination buffer to write to.
 * size: the size of the allocation to write to
 * terminate: whether or not to add a terminator to the string
 * utf8: the utf8 source
 *
 * returns the number of characters written to s, including the NUL
 * terminator if "terminate" is true, or -1 on error.  In the case of an
 * error, the buffer will not be modified.
 */
static inline ssize_t UNUSED NONNULL(4)
utf8_to_ucs2(void *s, ssize_t size, bool terminate, const unsigned char *utf8)
{
	ssize_t req;
	ssize_t i, j;
	uint16_t *ucs2 = s;
	uint16_t val16;

	if (!ucs2 && size > 0) {
		errno = EINVAL;
		return -1;
	}

	req = utf8len(utf8, -1) * sizeof (uint16_t);
	if (terminate && req > 0)
		req += 1;

	if (size == 0 || req <= 0)
		return req;

	if (size < req) {
		errno = ENOSPC;
		return -1;
	}

	for (i=0, j=0; i < (size >= 0 ? size : i+1) && utf8[i] != '\0'; j++) {
		uint32_t val = 0;

		if ((utf8[i] & 0xe0) == 0xe0 && !(utf8[i] & 0x10)) {
			val = ((utf8[i+0] & 0x0f) << 12)
			     |((utf8[i+1] & 0x3f) << 6)
			     |((utf8[i+2] & 0x3f) << 0);
			i += 3;
		} else if ((utf8[i] & 0xc0) == 0xc0 && !(utf8[i] & 0x20)) {
			val = ((utf8[i+0] & 0x1f) << 6)
			     |((utf8[i+1] & 0x3f) << 0);
			i += 2;
		} else {
			val = utf8[i] & 0x7f;
			i += 1;
		}
		val16 = val;
		ucs2[j] = val16;
	}
	if (terminate) {
		val16 = 0;
		ucs2[j++] = val16;
	}
	return j;
};

#endif /* _EFIVAR_UCS2_H */

// vim:fenc=utf-8:tw=75:noet
