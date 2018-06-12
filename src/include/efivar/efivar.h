/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012-2014 Red Hat, Inc.
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
#ifndef EFIVAR_H
#define EFIVAR_H 1

#include <endian.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <byteswap.h>

typedef struct {
	uint32_t	a;
	uint16_t	b;
	uint16_t	c;
	uint16_t	d;
	uint8_t		e[6];
} efi_guid_t;

typedef struct {
	uint8_t		addr[4];
} efi_ipv4_addr_t;

typedef struct {
	uint8_t		addr[16];
} efi_ipv6_addr_t;

typedef union {
	uint32_t	addr[4];
	efi_ipv4_addr_t	v4;
	efi_ipv6_addr_t	v6;
} efi_ip_addr_t;

typedef struct {
	uint8_t		addr[32];
} efi_mac_addr_t;

#ifndef EFIVAR_BUILD_ENVIRONMENT
#include <efivar/efivar-guids.h>
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
#define EFI_GUID(a,b,c,d,e0,e1,e2,e3,e4,e5) \
((efi_guid_t) {(a), (b), (c), __builtin_bswap16(d), { (e0), (e1), (e2), (e3), (e4), (e5) }})
#else
#define EFI_GUID(a,b,c,d,e0,e1,e2,e3,e4,e5) \
((efi_guid_t) {(a), (b), (c), (d), { (e0), (e1), (e2), (e3), (e4), (e5) }})
#endif

#define EFI_GLOBAL_GUID EFI_GUID(0x8be4df61,0x93ca,0x11d2,0xaa0d,0x00,0xe0,0x98,0x03,0x2b,0x8c)

#define EFI_VARIABLE_NON_VOLATILE	0x0000000000000001
#define EFI_VARIABLE_BOOTSERVICE_ACCESS	0x0000000000000002
#define EFI_VARIABLE_RUNTIME_ACCESS	0x0000000000000004
#define EFI_VARIABLE_HARDWARE_ERROR_RECORD	0x0000000000000008
#define EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS	0x0000000000000010
#define EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS 0x0000000000000020
#define EFI_VARIABLE_APPEND_WRITE	0x0000000000000040

#define EFI_VARIABLE_HAS_AUTH_HEADER	0x0000000100000000
#define EFI_VARIABLE_HAS_SIGNATURE	0x0000000200000000

extern int efi_variables_supported(void);
extern int efi_get_variable_size(efi_guid_t guid, const char *name,
				 size_t *size)
				__attribute__((__nonnull__ (2, 3)));
extern int efi_get_variable_attributes(efi_guid_t, const char *name,
				       uint32_t *attributes)
				__attribute__((__nonnull__ (2, 3)));
extern int efi_get_variable_exists(efi_guid_t, const char *name)
				__attribute__((__nonnull__ (2)));
extern int efi_get_variable(efi_guid_t guid, const char *name, uint8_t **data,
			    size_t *data_size, uint32_t *attributes)
				__attribute__((__nonnull__ (2, 3, 4, 5)));
extern int efi_del_variable(efi_guid_t guid, const char *name)
				__attribute__((__nonnull__ (2)));
extern int efi_set_variable(efi_guid_t guid, const char *name,
			    uint8_t *data, size_t data_size,
			    uint32_t attributes, mode_t mode)
				__attribute__((__nonnull__ (2, 3)));
extern int efi_append_variable(efi_guid_t guid, const char *name,
			       uint8_t *data, size_t data_size,
			       uint32_t attributes)
			      __attribute__((__nonnull__ (2, 3)));
extern int efi_get_next_variable_name(efi_guid_t **guid, char **name)
			      __attribute__((__nonnull__ (1, 2)));
extern int efi_chmod_variable(efi_guid_t guid, const char *name, mode_t mode)
			      __attribute__((__nonnull__ (2)));

extern int efi_str_to_guid(const char *s, efi_guid_t *guid)
			  __attribute__((__nonnull__ (1, 2)));
extern int efi_guid_to_str(const efi_guid_t *guid, char **sp)
			  __attribute__((__nonnull__ (1)));
extern int efi_guid_to_id_guid(const efi_guid_t *guid, char **sp)
			      __attribute__((__nonnull__ (1)));
extern int efi_guid_to_symbol(efi_guid_t *guid, char **symbol)
			     __attribute__((__nonnull__ (1, 2)));
extern int efi_guid_to_name(efi_guid_t *guid, char **name)
			   __attribute__((__nonnull__ (1, 2)));
extern int efi_name_to_guid(const char *name, efi_guid_t *guid)
			   __attribute__((__nonnull__ (1, 2)));
extern int efi_id_guid_to_guid(const char *name, efi_guid_t *guid)
			      __attribute__((__nonnull__ (1, 2)));
extern int efi_symbol_to_guid(const char *symbol, efi_guid_t *guid)
			     __attribute__((__nonnull__ (1, 2)));

extern int efi_guid_is_zero(const efi_guid_t *guid);
extern int efi_guid_is_empty(const efi_guid_t *guid);
extern int efi_guid_cmp(const efi_guid_t *a, const efi_guid_t *b);

/* import / export functions */
typedef struct efi_variable efi_variable_t;

extern ssize_t efi_variable_import(uint8_t *data, size_t size,
				efi_variable_t **var)
			__attribute__((__nonnull__ (1, 3)));
extern ssize_t efi_variable_export(efi_variable_t *var, uint8_t *data,
				size_t size)
			__attribute__((__nonnull__ (1)));

extern efi_variable_t *efi_variable_alloc(void)
			__attribute__((__visibility__ ("default")));
extern void efi_variable_free(efi_variable_t *var, int free_data);

extern int efi_variable_set_name(efi_variable_t *var, char *name)
			__attribute__((__nonnull__ (1, 2)));
extern char *efi_variable_get_name(efi_variable_t *var)
			__attribute__((__visibility__ ("default")))
			__attribute__((__nonnull__ (1)));

extern int efi_variable_set_guid(efi_variable_t *var, efi_guid_t *guid)
			__attribute__((__nonnull__ (1, 2)));
extern int efi_variable_get_guid(efi_variable_t *var, efi_guid_t **guid)
			__attribute__((__nonnull__ (1, 2)));

extern int efi_variable_set_data(efi_variable_t *var, uint8_t *data,
				size_t size)
			__attribute__((__nonnull__ (1, 2)));
extern ssize_t efi_variable_get_data(efi_variable_t *var, uint8_t **data,
				size_t *size)
			__attribute__((__nonnull__ (1, 2, 3)));

extern int efi_variable_set_attributes(efi_variable_t *var, uint64_t attrs)
			__attribute__((__nonnull__ (1)));
extern int efi_variable_get_attributes(efi_variable_t *var, uint64_t *attrs)
			__attribute__((__nonnull__ (1, 2)));

extern int efi_variable_realize(efi_variable_t *var)
			__attribute__((__nonnull__ (1)));

#ifndef EFIVAR_BUILD_ENVIRONMENT
extern int efi_error_get(unsigned int n,
			 char ** const filename,
			 char ** const function,
			 int *line,
			 char ** const message,
			 int *error)
			__attribute__((__nonnull__ (2, 3, 4, 5, 6)));
extern int efi_error_set(const char *filename,
			 const char *function,
			 int line,
			 int error,
			 const char *fmt, ...)
			__attribute__((__visibility__ ("default")))
			__attribute__((__nonnull__ (1, 2, 5)))
			__attribute__((__format__ (printf, 5, 6)));
extern void efi_error_clear(void);
#else
static inline int
__attribute__((__nonnull__ (2, 3, 4, 5, 6)))
efi_error_get(unsigned int n __attribute__((__unused__)),
	      char ** const filename __attribute__((__unused__)),
	      char ** const function __attribute__((__unused__)),
	      int *line __attribute__((__unused__)),
	      char ** const message __attribute__((__unused__)),
	      int *error __attribute__((__unused__)))
{
	return 0;
}

static inline int
__attribute__((__nonnull__ (1, 2, 5)))
__attribute__((__format__ (printf, 5, 6)))
efi_error_set(const char *filename __attribute__((__unused__)),
	      const char *function __attribute__((__unused__)),
	      int line __attribute__((__unused__)),
	      int error __attribute__((__unused__)),
	      const char *fmt __attribute__((__unused__)),
	      ...)
{
	return 0;
}

static inline void
efi_error_clear(void)
{
	return;
}
#endif

#define efi_error_real__(errval, file, function, line, fmt, args...) \
	efi_error_set(file, function, line, errval, (fmt), ## args)

#define efi_error(fmt, args...) \
	efi_error_real__(errno, __FILE__, __func__, __LINE__, (fmt), ## args)
#define efi_error_val(errval, msg, args...) \
	efi_error_real__(errval, __FILE__, __func__, __LINE__, (fmt), ## args)

extern void efi_set_verbose(int verbosity, FILE *errlog)
        __attribute__((__visibility__("default")));
extern int efi_get_verbose(void)
        __attribute__((__visibility__("default")));
extern FILE * efi_get_logfile(void)
        __attribute__((__visibility__("default")));

#include <efivar/efivar-dp.h>

#endif /* EFIVAR_H */
