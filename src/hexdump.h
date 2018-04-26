#ifndef STATIC_HEXDUMP_H
#define STATIC_HEXDUMP_H

#include <ctype.h>

static inline unsigned long UNUSED
prepare_hex(uint8_t *data, unsigned long size, char *buf)
{
	unsigned long sz = (unsigned long)data % 16;
	char hexchars[] = "0123456789abcdef";
	int offset = 0;
	unsigned long i;
	unsigned long j;

	for (i = 0; i < sz; i++) {
		buf[offset++] = ' ';
		buf[offset++] = ' ';
		buf[offset++] = ' ';
		if (i == 7)
			buf[offset++] = ' ';
	}
	for (j = sz; j < 16 && j < size; j++) {
		uint8_t d = data[j-sz];
		buf[offset++] = hexchars[(d & 0xf0) >> 4];
		buf[offset++] = hexchars[(d & 0x0f)];
		if (j != 15)
			buf[offset++] = ' ';
		if (j == 7)
			buf[offset++] = ' ';
	}
	for (i = j; i < 16; i++) {
		buf[offset++] = ' ';
		buf[offset++] = ' ';
		if (i != 15)
			buf[offset++] = ' ';
		if (i == 7)
			buf[offset++] = ' ';
	}
	buf[offset] = '\0';
	return j - sz;
}

static inline void UNUSED
prepare_text(uint8_t *data, unsigned long size, char *buf)
{
	unsigned long sz = (unsigned long)data % 16;
	int offset = 0;
	unsigned long i;
	unsigned long j;

	for (i = 0; i < sz; i++)
		buf[offset++] = ' ';
	buf[offset++] = '|';
	for (j = sz; j < 16 && j < size; j++) {
		if (isprint(data[j-sz]))
			buf[offset++] = data[j-sz];
		else
			buf[offset++] = '.';
	}
	buf[offset++] = '|';
	for (i = j; i < 16; i++)
		buf[offset++] = ' ';
	buf[offset] = '\0';
}

static inline void UNUSED
hexdump(uint8_t *data, unsigned long size)
{
	unsigned long display_offset = (unsigned long)data & 0xffffffff;
	unsigned long offset = 0;
	//printf("hexdump: data=0x%016x size=0x%x\n", data, size);

	while (offset < size) {
		char hexbuf[49];
		char txtbuf[19];
		unsigned long sz;

		sz = prepare_hex(data+offset, size-offset, hexbuf);
		if (sz == 0)
			return;

		prepare_text(data+offset, size-offset, txtbuf);
		printf("%016lx  %s  %s\n", display_offset, hexbuf, txtbuf);

		display_offset += sz;
		offset += sz;
	}
}

#endif /* STATIC_HEXDUMP_H */
