/*
* libefivar - library for the manipulation of EFI variables
* Copyright 2012-2015 Red Hat, Inc.
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef _EFIVAR_INTERNAL_DP_H
#define _EFIVAR_INTERNAL_DP_H

#include <alloca.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define pbufx(buf, size, off, fmt, args...) ({				\
		ssize_t __x;						\
		__x = snprintf(((buf)+(off)),				\
			       ((size)?((size)-(off)):0),		\
			       fmt, ## args);				\
		if (__x < 0)						\
			return -1;					\
		__x;							\
	})

static inline int
__attribute__((__unused__))
print_hex(char *buf, size_t size, const void const *addr, const size_t len)
{
	size_t off = 0;
	for (size_t i = 0; i < len; i++)
		off += pbufx(buf, size, off, "%02x",
			     *((const unsigned char const *)addr+i));
	return off;
}

#define onstack(buf, len) ({						\
		char *__newbuf = alloca(len);				\
		memcpy(__newbuf, buf, len);				\
		free(buf);						\
		__newbuf;						\
	})

static inline int
__attribute__((__unused__))
print_vendor(char *buf, size_t size, char *label, const_efidp dp)
{
	char *guidstr = NULL;
	int rc;
	size_t off = 0;

	rc = efi_guid_to_str(&dp->hw_vendor.vendor_guid, &guidstr);
	if (rc < 0)
		return rc;

	guidstr = onstack(guidstr, strlen(guidstr)+1);

	off = pbufx(buf, size, off, "%s(%s,", label, guidstr);

	size_t sz = print_hex(buf+off, size?size-off:0,
			      dp->hw_vendor.vendor_data,
			      efidp_node_size(dp) - 4 - sizeof (efi_guid_t));
	if (sz < 0)
		return sz;
	off += sz;
	off += pbufx(buf, size, off, ")");
	return off;
}

static inline int
__attribute__((__unused__))
peek_dn_type(const_efidp dp, uint8_t type, uint8_t subtype)
{
	if (efidp_type(dp) == EFIDP_END_TYPE &&
			efidp_subtype(dp) == EFIDP_END_ENTIRE)
		return 0;

	dp = (const_efidp)(const efidp_header const *)((uint8_t *)dp + dp->length);

	if (efidp_type(dp) == type && efidp_subtype(dp) == subtype)
		return 1;
	return 0;
}

extern ssize_t print_hw_dn(char *buf, size_t size, const_efidp dp);
extern ssize_t print_acpi_dn(char *buf, size_t size, const_efidp dp);
extern ssize_t print_message_dn(char *buf, size_t size, const_efidp dp);
extern ssize_t print_media_dn(char *buf, size_t size, const_efidp dp);
extern ssize_t print_bios_boot_dn(char *buf, size_t size, const_efidp dp);

#endif /* _EFIVAR_INTERNAL_DP_H */
