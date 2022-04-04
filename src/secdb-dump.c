// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * secdb-dump.c - efi_secdb_t hex-dump + annotation
 * Copyright Peter Jones <pjones@redhat.com>
 */

#include "efisec.h"
#include "hexdump.h"

#undef DEBUG_LEVEL
#define DEBUG_LEVEL LOG_DEBUG_DUMPER

static bool annotate = false;
static char *buf = NULL;
static size_t bufsz = 0;
static size_t start = 0;

static inline void
secdb_dump_finish(void)
{
	if (buf && bufsz) {
		hexdumpat((uint8_t *)buf, bufsz, start);
		free(buf);
	}
	buf = NULL;
	bufsz = 0;
}

static inline ssize_t
secdb_buffer(char *val, size_t valsz, ssize_t offset)
{
	char *nbuf;
	size_t nbufsz, allocsz;

	if (!buf) {
		// just because valgrind is annoying about realloc(NULL, ...)
		buf = calloc(1, page_size);
		if (!buf)
			return -1;
	}

	nbufsz = offset + valsz;
	allocsz = ALIGN_UP(nbufsz, page_size);
	nbuf = realloc(buf, allocsz);
	if (!nbuf) {
		free(buf);
		buf = NULL;
		bufsz = 0;
		return -1;
	}
	memset(nbuf + bufsz, '\0', allocsz - bufsz);
	memcpy(nbuf + offset, val, valsz);
	buf = nbuf;
	bufsz = nbufsz;
	return nbufsz;
}

static inline ssize_t
secdb_dump_value(char *val, size_t size, ssize_t offset, char *fmt, ...)
{
	char posbuf[9];
	char hexbuf[49];
	char textbuf[19];

	size_t printed = 0;
	va_list ap;
	bool once = false;
	unsigned int i;

	posbuf[sizeof(posbuf)-1] = '\0';
	hexbuf[sizeof(hexbuf)-1] = '\0';
	textbuf[sizeof(textbuf)-1] = '\0';

	if (!annotate) {
		if (size == 0)
			return offset;
		return secdb_buffer(val, size, offset);
	}
	if (size == 0 && strlen(fmt) == 0)
		return offset;

	if (size == 0) {
		memset(posbuf, ' ', sizeof(posbuf)-1);
		memset(hexbuf, ' ', sizeof(hexbuf)-1);
		memset(textbuf, ' ', sizeof(textbuf)-1);

		sprintf(posbuf, "%08zx", offset + printed);
		prepare_text("", 0, textbuf, offset + printed);

		for (i = strlen(textbuf); annotate && i < sizeof(textbuf)-1; i++)
			textbuf[i] = ' ';
		textbuf[sizeof(textbuf)-1] = '\0';
		printf("%s  %s  %s", posbuf, hexbuf, textbuf);
		if (annotate) {
			printf("  ");
			va_start(ap, fmt);
			vprintf(fmt, ap);
			va_end(ap);
		}
		printf("\n");
	}
	while (size - printed) {
		size_t sz;

		memset(posbuf, ' ', sizeof(posbuf)-1);
		memset(hexbuf, ' ', sizeof(hexbuf)-1);
		memset(textbuf, ' ', sizeof(textbuf)-1);

		sprintf(posbuf, "%08zx", offset + printed);
		debug("size:%zd printed:%zd", size, printed);
		sz = prepare_hex(val+printed, size-printed, hexbuf, offset+printed);
		prepare_text(val+printed, size-printed, textbuf, offset+printed);

		for (i = strlen(textbuf); annotate && i < sizeof(textbuf)-1; i++)
			textbuf[i] = ' ';
		textbuf[sizeof(textbuf)-1] = '\0';

		printed += sz;

		printf("%s  %s  %s", posbuf, hexbuf, textbuf);
		if (!once && annotate) {
			printf("  ");
			va_start(ap, fmt);
			vprintf(fmt, ap);
			va_end(ap);
			once = true;
		}
		printf("\n");
	}

	return offset + printed;
}

static inline ssize_t
secdb_dump_esl(efi_secdb_t *secdb, int esl, ssize_t offset)
{
	const efi_guid_t *alg;
	char *id_guid = NULL;

	alg = secdb_guid_from_type(secdb->algorithm);
	if (!alg)
		alg = &efi_guid_empty;
	efi_guid_to_id_guid(alg, &id_guid);
	offset = secdb_dump_value((char *)alg, sizeof(efi_guid_t), offset,
				  "esl[%d].signature_type = %s", esl, id_guid);
	xfree(id_guid);
	if (offset < 0)
		return offset;

	offset = secdb_dump_value((char *)&secdb->listsz,
				  sizeof(secdb->listsz), offset,
				  "esl[%d].signature_list_size = %d (0x%x)",
				  esl, secdb->listsz, secdb->listsz);
	if (offset < 0)
		return offset;

	offset = secdb_dump_value((char *)&secdb->hdrsz,
				  sizeof(secdb->hdrsz), offset,
				  "esl[%d].signature_header_size = %d",
				  esl, secdb->hdrsz);
	if (offset < 0)
		return offset;

	offset = secdb_dump_value((char *)&secdb->sigsz,
				  sizeof(secdb->sigsz), offset,
				  "esl[%d].signature_size = %d",
				  esl, secdb->sigsz);
	if (offset < 0)
		return offset;

	offset = secdb_dump_value((char *)NULL, secdb->hdrsz, offset,
				  "esl[%d].signature_header (end:0x%08zx)",
				  esl, offset + secdb->hdrsz);

	return offset;
}

static inline ssize_t
secdb_dump_esd(secdb_entry_t *entry, int esl, int esd, size_t data_size,
               ssize_t offset)
{
	char *id_guid = NULL;

	efi_guid_to_id_guid(&entry->owner, &id_guid);
	offset = secdb_dump_value((char *)&entry->owner,
				  sizeof(efi_guid_t), offset,
				  "esl[%d].signature[%d].owner = %s",
				  esl, esd, id_guid);
	xfree(id_guid);
	if (offset < 0)
		return offset;
	offset = secdb_dump_value((char *)&entry->data, data_size, offset,
				  "esl[%d].signature[%d].data (end:0x%08zx)",
				  esl, esd, offset+data_size);
	return offset;
}

void
secdb_dump(efi_secdb_t *secdb, bool annotations)
{
	int esln = 0;
	list_t *pos0, *pos1;
	ssize_t offset = 0;

	start = offset;
	annotate = annotations;

	for_each_secdb(pos0, &secdb->list) {
		efi_secdb_t *esl;
		int esdn = 0;

		esl = list_entry(pos0, efi_secdb_t, list);

		debug("esl[%d]:%p", esln, esl);
		offset = secdb_dump_esl(esl, esln, offset);
		if (offset < 0)
			break;

		for_each_secdb_entry(pos1, &esl->entries) {
			secdb_entry_t *esd;
			bool has_owner = true;
			size_t datasz = esl->sigsz;
			int rc;

			rc = secdb_entry_has_owner_from_type(esl->algorithm,
							     &has_owner);
			if (rc < 0) {
				int saved_errno = errno;
				secdb_dump_finish();
				errno = saved_errno;
				efi_error("could not determine signature type");
				return;
			}
			if (has_owner)
				datasz -= sizeof(efi_guid_t);

			esd = list_entry(pos1, secdb_entry_t, list);
			debug("esl[%d].esd[%d]:%p owner:%p data:%p-%p datasz:%zd",
			      esln, esdn, esd, &esd->owner,
			      &esd->data, &esd->data+datasz, datasz);
			offset = secdb_dump_esd(esd, esln, esdn, datasz, offset);
			esdn += 1;
			if (offset < 0)
				break;
		}
		if (offset < 0)
			break;
		esln += 1;
	}
	secdb_dump_finish();
	printf("%08zx\n", offset);

	fflush(stdout);
}

// vim:fenc=utf-8:tw=75:noet
