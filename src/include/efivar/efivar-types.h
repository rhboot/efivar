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

#define EFI_VARIABLE_NON_VOLATILE				((uint64_t)0x0000000000000001)
#define EFI_VARIABLE_BOOTSERVICE_ACCESS				((uint64_t)0x0000000000000002)
#define EFI_VARIABLE_RUNTIME_ACCESS				((uint64_t)0x0000000000000004)
#define EFI_VARIABLE_HARDWARE_ERROR_RECORD			((uint64_t)0x0000000000000008)
#define EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS			((uint64_t)0x0000000000000010)
#define EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS	((uint64_t)0x0000000000000020)
#define EFI_VARIABLE_APPEND_WRITE				((uint64_t)0x0000000000000040)
#define EFI_VARIABLE_ENHANCED_AUTHENTICATED_ACCESS		((uint64_t)0x0000000000000080)

#endif /* EFI_TYPES_H */
// vim:fenc=utf-8:tw=75:noet
