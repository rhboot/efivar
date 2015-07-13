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

struct pci_root_info {
	uint16_t root_pci_domain;
	uint8_t root_pci_bus;
	uint32_t root_pci_acpi_hid;
	uint32_t root_pci_acpi_uid;
};

struct pci_dev_info {
	uint16_t pci_domain;
	uint8_t pci_bus;
	uint8_t pci_device;
	uint8_t pci_function;
};

struct scsi_info {
	uint32_t scsi_bus;
	uint32_t scsi_device;
	uint32_t scsi_target;
	uint64_t scsi_lun;
};

struct sas_info {
	uint32_t scsi_bus;
	uint32_t scsi_device;
	uint32_t scsi_target;
	uint64_t scsi_lun;

	uint64_t sas_address;
};

struct sata_info {
	uint32_t scsi_bus;
	uint32_t scsi_device;
	uint32_t scsi_target;
	uint64_t scsi_lun;

	uint32_t ata_devno;
	uint32_t ata_port;
	uint32_t ata_pmp;
};

struct disk_info {
	int interface_type;
	unsigned int controllernum;
	unsigned int disknum;
	unsigned char part;
	uint64_t major;
	unsigned char minor;
	uint32_t edd10_devicenum;

	struct pci_root_info pci_root;
	struct pci_dev_info pci_dev;

	union {
		struct scsi_info scsi_info;
		struct sas_info sas_info;
		struct sata_info sata_info;
	};

	char *disk_name;
	char *part_name;
};

enum _bus_type {bus_type_unknown, isa, pci};
enum _interface_type {interface_type_unknown,
		      ata, atapi, scsi, sata, sas, usb,
		      i1394, fibre, i2o, md,
		      virtblk, nvme};

extern int eb_disk_info_from_fd(int fd, struct disk_info *info);
extern int set_disk_and_part_name(struct disk_info *info);
extern int make_blockdev_path(uint8_t *buf, ssize_t size, int fd,
				struct disk_info *info);

extern int eb_nvme_ns_id(int fd, uint32_t *ns_id);

extern int get_partition_number(const char *devpath)
	__attribute__((__visibility__ ("hidden")));

extern ssize_t make_mac_path(uint8_t *buf, ssize_t size,
			     const char * const ifname)
	__attribute__((__visibility__ ("hidden")));

#endif /* _EFIBOOT_LINUX_H */
