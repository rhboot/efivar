// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012-2015 Red Hat, Inc.
 */
#ifndef _EFIVAR_DP_H
#define _EFIVAR_DP_H 1

#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

#define efidp_encode_bitfield_(name, shift, mask)			\
	(((name) << (shift)) & (mask))
#define efidp_decode_bitfield_(value, name, shift, mask)		\
	({ (name) = ((value) & (mask)) >> (shift); })

#define efidp_size_after(dp, field)					\
	(efidp_size((const_efidp)dp) -					\
	 (offsetof(__typeof__ (*(dp)), field) + sizeof((dp)->field)))

#define EFIVAR_PACKED __attribute__((__packed__))

/* Generic device path header */
typedef struct {
	uint8_t type;
	uint8_t subtype;
	uint16_t length;
} EFIVAR_PACKED efidp_header;

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
#define EFIDP_HW_PCI	0x01
typedef struct {
	efidp_header	header;
	uint8_t		function;
	uint8_t		device;
} EFIVAR_PACKED efidp_pci;
extern ssize_t efidp_make_pci(uint8_t *buf, ssize_t size, uint8_t device,
			      uint8_t function);

#define EFIDP_HW_PCCARD	0x02
typedef struct {
	efidp_header	header;
	uint8_t		function;
} EFIVAR_PACKED efidp_pccard;

#define EFIDP_HW_MMIO		0x03
typedef struct {
	efidp_header	header;
	uint32_t	memory_type;
	uint64_t	starting_address;
	uint64_t	ending_address;
} EFIVAR_PACKED efidp_mmio;

#define EFIDP_HW_VENDOR		0x04
typedef struct {
	efidp_header	header;
	efi_guid_t	vendor_guid;
	uint8_t		vendor_data[];
} EFIVAR_PACKED efidp_hw_vendor;
typedef efidp_hw_vendor efidp_vendor_hw;
#define efidp_make_hw_vendor(buf, size, guid, data, data_size)		\
	efidp_make_vendor(buf, size, EFIDP_HARDWARE_TYPE,		\
			  EFIDP_HW_VENDOR, guid, data, data_size)

#define EDD10_HARDWARE_VENDOR_PATH_GUID \
	EFI_GUID(0xCF31FAC5,0xC24E,0x11d2,0x85F3,0x00,0xA0,0xC9,0x3E,0xC9,0x3B)
typedef struct {
	efidp_header	header;
	efi_guid_t	vendor_guid;
	uint32_t	hardware_device;
} EFIVAR_PACKED efidp_edd10;
extern ssize_t efidp_make_edd10(uint8_t *buf, ssize_t size,
				uint32_t hardware_device);

#define EFIDP_HW_CONTROLLER	0x05
typedef struct {
	efidp_header	header;
	uint32_t	controller;
} EFIVAR_PACKED efidp_controller;

#define EFIDP_HW_BMC		0x06
typedef struct {
	efidp_header	header;
	uint8_t		interface_type;
	uint64_t	base_addr;
} EFIVAR_PACKED efidp_bmc;

#define EFIDP_BMC_UNKNOWN	0x00
#define EFIDP_BMC_KEYBOARD	0x01
#define EFIDP_BMC_SMIC		0x02
#define EFIDP_BMC_BLOCK		0x03


/* Each ACPI subtype */
#define EFIDP_ACPI_HID		0x01
typedef struct {
	efidp_header	header;
	uint32_t	hid;
	uint32_t	uid;
} EFIVAR_PACKED efidp_acpi_hid;
extern ssize_t efidp_make_acpi_hid(uint8_t *buf, ssize_t size, uint32_t hid,
				   uint32_t uid);

#define EFIDP_ACPI_HID_EX	0x02
typedef struct {
	efidp_header	header;
	uint32_t	hid;
	uint32_t	uid;
	uint32_t	cid;
	/* three ascii string fields follow */
	char		hidstr[];
} EFIVAR_PACKED efidp_acpi_hid_ex;
extern ssize_t
efidp_make_acpi_hid_ex(uint8_t *buf, ssize_t size,
		       uint32_t hid, uint32_t uid, uint32_t cid,
		       const char *hidstr, const char *uidstr,
		       const char *cidstr);

#define EFIDP_PNP_EISA_ID_CONST		0x41d0
#define EFIDP_PNP_ACPI_ID_CONST		0x8e09
#define EFIDP_EISA_ID(_Name, _Num)	((uint32_t)((_Name) | (_Num) << 16))
#define EFIDP_EISA_PNP_ID(_PNPId)	EFIDP_EISA_ID(EFIDP_PNP_EISA_ID_CONST,\
							(_PNPId))
#define EFIDP_EFI_PNP_ID(_PNPId)	EFIDP_EISA_ID(EFIDP_PNP_EISA_ID_CONST,\
							(_PNPId))
#define EFIDP_EFI_ACPI_ID(_HID)		EFIDP_EISA_ID(EFIDP_PNP_ACPI_ID_CONST,\
						      (_HID))

#define EFIDP_PNP_EISA_ID_MASK		0xffff
#define EFIDP_EISA_ID_TO_NUM(_Id)	((_Id) >> 16)
#define EFIDP_ACPI_ID_MASK		0xffff
#define EFIDP_ACPI_ID_TO_NUM(_HID)	((_HID) >> 16)

#define EFIDP_ACPI_PCI_ROOT_HID		EFIDP_EFI_PNP_ID(0x0a03)
#define EFIDP_ACPI_CONTAINER_0A05_HID	EFIDP_EFI_PNP_ID(0x0a05)
#define EFIDP_ACPI_CONTAINER_0A06_HID	EFIDP_EFI_PNP_ID(0x0a06)
#define EFIDP_ACPI_PCIE_ROOT_HID	EFIDP_EFI_PNP_ID(0x0a08)
#define EFIDP_ACPI_EC_HID		EFIDP_EFI_PNP_ID(0x0a09)
#define EFIDP_ACPI_FLOPPY_HID		EFIDP_EFI_PNP_ID(0x0604)
#define EFIDP_ACPI_KEYBOARD_HID		EFIDP_EFI_PNP_ID(0x0301)
#define EFIDP_ACPI_SERIAL_HID		EFIDP_EFI_PNP_ID(0x0501)
#define EFIDP_ACPI_PARALLEL_HID		EFIDP_EFI_PNP_ID(0x0401)
#define EFIDP_ACPI_NVDIMM_HID		EFIDP_EFI_ACPI_ID(0x0012)

#define EFIDP_ACPI_ADR		0x03
typedef struct {
	efidp_header	header;
	uint32_t	adr[];
} EFIVAR_PACKED efidp_acpi_adr;

#define EFIDP_ACPI_ADR_DEVICE_ID_SCHEME_MASK		0x80000000
#define EFIDP_ACPI_ADR_DEVICE_ID_SCHEME_SHIFT		31
#define EFIDP_ACPI_ADR_DEVICE_ID_SCHEME_ACPI		0x1
#define EFIDP_ACPI_ADR_DEVICE_ID_SCHEME_OTHER		0x0

#define EFIDP_ACPI_ADR_VGA_PIPE_ID_MASK			0x001C0000
#define EFIDP_ACPI_ADR_VGA_PIPE_ID_SHIFT		18

#define EFIDP_ACPI_ADR_NONVGA_OUTPUT_ID_MASK		0x00020000
#define EFIDP_ACPI_ADR_NONVGA_OUTPUT_ID_SHIFT		17

#define EFIDP_ACPI_ADR_FIRMWARE_DETECT_MASK		0x00010000
#define EFIDP_ACPI_ADR_FIRMWARE_DETECT_SHIFT		16

#define EFIDP_ACPI_ADR_VENDOR_INFO_MASK			0x0000f000
#define EFIDP_ACPI_ADR_VENDOR_INFO_SHIFT		12

#define EFIDP_ACPI_ADR_DISPLAY_TYPE_MASK		0x00000f00
#define EFIDP_ACPI_ADR_DISPLAY_TYPE_SHIFT		8
#define EFIDP_ACPI_ADR_DISPLAY_TYPE_OTHER		0
#define EFIDP_ACPI_ADR_DISPLAY_TYPE_VGA			1
#define EFIDP_ACPI_ADR_DISPLAY_TYPE_TV			2
#define EFIDP_ACPI_ADR_DISPLAY_TYPE_EXTERNAL_DIGITAL	3
#define EFIDP_ACPI_ADR_DISPLAY_TYPE_INTERNAL_DIGITAL	4

#define EFIDP_ACPI_ADR_DISPLAY_PORT_MASK		0x000000f0
#define EFIDP_ACPI_ADR_DISPLAY_PORT_SHIFT		4

#define EFIDP_ACPI_ADR_DISPLAY_INDEX_MASK		0x0000000f
#define EFIDP_ACPI_ADR_DISPLAY_INDEX_SHIFT		0

#define efidp_encode_acpi_display_adr(device_id_scheme, pipe_id,	\
				      nonvga_output,			\
				      firmware_can_detect, vendor_info,	\
				      type, port, index, adr)		\
	({								\
	((uint32_t)(adr)) = ((uint32_t)(				\
	 efidp_encode_bitfield_(device_id_scheme,			\
				EFIDP_ACPI_ADR_DEVICE_ID_SCHEME_SHIFT,	\
				EFIDP_ACPI_ADR_DEVICE_ID_SCHEME_MASK) |	\
	 efidp_encode_bitfield_(pipe_id,				\
				EFIDP_ACPI_ADR_VGA_PIPE_ID_SHIFT,	\
				EFIDP_ACPI_ADR_VGA_PIPE_ID_MASK) |	\
	 efidp_encode_bitfield_(nonvga_output,				\
				EFIDP_ACPI_ADR_NONVGA_OUTPUT_ID_SHIFT,	\
				EFIDP_ACPI_ADR_NONVGA_OUTPUT_ID_MASK) |	\
	 efidp_encode_bitfield_(firmware_can_detect,			\
				EFIDP_ACPI_ADR_FIRMWARE_DETECT_SHIFT,	\
				EFIDP_ACPI_ADR_FIRMWARE_DETECT_MASK) |	\
	 efidp_encode_bitfield_(vendor_info,				\
				EFIDP_ACPI_ADR_VENDOR_INFO_SHIFT,	\
				EFIDP_ACPI_ADR_VENDOR_INFO_MASK) |	\
	 efidp_encode_bitfield_(type,					\
				EFIDP_ACPI_ADR_DISPLAY_TYPE_SHIFT,	\
				EFIDP_ACPI_ADR_DISPLAY_TYPE_MASK) |	\
	 efidp_encode_bitfield_(port,					\
				EFIDP_ACPI_ADR_DISPLAY_PORT_SHIFT,	\
				EFIDP_ACPI_ADR_DISPLAY_PORT_MASK) |	\
	 efidp_encode_bitfield_(index,					\
				EFIDP_ACPI_ADR_DISPLAY_INDEX_SHIFT,	\
				EFIDP_ACPI_ADR_DISPLAY_INDEX_MASK)));	\
	(device_id_scheme == EFIDP_ACPI_ADR_DEVICE_ID_SCHEME_ACPI);	\
	})

#define efidp_decode_acpi_display_adr(adr, device_id_scheme, pipe_id,	\
				      nonvga_output,			\
				      firmware_can_detect, vendor_info,	\
				      type, port, index) ({		\
	 efidp_decode_bitfield_(adr, *(device_id_scheme),		\
				EFIDP_ACPI_ADR_DEVICE_ID_SCHEME_SHIFT,	\
				EFIDP_ACPI_ADR_DEVICE_ID_SCHEME_MASK);	\
	 efidp_decode_bitfield_(adr, *(pipe_id),			\
				EFIDP_ACPI_ADR_VGA_PIPE_ID_SHIFT,	\
				EFIDP_ACPI_ADR_VGA_PIPE_ID_MASK);	\
	 efidp_decode_bitfield_(adr, *(nonvga_output),			\
				EFIDP_ACPI_ADR_NONVGA_OUTPUT_ID_SHIFT,	\
				EFIDP_ACPI_ADR_NONVGA_OUTPUT_ID_MASK);	\
	 efidp_decode_bitfield_(adr, *(firmware_can_detect),		\
				EFIDP_ACPI_ADR_FIRMWARE_DETECT_SHIFT,	\
				EFIDP_ACPI_ADR_FIRMWARE_DETECT_MASK);	\
	 efidp_decode_bitfield_(adr, *(vendor_info),			\
				EFIDP_ACPI_ADR_VENDOR_INFO_SHIFT,	\
				EFIDP_ACPI_ADR_VENDOR_INFO_MASK);	\
	 efidp_decode_bitfield_(adr, *(type),				\
				EFIDP_ACPI_ADR_DISPLAY_TYPE_SHIFT,	\
				EFIDP_ACPI_ADR_DISPLAY_TYPE_MASK);	\
	 efidp_decode_bitfield_(adr, *(port),				\
				EFIDP_ACPI_ADR_DISPLAY_PORT_SHIFT,	\
				EFIDP_ACPI_ADR_DISPLAY_PORT_MASK);	\
	 efidp_decode_bitfield_(adr, *(index),				\
				EFIDP_ACPI_ADR_DISPLAY_INDEX_SHIFT,	\
				EFIDP_ACPI_ADR_DISPLAY_INDEX_MASK);	\
	(*(device_id_scheme)) == EFIDP_ACPI_ADR_DEVICE_ID_SCHEME_ACPI;	\
	})

#define EFIDP_ACPI_ADR_NVDIMM_NODE_CONTROLLER_MASK	0x0fff0000
#define EFIDP_ACPI_ADR_NVDIMM_NODE_CONTROLLER_SHIFT	16

#define EFIDP_ACPI_ADR_NVDIMM_SOCKET_ID_MASK		0x0000f000
#define EFIDP_ACPI_ADR_NVDIMM_SOCKET_ID_SHIFT		12

#define EFIDP_ACPI_ADR_NVDIMM_MEMORY_CONTROLLER_MASK	0x00000f00
#define EFIDP_ACPI_ADR_NVDIMM_MEMORY_CONTROLLER_SHIFT	8

#define EFIDP_ACPI_ADR_NVDIMM_MEMORY_CHANNEL_MASK	0x000000f0
#define EFIDP_ACPI_ADR_NVDIMM_MEMORY_CHANNEL_SHIFT	4

#define EFIDP_ACPI_ADR_NVDIMM_DIMM_MASK			0x0f00000f
#define EFIDP_ACPI_ADR_NVDIMM_DIMM_SHIFT		0

#define efidp_encode_acpi_nvdimm_adr(node_controller,			\
				     socket, memory_controller,		\
				     memory_channel, dimm, adr)		\
	({								\
	((uint32_t)(adr)) = ((uint32_t)(				\
	 efidp_encode_bitfield_(node_controller,			\
				EFIDP_ACPI_ADR_NVDIMM_NODE_CONTROLLER_SHIFT,\
				EFIDP_ACPI_ADR_NVDIMM_NODE_CONTROLLER_MASK) | \
	 efidp_encode_bitfield_(socket,					\
				EFIDP_ACPI_ADR_NVDIMM_SOCKET_ID_SHIFT,	\
				EFIDP_ACPI_ADR_NVDIMM_SOCKET_ID_MASK) |	\
	 efidp_encode_bitfield_(memory_controller,			\
				EFIDP_ACPI_ADR_NVDIMM_MEMORY_CONTROLLER_SHIFT,\
				EFIDP_ACPI_ADR_NVDIMM_MEMORY_CONTROLLER_MASK)|\
	 efidp_encode_bitfield_(memory_channel,				\
				EFIDP_ACPI_ADR_NVDIMM_MEMORY_CHANNEL_SHIFT,\
				EFIDP_ACPI_ADR_NVDIMM_MEMORY_CHANNEL_MASK) | \
	 efidp_encode_bitfield_(dimm,					\
				EFIDP_ACPI_ADR_NVDIMM_DIMM_SHIFT,	\
				EFIDP_ACPI_ADR_NVDIMM_DIMM_MASK));	\
	})

#define efidp_decode_acpi_nvdimm_adr(adr, node_controller, socket,	\
				     memory_controller, memory_channel,	\
				     dimm)				\
	({								\
	 efidp_decode_bitfield_(adr, *(node_controller),		\
				EFIDP_ACPI_ADR_NVDIMM_NODE_CONTROLLER_SHIFT,\
				EFIDP_ACPI_ADR_NVDIMM_NODE_CONTROLLER_MASK);\
	 efidp_decode_bitfield_(adr, *(socket),				\
				EFIDP_ACPI_ADR_NVDIMM_SOCKET_ID_SHIFT,	\
				EFIDP_ACPI_ADR_NVDIMM_SOCKET_ID_MASK);	\
	 efidp_decode_bitfield_(adr, *(memory_controller),		\
				EFIDP_ACPI_ADR_NVDIMM_MEMORY_CONTROLLER_SHIFT,\
				EFIDP_ACPI_ADR_NVDIMM_MEMORY_CONTROLLER_MASK);\
	 efidp_decode_bitfield_(adr, *(memory_channel),			\
				EFIDP_ACPI_ADR_NVDIMM_MEMORY_CHANNEL_SHIFT,\
				EFIDP_ACPI_ADR_NVDIMM_MEMORY_CHANNEL_MASK);\
	 efidp_decode_bitfield_(adr, *(dimm),				\
				EFIDP_ACPI_ADR_NVDIMM_DIMM_SHIFT,	\
				EFIDP_ACPI_ADR_NVDIMM_DIMM_MASK);	\
	 0;								\
	})

/* Each messaging subtype */
#define EFIDP_MSG_ATAPI		0x01
typedef struct {
	efidp_header	header;
	uint8_t		primary;
	uint8_t		slave;
	uint16_t	lun;
} EFIVAR_PACKED efidp_atapi;
extern ssize_t efidp_make_atapi(uint8_t *buf, ssize_t size, uint16_t primary,
		uint16_t slave, uint16_t lun);

#define EFIDP_MSG_SCSI		0x02
typedef struct {
	efidp_header	header;
	uint16_t	target;
	uint16_t	lun;
} EFIVAR_PACKED efidp_scsi;
extern ssize_t efidp_make_scsi(uint8_t *buf, ssize_t size, uint16_t target,
			       uint16_t lun);

#define EFIDP_MSG_FIBRECHANNEL	0x03
typedef struct {
	efidp_header	header;
	uint32_t	reserved;
	uint64_t	wwn;
	uint64_t	lun;
} EFIVAR_PACKED efidp_fc;

#define EFIDP_MSG_FIBRECHANNELEX 0x15
typedef struct {
	efidp_header	header;
	uint32_t	reserved;
	uint8_t		wwn[8];
	uint8_t		lun[8];
} EFIVAR_PACKED efidp_fcex;

#define EFIDP_MSG_1394		0x04
typedef struct {
	efidp_header	header;
	uint32_t	reserved;
	uint64_t	guid;
} EFIVAR_PACKED efidp_1394;

#define EFIDP_MSG_USB		0x05
typedef struct {
	efidp_header	header;
	uint8_t		parent_port;
	uint8_t		interface;
} EFIVAR_PACKED efidp_usb;

#define EFIDP_MSG_USB_CLASS	0x0f
typedef struct {
	efidp_header	header;
	uint16_t	vendor_id;
	uint16_t	product_id;
	uint8_t		device_class;
	uint8_t		device_subclass;
	uint8_t		device_protocol;
} EFIVAR_PACKED efidp_usb_class;

#define EFIDP_USB_CLASS_AUDIO		0x01
#define EFIDP_USB_CLASS_CDC_CONTROL	0x02
#define EFIDP_USB_CLASS_HID		0x03
#define EFIDP_USB_CLASS_IMAGE		0x06
#define EFIDP_USB_CLASS_PRINTER		0x07
#define EFIDP_USB_CLASS_MASS_STORAGE	0x08
#define EFIDP_USB_CLASS_HUB		0x09
#define EFIDP_USB_CLASS_CDC_DATA	0x0a
#define EFIDP_USB_CLASS_SMARTCARD	0x0b
#define EFIDP_USB_CLASS_VIDEO		0x0e
#define EFIDP_USB_CLASS_DIAGNOSTIC	0xdc
#define EFIDP_USB_CLASS_WIRELESS	0xde
#define EFIDP_USB_CLASS_254		0xfe
#define EFIDP_USB_SUBCLASS_FW_UPDATE		0x01
#define EFIDP_USB_SUBCLASS_IRDA_BRIDGE		0x02
#define EFIDP_USB_SUBCLASS_TEST_AND_MEASURE	0x03

#define EFIDP_MSG_USB_WWID	0x10
typedef struct {
	efidp_header	header;
	uint16_t	interface;
	uint16_t	vendor_id;
	uint16_t	product_id;
	uint16_t	serial_number[];
} EFIVAR_PACKED efidp_usb_wwid;

#define EFIDP_MSG_LUN		0x11
typedef struct {
	efidp_header	header;
	uint8_t		lun;
} EFIVAR_PACKED efidp_lun;

#define EFIDP_MSG_SATA		0x12
typedef struct {
	efidp_header	header;
	uint16_t	hba_port;
	uint16_t	port_multiplier_port;
	uint16_t	lun;
} EFIVAR_PACKED efidp_sata;
#define SATA_HBA_DIRECT_CONNECT_FLAG	0x8000
extern ssize_t efidp_make_sata(uint8_t *buf, ssize_t size, uint16_t hba_port,
			       int16_t port_multiplier_port, uint16_t lun);

#define	EFIDP_MSG_I2O		0x06
typedef struct {
	efidp_header	header;
	uint32_t	target;
} EFIVAR_PACKED efidp_i2o;

#define EFIDP_MSG_MAC_ADDR	0x0b
typedef struct {
	efidp_header	header;
	uint8_t		mac_addr[32];
	uint8_t		if_type;
} EFIVAR_PACKED efidp_mac_addr;
extern ssize_t efidp_make_mac_addr(uint8_t *buf, ssize_t size,
				   uint8_t if_type,
				   const uint8_t * const mac_addr,
				   ssize_t mac_addr_size);

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
} EFIVAR_PACKED efidp_ipv4_addr;
/* everything here is in host byte order */
extern ssize_t efidp_make_ipv4(uint8_t *buf, ssize_t size,
			       uint32_t local, uint32_t remote,
			       uint32_t gateway, uint32_t netmask,
			       uint16_t local_port, uint16_t remote_port,
			       uint16_t protocol, int is_static);

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
} EFIVAR_PACKED efidp_ipv6_addr;

#define EFIDP_IPv6_ORIGIN_MANUAL	0x00
#define EFIDP_IPv6_ORIGIN_AUTOCONF	0x01
#define EFIDP_IPv6_ORIGIN_STATEFUL	0x02

#define EFIDP_MSG_VLAN		0x14
typedef struct {
	efidp_header	header;
	uint16_t	vlan_id;
} EFIVAR_PACKED efidp_vlan;

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
} EFIVAR_PACKED efidp_infiniband;

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
} EFIVAR_PACKED efidp_uart;

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
	uint8_t		vendor_data[];
} EFIVAR_PACKED efidp_msg_vendor;
typedef efidp_msg_vendor efidp_vendor_msg;
#define efidp_make_msg_vendor(buf, size, guid, data, data_size)		\
	efidp_make_vendor(buf, size, EFIDP_MESSAGE_TYPE,		\
			  EFIDP_MSG_VENDOR, guid, data, data_size)

/* The next ones are phrased as vendor specific, but they're in the spec. */
#define EFIDP_MSG_UART_GUID \
	EFI_GUID(0x37499a9d,0x542f,0x4c89,0xa026,0x35,0xda,0x14,0x20,0x94,0xe4)
typedef struct {
	efidp_header	header;
	efi_guid_t	vendor_guid;
	uint32_t	flow_control_map;
} EFIVAR_PACKED efidp_uart_flow_control;

#define EFIDP_UART_FLOW_CONTROL_HARDWARE	0x1
#define EFIDP_UART_FLOW_CONTROL_XONXOFF		0x2

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
} EFIVAR_PACKED efidp_sas;
extern ssize_t efidp_make_sas(uint8_t *buf, ssize_t size, uint64_t sas_address);

/* device_topology_info Bits 0:3 (enum) */
#define EFIDP_SAS_TOPOLOGY_MASK		0x02
#define EFIDP_SAS_TOPOLOGY_NONE		0x0
#define EFIDP_SAS_TOPOLOGY_THISBYTE	0x1
#define EFIDP_SAS_TOPOLOGY_NEXTBYTE	0x2

/* device_topology_info Bits 4:5 (enum) */
#define EFIDP_SAS_DEVICE_MASK		0x30
#define EFIDP_SAS_DEVICE_SHIFT		4
#define EFIDP_SAS_DEVICE_SAS_INTERNAL	0x0
#define EFIDP_SAS_DEVICE_SATA_INTERNAL	0x1
#define EFIDP_SAS_DEVICE_SAS_EXTERNAL	0x2
#define EFIDP_SAS_DEVICE_SATA_EXTERNAL	0x3

/* device_topology_info Bits 6:7 (enum) */
#define EFIDP_SAS_CONNECT_MASK		0x40
#define EFIDP_SAS_CONNECT_SHIFT		6
#define EFIDP_SAS_CONNECT_DIRECT	0x0
#define EFIDP_SAS_CONNECT_EXPANDER	0x1

#define EFIDP_MSG_SAS_EX	0x16
typedef struct {
	efidp_header	header;
	uint8_t		sas_address[8];
	uint8_t		lun[8];
	uint8_t		device_topology_info;
	uint8_t		drive_bay_id; /* If EFIDP_SAS_TOPOLOGY_NEXTBYTE set */
	uint16_t	rtp;
} EFIVAR_PACKED efidp_sas_ex;

#define EFIDP_MSG_DEBUGPORT_GUID \
	EFI_GUID(0xEBA4E8D2,0x3858,0x41EC,0xA281,0x26,0x47,0xBA,0x96,0x60,0xD0)

#define EFIDP_MSG_ISCSI		0x13
typedef struct {
	efidp_header	header;
	uint16_t	protocol;
	uint16_t	options;
	uint8_t		lun[8];
	uint16_t	tpgt;
	uint8_t		target_name[];
} EFIVAR_PACKED efidp_iscsi;

/* options bits 0:1 */
#define EFIDP_ISCSI_HEADER_DIGEST_SHIFT	0
#define EFIDP_ISCSI_NO_HEADER_DIGEST	0x0
#define EFIDP_ISCSI_HEADER_CRC32	0x2

/* option bits 2:3 */
#define EFIDP_ISCSI_DATA_DIGEST_SHIFT	2
#define EFIDP_ISCSI_NO_DATA_DIGEST	0x0
#define EFIDP_ISCSI_DATA_CRC32		0x2

/* option bits 4:9 */
#define EFIDP_ISCSI_RESERVED		0x0

/* option bits 10:11 */
#define EFIDP_ISCSI_AUTH_SHIFT		10
#define EFIDP_ISCSI_AUTH_CHAP		0x0
#define EFIDP_ISCSI_AUTH_NONE		0x2

/* option bit 12 */
#define EFIDP_ISCSI_CHAP_SHIFT		12
#define EFIDP_ISCSI_CHAP_BI		0x0
#define EFIDP_ISCSI_CHAP_UNI		0x1

#define EFIDP_ISCSI_MAX_TARGET_NAME_LEN		223

#define EFIDP_MSG_NVME		0x17
typedef struct {
	efidp_header	header;
	uint32_t	namespace_id;
	uint8_t		ieee_eui_64[8];
} EFIVAR_PACKED efidp_nvme;
extern ssize_t efidp_make_nvme(uint8_t *buf, ssize_t size,
			       uint32_t namespace_id, uint8_t *ieee_eui_64);

#define EFIDP_MSG_URI		0x18
typedef struct {
	efidp_header	header;
	uint8_t		uri[];
} EFIVAR_PACKED efidp_uri;

#define EFIDP_MSG_UFS		0x19
typedef struct {
	efidp_header	header;
	uint8_t		target_id;
	uint8_t		lun;
} EFIVAR_PACKED efidp_ufs;

#define EFIDP_MSG_SD		0x1a
typedef struct {
	efidp_header	header;
	uint8_t		slot_number;
} EFIVAR_PACKED efidp_sd;

#define EFIDP_MSG_BT		0x1b
typedef struct {
	efidp_header	header;
	uint8_t		addr[6];
} EFIVAR_PACKED efidp_bt;

#define EFIDP_MSG_WIFI		0x1c
typedef struct {
	efidp_header	header;
	uint8_t		ssid[32];
} EFIVAR_PACKED efidp_wifi;

#define EFIDP_MSG_EMMC		0x1d
typedef struct {
	efidp_header	header;
	uint8_t		slot;
} EFIVAR_PACKED efidp_emmc;

extern ssize_t efidp_make_emmc(uint8_t *buf, ssize_t size, uint32_t slot_id);

#define EFIDP_MSG_BTLE		0x1e
typedef struct {
	efidp_header	header;
	uint8_t		addr[6];
	uint8_t		addr_type;
} EFIVAR_PACKED efidp_btle;

#define EFIDP_MSG_BTLE_ADDR_TYPE_PUBLIC	0
#define EFIDP_MSG_BTLE_ADDR_TYPE_RANDOM	1

#define EFIDP_MSG_DNS		0x1f
typedef struct {
	efidp_header	header;
	uint8_t		is_ipv6;
	efi_ip_addr_t	addrs[];
} EFIVAR_PACKED efidp_dns;

#define EFIDP_MSG_NVDIMM	0x20
typedef struct {
	efidp_header	header;
	efi_guid_t	uuid;
} EFIVAR_PACKED efidp_nvdimm;
extern ssize_t efidp_make_nvdimm(uint8_t *buf, ssize_t size, efi_guid_t *uuid);

/* Each media subtype */
#define EFIDP_MEDIA_HD		0x1
typedef struct {
	efidp_header	header;
	uint32_t	partition_number;
	uint64_t	start;
	uint64_t	size;
	uint8_t		signature[16];
	uint8_t		format;
	uint8_t		signature_type;
#ifdef __ia64
	uint8_t		padding[6]; /* Emperically needed */
#endif
} EFIVAR_PACKED EFIVAR_PACKED efidp_hd;
extern ssize_t efidp_make_hd(uint8_t *buf, ssize_t size, uint32_t num,
			     uint64_t part_start, uint64_t part_size,
			     uint8_t *signature, uint8_t format,
			     uint8_t signature_type);

#define EFIDP_HD_FORMAT_PCAT	0x01
#define EFIDP_HD_FORMAT_GPT	0x02

#define EFIDP_HD_SIGNATURE_NONE		0x00
#define EFIDP_HD_SIGNATURE_MBR		0x01
#define EFIDP_HD_SIGNATURE_GUID		0x02

#define EFIDP_MEDIA_CDROM	0x2
typedef struct {
	efidp_header	header;
	uint32_t	boot_catalog_entry;
	uint64_t	partition_rba;
	uint64_t	sectors;
} EFIVAR_PACKED efidp_cdrom;

#define EFIDP_MEDIA_VENDOR	0x3
typedef struct {
	efidp_header	header;
	efi_guid_t	vendor_guid;
	uint8_t		vendor_data[];
} EFIVAR_PACKED efidp_media_vendor;
typedef efidp_media_vendor efidp_vendor_media;
#define efidp_make_media_vendor(buf, size, guid, data, data_size)	\
	efidp_make_vendor(buf, size, EFIDP_MEDIA_TYPE,			\
			  EFIDP_MEDIA_VENDOR, guid, data, data_size)

#define EFIDP_MEDIA_FILE	0x4
typedef struct {
	efidp_header	header;
	uint16_t	name[];
} EFIVAR_PACKED efidp_file;
extern ssize_t efidp_make_file(uint8_t *buf, ssize_t size, char *filename);

#define EFIDP_MEDIA_PROTOCOL	0x5
typedef struct {
	efidp_header	header;
	efi_guid_t	protocol_guid;
} EFIVAR_PACKED efidp_protocol;

#define EFIDP_MEDIA_FIRMWARE_FILE	0x6
typedef struct {
	efidp_header	header;
	uint8_t		pi_info[];
} EFIVAR_PACKED efidp_firmware_file;

#define EFIDP_MEDIA_FIRMWARE_VOLUME	0x7
typedef struct {
	efidp_header	header;
	uint8_t		pi_info[];
} EFIVAR_PACKED efidp_firmware_volume;

#define EFIDP_MEDIA_RELATIVE_OFFSET	0x8
typedef struct {
	efidp_header	header;
	uint32_t	reserved;
	uint64_t	first_byte;
	uint64_t	last_byte;
} EFIVAR_PACKED efidp_relative_offset;

#define EFIDP_MEDIA_RAMDISK	0x9
typedef struct {
	efidp_header	header;
	uint64_t	start_addr;
	uint64_t	end_addr;
	efi_guid_t	disk_type_guid;
	uint16_t	instance_number;
} EFIVAR_PACKED efidp_ramdisk;

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
	uint8_t		description[];
} EFIVAR_PACKED efidp_bios_boot;

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
	efidp_acpi_hid_ex acpi_hid_ex;
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
	efidp_bt bt;
	efidp_wifi wifi;
	efidp_emmc emmc;
	efidp_btle btle;
	efidp_dns dns;
	efidp_nvdimm nvdimm;
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
} EFIVAR_PACKED efidp_data;
typedef efidp_data *efidp;
typedef const efidp_data *const_efidp;

extern int efidp_set_node_data(const_efidp dn, void *buf, size_t bufsize);
extern int efidp_duplicate_path(const_efidp dp, efidp *out);
extern int efidp_append_path(const_efidp dp0, const_efidp dp1, efidp *out);
extern int efidp_append_node(const_efidp dp, const_efidp dn, efidp *out);
extern int efidp_append_instance(const_efidp dp, const_efidp dpi, efidp *out);

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpointer-bool-conversion"
#endif

#if defined(__GNUC__) && !defined(__clang__)
#define EFIVAR_ARTIFICIAL __attribute__((__artificial__))
#else
#define EFIVAR_ARTIFICIAL
#endif
#define EFIVAR_UNUSED __attribute__((__unused__))
#define EFIVAR_WARN_UNCHECKED __attribute__((__warn_unused_result__))

static inline int16_t
EFIVAR_ARTIFICIAL EFIVAR_UNUSED
efidp_type(const_efidp dp)
{
	if (!dp) {
		errno = EINVAL;
		return -1;
	}
	return (uint8_t)dp->type;
}

static inline int16_t
EFIVAR_ARTIFICIAL EFIVAR_UNUSED
efidp_subtype(const_efidp dp)
{
	if (!dp) {
		errno = EINVAL;
		return -1;
	}
	return (uint8_t)dp->subtype;
}

static inline ssize_t
EFIVAR_ARTIFICIAL EFIVAR_UNUSED EFIVAR_WARN_UNCHECKED
efidp_node_size(const_efidp dn)
{
	if (!dn || dn->length < 4) {
		errno = EINVAL;
		return -1;
	}
	return dn->length;
}

static inline int
EFIVAR_ARTIFICIAL EFIVAR_UNUSED EFIVAR_WARN_UNCHECKED
efidp_next_node(const_efidp in, const_efidp *out)
{
	ssize_t sz;

	if (efidp_type(in) == EFIDP_END_TYPE &&
	    efidp_subtype(in) == EFIDP_END_ENTIRE)
		return 0;

	sz = efidp_node_size(in);
	if (sz < 0)
		return -1;

	/* I love you gcc. */
	*out = (const_efidp)(const efidp_header *)((const uint8_t *)in + sz);
	if (*out < in) {
		errno = EINVAL;
		return -1;
	}
	return 1;
}

static inline int
EFIVAR_ARTIFICIAL EFIVAR_UNUSED EFIVAR_WARN_UNCHECKED
efidp_next_instance(const_efidp in, const_efidp *out)
{
	ssize_t sz;

	if (efidp_type(in) != EFIDP_END_TYPE ||
			efidp_subtype(in) != EFIDP_END_INSTANCE) {
		errno = EINVAL;
		return -1;
	}

	sz = efidp_node_size(in);
	if (sz < 0)
		return -1;

	/* I love you gcc. */
	*out = (const_efidp)(const efidp_header *)((uint8_t *)in + sz);
	if (*out < in) {
		errno = EINVAL;
		return -1;
	}
	return 1;
}

static inline int
EFIVAR_ARTIFICIAL EFIVAR_UNUSED EFIVAR_WARN_UNCHECKED
efidp_is_multiinstance(const_efidp dn)
{
	while (1) {
		const_efidp next = NULL;
		int rc;

		rc = efidp_next_node(dn, &next);
		if (rc < 0) {
			errno = EINVAL;
			return -1;
		} else if (rc == 0) {
			return 0;
		}

		dn = next;
		if (efidp_type(dn) == EFIDP_END_TYPE &&
		    efidp_subtype(dn) == EFIDP_END_INSTANCE)
			return 1;
		if (efidp_type(dn) == EFIDP_END_TYPE &&
		    efidp_subtype(dn) == EFIDP_END_ENTIRE)
			return 0;
	}

	return 0;
}

static inline int
EFIVAR_ARTIFICIAL EFIVAR_UNUSED EFIVAR_WARN_UNCHECKED
efidp_get_next_end(const_efidp in, const_efidp *out)
{
	while (1) {
		const_efidp next;
		ssize_t sz;

		if (efidp_type(in) == EFIDP_END_TYPE) {
			*out = in;
			return 0;
		}

		sz = efidp_node_size(in);
		if (sz < 0)
			break;

		next = (const_efidp)(const efidp_header *)((uint8_t *)in + sz);
		if (next < in) {
			errno = EINVAL;
			return -1;
		}
		in = next;
	}
	return -1;
}

static inline ssize_t
EFIVAR_ARTIFICIAL EFIVAR_UNUSED EFIVAR_WARN_UNCHECKED
efidp_size(const_efidp dp)
{
	ssize_t ret = 0;
	if (!dp) {
		errno = EINVAL;
		return -1;
	}

	if (efidp_type(dp) == EFIDP_END_TYPE &&
	    efidp_subtype(dp) == EFIDP_END_ENTIRE)
		return efidp_node_size(dp);

	while (1) {
		int rc;
		ssize_t sz;
		const_efidp next = NULL;

		sz = efidp_node_size(dp);
		if (sz < 0)
			return sz;
		ret += sz;

		rc = efidp_next_instance(dp, &next);
		if (rc < 0) {
			rc = efidp_next_node(dp, &next);
			if (rc == 0)
				break;
		}
		if (rc < 0)
			return rc;

		dp = next;
	}
	return ret;
}

static inline ssize_t
EFIVAR_ARTIFICIAL EFIVAR_UNUSED EFIVAR_WARN_UNCHECKED
efidp_instance_size(const_efidp dpi)
{
	ssize_t ret = 0;
	while (1) {
		ssize_t sz;
		const_efidp next = NULL;
		int rc;

		sz = efidp_node_size(dpi);
		if (sz < 0)
			return sz;
		ret += sz;

		if (efidp_type(dpi) == EFIDP_END_TYPE)
			break;

		rc = efidp_next_node(dpi, &next);
		if (rc < 0)
			return rc;
		dpi = next;
	}
	return ret;
}

static inline int
EFIVAR_ARTIFICIAL EFIVAR_UNUSED
efidp_is_valid(const_efidp dp, ssize_t limit)
{
	efidp_header *hdr = (efidp_header *)dp;
	/* just to make it so I'm not checking for negatives everywhere,
	 * limit this at a truly absurdly large size. */
	if (limit < 0)
		limit = INT_MAX;

	while (limit > 0 && hdr) {
		efidp_header *next;
		if (limit < (int64_t)(sizeof (efidp_header)))
			return 0;

		switch (hdr->type) {
		case EFIDP_HARDWARE_TYPE:
			if (hdr->subtype != EFIDP_HW_VENDOR &&
			    hdr->length > 1024) {
				errno = EINVAL;
				efi_error("invalid hardware node");
				return 0;
			}
			break;
		case EFIDP_ACPI_TYPE:
			if (hdr->length > 1024) {
				errno = EINVAL;
				efi_error("invalid ACPI node");
				return 0;
			}
			break;
		case EFIDP_MESSAGE_TYPE:
			if (hdr->subtype != EFIDP_MSG_VENDOR &&
			    hdr->length > 1024) {
				errno = EINVAL;
				efi_error("invalid message node");
				return 0;
			}
			break;
		case EFIDP_MEDIA_TYPE:
			if (hdr->subtype != EFIDP_MEDIA_VENDOR &&
			    hdr->length > 1024) {
				errno = EINVAL;
				efi_error("invalid media node");
				return 0;
			}
			break;
		case EFIDP_BIOS_BOOT_TYPE:
			break;
		case EFIDP_END_TYPE:
			if (hdr->length > 4) {
				errno = EINVAL;
				efi_error("invalid end node");
				return 0;
			}
			break;
		default:
			errno = EINVAL;
			efi_error("invalid device path node type");
			return 0;
		}

		if (limit < hdr->length) {
			errno = EINVAL;
			efi_error("device path node length overruns buffer");
			return 0;
		}
		limit -= hdr->length;

		if (hdr->type != EFIDP_END_TYPE &&
		    hdr->type != EFIDP_END_ENTIRE)
			break;

		next = (efidp_header *)((uint8_t *)hdr + hdr->length);
		if (next < hdr) {
			errno = EINVAL;
			return -1;
		}
		hdr = next;
	}
	if (limit < 0) {
		errno = EINVAL;
		efi_error("device path node length overruns buffer");
		return 0;
	}
	return 1;
}

/* and now, printing and parsing */
extern ssize_t efidp_parse_device_node(unsigned char *path,
				       efidp out, size_t size);
extern ssize_t efidp_parse_device_path(unsigned char *path,
				       efidp out, size_t size);
extern ssize_t efidp_format_device_path(unsigned char *buf, size_t size,
					const_efidp dp, ssize_t limit);
extern ssize_t efidp_make_vendor(uint8_t *buf, ssize_t size, uint8_t type,
				 uint8_t subtype,  efi_guid_t vendor_guid,
				 void *data, size_t data_size);
extern ssize_t efidp_make_generic(uint8_t *buf, ssize_t size, uint8_t type,
				  uint8_t subtype, ssize_t total_size);
#define efidp_make_end_entire(buf, size)				\
	efidp_make_generic(buf, size, EFIDP_END_TYPE, EFIDP_END_ENTIRE,	\
			   sizeof (efidp_header));
#define efidp_make_end_instance(buf, size)				\
	efidp_make_generic(buf, size, EFIDP_END_TYPE,			\
			   EFIDP_END_INSTANCE, sizeof (efidp_header));

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _EFIVAR_DP_H */

// vim:fenc=utf-8:tw=75:noet
