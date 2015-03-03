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
#ifndef _EFIVAR_LOAD_H
#define _EFIVAR_LOAD_H 1

typedef struct efi_load_option_s efi_load_option;

extern ssize_t efi_make_load_option(uint8_t *buf, ssize_t size,
				    uint32_t attributes, efidp dp,
				    char *description,
				    uint8_t *optional_data,
				    size_t optional_data_size);
extern efidp efi_load_option_path(efi_load_option *opt);

#endif /* _EFIVAR_LOAD_H */
