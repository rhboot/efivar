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
#ifndef _EFIVAR_DP_H
#define _EFIVAR_DP_H 1

/* Generic device path header */
typedef struct {
	uint8_t type;
	uint8_t subtype;
	uint16_t length;
} efidp_header;

/* A little bit of housekeeping... */
typedef uint8_t efidp_boolean;

/* Each of the top-level types */
#define	EFIDP_HARDWARE_TYPE	0x01
#define EFIDP_ACPI_TYPE		0x02
#define EFIDP_MESSAGE_TYPE	0x03
#define EFIDP_MEDIA_TYPE	0x04
#define EFIDP_BIOS_BOOT_TYPE	0x05
#define EFIDP_END_TYPE		0x7f

/* Each hardware subtype */
#define EFIDP_HW_PCI_SUBTYPE	0x01
typedef struct {
	efidp_header	header;
	uint8_t		function;
	uint8_t		device;
} efidp_pci;

#define EFIDP_HW_PCCARD_SUBTYPE	0x02
typedef struct {
	efidp_header	header;
	uint8_t		function;
} efidp_pccard;

#define EFIDP_HW_MMIO		0x03
typedef struct {
	efidp_header	header;
	uint32_t	memory_type;
	uint64_t	starting_address;
	uint64_t	ending_address;
} efidp_mmio;

#define EFIDP_HW_VENDOR		0x04
typedef struct {
	efidp_header	header;
	efi_guid_t	vendor_guid;
	uint8_t		vendor_data[0];
} efidp_hw_vendor;
typedef efidp_hw_vendor efidp_vendor_hw;

#define EFIDP_HW_CONTROLLER	0x05
typedef struct {
	efidp_header	header;
	uint32_t	controller;
} efidp_controller;

#define EFIDP_BMC		0x06
typedef struct {
	efidp_header	header;
	uint8_t		interface_type;
	uint64_t	base_addr;
} efidp_bmc;

#define EFIDP_BMC_UNKNOWN	0x00
#define EFIDP_BMC_KEYBOARD	0x01
#define EFIDP_BMC_SMIC		0x02
#define EFIDP_BMC_BLOCK		0x03


/* Each ACPI subtype */
#define EFIDP_ACPI_HID		0x02
typedef struct {
	efidp_header	header;
	uint32_t	hid;
	uint32_t	uid;
	uint32_t	cid;
} efidp_acpi_hid;

#define EFIDP_PNP_EISA_ID_CONST		0x41d0
#define EFIDP_EISA_ID(_Name, _Num)	((uint32_t)((_Name) | (_Num) << 16))
#define EFIDP_EISA_PNP_ID(_PNPId)	EFIDP_EISA_ID(EFIDP_PNP_EISA_ID_CONST,\
							(_PNPId))
#define EFIDP_EFI_PNP_ID(_PNPId)	EFIDP_EISA_ID(EFIDP_PNP_EISA_ID_CONST,\
							(_PNPId))

#define EFIDP_PNP_EISA_ID_MASK		0xffff
#define EFIDP_EISA_ID_TO_NUM(_Id)	((_Id) >> 16)

#define EFIDP_ACPI_PCI_ROOT_HID	EFIDP_EFI_PNP_ID(0x0a03)
#define EFIDP_ACPI_FLOPPY_HID	EFIDP_EFI_PNP_ID(0x0604)
#define EFIDP_ACPI_KEYBOARD_HID	EFIDP_EFI_PNP_ID(0x0301)
#define EFIDP_ACPI_SERIAL_HID	EFIDP_EFI_PNP_ID(0x0501)
#define EFIDP_ACPI_PARALLEL_HID	EFIDP_EFI_PNP_ID(0x0401)

#define EFIDP_ACPI_ADR		0x03
typedef struct {
	efidp_header	header;
	uint32_t	adr;
} efidp_acpi_adr;

#define EFIDP_ACPI_ADR_DISPLAY_TYPE_OTHER		0
#define EFIDP_ACPI_ADR_DISPLAY_TYPE_VGA			1
#define EFIDP_ACPI_ADR_DISPLAY_TYPE_TV			2
#define EFIDP_ACPI_ADR_DISPLAY_TYPE_EXTERNAL_DIGITAL	3
#define EFIDP_ACPI_ADR_DISPLAY_TYPE_INTERNAL_DIGITAL	4

#define EFIDP_ACPI_DISPLAY_ADR(_DeviceIdScheme, _HeadId, _NonVgaOutput,\
			       _BiosCanDetect, _VendorInfo, _Type, _Port,\
			       _Index) \
	((UINT32)((((_DeviceIdScheme) & 0x1) << 31) | \
		  (((_HeadId)         & 0x7) << 18) | \
		  (((_NonVgaOutput)   & 0x1) << 17) | \
		  (((_BiosCanDetect)  & 0x1) << 16) | \
		  (((_VendorInfo)     & 0xf) << 12) | \
		  (((_Type)           & 0xf) << 8)  | \
		  (((_Port)           & 0xf) << 4)  | \
		  (((_Index)          & 0xf) << 0)))

/* Each messaging subtype */
#define EFIDP_MSG_ATAPI		0x01
typedef struct {
	efidp_header	header;
	uint8_t		primary;
	uint8_t		slave;
	uint16_t	lun;
} efidp_atapi;

#define EFIDP_MSG_SCSI		0x02
typedef struct {
	efidp_header	header;
	uint16_t	target;
	uint16_t	lun;
} efidp_scsi;

#define EFIDP_MSG_FIBRECHANNEL	0x03
typedef struct {
	efidp_header	header;
	uint32_t	reserved;
	uint64_t	wwn;
	uint64_t	lun;
} efidp_fc;

#define EFIDP_MSG_FIBRECHANNELEX 0x15
typedef struct {
	efidp_header	header;
	uint32_t	reserved;
	uint8_t		wwn[8];
	uint8_t		lun[8];
} efidp_fcex;

#define EFIDP_MSG_1394		0x04
typedef struct {
	efidp_header	header;
	uint32_t	reserved;
	uint64_t	guid;
} efidp_1394;

#define EFIDP_MSG_USB		0x05
typedef struct {
	efidp_header	header;
	uint8_t		parent_port;
	uint8_t		interface;
} efidp_usb;

#define EFIDP_MSG_USB_CLASS	0x0f
typedef struct {
	efidp_header	header;
	uint16_t	vendor_id;
	uint16_t	product_id;
	uint8_t		device_class;
	uint8_t		device_subclass;
	uint8_t		device_protocol;
} efidp_usb_class;

#define EFIDP_MSG_USB_WWID	0x10
typedef struct {
	efidp_header	header;
	uint16_t	interface;
	uint16_t	vendor_id;
	uint16_t	product_id;
} efidp_usb_wwid;

#define EFIDP_MSG_LUN		0x11
typedef struct {
	efidp_header	header;
	uint8_t		lun;
} efidp_lun;

#define EFIDP_MSG_SATA		0x12
typedef struct {
	efidp_header	header;
	uint16_t	hba_port;
	uint16_t	port_multiplier_port;
	uint16_t	lun;
} efidp_sata;
#define SATA_HBA_DIRECT_CONNECT_FLAG	0x8000

#define	EFIDP_MSG_I2O		0x06
typedef struct {
	efidp_header	header;
	uint32_t	target;
} efidp_i2o;

#define EFIDP_MSG_MAC_ADDR
typedef struct {
	efidp_header	header;
	uint8_t		mac_addr[32];
	uint8_t		if_type;
} efidp_mac_addr;

#define EFIDP_MSG_IPv4		0x0c
typedef struct {
	efidp_header	header;
	uint8_t		local_ipv4_addr[4];
	uint8_t		remote_ipv4_addr[4];
	uint16_t	local_port;
	uint16_t	remote_port;
	uint16_t	protocol;
	efidp_boolean	static_ip_addr;
	uint8_t		gateway[4];
	uint8_t		netmask[4];
} efidp_ipv4_addr;

#define EFIDP_IPv4_ORIGIN_DHCP		0x00
#define EFIDP_IPv4_ORIGIN_STATIC	0x01

#define EFIDP_MSG_IPv6		0x0d
typedef struct {
	efidp_header	header;
	uint8_t		local_ipv6_addr[16];
	uint8_t		remote_ipv6_addr[16];
	uint16_t	local_port;
	uint16_t	remote_port;
	uint16_t	protocol;
	uint8_t		ip_addr_origin;
	uint8_t		prefix_length;
	uint8_t		gateway_ipv6_addr;
} efidp_ipv6_addr;

#define EFIDP_IPv6_ORIGIN_MANUAL	0x00
#define EFIDP_IPv6_ORIGIN_AUTOCONF	0x01
#define EFIDP_IPv6_ORIGIN_STATEFUL	0x02

#define EFIDP_MSG_VLAN		0x14
typedef struct {
	efidp_header	header;
	uint16_t	vlan_id;
} efidp_vlan;

#define EFIDP_MSG_INFINIBAND	0x09
typedef struct {
	efidp_header	header;
	uint32_t	resource_flags;
	uint64_t	port_gid[2];
	union {
		uint64_t	ioc_guid;
		uint64_t	service_id;
	};
	uint64_t	target_port_id;
	uint64_t	device_id;
} efidp_infiniband;

#define EFIDP_INFINIBAND_RESOURCE_IOC_SERVICE	0x01
#define EFIDP_INFINIBAND_RESOURCE_EXTENDED_BOOT	0x02
#define EFIDP_INFINIBAND_RESOURCE_CONSOLE	0x04
#define EFIDP_INFINIBAND_RESOURCE_STORAGE	0x08
#define EFIDP_INFINIBAND_RESOURCE_NETWORK	0x10

#define EFIDP_MSG_UART		0x0e
typedef struct {
	efidp_header	header;
	uint32_t	reserved;
	uint64_t	baud_rate;
	uint8_t		data_bits;
	uint8_t		parity;
	uint8_t		stop_bits;
} efidp_uart;

#define EFIDP_UART_PARITY_DEFAULT	0x00
#define EFIDP_UART_PARITY_NONE		0x01
#define EFIDP_UART_PARITY_EVEN		0x02
#define EFIDP_UART_PARITY_ODD		0x03
#define EFIDP_UART_PARITY_MARK		0x04
#define EFIDP_UART_PARITY_SPACE		0x05

#define EFIDP_UART_STOP_BITS_DEFAULT	0x00
#define EFIDP_UART_STOP_BITS_ONE	0x01
#define EFIDP_UART_STOP_BITS_ONEFIVE	0x02
#define EFIDP_UART_STOP_BITS_TWO	0x03

#define EFIDP_PC_ANSI_GUID \
	EFI_GUID(0xe0c14753,0xf9be,0x11d2,0x9a0c,0x00,0x90,0x27,0x3f,0xc1,0x4d)
#define EFIDP_VT_100_GUID \
	EFI_GUID(0xdfa66065,0xb419,0x11d3,0x9a2d,0x00,0x90,0x27,0x3f,0xc1,0x4d)
#define EFIDP_VT_100_PLUS_GUID\
	EFI_GUID(0x7baec70b,0x57e0,0x4c76,0x8e87,0x2f,0x9e,0x28,0x08,0x83,0x43)
#define EFIDP_VT_UTF8_GUID\
	EFI_GUID(0xad15a0d6,0x8bec,0x4acf,0xa073,0xd0,0x1d,0xe7,0x7e,0x2d,0x88)

#define EFIDP_MSG_VENDOR	0x0a
typedef struct {
	efidp_header	header;
	efi_guid_t	vendor_guid;
	uint8_t		vendor_data[0];
} efidp_msg_vendor;
typedef efidp_msg_vendor efidp_vendor_msg;

/* The next ones are phrased as vendor specific, but they're in the spec. */
#define EFIDP_MSG_UART_FLOW_CONTROL	0x10
#define EFIDP_MSG_UART_GUID \
	EFI_GUID(0x37499a9d,0x542f,0x4c89,0xa026,0x35,0xda,0x14,0x20,0x94,0xe4)
typedef struct {
	efidp_header	header;
	efi_guid_t	vendor_guid;
	uint32_t	flow_control_map;
} efidp_uart_flow_control;

#define EFIDP_UART_FLOW_CONTROL_HARDWARE	0x1
#define EFIDP_UART_FLOW_CONTROL_XONXOFF		0x2

#define EFIDP_MSG_SAS		0x0a
#define EFIDP_MSG_SAS_GUID \
	EFI_GUID(0xd487ddb4,0x008b,0x11d9,0xafdc,0x00,0x10,0x83,0xff,0xca,0x4d)
typedef struct {
	efidp_header	header;
	efi_guid_t	vendor_guid;
	uint32_t	reserved;
	uint64_t	sas_address;
	uint64_t	lun;
	uint8_t		device_topology_info;
	uint8_t		drive_bay_id; /* If EFIDP_SAS_TOPOLOGY_NEXTBYTE set */
	uint16_t	rtp;
} efidp_sas;

/* device_topology_info Bits 0:3 (enum) */
#define EFIDP_SAS_TOPOLOGY_NONE		0x00
#define EFIDP_SAS_TOPOLOGY_THISBYTE	0x01
#define EFIDP_SAS_TOPOLOGY_NEXTBYTE	0x02

/* device_topology_info Bits 4:5 (enum) */
#define EFIDP_SAS_DEVICE_SAS_INTERNAL	0x00
#define EFIDP_SAS_DEVICE_SATA_INTERNAL	0x10
#define EFIDP_SAS_DEVICE_SAS_EXTERNAL	0x20
#define EFIDP_SAS_DEVICE_SATA_EXTERNAL	0x30

/* device_topology_info Bits 6:7 (enum) */
#define EFIDP_SAS_TOPOLOGY_DIRECT	0x00
#define EFIDP_SAS_TOPOLOGY_EXPANDER	0x40

#define EFIDP_MSG_SAS_EX	0x16
typedef struct {
	efidp_header	header;
	uint8_t		sas_address[8];
	uint8_t		lun[8];
	uint8_t		device_topology_info;
	uint8_t		drive_bay_id; /* If EFIDP_SAS_TOPOLOGY_NEXTBYTE set */
	uint16_t	rtp;
} efidp_sas_ex;

#define EFIDP_MSG_ISCSI		0x13
typedef struct {
	efidp_header	header;
	uint16_t	protocol;
	uint16_t	options;
	uint8_t		lun[8];
	uint16_t	tpgt;
	uint8_t		target_name[0];
} efidp_iscsi;

/* options bits 0:1 */
#define EFIDP_ISCSI_OPTION_NO_HEADER_DIGEST	0x0000
#define EFIDP_ISCSI_OPTION_HEADER_CRC32		0x0002

/* option bits 2:3 */
#define EFIDP_ISCSI_OPTION_NO_DATA_DIGEST	0x0000
#define EFIDP_ISCSI_OPTION_DATA_CRC32		0x0008

/* option bits 4:9 */
#define EFIDP_ISCSI_OPTION_RESERVED		0x0000

/* option bits 10:11 */
#define EFIDP_ISCSI_OPTION_AUTH_CHAP		0x0000
#define EFIDP_ISCSI_OPTION_AUTH_NONE		0x0800

/* option bit 12 */
#define EFIDP_ISCSI_OPTION_CHAP_BI		0x0000
#define EFIDP_ISCSI_OPTION_CHAP_UNI		0x1000

#define EFIDP_ISCSI_MAX_TARGET_NAME_LEN		223

#define EFIDP_MSG_NVME		0x17
typedef struct {
	efidp_header	header;
	uint32_t	namespace_id;
	uint64_t	ieee_eui_64;
} efidp_nvme;

#define EFIDP_MSG_URI		0x18
typedef struct {
	efidp_header	header;
	uint8_t		uri[0];
} efidp_uri;

#define EFIDP_MSG_UFS		0x19
typedef struct {
	efidp_header	header;
	uint8_t		target_id;
	uint8_t		lun;
} efidp_ufs;

#define EFIDP_MSG_SD		0x1a
typedef struct {
	efidp_header	header;
	uint8_t		slot_number;
} efidp_sd;

/* Each media subtype */
#define efidp_HD		0x1
typedef struct {
	efidp_header	header;
	uint32_t	partition_number;
	uint64_t	start;
	uint64_t	size;
	uint8_t		signature[16];
	uint8_t		format;
	uint8_t		signature_type;
} efidp_hd;

#define EFIDP_HD_FORMAT_PCAT	0x01
#define EFIDP_HD_FORMAT_GPT	0x02

#define EFIDP_HD_SIGNATURE_NONE		0x00
#define EFIDP_HD_SIGNATURE_MBR		0x01
#define EFIDP_HD_SIGNATURE_GUID		0x02

#define efidp_CDROM	0x2
typedef struct {
	efidp_header	header;
	uint32_t	boot_catalog_entry;
	uint64_t	partition_rba;
	uint64_t	sectors;
} efidp_cdrom;

#define efidp_VENDOR	0x3
typedef struct {
	efidp_header	header;
	efi_guid_t	vendor_guid;
	uint8_t		vendor_data[0];
} efidp_media_vendor;
typedef efidp_media_vendor efidp_vendor_media;

#define efidp_FILE	0x4
typedef struct {
	efidp_header	header;
	uint8_t		name[0];
} efidp_file;

#define efidp_PROTOCOL	0x5
typedef struct {
	efidp_header	header;
	efi_guid_t	protocol_guid;
} efidp_protocol;

#define efidp_FIRMWARE_FILE	0x6
typedef struct {
	efidp_header	header;
	uint8_t		pi_info[0];
} efidp_firmware_file;

#define efidp_FIRMWARE_VOLUME	0x7
typedef struct {
	efidp_header	header;
	uint8_t		pi_info[0];
} efidp_firmware_volume;

#define efidp_RELATIVE_OFFSET	0x8
typedef struct {
	efidp_header	header;
	uint32_t	reserved;
	uint64_t	first_byte;
	uint64_t	last_byte;
} efidp_relative_offset;

#define efidp_RAMDISK	0x9
typedef struct {
	efidp_header	header;
	uint64_t	start_addr;
	uint64_t	end_addr;
	efi_guid_t	disk_type_guid;
	uint16_t	instance_number;
} efidp_ramdisk;

#define EFIDP_VIRTUAL_DISK_GUID \
	EFI_GUID(0x77AB535A,0x45FC,0x624B,0x5560,0xF7,0xB2,0x81,0xD1,0xF9,0x6E)
#define EFIDP_VIRTUAL_CD_GUID \
	EFI_GUID(0x3D5ABD30,0x4175,0x87CE,0x6D64,0xD2,0xAD,0xE5,0x23,0xC4,0xBB)
#define EFIDP_PERSISTENT_VIRTUAL_DISK_GUID \
	EFI_GUID(0x5CEA02C9,0x4D07,0x69D3,0x269F,0x44,0x96,0xFB,0xE0,0x96,0xF9)
#define EFIDP_PERSISTENT_VIRTUAL_CD_GUID \
	EFI_GUID(0x08018188,0x42CD,0xBB48,0x100F,0x53,0x87,0xD5,0x3D,0xED,0x3D)

/* Each BIOS Boot subtype */
#define EFIDP_BIOS_BOOT	0x1
typedef struct {
	efidp_header	header;
	uint16_t	device_type;
	uint16_t	status;
	uint8_t		description[0];
} efidp_bios_boot;

#define EFIDP_BIOS_BOOT_DEVICE_TYPE_FLOPPY	1
#define EFIDP_BIOS_BOOT_DEVICE_TYPE_HD		2
#define EFIDP_BIOS_BOOT_DEVICE_TYPE_CDROM	3
#define EFIDP_BIOS_BOOT_DEVICE_TYPE_PCMCIA	4
#define EFIDP_BIOS_BOOT_DEVICE_TYPE_USB		5
#define EFIDP_BIOS_BOOT_DEVICE_TYPE_EMBEDDED_NET 6
#define EFIDP_BIOS_BOOT_DEVICE_TYPE_UNKNOWN	0xff

#define EFIDP_END_ENTIRE	0xff
#define EFIDP_END_INSTANCE	0x01

/* utility functions */
typedef union {
	struct {
		uint8_t type;
		uint8_t subtype;
		uint16_t length;
	}; 
	efidp_header header;
	efidp_pci pci;
	efidp_pccard pccard;
	efidp_mmio mmio;
	efidp_hw_vendor hw_vendor;
	efidp_controller controller;
	efidp_bmc bmc;
	efidp_acpi_hid acpi_hid;
	efidp_acpi_adr acpi_adr;
	efidp_atapi atapi;
	efidp_scsi scsi;
	efidp_fc fc;
	efidp_fcex fcex;
	efidp_1394 firewire;
	efidp_usb usb;
	efidp_usb_class usb_class;
	efidp_usb_wwid usb_wwid;
	efidp_lun lun;
	efidp_sata sata;
	efidp_i2o i2o;
	efidp_mac_addr mac_addr;
	efidp_ipv4_addr ipv4_addr;
	efidp_ipv6_addr ipv6_addr;
	efidp_vlan vlan;
	efidp_infiniband infiniband;
	efidp_uart uart;
	efidp_msg_vendor msg_vendor;
	efidp_uart_flow_control uart_flow_control;
	efidp_sas sas;
	efidp_sas_ex sas_ex;
	efidp_iscsi iscsi;
	efidp_nvme nvme;
	efidp_uri uri;
	efidp_ufs ufs;
	efidp_sd sd;
	efidp_hd hd;
	efidp_cdrom cdrom;
	efidp_media_vendor media_vendor;
	efidp_file file;
	efidp_protocol protocol;
	efidp_firmware_file firmware_file;
	efidp_firmware_volume firmware_volume;
	efidp_relative_offset relative_offset;
	efidp_ramdisk ramdisk;
	efidp_bios_boot bios_boot;
} efidp_data;
typedef efidp_data *efidp;
typedef const efidp_data const *const_efidp;

extern ssize_t efidp_create_node(uint8_t type, uint8_t subtype, size_t len,
				 void *buf, size_t bufsize);
extern int efidp_set_node_data(const_efidp dn, void *buf, size_t bufsize);
extern int efidp_duplicate_path(const_efidp dp, efidp *out);
extern int efidp_append_path(const_efidp dp0, const_efidp dp1, efidp *out);
extern int efidp_append_node(const_efidp dp, const_efidp dn, efidp *out);
extern int efidp_append_instance(const_efidp dp, const_efidp dpi, efidp *out);

static inline int8_t
__attribute__((__unused__))
efidp_type(const_efidp dp)
{
	if (!dp) {
		errno = EINVAL;
		return -1;
	}
	return dp->type;
}

static inline int8_t
__attribute__((__unused__))
efidp_subtype(const_efidp dp)
{
	if (!dp) {
		errno = EINVAL;
		return -1;
	}
	return dp->subtype;
}

static inline ssize_t
__attribute__((__unused__))
efidp_node_size(const_efidp dn)
{
	if (!dn || dn->length < 4) {
		errno = EINVAL;
		return -1;
	}
	return dn->length;
}

static inline int
__attribute__((__unused__))
efidp_next_node(const_efidp in, const_efidp *out)
{
	if (efidp_type(in) == EFIDP_END_TYPE)
		return -1;

	ssize_t sz = efidp_node_size(in);
	if (sz < 0)
		return -1;

	/* I love you gcc. */
	*out = (const_efidp)(const efidp_header const *)((uint8_t *)in + sz);
	return 0;
}

static inline int
__attribute__((__unused__))
efidp_next_instance(const_efidp in, const_efidp *out)
{
	if (efidp_type(in) != EFIDP_END_TYPE ||
			efidp_subtype(in) != EFIDP_END_INSTANCE)
		return -1;

	ssize_t sz = efidp_node_size(in);
	if (sz < 0)
		return -1;

	/* I love you gcc. */
	*out = (const_efidp)(const efidp_header const *)((uint8_t *)in + sz);
	return 0;
}

static inline int
__attribute__((__unused__))
efidp_dp_is_multiinstance(const_efidp dn)
{
	while (1) {
		const_efidp next;
		int rc = efidp_next_node(dn, &next);
		if (rc < 0)
			break;
	}

	if (efidp_type(dn) == EFIDP_END_TYPE &&
			efidp_subtype(dn) == EFIDP_END_INSTANCE)
		return 1;
	return 0;
}

static inline int
__attribute__((__unused__))
efidp_get_next_end(const_efidp in, const_efidp *out)
{
	while (1) {
		if (efidp_type(in) == EFIDP_END_TYPE) {
			*out = in;
			return 0;
		}

		ssize_t sz;
		sz = efidp_node_size(in);
		if (sz < 0)
			break;

		in = (const_efidp)(const efidp_header const *)((uint8_t *)in + sz);
	}
	return -1;
}

static inline ssize_t
__attribute__((__unused__))
efidp_size(const_efidp dp)
{
	ssize_t ret = 0;
	if (!dp) {
		errno = EINVAL;
		return -1;
	}

	while (1) {
		ssize_t sz;
		int rc;
		const_efidp next;

		sz = efidp_node_size(dp);
		if (sz < 0)
			return sz;
		ret += sz;

		if (efidp_type(dp) == EFIDP_END_TYPE &&
				efidp_subtype(dp) == EFIDP_END_ENTIRE)
			break;

		rc = efidp_next_instance(dp, &next);
		if (rc < 0)
			rc = efidp_next_node(dp, &next);
		if (rc < 1)
			return -1;

		dp = next;
	}
	return ret;
}

static inline ssize_t
__attribute__((__unused__))
efidp_instance_size(const_efidp dpi)
{
	ssize_t ret = 0;
	while (1) {
		ssize_t sz;
		const_efidp next;

		sz = efidp_node_size(dpi);
		if (sz < 0)
			return sz;
		ret += sz;

		if (efidp_type(dpi) == EFIDP_END_TYPE)
			break;

		int rc = efidp_next_node(dpi, &next);
		if (rc < 1)
			return -1;
		dpi = next;
	}
	return ret;
}

/* and now, printing and parsing */
extern ssize_t efidp_parse_device_node(char *path, efidp out, size_t size);
extern ssize_t efidp_parse_device_path(char *path, efidp out, size_t size);
extern ssize_t efidp_print_device_path(char *buf, size_t size, const_efidp dp);

#endif /* _EFIVAR_DP_H */
