/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2015 Red Hat, Inc.
 * Copyright (C) 2001 Dell Computer Corporation <Matt_Domsch@dell.com>
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

struct nvme_info {
	int32_t ctrl_id;
	int32_t ns_id;
	int has_eui;
	uint8_t eui[8];
};

struct disk_info {
	int interface_type;
	unsigned int controllernum;
	unsigned int disknum;
	unsigned char part;
	uint64_t major;
	uint32_t minor;
	uint32_t edd10_devicenum;

	struct pci_root_info pci_root;
	struct pci_dev_info pci_dev;

	union {
		struct scsi_info scsi_info;
		struct sas_info sas_info;
		struct sata_info sata_info;
		struct nvme_info nvme_info;
		efi_guid_t nvdimm_label;
	};

	char *disk_name;
	char *part_name;
};

enum _bus_type {bus_type_unknown, isa, pci};
enum _interface_type {interface_type_unknown,
		      ata, atapi, scsi, sata, sas, usb,
		      i1394, fibre, i2o, md,
		      virtblk, nvme, nd_pmem};

extern int eb_disk_info_from_fd(int fd, struct disk_info *info);
extern int set_disk_and_part_name(struct disk_info *info);
extern int make_blockdev_path(uint8_t *buf, ssize_t size,
			      struct disk_info *info);

extern int eb_nvme_ns_id(int fd, uint32_t *ns_id);

extern int HIDDEN get_partition_number(const char *devpath);

extern int HIDDEN find_parent_devpath(const char * const child,
                                      char **parent);

extern ssize_t HIDDEN make_mac_path(uint8_t *buf, ssize_t size,
                                    const char * const ifname);

#define read_sysfs_file(buf, fmt, args...)                              \
        ({                                                              \
                uint8_t *buf_ = NULL;                                   \
                ssize_t bufsize_ = -1;                                  \
                int error_;                                             \
                                                                        \
                bufsize_ = get_file(&buf_, "/sys/" fmt, ## args);       \
                if (bufsize_ > 0) {                                     \
                        uint8_t *buf2_ = alloca(bufsize_);              \
                        error_ = errno;                                 \
                        if (buf2_)                                      \
                                memcpy(buf2_, buf_, bufsize_);          \
                        free(buf_);                                     \
                        *(buf) = (__typeof__(*(buf)))buf2_;             \
                        errno = error_;                                 \
                }                                                       \
                bufsize_;                                               \
        })

#define sysfs_readlink(linkbuf, fmt, args...)                           \
        ({                                                              \
                char *_lb = alloca(PATH_MAX+1);                         \
                char *_pn;                                              \
                int _rc;                                                \
                                                                        \
                *(linkbuf) = NULL;                                      \
                _rc = asprintfa(&_pn, "/sys/" fmt, ## args);            \
                if (_rc >= 0) {                                         \
                        ssize_t _linksz;                                \
                        _rc = _linksz = readlink(_pn, _lb, PATH_MAX);   \
                        if (_linksz >= 0)                               \
                                _lb[_linksz] = '\0';                    \
                        else                                            \
                                efi_error("readlink of %s failed", _pn);\
                        *(linkbuf) = _lb;                               \
                } else {                                                \
                        efi_error("could not allocate memory");         \
                }                                                       \
                _rc;                                                    \
        })

#define sysfs_stat(statbuf, fmt, args...)                               \
        ({                                                              \
                int rc_;                                                \
                char *pn_;                                              \
                                                                        \
                rc_ = asprintfa(&pn_, "/sys/" fmt, ## args);            \
                if (rc_ >= 0) {                                         \
                        rc_ = stat(pn_, statbuf);                       \
                        if (rc_ < 0)                                    \
                                efi_error("could not stat %s", pn_);    \
                } else {                                                \
                        efi_error("could not allocate memory");         \
                }                                                       \
                rc_;                                                    \
        })

#define sysfs_opendir(fmt, args...)                                     \
        ({                                                              \
                int rc_;                                                \
                char *pn_;                                              \
                DIR *dir_ = NULL;                                       \
                                                                        \
                rc_ = asprintfa(&pn_, "/sys/" fmt, ## args);            \
                if (rc_ >= 0) {                                         \
                        dir_ = opendir(pn_);                            \
                        if (dir_ == NULL)                               \
                                efi_error("could not open %s", pn_);    \
                } else {                                                \
                        efi_error("could not allocate memory");         \
                }                                                       \
                dir_;                                                   \
        })

#endif /* _EFIBOOT_LINUX_H */
