/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2015 Red Hat, Inc.
 * Copyright (C) 2001 Dell Computer Corporation <Matt_Domsch@dell.com>
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
#ifndef _EFIBOOT_LOADOPT_H
#define _EFIBOOT_LOADOPT_H 1

typedef struct efi_load_option_s efi_load_option;

extern ssize_t efi_loadopt_create(uint8_t *buf, ssize_t size,
				  uint32_t attributes, efidp dp,
				  ssize_t dp_size, unsigned char *description,
				  uint8_t *optional_data,
				  size_t optional_data_size)
	__attribute__((__nonnull__ (6)));

extern efidp efi_loadopt_path(efi_load_option *opt, ssize_t limit)
	__attribute__((__nonnull__ (1)));
extern const unsigned char * efi_loadopt_desc(efi_load_option *opt,
					      ssize_t limit)
	__attribute__((__visibility__ ("default")))
	__attribute__((__nonnull__ (1)));
extern uint32_t efi_loadopt_attrs(efi_load_option *opt)
	__attribute__((__nonnull__ (1)))
	__attribute__((__visibility__ ("default")));
extern void efi_loadopt_attr_set(efi_load_option *opt, uint16_t attr)
	__attribute__((__nonnull__ (1)))
	__attribute__((__visibility__ ("default")));
extern void efi_loadopt_attr_clear(efi_load_option *opt, uint16_t attr)
	__attribute__((__nonnull__ (1)))
	__attribute__((__visibility__ ("default")));
extern uint16_t efi_loadopt_pathlen(efi_load_option *opt, ssize_t limit)
	__attribute__((__nonnull__ (1)))
	__attribute__((__visibility__ ("default")));
extern int efi_loadopt_optional_data(efi_load_option *opt, size_t opt_size,
				     unsigned char **datap, size_t *len)
	__attribute__((__nonnull__ (1,3)))
	__attribute__((__visibility__ ("default")));

extern ssize_t efi_loadopt_args_from_file(uint8_t *buf, ssize_t size,
					  char *filename)
	__attribute__((__nonnull__ (3)))
	__attribute__((__visibility__ ("default")));
extern ssize_t efi_loadopt_args_as_utf8(uint8_t *buf, ssize_t size,
					uint8_t *utf8)
	__attribute__((__nonnull__ (3)))
	__attribute__((__visibility__ ("default")));
extern ssize_t efi_loadopt_args_as_ucs2(uint16_t *buf, ssize_t size,
					uint8_t *utf8)
	__attribute__((__nonnull__ (3)))
	__attribute__((__visibility__ ("default")));

extern ssize_t efi_loadopt_optional_data_size(efi_load_option *opt, size_t size)
	__attribute__((__nonnull__ (1)))
	__attribute__((__visibility__ ("default")));
extern int efi_loadopt_is_valid(efi_load_option *opt, size_t size)
	__attribute__((__nonnull__ (1)))
	__attribute__((__visibility__ ("default")));

#endif /* _EFIBOOT_LOADOPT_H */
