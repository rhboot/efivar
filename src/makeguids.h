// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * makeguids.h - stuff makeguids needs that we also need at runtime
 * Copyright Peter Jones <pjones@redhat.com>
 */

#ifndef EFIVAR_MAKEGUIDS_H_
#define EFIVAR_MAKEGUIDS_H_

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "compiler.h"
#include "guid.h"
#include "util.h"

struct guidname_offset {
	efi_guid_t guid;
	off_t nameoff;
	off_t symoff;
	off_t descoff;
};

struct guidname_index {
	union {
		struct {
			char *strtab;
			size_t strsz;
			size_t nguids;
		};
		struct guidname_offset pad[1];
	};
	struct guidname_offset offsets[];
};

static int
gnopguidcmp(const void *p1, const void *p2, void *ctxp UNUSED)
{
	struct guidname_offset *gno1 = (struct guidname_offset *)p1;
	struct guidname_offset *gno2 = (struct guidname_offset *)p2;

	return efi_guid_cmp_(&gno1->guid, &gno2->guid);
}

static inline int __attribute__((__unused__))
read_guids_at(const int dirfd, const char * const path,
	      struct guidname_index **guidnamesp)
{
	int fd = -1;
	char *buf = NULL;
	size_t bufsz = 0;
	int errnum;
	int rc = -1;
	struct guidname_offset *guids = NULL;
	char *strtab = NULL;

	if ((dirfd < 0 && !(dirfd & AT_FDCWD))
	    || path == NULL
	    || (path[0] == '\0' && !(dirfd & AT_EMPTY_PATH))
	    || guidnamesp == NULL) {
		errno = EINVAL;
		return -1;
	}

	fd = openat(dirfd, path, O_RDONLY|O_CLOEXEC);
	if (fd < 0)
		return fd;

	rc = read_file(fd, (uint8_t **)&buf, &bufsz);
	errnum = errno;
	close(fd);
	if (rc < 0) {
		errno = errnum;
		return -1;
	}

	/*
	 * okay, so here's how we're doing this:
	 *
	 * 1) make an allocation that's big enough for one offset table.
	 *    The offset data takes up:
	 *
	 *      bufsz
	 *      - nguids * (sizeof(efi_guid_t) * 2 + sizeof('-') * 4
	 *                  + sizeof('\t') + strlen(label)
	 *                  + sizeof('\t') + strlen(desc)
	 *                  + sizeof('\n'))
	 *      + nguids * (sizeof(efi_guid_t) + sizeof(off_t) * 2)
	 *
	 *    which on a an ILP64 machine (the biggest case) means
	 *
	 *      bufsz
	 *      - nguids * (36 + 2 + 2 + 1)
	 *      + nguids * (16 + 16)
	 *
	 *    so for nguids = {1..N} entries that's
	 *
	 *      bufsz - 1 * 41 + 1 * 32 == bufsz - 41 + 32 == bufsz - 9
	 *      bufsz - 2 * 41 + 2 * 32 == bufsz - 82 + 64 == bufsz - 18
	 *      etc
	 *
	 *    There may be one fewer newline at the end but we've added a
	 *    '\0' to be sure, so bufsz is almost big enough.  And of
	 *    course if the strings or whitespace are bigger we reclaim
	 *    space even faster.  So bufsz will do just fine.
	 *
	 * 2) make an allocation that is sure to be big enough for all the
	 *    string data we save.  In practice this is
	 *
	 *      nguids * (strlen(name) + sizeof('\0')
	 *                + strlen(name) + sizeof("efi_guid_"))
	 *
	 *    but we don't know nguids yet, but we know the maximum nguids
	 *    is:
	 *
	 *      bufsz / (sizeof(efi_guid_t) * 2 + sizeof('-') * 4
	 *               + sizeof('\t') + strlen(name)
	 *               + sizeof('\t') + strlen(desc)
	 *               + sizeof('\n'))
	 *
	 *    which in the largest valid case, where strlen(name) == 1 and
	 *    strlen(desc) == 1, is
	 *
	 *      bufsz / (16 + 2 + 4 + 1 + 1 + 1 + 1 + 1)
	 *
	 *    and so the maximum number of entries is bufsz/27, and the
	 *    most storage we can need is
	 *
	 *       (bufsz / 27) * (strlen(name) + sizeof('\0')
	 *                      + strlen(name) + sizeof("efi_guid_"))
	 *       ===
	 *       (bufsz / 27) * (1 + 1 + 1 + + 1)
	 *       ===
	 *       (bufsz / 27) * 15
	 *
	 *    which is to say it's a bit over half the size of the input
	 *    data.  I'm just going to use bufsz.
	 *
	 * 3) do one pass across buf, copying strings to strtab and building
	 *    guids in place
	 *
	 * 4) move the first entry to the end of guids
	 *
	 * 5) store a struct strtab in the first position
	 *
	 * 6) realloc guids and strtab down to size
	 *
	 * 8) sort guids[1:n]
	 *
	 * 9) set guidnames = guids
	 *
	 */

	/*
	 * pad (at least) one NUL char at the end.
	 */
	char *nbuf = realloc(buf, ALIGN_UP((bufsz+1), page_size));
	if (!nbuf) {
		efi_error("Could not realloc guid file buffer");
		goto err;
	}
	buf = nbuf;
	buf[bufsz] = '\0';

	guids = calloc(1, ALIGN_UP(bufsz, page_size));
	if (!guids)
		goto err;

	strtab = calloc(1, ALIGN_UP(bufsz, page_size));
	if (!strtab)
		goto err;

	size_t bpos = 0, spos = 0, nguids = 0;
	for (; bpos < bufsz && buf[bpos] != 0; nguids++) {
		struct guidname_offset *gno = &guids[nguids];
		char guidstr[37];

		char *symbol = strchr(&buf[bpos], '\t');
		if (symbol == NULL) {
			efi_error("invalid guid string data on line %zd", nguids);
			goto err;
		}
		*symbol = '\0';
		symbol += 1;

		strncpy(guidstr, &buf[bpos], 37);
		guidstr[36] = '\0';

		char *desc = strchrnul(symbol, '\t');
		if (*desc != '\0') {
			*desc = '\0';
			desc += 1;
		}

		char *end = strchrnul(desc, '\n');
		*end = '\0';

		efi_guid_t guid;
		rc = efi_str_to_guid_(guidstr, &guid);
		if (rc < 0) {
			efi_error("unparsable guid on line %zd", nguids);
			goto err;
		}

		char *s = &strtab[spos];
		off_t nameoff, symoff, descoff;

		nameoff = s - strtab;
		s = stpcpy(s, symbol);
		*(s++) = '\0';
		symoff = s - strtab;
		s = stpcpy(s, "efi_guid_");
		s = stpcpy(s, symbol);
		*(s++) = '\0';
		descoff = s - strtab;
		s = stpcpy(s, desc);
		*(s++) = '\0';
		spos = s - strtab;

		gno->guid = guid;
		gno->nameoff = nameoff;
		gno->symoff = symoff;
		gno->descoff = descoff;
		bpos = end - buf + 1;
	}

	if (spos == 0) {
		efi_error("guid table produced no strings");
		goto err;
	}

	char *newstrtab = realloc(strtab, ALIGN_UP(spos, page_size));
	if (!newstrtab) {
		efi_error("could not realloc strtab for guid table");
		goto err;
	}
	strtab = newstrtab;

	struct guidname_offset *newguids;
	newguids = realloc(guids,
			   ALIGN_UP((1+nguids) * sizeof(newguids[0]),
				    page_size));
	if (!newguids) {
		efi_error("could not realloc guidnames table");
		goto err;
	}
	guids = newguids;

	memcpy(&guids[nguids], &guids[0], sizeof(guids[0]));

	struct guidname_index *guidnames;
	guidnames = (struct guidname_index *)guids;

	guidnames->strtab = strtab;
	guidnames->nguids = nguids;
	guidnames->strsz = spos;

	qsort_r(&guidnames->offsets[0], nguids,
		sizeof(struct guidname_offset), gnopguidcmp, guidnames);

	*guidnamesp = guidnames;

	xfree(buf);
	return 0;

err:
	xfree(strtab);
	xfree(guids);
	xfree(buf);
	return rc;
}

#endif /* !EFIVAR_MAKEGUIDS_H_ */
// vim:fenc=utf-8:tw=75:noet
