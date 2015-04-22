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
#ifndef _EFIBOOT_CREATOR_H
#define _EFIBOOT_CREATOR_H

#define EFIBOOT_ABBREV_NONE	0x00000001
#define EFIBOOT_ABBREV_HD	0x00000002
#define EFIBOOT_ABBREV_FILE	0x00000003
#define EFIBOOT_ABBREV_EDD10	0x80000000

extern ssize_t efi_generate_file_device_path(uint8_t *buf, ssize_t size,
					     const char const *filepath,
					     uint32_t abbrev,
					     uint32_t extra, /* :/ */
					     int ignore_fs_err,
					     int write_signature,
					     int ignore_pmbr_error)
	__attribute__((__nonnull__ (3)));

extern ssize_t efi_generate_network_device_path(uint8_t *buf, ssize_t size,
						const char const *ifname,
						uint32_t abbrev)
	__attribute__((__nonnull__ (3)));

#endif /* _EFIBOOT_CREATOR_H */
