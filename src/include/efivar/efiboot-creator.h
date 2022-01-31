// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2015 Red Hat, Inc.
 * Copyright (C) 2001 Dell Computer Corporation <Matt_Domsch@dell.com>
 */
#ifndef _EFIBOOT_CREATOR_H
#define _EFIBOOT_CREATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#define EFIBOOT_ABBREV_NONE		0x00000001
#define EFIBOOT_ABBREV_HD		0x00000002
#define EFIBOOT_ABBREV_FILE		0x00000004
#define EFIBOOT_ABBREV_EDD10		0x00000008
#define EFIBOOT_OPTIONS_IGNORE_FS_ERROR	0x00000010
#define EFIBOOT_OPTIONS_WRITE_SIGNATURE	0x00000020
#define EFIBOOT_OPTIONS_IGNORE_PMBR_ERR	0x00000040

extern ssize_t efi_generate_file_device_path(uint8_t *buf, ssize_t size,
					     const char * const filepath,
					     uint32_t options, ...)
	__attribute__((__nonnull__ (3)));

extern ssize_t efi_generate_file_device_path_from_esp(uint8_t *buf,
						      ssize_t size,
						      const char *devpath,
						      int partition,
						      const char *relpath,
						      uint32_t options, ...)
	__attribute__((__nonnull__ (3, 5)))
	__attribute__((__visibility__ ("default")));


extern ssize_t efi_generate_ipv4_device_path(uint8_t *buf, ssize_t size,
					     const char * const ifname,
					     const char * const local_addr,
					     const char * const remote_addr,
					     const char * const gateway_addr,
					     const char * const netmask,
					     uint16_t local_port,
					     uint16_t remote_port,
					     uint16_t protocol,
					     uint8_t addr_origin)
	__attribute__((__nonnull__ (3,4,5,6,7)))
	__attribute__((__visibility__ ("default")));

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _EFIBOOT_CREATOR_H */

// vim:fenc=utf-8:tw=75:noet
