// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * path-helper.c
 * Copyright 2018 Peter Jones <pjones@redhat.com>
 */
#include "fix_coverity.h"

#include "efivar.h"

static bool
cinpat(const char c, const char *pat)
{
	for (unsigned int i = 0; pat[i]; i++)
	        if (pat[i] == c)
	                return true;
	return false;
}

static unsigned int
strxcspn(const char *s, const char *pattern)
{
	unsigned int i;
	for (i = 0; s[i]; i++) {
	        if (!cinpat(s[i], pattern))
	                break;
	}
	return i;
}

struct span {
	const char *pos;
	size_t len;
};

/*
 * count how many parts of a path there are, with some caveats:
 * a leading / is one because it's a directory, but all other slashes are
 * treated as separators, so i.e.:
 * 1: /
 * 2: /foo foo/bar foo/bar/
 * 3: /foo/bar /foo/bar/ foo/bar/baz
 *
 * the usage model here is 1 pass to count, one allocation, one pass to
 * separate.
 */
unsigned int HIDDEN
count_spans(const char *str, const char *reject, unsigned int *chars)
{
	unsigned int s = 0, c = 0, pos = 0;

	if (str[0] == '/') {
	        s += 1;
	        c += 2;
	        pos += 1;
	}

	while (str[pos]) {
	        unsigned int n;

	        n = strcspn(str + pos, reject);
	        if (n) {
	                s += 1;
	                c += n + 1;
	                pos += n;
	        }

	        pos += strxcspn(str + pos, reject);
	}

	if (chars)
	        *chars = c;
	return s;
}

void HIDDEN
fill_spans(const char *str, const char *reject, void *spanbuf)
{
	struct span *spans = (struct span *)spanbuf;
	struct span *span = spans;
	unsigned int pos = 0;

	if (str[0] == '/') {
	        span->pos = str;
	        span->len = 1;
	        span++;
	        pos += 1;
	}

	while (str[pos]) {
	        unsigned int n;

	        n = strcspn(str + pos, reject);
	        if (n) {
	                span->pos = str + pos;
	                span->len = n;
	                span++;
	                pos += n;
	        }

	        pos += strxcspn(str + pos, reject);
	}
	span->pos = NULL;
	span->len = 0;
}

#define split_spans(str, reject)                                        \
	({                                                              \
	        struct span *ret_ = NULL;                               \
	        unsigned int s_, c_;                                    \
	                                                                \
	        s_ = count_spans(str, "/", &c_);                        \
	        if (s_) {                                               \
	                ret_ = alloca(sizeof(struct span[s_+1]));       \
	                if (ret_)                                       \
	                        fill_spans(str, reject, ret_);          \
	        } else {                                                \
	                errno = 0;                                      \
	        }                                                       \
	        ret_;                                                   \
	})

int HIDDEN
find_path_segment(const char *path, int segment, const char **pos, size_t *len)
{
	struct span *span, *last;
	int nspans = 0;

	if (!pos || !len) {
	        errno = EINVAL;
	        return -1;
	}

	span = split_spans(path, "/");
	if (!span) {
	        if (errno)
	                return -1;
	        *pos = NULL;
	        *len = 0;
	        return 0;
	}

	for (last = span; last->pos; last++)
	        nspans += 1;

	if (segment < 0)
	        segment = nspans + segment;

	if (nspans < 1 || segment < 0 || segment >= nspans) {
	        errno = ENOENT;
	        return -1;
	}

	for (int i = 0; i < segment; i++)
	        span++;

	*pos = span->pos;
	*len = span->len;
	return 0;
}

// vim:fenc=utf-8:tw=75:noet
