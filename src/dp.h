/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012-2015 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 */
#ifndef _EFIVAR_INTERNAL_DP_H
#define _EFIVAR_INTERNAL_DP_H

#include <alloca.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "ucs2.h"

#define format(buf, size, off, fmt, args...) ({			\
		ssize_t _x = 0;						\
		if ((off) >= 0) {					\
			_x = snprintf(((buf)+(off)),			\
			       ((size)?((size)-(off)):0),		\
			       fmt, ## args);				\
			if (_x < 0)					\
				return _x;				\
			(off) += _x;					\
		}							\
		off;							\
	})

#define format_helper(fn, buf, size, off, args...) ({		\
		ssize_t _x;						\
		_x = (fn)(((buf)+(off)),				\
			  ((size)?((size)-(off)):0), ## args);		\
		if (_x < 0)						\
		(off) += _x;						\
	})

#define onstack(buf, len) ({						\
		char *__newbuf = alloca(len);				\
		memcpy(__newbuf, buf, len);				\
		free(buf);						\
		(void *)__newbuf;					\
	})

#define format_guid(buf, size, off, guid) ({				\
		int _rc;						\
		char *_guidstr = NULL;					\
									\
		_rc = efi_guid_to_str(guid, &_guidstr);			\
		format(buf, size, off, "%s", _guidstr);			\
	})

static inline ssize_t
__attribute__((__unused__))
format_hex_helper(char *buf, size_t size, const void * const addr,
		  const size_t len)
{
	ssize_t off = 0;
	for (size_t i = 0; i < len; i++) {
		format(buf, size, off, "%02x",
		       *((const unsigned char * const )addr+i));
	}
	return off;
}

#define format_hex(buf, size, off, addr, len)				\
	format_helper(format_hex_helper, buf, size, off, addr, len)

static inline ssize_t
__attribute__((__unused__))
format_vendor_helper(char *buf, size_t size, char *label, const_efidp dp)
{
	ssize_t off = 0;
	ssize_t bytes = efidp_node_size(dp)
			- sizeof (efidp_header)
			- sizeof (efi_guid_t);

	format(buf, size, off, label, "%s(", label);
	format_guid(buf, size, off, label, &dp->hw_vendor.vendor_guid);
	if (bytes) {
		format(buf, size, off, label, ",");
		format_hex(buf, size, off, label, dp->hw_vendor.vendor_data,
			   bytes);
	}
	format(buf, size, off, label, ")");
	return off;
}

#define format_vendor(buf, size, off, label, dp)			\
	format_helper(format_vendor_helper, buf, size, off, label, dp)

#define format_ucs2(buf, size, off, str, len) ({			\
		uint16_t _ucs2buf[(len)];				\
		memset(_ucs2buf, '\0', sizeof (_ucs2buf));		\
		memcpy(_ucs2buf, str, sizeof (_ucs2buf)			\
				      - sizeof (_ucs2buf[0]));		\
		unsigned char *_asciibuf;				\
		_asciibuf = ucs2_to_utf8(_ucs2buf, (len) - 1);		\
		if (_asciibuf == NULL)					\
			return -1;					\
		_asciibuf = onstack(_asciibuf, (len));			\
		format(buf, size, off, "%s", _asciibuf);		\
       })

#define format_array(buf, size, off, fmt, type, addr, len) ({		\
		for (size_t _i = 0; _i < len; _i++) {			\
			if (_i != 0)					\
				format(buf, size, off, ",");		\
			format(buf, size, off, fmt,			\
			       ((type *)addr)[_i]);			\
		}							\
		(off);							\
	})

extern ssize_t _format_hw_dn(char *buf, size_t size, const_efidp dp);
extern ssize_t _format_acpi_dn(char *buf, size_t size, const_efidp dp);
extern ssize_t _format_message_dn(char *buf, size_t size, const_efidp dp);
extern ssize_t _format_media_dn(char *buf, size_t size, const_efidp dp);
extern ssize_t _format_bios_boot_dn(char *buf, size_t size, const_efidp dp);

#define format_helper_2(name, buf, size, off, dp) ({			\
		ssize_t _sz;						\
		_sz = name(((buf)+(off)),				\
			   ((size)?((size)-(off)):0),			\
			   (dp));					\
		if (_sz < 0) {						\
			efi_error("%s failed", #name);			\
			return -1;					\
		}							\
		(off) += _sz;						\
	})

#define format_hw_dn(buf, size, off, dp) \
	format_helper_2(_format_hw_dn, buf, size, off, dp)
#define format_acpi_dn(buf, size, off, dp) \
	format_helper_2(_format_acpi_dn, buf, size, off, dp)
#define format_message_dn(buf, size, off, dp) \
	format_helper_2(_format_message_dn, buf, size, off, dp)
#define format_media_dn(buf, size, off, dp) \
	format_helper_2(_format_media_dn, buf, size, off, dp)
#define format_bios_boot_dn(buf, size, off, dp) \
	format_helper_2(_format_bios_boot_dn, buf, size, off, dp)

#endif /* _EFIVAR_INTERNAL_DP_H */
