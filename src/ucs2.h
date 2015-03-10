#ifndef _EFIVAR_UCS2_H
#define _EFIVAR_UCS2_H

#define ev_bits(val, mask, shift) \
	(((val) & ((mask) << (shift))) >> (shift))

static inline char *
__attribute__((__unused__))
ucs2_to_utf8(const uint16_t const *chars, size_t max)
{
	size_t i, j;
	char *ret = alloca(max * 6 + 1);
	if (!ret)
		return NULL;
	memset(ret, 0, max * 6 +1);

	for (i=0, j=0; chars[i] && i < max; i++,j++) {
		if (chars[i] <= 0x7f) {
			ret[j] = chars[i];
		} else if (chars[i] > 0x7f && chars[i] <= 0x7ff) {
			ret[j++] = 0xc0 | ev_bits(chars[i], 0x1f, 6);
			ret[j]   = 0x80 | ev_bits(chars[i], 0x3f, 0);
		} else if (chars[i] > 0x7ff && chars[i] < 0x10000) {
			ret[j++] = 0xe0 | ev_bits(chars[i], 0xf, 12);
			ret[j++] = 0x80 | ev_bits(chars[i], 0x3f, 6);
			ret[j]   = 0x80| ev_bits(chars[i], 0x3f, 0);
		} else if (chars[i] > 0xffff && chars[i] < 0x200000) {
			ret[j++] = 0xf0 | ev_bits(chars[i], 0x7, 18);
			ret[j++] = 0x80 | ev_bits(chars[i], 0x3f, 12);
			ret[j++] = 0x80 | ev_bits(chars[i], 0x3f, 6);
			ret[j]   = 0x80| ev_bits(chars[i], 0x3f, 0);
		} else if (chars[i] > 0x1fffff && chars[i] < 0x4000000) {
			ret[j++] = 0xf8 | ev_bits(chars[i], 0x3, 24);
			ret[j++] = 0x80 | ev_bits(chars[i], 0x3f, 18);
			ret[j++] = 0x80 | ev_bits(chars[i], 0x3f, 12);
			ret[j++] = 0x80 | ev_bits(chars[i], 0x3f, 6);
			ret[j]   = 0x80 | ev_bits(chars[i], 0x3f, 0);
		} else if (chars[i] > 0x3ffffff) {
			ret[j++] = 0xfc | ev_bits(chars[i], 0x1, 30);
			ret[j++] = 0x80 | ev_bits(chars[i], 0x3f, 24);
			ret[j++] = 0x80 | ev_bits(chars[i], 0x3f, 18);
			ret[j++] = 0x80 | ev_bits(chars[i], 0x3f, 12);
			ret[j++] = 0x80 | ev_bits(chars[i], 0x3f, 6);
			ret[j]   = 0x80 | ev_bits(chars[i], 0x3f, 0);
		}
	}
	ret[j] = '\0';
	return strdup(ret);
}

static inline size_t
__attribute__((__unused__))
utf8len(uint8_t *s, ssize_t limit)
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

static inline uint16_t *
__attribute__((__unused__))
utf8_to_ucs2(uint8_t *utf8, ssize_t max)
{
	ssize_t i, j;
	uint16_t *ret = calloc(utf8len(utf8, max) + 1, sizeof (uint16_t));
	if (!ret)
		return NULL;

	for (i=0, j=0; i < (max >= 0 ? max : i+1) && utf8[i] != '\0'; j++) {
		uint32_t val = 0;

		if ((utf8[i] & 0xe0) == 0xe0 && !(utf8[i] & 0x10)) {
			val = ((utf8[i+0] & 0x0f) << 10)
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
		ret[j] = val;
	}
	ret[j] = L'\0';
	return ret;
};

static inline size_t
__attribute__((__unused__))
ucs2len(uint16_t *s, ssize_t limit)
{
	ssize_t i;
	for (i = 0; i < limit && s[i] != L'\0'; i++)
		;
	return i;
}

#endif /* _EFIVAR_UCS2_H */
