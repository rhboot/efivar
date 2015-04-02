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
#ifndef _EFIBOOT_LINUX_H
#define _EFIBOOT_LINUX_H 1

extern int efi_linux_nvme_ns_id(int fd, uint32_t *ns_id);

extern int efi_linux_scsi_idlun(int fd, uint8_t *host, uint8_t *channel,
			      uint8_t *id, uint8_t *lun);
extern int efi_linux_scsi_pci(int fd, char *slot_name, size_t size);

#endif /* _EFIBOOT_LINUX_H */
