// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012-2015 Red Hat, Inc.
 */

#include "fix_coverity.h"

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <stddef.h>

#include "efivar.h"

static ssize_t
format_ipv4_addr_helper(unsigned char *buf, size_t size, const char *dp_type,
			const uint8_t *ipaddr, int32_t port)
{
	ssize_t off = 0;
	format(buf, size, off, dp_type, "%hhu.%hhu.%hhu.%hhu",
	       ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3]);
	if (port > 0)
		format(buf, size, off, dp_type, ":%hu", (uint16_t)port);
	return off;
}

static ssize_t
format_ipv6_addr_helper(unsigned char *buf, size_t size, const char *dp_type,
			const uint8_t *ipaddr, int32_t port)
{
	uint16_t *ip = (uint16_t *)ipaddr;
	ssize_t off = 0;

	format(buf, size, off, dp_type, "[");

	// deciding how to print an ipv6 ip requires 2 passes, because
	// RFC5952 says we have to use :: a) only once and b) to maximum effect.
	int largest_zero_block_size = 0;
	int largest_zero_block_offset = -1;

	int this_zero_block_size = 0;
	int this_zero_block_offset = -1;

	int in_zero_block = 0;

	int i;
	for (i = 0; i < 8; i++) {
		if (ip[i] != 0 && in_zero_block) {
			if (this_zero_block_size > largest_zero_block_size) {
				largest_zero_block_size = this_zero_block_size;
				largest_zero_block_offset =
							this_zero_block_offset;
				this_zero_block_size = 0;
				this_zero_block_offset = -1;
				in_zero_block = 0;
			}
		}
		if (ip[i] == 0) {
			if (in_zero_block == 0) {
				in_zero_block = 1;
				this_zero_block_offset = i;
			}
			this_zero_block_size++;
		}
	}
	if (this_zero_block_size > largest_zero_block_size) {
		largest_zero_block_size = this_zero_block_size;
		largest_zero_block_offset = this_zero_block_offset;
		/*
		 * clang-analyzer hates these because they're the last use,
		 * and they don't believe in writing code so that bugs won't
		 * be introduced later...
		 */
#if 0
		this_zero_block_size = 0;
		this_zero_block_offset = -1;
		in_zero_block = 0;
#endif
	}
	if (largest_zero_block_size == 1)
		largest_zero_block_offset = -1;

	for (i = 0; i < 8; i++) {
		if (largest_zero_block_offset == i) {
			format(buf, size, off, dp_type, "::");
			i += largest_zero_block_size -1;
			continue;
		} else if (i > 0) {
			format(buf, size, off, dp_type, ":");
		}

		format(buf, size, off, dp_type, "%x", ip[i]);
	}

	format(buf, size, off, dp_type, "]");
	if (port > 0)
		format(buf, size, off, dp_type, ":%hu", (uint16_t)port);

	return off;
}

#define format_ip_protocol(buf, size, off, dp_type, proto)	\
({								\
	switch(proto) {						\
	case 6:							\
		format(buf, size, off, dp_type, "TCP");		\
		break;						\
	case 17:						\
		format(buf, size, off, dp_type, "UDP");		\
		break;						\
	default:						\
		format(buf, size, off, dp_type, "%u", proto);	\
		break;						\
	}							\
})

#define format_ipv4_addr(buf, size, off, addr, port)		\
	format_helper(format_ipv4_addr_helper, buf, size, off,	\
		      "IPv4", addr, port)

#define format_ipv6_addr(buf, size, off, addr, port)		\
	format_helper(format_ipv6_addr_helper, buf, size, off,	\
		      "IPv6", addr, port)

static ssize_t
format_ip_addr_helper(unsigned char *buf, size_t size,
		      const char *dp_type UNUSED,
		      int is_ipv6, const efi_ip_addr_t *addr)
{
	ssize_t off = 0;
	if (is_ipv6)
		format_helper(format_ipv6_addr_helper, buf, size, off, "IPv6",
			      (const uint8_t *)&addr->v6, -1);
	else
		format_helper(format_ipv4_addr_helper, buf, size, off, "IPv4",
			      (const uint8_t *)&addr->v4, -1);
	return off;
}

#define format_ip_addr(buf, size, off, dp_type, is_ipv6, addr)		\
	format_helper(format_ip_addr_helper, buf, size, off,		\
		      dp_type, is_ipv6, addr)

static ssize_t
format_uart(unsigned char *buf, size_t size,
	    const char *dp_type UNUSED,
	    const_efidp dp)
{
	uint32_t value;
	ssize_t off = 0;
	char *labels[] = {"None", "Hardware", "XonXoff", ""};

	value = dp->uart_flow_control.flow_control_map;
	if (value > 2) {
		format(buf, size, off, "UartFlowControl",
			    "UartFlowControl(%d)", value);
		return off;
	}
	format(buf, size, off, "UartFlowControl", "UartFlowControl(%s)",
	       labels[value]);
	return off;
}

static ssize_t
format_sas(unsigned char *buf, size_t size,
	   const char *dp_type UNUSED,
	   const_efidp dp)
{
	ssize_t off = 0;
	const efidp_sas * const s = &dp->sas;

	int more_info = 0;
	int sassata = 0;
	int location = 0;
	int connect = 0;
	int drive_bay = -1;

	const char * const sassata_label[] = {"NoTopology", "SAS", "SATA"};
	const char * const location_label[] = {"Internal", "External" };
	const char * const connect_label[] = {"Direct", "Expanded" };

	more_info = s->device_topology_info & EFIDP_SAS_TOPOLOGY_MASK;

	if (more_info) {
		sassata = (s->device_topology_info & EFIDP_SAS_DEVICE_MASK)
			  >> EFIDP_SAS_DEVICE_SHIFT;
		if (sassata == EFIDP_SAS_DEVICE_SATA_EXTERNAL
				|| sassata == EFIDP_SAS_DEVICE_SAS_EXTERNAL)
			location = 1;

		if (sassata == EFIDP_SAS_DEVICE_SAS_INTERNAL
				|| sassata == EFIDP_SAS_DEVICE_SATA_INTERNAL)
			sassata = 1;
		else
			sassata = 2;

		connect = (s->device_topology_info & EFIDP_SAS_CONNECT_MASK)
			   >> EFIDP_SAS_CONNECT_SHIFT;
		if (more_info == EFIDP_SAS_TOPOLOGY_NEXTBYTE)
			drive_bay = s->drive_bay_id + 1;
	}

	format(buf, size, off, "SAS", "SAS(%"PRIx64",%"PRIx64",%"PRIx16",%s",
	       dp->subtype == EFIDP_MSG_SAS_EX ?
			be64_to_cpu(s->sas_address) :
			le64_to_cpu(s->sas_address),
		dp->subtype == EFIDP_MSG_SAS_EX ?
			be64_to_cpu(s->lun) :
			le64_to_cpu(s->lun),
		s->rtp, sassata_label[sassata]);

	if (more_info) {
		format(buf, size, off, "SAS", ",%s,%s",
		       location_label[location], connect_label[connect]);
	}

	if (more_info == 2 && drive_bay >= 0) {
		format(buf, size, off, "SAS", ",%d", drive_bay);
	}

	format(buf, size, off, "SAS", ")");
	return off;
}

#define class_helper(buf, size, off, label, dp)			\
	format(buf, size, off, label,				\
	       "%s(0x%"PRIx16",0x%"PRIx16",%d,%d)",		\
	       label,						\
	       dp->usb_class.vendor_id,				\
	       dp->usb_class.product_id,			\
	       dp->usb_class.device_subclass,			\
	       dp->usb_class.device_protocol)

static ssize_t
format_usb_class(unsigned char *buf, size_t size,
		 const char *dp_type UNUSED,
		 const_efidp dp)
{
	ssize_t off = 0;
	switch (dp->usb_class.device_class) {
	case EFIDP_USB_CLASS_AUDIO:
		class_helper(buf, size, off, "UsbAudio", dp);
		break;
	case EFIDP_USB_CLASS_CDC_CONTROL:
		class_helper(buf, size, off, "UsbCDCControl", dp);
		break;
	case EFIDP_USB_CLASS_HID:
		class_helper(buf, size, off, "UsbHID", dp);
		break;
	case EFIDP_USB_CLASS_IMAGE:
		class_helper(buf, size, off, "UsbImage", dp);
		break;
	case EFIDP_USB_CLASS_PRINTER:
		class_helper(buf, size, off, "UsbPrinter", dp);
		break;
	case EFIDP_USB_CLASS_MASS_STORAGE:
		class_helper(buf, size, off, "UsbMassStorage", dp);
		break;
	case EFIDP_USB_CLASS_HUB:
		class_helper(buf, size, off, "UsbHub", dp);
		break;
	case EFIDP_USB_CLASS_CDC_DATA:
		class_helper(buf, size, off, "UsbCDCData", dp);
		break;
	case EFIDP_USB_CLASS_SMARTCARD:
		class_helper(buf, size, off, "UsbSmartCard", dp);
		break;
	case EFIDP_USB_CLASS_VIDEO:
		class_helper(buf, size, off, "UsbVideo", dp);
		break;
	case EFIDP_USB_CLASS_DIAGNOSTIC:
		class_helper(buf, size, off, "UsbDiagnostic", dp);
		break;
	case EFIDP_USB_CLASS_WIRELESS:
		class_helper(buf, size, off, "UsbWireless", dp);
		break;
	case EFIDP_USB_CLASS_254:
		switch (dp->usb_class.device_subclass) {
		case EFIDP_USB_SUBCLASS_FW_UPDATE:
			format(buf, size, off, "UsbDeviceFirmwareUpdate",
			  "UsbDeviceFirmwareUpdate(0x%"PRIx16",0x%"PRIx16",%d)",
			  dp->usb_class.vendor_id,
			  dp->usb_class.product_id,
			  dp->usb_class.device_protocol);
			break;
		case EFIDP_USB_SUBCLASS_IRDA_BRIDGE:
			format(buf, size, off, "UsbIrdaBridge",
			       "UsbIrdaBridge(0x%"PRIx16",0x%"PRIx16",%d)",
			       dp->usb_class.vendor_id,
			       dp->usb_class.product_id,
			       dp->usb_class.device_protocol);
			break;
		case EFIDP_USB_SUBCLASS_TEST_AND_MEASURE:
			format(buf, size, off, "UsbTestAndMeasurement",
			  "UsbTestAndMeasurement(0x%"PRIx16",0x%"PRIx16",%d)",
			  dp->usb_class.vendor_id,
			  dp->usb_class.product_id,
			  dp->usb_class.device_protocol);
			break;
		}
		break;
	default:
		format(buf, size, off, "UsbClass",
		       "UsbClass(%"PRIx16",%"PRIx16",%d,%d)",
		       dp->usb_class.vendor_id,
		       dp->usb_class.product_id,
		       dp->usb_class.device_subclass,
		       dp->usb_class.device_protocol);
		break;
	}
	return off;
}

ssize_t
_format_message_dn(unsigned char *buf, size_t size, const_efidp dp)
{
	ssize_t off = 0;
	switch (dp->subtype) {
	case EFIDP_MSG_ATAPI:
		format(buf, size, off, "Ata", "Ata(%d,%d,%d)",
			      dp->atapi.primary, dp->atapi.slave,
			      dp->atapi.lun);
		break;
	case EFIDP_MSG_SCSI:
		format(buf, size, off, "SCSI", "SCSI(%d,%d)",
			      dp->scsi.target, dp->scsi.lun);
		break;
	case EFIDP_MSG_FIBRECHANNEL:
		format(buf, size, off, "Fibre", "Fibre(%"PRIx64",%"PRIx64")",
			      le64_to_cpu(dp->fc.wwn),
			      le64_to_cpu(dp->fc.lun));
		break;
	case EFIDP_MSG_FIBRECHANNELEX:
		format(buf, size, off, "Fibre", "Fibre(%"PRIx64",%"PRIx64")",
			      be64_to_cpu(dp->fc.wwn),
			      be64_to_cpu(dp->fc.lun));
		break;
	case EFIDP_MSG_1394:
		format(buf, size, off, "I1394", "I1394(0x%"PRIx64")",
			      dp->firewire.guid);
		break;
	case EFIDP_MSG_USB:
		format(buf, size, off, "USB", "USB(%d,%d)",
			      dp->usb.parent_port, dp->usb.interface);
		break;
	case EFIDP_MSG_I2O:
		format(buf, size, off, "I2O", "I2O(%d)", dp->i2o.target);
		break;
	case EFIDP_MSG_INFINIBAND:
		format(buf, size, off, "Infiniband",
		       "Infiniband(%08x,%"PRIx64"%"PRIx64",%"PRIx64",%"PRIu64",%"PRIu64")",
		       dp->infiniband.resource_flags,
		       dp->infiniband.port_gid[1],
		       dp->infiniband.port_gid[0],
		       dp->infiniband.ioc_guid,
		       dp->infiniband.target_port_id,
		       dp->infiniband.device_id);
		break;
	case EFIDP_MSG_MAC_ADDR:
		format(buf, size, off, "MAC", "MAC(");
		format_hex(buf, size, off, "MAC", dp->mac_addr.mac_addr,
				  dp->mac_addr.if_type < 2 ? 6
					: sizeof(dp->mac_addr.mac_addr));
		format(buf, size, off, "MAC", ",%d)", dp->mac_addr.if_type);
		break;
	case EFIDP_MSG_IPv4: {
		efidp_ipv4_addr const *a = &dp->ipv4_addr;
		format(buf, size, off, "IPv4", "IPv4(");
		format_ipv4_addr(buf, size, off,
				 a->remote_ipv4_addr, a->remote_port);
		format(buf, size, off, "IPv4", ",");
		format_ip_protocol(buf, size, off, "IPv4", a->protocol);
		format(buf, size, off, "IPv4", ",%s,", a->static_ip_addr
						      ?"Static" :"DHCP");
		format_ipv4_addr(buf, size, off,
				 a->local_ipv4_addr, a->local_port);
		format(buf, size, off, "IPv4", ",");
		format_ipv4_addr(buf, size, off, a->gateway, 0);
		format(buf, size, off, "IPv4", ",");
		format_ipv4_addr(buf, size, off, a->netmask, 0);
		format(buf, size, off, "IPv4", ")");
		break;
			     }
	case EFIDP_MSG_VENDOR: {
		struct {
			efi_guid_t guid;
			char label[40];
			ssize_t (*formatter)(unsigned char *buf, size_t size,
				const char *dp_type UNUSED,
				const_efidp dp);
		} subtypes[] = {
			{ .guid = EFIDP_PC_ANSI_GUID,
			  .label = "VenPcAnsi" },
			{ .guid = EFIDP_VT_100_GUID,
			  .label = "VenVt100" },
			{ .guid = EFIDP_VT_100_PLUS_GUID,
			  .label = "VenVt100Plus" },
			{ .guid = EFIDP_VT_UTF8_GUID,
			  .label = "VenUtf8" },
			{ .guid = EFIDP_MSG_DEBUGPORT_GUID,
			  .label = "DebugPort" },
			{ .guid = EFIDP_MSG_UART_GUID,
			  .label = "",
			  .formatter = format_uart },
			{ .guid = EFIDP_MSG_SAS_GUID,
			  .label = "",
			  .formatter = format_sas },
			{ .guid = efi_guid_empty,
			  .label = "" }
		};
		char *label = NULL;
		ssize_t (*formatter)(unsigned char *buf, size_t size,
			const char *dp_type UNUSED,
			const_efidp dp) = NULL;

		for (int i = 0; !efi_guid_is_zero(&subtypes[i].guid); i++) {
			if (efi_guid_cmp(&subtypes[i].guid,
					  &dp->msg_vendor.vendor_guid))
				continue;

			if (subtypes[i].label[0])
				label = subtypes[i].label;
			formatter = subtypes[i].formatter;
			break;
		}

		if (!label && !formatter) {
			format_vendor(buf, size, off, "VenMsg", dp);
			break;
		} else if (!label && formatter) {
			format_helper(formatter, buf, size, off, "VenMsg", dp);
			break;
		}

		format(buf, size, off, label, "%s(", label);
		if (efidp_node_size(dp) >
				(ssize_t)(sizeof (efidp_header)
					  + sizeof (efi_guid_t))) {
			format_hex(buf, size, off, label,
					  dp->msg_vendor.vendor_data,
					  efidp_node_size(dp)
						- sizeof (efidp_header)
						- sizeof (efi_guid_t));
		}
		format(buf, size, off, label, ")");
		break;
			       }
	case EFIDP_MSG_IPv6: {
		efidp_ipv6_addr const *a = &dp->ipv6_addr;

		format(buf, size, off, "IPv6", "IPv6(");
		format_ipv6_addr(buf, size, off, a->remote_ipv6_addr,
				 a->remote_port);
		format(buf, size, off, "IPv6",",");
		format_ip_protocol(buf, size, off, "IPv6", a->protocol);
		format(buf, size, off, "IPv6",",");
		switch (a->ip_addr_origin) {
		case EFIDP_IPv6_ORIGIN_STATIC:
			format(buf, size, off, "IPv6", "Static,");
			break;
		case EFIDP_IPv6_ORIGIN_AUTOCONF:
			format(buf, size, off, "IPv6",
			       "StatelessAutoConfigure,");
			break;
		case EFIDP_IPv6_ORIGIN_STATEFUL:
			format(buf, size, off, "IPv6",
			       "StatefulAutoConfigure,");
			break;
		default:
			format(buf, size, off, "IPv6",
			       "0x%hx,", a->ip_addr_origin);
			break;
		}

		format_ipv6_addr(buf, size, off, a->local_ipv6_addr,
				 a->local_port);
		format(buf, size, off, "IPv6",",");
		format_ipv6_addr(buf, size, off, a->gateway_ipv6_addr, 0);
		format(buf, size, off, "IPv6",",%u)", a->prefix_length);

		break;
			     }
	case EFIDP_MSG_UART: {
		int parity = dp->uart.parity;
		char parity_label[] = "DNEOMS";
		int stop_bits = dp->uart.stop_bits;
		char *sb_label[] = {"D", "1", "1.5", "2"};

		format(buf, size, off, "Uart", "Uart(%"PRIu64",%d,",
			    dp->uart.baud_rate ? dp->uart.baud_rate : 115200,
			    dp->uart.data_bits ? dp->uart.data_bits : 8);
		format(buf, size, off, "Uart",
			    parity > 5 ? "%d," : "%c,",
			    parity > 5 ? parity : parity_label[parity]);
		if (stop_bits > 3)
			format(buf, size, off, "Uart", "%d)", stop_bits);
		else
			format(buf, size, off, "Uart", "%s)",
			       sb_label[stop_bits]);
		break;
			     }
	case EFIDP_MSG_USB_CLASS:
		format_helper(format_usb_class, buf, size, off, "UsbClass", dp);
		break;
	case EFIDP_MSG_USB_WWID: {
		ssize_t limit = efidp_node_size(dp);
		size_t offset = offsetof(efidp_usb_wwid, serial_number);
		if (limit <= 0 ||
		    SUB(limit,  offset, &limit) ||
		    DIV(limit, 2, &limit)) {
			efi_error("bad DP node size");
			return -1;
		}
		format(buf, size, off, "UsbWwid",
			    "UsbWwid(%"PRIx16",%"PRIx16",%d,",
			    dp->usb_wwid.vendor_id, dp->usb_wwid.product_id,
			    dp->usb_wwid.interface);
		format_ucs2(buf, size, off, "UsbWwid",
			    dp->usb_wwid.serial_number, limit);
		format(buf, size, off, "UsbWwid", ")");
		break;
				 }
	case EFIDP_MSG_LUN:
		format(buf, size, off, "Unit", "Unit(%d)", dp->lun.lun);
		break;
	case EFIDP_MSG_SATA:
		format(buf, size, off, "Sata", "Sata(%d,%d,%d)",
			    dp->sata.hba_port, dp->sata.port_multiplier_port,
			    dp->sata.lun);
		break;
	case EFIDP_MSG_ISCSI: {
		ssize_t sz = efidp_node_size(dp)
			- offsetof(efidp_iscsi, target_name);
		if (sz < 0) {
			efi_error("bad DP node size");
			return -1;
		}

		if (sz > EFIDP_ISCSI_MAX_TARGET_NAME_LEN)
			sz = EFIDP_ISCSI_MAX_TARGET_NAME_LEN;

		char target_name[sz + 1];
		memcpy(target_name, dp->iscsi.target_name, sz);
		target_name[sz] = '\0';
		uint64_t lun;

		memcpy(&lun, dp->iscsi.lun, sizeof (lun));

		format(buf, size, off, "iSCSI",
			      "iSCSI(%s,%d,0x%"PRIx64",%s,%s,%s,%s)",
			      target_name, dp->iscsi.tpgt,
			      be64_to_cpu(lun),
			      (dp->iscsi.options >> EFIDP_ISCSI_HEADER_DIGEST_SHIFT) & EFIDP_ISCSI_HEADER_CRC32 ? "CRC32" : "None",
			      (dp->iscsi.options >> EFIDP_ISCSI_DATA_DIGEST_SHIFT) & EFIDP_ISCSI_DATA_CRC32 ? "CRC32" : "None",
			      (dp->iscsi.options >> EFIDP_ISCSI_AUTH_SHIFT) & EFIDP_ISCSI_AUTH_NONE ? "None" : \
				      (dp->iscsi.options >> EFIDP_ISCSI_CHAP_SHIFT) & EFIDP_ISCSI_CHAP_UNI ? "CHAP_UNI" : "CHAP_BI",
			      dp->iscsi.protocol == 0 ? "TCP" : "Unknown");
		break;
			      }
	case EFIDP_MSG_VLAN:
		format(buf, size, off, "Vlan", "Vlan(%d)", dp->vlan.vlan_id);
		break;
	case EFIDP_MSG_SAS_EX:
		format_sas(buf, size, NULL, dp);
		break;
	case EFIDP_MSG_NVME:
		format(buf, size, off, "NVMe", "NVMe(0x%"PRIx32","
			   "%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X)",
			   dp->nvme.namespace_id, dp->nvme.ieee_eui_64[0],
			   dp->nvme.ieee_eui_64[1], dp->nvme.ieee_eui_64[2],
			   dp->nvme.ieee_eui_64[3], dp->nvme.ieee_eui_64[4],
			   dp->nvme.ieee_eui_64[5], dp->nvme.ieee_eui_64[6],
			   dp->nvme.ieee_eui_64[7]);
		break;
	case EFIDP_MSG_URI: {
		ssize_t sz = efidp_node_size(dp) - offsetof(efidp_uri, uri);
		if (sz < 0) {
			efi_error("bad DP node size");
			return -1;
		}

		char uri[sz + 1];
		memcpy(uri, dp->uri.uri, sz);
		uri[sz] = '\0';
		format(buf, size, off, "Uri", "Uri(%s)", uri);
		break;
			    }
	case EFIDP_MSG_UFS:
		format(buf, size, off, "UFS", "UFS(%d,0x%02x)",
			    dp->ufs.target_id, dp->ufs.lun);
		break;
	case EFIDP_MSG_SD:
		format(buf, size, off, "SD", "SD(%d)", dp->sd.slot_number);
		break;
	case EFIDP_MSG_BT:
		format(buf, size, off, "Bluetooth", "Bluetooth(");
		format_hex_separated(buf, size, off, "Bluetooth", ":", 1,
				     dp->bt.addr, sizeof(dp->bt.addr));
		format(buf, size, off, "Bluetooth", ")");
		break;
	case EFIDP_MSG_WIFI:
		format(buf, size, off, "Wi-Fi", "Wi-Fi(");
		format_hex_separated(buf, size, off, "Wi-Fi", ":", 1,
				     dp->wifi.ssid, sizeof(dp->wifi.ssid));
		format(buf, size, off, "Wi-Fi", ")");
		break;
	case EFIDP_MSG_EMMC:
		format(buf, size, off, "eMMC", "eMMC(%d)", dp->emmc.slot);
		break;
	case EFIDP_MSG_BTLE:
		format(buf, size, off, "BluetoothLE", "BluetoothLE(");
		format_hex_separated(buf, size, off, "BluetoothLE", ":", 1,
				     dp->btle.addr, sizeof(dp->btle.addr));
		format(buf, size, off, "BluetoothLE", ",%d)",
		       dp->btle.addr_type);
		break;
	case EFIDP_MSG_DNS: {
		int end = (efidp_node_size(dp)
			   - sizeof(dp->dns.header)
			   - sizeof(dp->dns.is_ipv6)
			  ) / sizeof(efi_ip_addr_t);
		format(buf, size, off, "Dns", "Dns(");
		for (int i=0; i < end; i++) {
			efi_ip_addr_t addr;

			memcpy(&addr, &dp->dns.addrs[i], sizeof(addr));
			if (i != 0)
				format(buf, size, off, "Dns", ",");
			format_ip_addr(buf, size, off, "Dns",
				       dp->dns.is_ipv6, &addr);
		}
		format(buf, size, off, "Dns", ")");
		break;
	}
	case EFIDP_MSG_NVDIMM:
		format(buf, size, off, "NVDIMM", "NVDIMM(");
		format_guid(buf, size, off, "NVDIMM", &dp->nvdimm.uuid);
		format(buf, size, off, "NVDIMM", ")");
		break;
	default: {
		ssize_t limit = efidp_node_size(dp);
		if (SUB(limit, 4, &limit) ||
		    limit < 0) {
			efi_error("bad DP node size");
			return -1;
		}
		format(buf, size, off, "Msg", "Msg(%d,", dp->subtype);
		format_hex(buf, size, off, "Msg", (uint8_t *)dp+4, limit);
		format(buf, size, off, "Msg", ")");
		break;
		 }
	}
	return off;
}

ssize_t PUBLIC
efidp_make_mac_addr(uint8_t *buf, ssize_t size, uint8_t if_type,
		    const uint8_t * const mac_addr, ssize_t mac_addr_size)
{
	efidp_mac_addr *mac = (efidp_mac_addr *)buf;

	ssize_t sz = efidp_make_generic(buf, size, EFIDP_MESSAGE_TYPE,
					EFIDP_MSG_MAC_ADDR, sizeof (*mac));
	ssize_t req = sizeof (*mac);
	if (size && sz == req) {
		mac->if_type = if_type;
		memcpy(mac->mac_addr, mac_addr,
		       mac_addr_size > 32 ? 32 : mac_addr_size);
	}

	if (sz < 0)
		efi_error("efidp_make_generic failed");

	return sz;
}

ssize_t PUBLIC
efidp_make_ipv4(uint8_t *buf, ssize_t size, uint32_t local, uint32_t remote,
		uint32_t gateway, uint32_t netmask,
		uint16_t local_port, uint16_t remote_port,
		uint16_t protocol, int is_static)
{
	efidp_ipv4_addr *ipv4 = (efidp_ipv4_addr *)buf;
	ssize_t sz = efidp_make_generic(buf, size, EFIDP_MESSAGE_TYPE,
					EFIDP_MSG_IPv4, sizeof (*ipv4));
	ssize_t req = sizeof (*ipv4);
	if (size && sz == req) {
		*((uint32_t *)ipv4->local_ipv4_addr) = htonl(local);
		*((uint32_t *)ipv4->remote_ipv4_addr) = htonl(remote);
		ipv4->local_port = htons(local_port);
		ipv4->remote_port = htons(remote_port);
		ipv4->protocol = htons(protocol);
		ipv4->static_ip_addr = 0;
		if (is_static)
			ipv4->static_ip_addr = 1;
		*((uint32_t *)ipv4->gateway) = htonl(gateway);
		*((uint32_t *)ipv4->netmask) = htonl(netmask);
	}

	if (sz < 0)
		efi_error("efidp_make_generic failed");

	return sz;
}

ssize_t PUBLIC
efidp_make_scsi(uint8_t *buf, ssize_t size, uint16_t target, uint16_t lun)
{
	efidp_scsi *scsi = (efidp_scsi *)buf;
	ssize_t req = sizeof (*scsi);
	ssize_t sz = efidp_make_generic(buf, size, EFIDP_MESSAGE_TYPE,
					EFIDP_MSG_SCSI, sizeof (*scsi));
	if (size && sz == req) {
		scsi->target = target;
		scsi->lun = lun;
	}

	if (sz < 0)
		efi_error("efidp_make_generic failed");

	return sz;
}

ssize_t PUBLIC
efidp_make_nvme(uint8_t *buf, ssize_t size, uint32_t namespace_id,
		uint8_t *ieee_eui_64)
{
	efidp_nvme *nvme = (efidp_nvme *)buf;
	ssize_t req = sizeof (*nvme);
	ssize_t sz;

	sz = efidp_make_generic(buf, size, EFIDP_MESSAGE_TYPE,
					EFIDP_MSG_NVME, sizeof (*nvme));
	if (size && sz == req) {
		nvme->namespace_id = namespace_id;
		if (ieee_eui_64)
			memcpy(nvme->ieee_eui_64, ieee_eui_64,
			       sizeof (nvme->ieee_eui_64));
		else
			memset(nvme->ieee_eui_64, '\0',
			       sizeof (nvme->ieee_eui_64));
	}

	if (sz < 0)
		efi_error("efidp_make_generic failed");

	return sz;
}

ssize_t PUBLIC
efidp_make_sata(uint8_t *buf, ssize_t size, uint16_t hba_port,
		int16_t port_multiplier_port, uint16_t lun)
{
	efidp_sata *sata = (efidp_sata *)buf;
	ssize_t req = sizeof (*sata);
	ssize_t sz;

	sz = efidp_make_generic(buf, size, EFIDP_MESSAGE_TYPE,
					EFIDP_MSG_SATA, sizeof (*sata));
	if (size && sz == req) {
		sata->hba_port = hba_port;
		sata->port_multiplier_port = port_multiplier_port;
		sata->lun = lun;
	}

	if (sz < 0)
		efi_error("efidp_make_generic failed");

	return sz;
}

ssize_t PUBLIC
efidp_make_atapi(uint8_t *buf, ssize_t size, uint16_t primary,
		uint16_t slave, uint16_t lun)
{
	efidp_atapi *atapi = (efidp_atapi *)buf;
	ssize_t req = sizeof (*atapi);
	ssize_t sz;

	sz = efidp_make_generic(buf, size, EFIDP_MESSAGE_TYPE,
					EFIDP_MSG_ATAPI, sizeof (*atapi));
	if (size && sz == req) {
		atapi->primary = primary;
		atapi->slave = slave;
		atapi->lun = lun;
	}

	if (sz < 0)
		efi_error("efidp_make_generic failed");

	return sz;
}


ssize_t PUBLIC
efidp_make_sas(uint8_t *buf, ssize_t size, uint64_t sas_address)
{
	efidp_sas *sas = (efidp_sas *)buf;
	ssize_t req = sizeof (*sas);
	ssize_t sz;

	sz = efidp_make_generic(buf, size, EFIDP_MESSAGE_TYPE,
					EFIDP_MSG_VENDOR, sizeof (*sas));
	if (size && sz == req) {
		sas->vendor_guid = EFIDP_MSG_SAS_GUID;
		sas->reserved = 0;
		sas->sas_address = sas_address;
		sas->lun = 0;
		sas->device_topology_info = 0;
		sas->drive_bay_id = 0;
		sas->rtp = 0;
	}

	if (sz < 0)
		efi_error("efidp_make_generic failed");

	return sz;
}

ssize_t PUBLIC
efidp_make_nvdimm(uint8_t *buf, ssize_t size, efi_guid_t *uuid)
{
	efidp_nvdimm *nvdimm = (efidp_nvdimm *)buf;
	ssize_t req = sizeof (*nvdimm);
	ssize_t sz;

	sz = efidp_make_generic(buf, size, EFIDP_MESSAGE_TYPE,
				EFIDP_MSG_NVDIMM, sizeof (*nvdimm));
	if (size && sz == req) {
		memcpy(&nvdimm->uuid, uuid, sizeof(*uuid));
	}

	if (sz < 0)
		efi_error("efidp_make_generic failed");

	return sz;
}

ssize_t PUBLIC
efidp_make_emmc(uint8_t *buf, ssize_t size, uint32_t slot_id)
{
	efidp_emmc *emmc = (efidp_emmc *)buf;
	ssize_t req = sizeof (*emmc);
	ssize_t sz;

	sz = efidp_make_generic(buf, size, EFIDP_MESSAGE_TYPE,
					EFIDP_MSG_NVME, sizeof (*emmc));
	if (size && sz == req)
		emmc->slot = slot_id;

	if (sz < 0)
		efi_error("efidp_make_generic failed");

	return sz;
}

// vim:fenc=utf-8:tw=75:noet
