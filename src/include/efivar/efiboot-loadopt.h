/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2015 Red Hat, Inc.
 * Copyright (C) 2001 Dell Computer Corporation <Matt_Domsch@dell.com>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _EFIBOOT_LOADOPT_H
#define _EFIBOOT_LOADOPT_H 1

typedef struct efi_load_option_s efi_load_option;

extern ssize_t efi_make_load_option(uint8_t *buf, ssize_t size,
				    uint32_t attributes, efidp dp,
				    unsigned char *description,
				    uint8_t *optional_data,
				    size_t optional_data_size)
	__attribute__((__nonnull__ (4,5)));

extern efidp efi_load_option_path(efi_load_option *opt)
	__attribute__((__nonnull__ (1)));
extern const unsigned char const *efi_load_option_desc(efi_load_option *opt)
	__attribute__((__visibility__ ("default")))
	__attribute__((__nonnull__ (1)));
extern uint32_t efi_load_option_attrs(efi_load_option *opt)
	__attribute__((__nonnull__ (1)))
	__attribute__((__visibility__ ("default")));
extern void efi_load_option_attr_set(efi_load_option *opt, uint16_t attr)
	__attribute__((__nonnull__ (1)))
	__attribute__((__visibility__ ("default")));
extern void efi_load_option_attr_clear(efi_load_option *opt, uint16_t attr)
	__attribute__((__nonnull__ (1)))
	__attribute__((__visibility__ ("default")));
extern uint16_t efi_load_option_pathlen(efi_load_option *opt)
	__attribute__((__nonnull__ (1)))
	__attribute__((__visibility__ ("default")));
extern int efi_load_option_optional_data(efi_load_option *opt, size_t opt_size,
					 unsigned char **datap, size_t *len)
	__attribute__((__nonnull__ (1,3)))
	__attribute__((__visibility__ ("default")));

extern ssize_t
efi_load_option_args_from_file(uint8_t *buf, ssize_t size, char *filename)
	__attribute__((__nonnull__ (3)))
	__attribute__((__visibility__ ("default")));
extern ssize_t
efi_load_option_args_as_utf8(uint8_t *buf, ssize_t size, char *utf8)
	__attribute__((__nonnull__ (3)))
	__attribute__((__visibility__ ("default")));
extern ssize_t
efi_load_option_args_as_ucs2(uint16_t *buf, ssize_t size, char *utf8)
	__attribute__((__nonnull__ (3)))
	__attribute__((__visibility__ ("default")));

#endif /* _EFIBOOT_LOADOPT_H */
