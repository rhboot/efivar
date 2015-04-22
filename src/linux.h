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
#ifndef _EFIBOOT_LINUX_H
#define _EFIBOOT_LINUX_H

struct disk_info {
	int interface_type;
	unsigned int controllernum;
	unsigned int disknum;
	unsigned char part;
	uint64_t major;
	unsigned char minor;
};

enum _bus_type {bus_type_unknown, isa, pci};
enum _interface_type {interface_type_unknown,
		      ata, atapi, scsi, usb,
		      i1394, fibre, i2o, md,
		      virtblk, nvme};

extern int eb_disk_info_from_fd(int fd, struct disk_info *info);

extern int eb_modern_block_pci(const struct disk_info *info, uint8_t *bus,
		       uint8_t *device, uint8_t *function);
extern int eb_scsi_pci(int fd, const struct disk_info *info, uint8_t *bus,
		       uint8_t *device, uint8_t *function);
extern int eb_ide_pci(int fd, const struct disk_info *info, uint8_t *bus,
		      uint8_t *device, uint8_t *function);

extern int eb_nvme_ns_id(int fd, uint32_t *ns_id);

extern int eb_scsi_idlun(int fd, uint8_t *host, uint8_t *channel,
			      uint8_t *id, uint8_t *lun);

#endif /* _EFIBOOT_LINUX_H */
