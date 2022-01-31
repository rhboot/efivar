// SPDX-License-Identifier: LGPL-2.1
/*
 * Copyright 2012-2020 Red Hat, Inc.
 * Copyright 2012-2020 Peter M. Jones <pjones@redhat.com>
 *
 * Author(s): Peter Jones <pjones@redhat.com>
 */
#ifndef EFI_TYPES_H
#define EFI_TYPES_H 1

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	uint32_t	a;
	uint16_t	b;
	uint16_t	c;
	uint16_t	d;
	uint8_t		e[6];
} efi_guid_t __attribute__((__aligned__(1)));

#if BYTE_ORDER == LITTLE_ENDIAN
#define EFI_GUID(a,b,c,d,e0,e1,e2,e3,e4,e5) \
((efi_guid_t) {(a), (b), (c), __builtin_bswap16(d), { (e0), (e1), (e2), (e3), (e4), (e5) }})
#else
#define EFI_GUID(a,b,c,d,e0,e1,e2,e3,e4,e5) \
((efi_guid_t) {(a), (b), (c), (d), { (e0), (e1), (e2), (e3), (e4), (e5) }})
#endif

#define EFI_GLOBAL_GUID EFI_GUID(0x8be4df61,0x93ca,0x11d2,0xaa0d,0x00,0xe0,0x98,0x03,0x2b,0x8c)

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

typedef unsigned long efi_status_t;
typedef uint16_t efi_char16_t;
typedef unsigned long uintn_t;
typedef long intn_t;

#if !defined(EFIVAR_NO_EFI_TIME_T) || EFIVAR_NO_EFI_TIME_T
#define EFIVAR_HAVE_EFI_TIME_T 1

/*
 * This can never be correct, as defined, in the face of leap seconds.
 * Because seconds here are defined with a range of [0,59], we can't
 * express leap seconds correctly there.  Because TimeZone is specified in
 * minutes West of UTC, rather than seconds (like struct tm), it can't be
 * used to correct when we cross a leap second boundary condition.  As a
 * result, EFI_TIME can only express UT1, rather than UTC, and there's no
 * way when converting to know wether the error has been taken into
 * account, nor if it should be.
 *
 * As I write this, there is a 37 second error.
 */
typedef struct {
	uint16_t	year;		// 1900 - 9999
	uint8_t		month;		// 1 - 12
	uint8_t		day;		// 1 - 31
	uint8_t		hour;		// 0 - 23
	uint8_t		minute;		// 0 - 59
	uint8_t		second;		// 0 - 59 // ha ha only serious
	uint8_t		pad1;		// 0
	uint32_t	nanosecond;	// 0 - 999,999,999
	int16_t		timezone;	// minutes from UTC or EFI_UNSPECIFIED_TIMEZONE
	uint8_t		daylight;	// bitfield
	uint8_t		pad2;		// 0
} efi_time_t __attribute__((__aligned__(1)));

#define EFI_TIME_ADJUST_DAYLIGHT        ((uint8_t)0x01)
#define EFI_TIME_IN_DAYLIGHT            ((uint8_t)0x02)

#define EFI_UNSPECIFIED_TIMEZONE        ((uint16_t)0x07ff)
#endif /* !defined(EFIVAR_NO_EFI_TIME_T) || EFIVAR_NO_EFI_TIME_T */

#define EFI_VARIABLE_NON_VOLATILE				((uint64_t)0x0000000000000001)
#define EFI_VARIABLE_BOOTSERVICE_ACCESS				((uint64_t)0x0000000000000002)
#define EFI_VARIABLE_RUNTIME_ACCESS				((uint64_t)0x0000000000000004)
#define EFI_VARIABLE_HARDWARE_ERROR_RECORD			((uint64_t)0x0000000000000008)
#define EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS			((uint64_t)0x0000000000000010)
#define EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS	((uint64_t)0x0000000000000020)
#define EFI_VARIABLE_APPEND_WRITE				((uint64_t)0x0000000000000040)
#define EFI_VARIABLE_ENHANCED_AUTHENTICATED_ACCESS		((uint64_t)0x0000000000000080)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* EFI_TYPES_H */
// vim:fenc=utf-8:tw=75:noet
