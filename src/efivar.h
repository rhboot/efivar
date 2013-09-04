/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012 Red Hat, Inc.
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
#ifndef EFIVAR_H
#define EFIVAR_H 1

#include <stdint.h>
#include <unistd.h>
#include <byteswap.h>

typedef struct {
	uint32_t	a;
	uint16_t	b;
	uint16_t	c;
	uint16_t	d;
	uint8_t		e[6];
} efi_guid_t;

#define EFI_GUID(a,b,c,d,e0,e1,e2,e3,e4,e5) \
((efi_guid_t) {(a), (b), (c), bswap_16(d), { (e0), (e1), (e2), (e3), (e4), (e5) }})

#define EFI_GLOBAL_GUID EFI_GUID(0x8be4df61,0x93ca,0x11d2,0xaa0d,0x00,0xe0,0x98,0x03,0x2b,0x8c)

#define EFI_VARIABLE_NON_VOLATILE	0x0000000000000001
#define EFI_VARIABLE_BOOTSERVICE_ACCESS	0x0000000000000002
#define EFI_VARIABLE_RUNTIME_ACCESS	0x0000000000000004
#define EFI_VARIABLE_HARDWARE_ERROR_RECORD	0x0000000000000008
#define EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS	0x0000000000000010
#define EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS 0x0000000000000020
#define EFI_VARIABLE_APPEND_WRITE	0x0000000000000040

extern int efi_variables_supported(void);
extern int efi_get_variable_size(efi_guid_t guid, const char *name,
				 size_t *size);
extern int efi_get_variable_attributes(efi_guid_t, const char *name,
				       uint32_t *attributes);
extern int efi_get_variable(efi_guid_t guid, const char *name, uint8_t **data,
			    size_t *data_size, uint32_t *attributes);
extern int efi_del_variable(efi_guid_t guid, const char *name);
extern int efi_set_variable(efi_guid_t guid, const char *name, uint8_t *data,
			    size_t data_size, uint32_t attributes);
extern int efi_append_variable(efi_guid_t guid, const char *name,
			       uint8_t *data, size_t data_size,
			       uint32_t attributes);
extern int efi_get_next_variable_name(efi_guid_t **guid, char **name);

extern int efi_str_to_guid(const char *s, efi_guid_t *guid);
extern int efi_guid_to_str(const efi_guid_t *guid, char **sp);

#endif /* EFIVAR_H */
