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

#include <stdarg.h>
#include <stdio.h>

#define pbufx(buf, size, off, fmt, args...) ({				\
		ssize_t __x;						\
		__x = snprintf(((buf)+(off)),				\
			       ((size)==0?0:((size)-(off))),		\
			       fmt, ## args);				\
		if (__x < 0)						\
			return -1;					\
		__x;							\
	})

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
