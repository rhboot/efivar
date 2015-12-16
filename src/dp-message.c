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

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <stddef.h>

#include <efivar.h>
#include "efivar_endian.h"
#include "dp.h"

static ssize_t
format_ipv6_port_helper(char *buffer, size_t buffer_size,
			uint8_t const *ipaddr, uint16_t port)
{
	uint16_t *ip = (uint16_t *)ipaddr;
	off_t offset = 0;
	ssize_t needed;

	needed = snprintf(buffer, buffer_size, "[");
	if (needed < 0)
		return -1;
	offset += needed;

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
		this_zero_block_size = 0;
		this_zero_block_offset = -1;
		in_zero_block = 0;
	}
	if (largest_zero_block_size == 1)
		largest_zero_block_offset = -1;

	for (i = 0; i < 8; i++) {
		if (largest_zero_block_offset == i) {
			needed = snprintf(buffer + offset,
					  buffer_size == 0 ? 0
						: buffer_size - offset,
					  "::");
			if (needed < 0)
				return -1;
			offset += needed;
			i += largest_zero_block_size -1;
			continue;
		} else if (i > 0) {
			needed = snprintf(buffer + offset,
					  buffer_size == 0 ? 0
						: buffer_size - offset,
					  ":");
			if (needed < 0)
				return -1;
			offset += needed;
		}

		needed = snprintf(buffer + offset,
				  buffer_size == 0 ? 0 : buffer_size - offset,
				  "%x", ip[i]);
		if (needed < 0)
			return -1;
		offset += needed;
	}

	needed = snprintf(buffer + offset,
			  buffer_size == 0 ? 0 : buffer_size - offset,
			  "]:%d", port);
	if (needed < 0)
		return -1;
	offset += needed;

	return offset;
}

#define format_ipv6_port(buf, size, off, ipaddr, port)			\
	format_helper(format_ipv6_port_helper, buf, size, off, ipaddr, port)

static ssize_t
format_uart(char *buf, size_t size, const_efidp dp)
{
	uint32_t value;
	ssize_t off = 0;
	char *labels[] = {"None", "Hardware", "XonXoff", ""};

	value = dp->uart_flow_control.flow_control_map;
	if (value > 2) {
		return format(buf, size, off, "UartFlowcontrol(%d)", value);
	}
	return format(buf, size, off, "UartFlowControl(%s)", labels[value]);
}

static ssize_t
format_sas(char *buf, size_t size, const_efidp dp)
{
	size_t off = 0;
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

	off += format(buf, size, off, "SAS(%"PRIx64",%"PRIx64",%"PRIx16",%s",
		      dp->subtype == EFIDP_MSG_SAS_EX ?
			be64_to_cpu(s->sas_address) :
			le64_to_cpu(s->sas_address),
		      dp->subtype == EFIDP_MSG_SAS_EX ?
			be64_to_cpu(s->lun) :
			le64_to_cpu(s->lun),
		      s->rtp, sassata_label[sassata]);

	if (more_info)
		off += format(buf, size, off, ",%s,%s",
			      location_label[location], connect_label[connect]);

	if (more_info == 2 && drive_bay >= 0)
		off += format(buf, size, off, ",%d", drive_bay);

	off += format(buf, size, off, ")");
	return off;
}

#define class_helper(buf, size, off, label, dp) ({			\
		off += format(buf, size, off,				\
			      "%s(0x%"PRIx16",0x%"PRIx16",%d,%d)",	\
			      label,					\
			      dp->usb_class.vendor_id,			\
			      dp->usb_class.product_id,			\
			      dp->usb_class.device_subclass,		\
			      dp->usb_class.device_protocol);		\
		off;							\
	})

static ssize_t
format_usb_class(char *buf, size_t size, const_efidp dp)
{
	ssize_t off = 0;
	switch (dp->usb_class.device_class) {
	case EFIDP_USB_CLASS_AUDIO:
		off += class_helper(buf, size, off, "UsbAudio", dp);
		break;
	case EFIDP_USB_CLASS_CDC_CONTROL:
		off += class_helper(buf, size, off, "UsbCDCControl", dp);
		break;
	case EFIDP_USB_CLASS_HID:
		off += class_helper(buf, size, off, "UsbHID", dp);
		break;
	case EFIDP_USB_CLASS_IMAGE:
		off += class_helper(buf, size, off, "UsbImage", dp);
		break;
	case EFIDP_USB_CLASS_PRINTER:
		off += class_helper(buf, size, off, "UsbPrinter", dp);
		break;
	case EFIDP_USB_CLASS_MASS_STORAGE:
		off += class_helper(buf, size, off, "UsbMassStorage", dp);
		break;
	case EFIDP_USB_CLASS_HUB:
		off += class_helper(buf, size, off, "UsbHub", dp);
		break;
	case EFIDP_USB_CLASS_CDC_DATA:
		off += class_helper(buf, size, off, "UsbCDCData", dp);
		break;
	case EFIDP_USB_CLASS_SMARTCARD:
		off += class_helper(buf, size, off, "UsbSmartCard", dp);
		break;
	case EFIDP_USB_CLASS_VIDEO:
		off += class_helper(buf, size, off, "UsbVideo", dp);
		break;
	case EFIDP_USB_CLASS_DIAGNOSTIC:
		off += class_helper(buf, size, off, "UsbDiagnostic", dp);
		break;
	case EFIDP_USB_CLASS_WIRELESS:
		off += class_helper(buf, size, off, "UsbWireless", dp);
		break;
	case EFIDP_USB_CLASS_254:
		switch (dp->usb_class.device_subclass) {
		case EFIDP_USB_SUBCLASS_FW_UPDATE:
			off += format(buf, size, off,
				      "UsbDeviceFirmwareUpdate(0x%"PRIx16",0x%"PRIx16",%d)",
				      dp->usb_class.vendor_id,
				      dp->usb_class.product_id,
				      dp->usb_class.device_protocol);
			break;
		case EFIDP_USB_SUBCLASS_IRDA_BRIDGE:
			off += format(buf, size, off,
				      "UsbIrdaBridge(0x%"PRIx16",0x%"PRIx16",%d)",
				      dp->usb_class.vendor_id,
				      dp->usb_class.product_id,
				      dp->usb_class.device_protocol);
			break;
		case EFIDP_USB_SUBCLASS_TEST_AND_MEASURE:
			off += format(buf, size, off,
				      "UsbTestAndMeasurement(0x%"PRIx16",0x%"PRIx16",%d)",
				      dp->usb_class.vendor_id,
				      dp->usb_class.product_id,
				      dp->usb_class.device_protocol);
			break;
		}
		break;
	default:
		off += format(buf, size, off,
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
__attribute__((__visibility__ ("default")))
_format_message_dn(char *buf, size_t size, const_efidp dp)
{
	ssize_t off = 0;
	ssize_t sz;
	switch (dp->subtype) {
	case EFIDP_MSG_ATAPI:
		off += format(buf, size, off, "Ata(%d,%d,%d)",
			      dp->atapi.primary, dp->atapi.slave,
			      dp->atapi.lun);
		break;
	case EFIDP_MSG_SCSI:
		off += format(buf, size, off, "SCSI(%d,%d)",
			      dp->scsi.target, dp->scsi.lun);
		break;
	case EFIDP_MSG_FIBRECHANNEL:
		off += format(buf, size, off, "Fibre(%"PRIx64",%"PRIx64")",
			      le64_to_cpu(dp->fc.wwn),
			      le64_to_cpu(dp->fc.lun));
		break;
	case EFIDP_MSG_FIBRECHANNELEX:
		off += format(buf, size, off, "Fibre(%"PRIx64",%"PRIx64")",
			      be64_to_cpu(dp->fc.wwn),
			      be64_to_cpu(dp->fc.lun));
		break;
	case EFIDP_MSG_1394:
		off += format(buf, size, off, "I1394(0x%"PRIx64")",
			      dp->firewire.guid);
		break;
	case EFIDP_MSG_USB:
		off += format(buf, size, off, "USB(%d,%d)",
			      dp->usb.parent_port, dp->usb.interface);
		break;
	case EFIDP_MSG_I2O:
		off += format(buf, size, off, "I2O(%d)", dp->i2o.target);
		break;
	case EFIDP_MSG_INFINIBAND:
		if (dp->infiniband.resource_flags &
				EFIDP_INFINIBAND_RESOURCE_IOC_SERVICE) {
			off += format(buf, size, off,
				      "Infiniband(%08x,%"PRIx64"%"PRIx64",%"PRIx64",%"PRIu64",%"PRIu64")",
				      dp->infiniband.resource_flags,
				      dp->infiniband.port_gid[1],
				      dp->infiniband.port_gid[0],
				      dp->infiniband.service_id,
				      dp->infiniband.target_port_id,
				      dp->infiniband.device_id);
		} else {
			off += format(buf, size, off,
				      "Infiniband(%08x,%"PRIx64"%"PRIx64",",
				      dp->infiniband.resource_flags,
				      dp->infiniband.port_gid[1],
				      dp->infiniband.port_gid[0]);
			off += format_guid(buf, size, off, (efi_guid_t *)
					   &dp->infiniband.ioc_guid);
			off += format(buf, size, off, ",%"PRIu64",%"PRIu64")",
				      dp->infiniband.target_port_id,
				      dp->infiniband.device_id);
		}
		break;
	case EFIDP_MSG_MAC_ADDR:
		off += format(buf, size, off, "MAC(");
		off += format_hex(buf, size, off, dp->mac_addr.mac_addr,
				  dp->mac_addr.if_type < 2 ? 6
					: sizeof(dp->mac_addr.mac_addr));
		off += format(buf, size, off, ",%d)", dp->mac_addr.if_type);
		break;
	case EFIDP_MSG_IPv4: {
		efidp_ipv4_addr const *a = &dp->ipv4_addr;
		off += format(buf, size, off,
			      "IPv4(%hhu.%hhu.%hhu.%hhu:%hu<->%hhu.%hhu.%hhu.%hhu:%hu,%hx,%hhx)",
			      a->local_ipv4_addr[0], a->local_ipv4_addr[1],
			      a->local_ipv4_addr[2], a->local_ipv4_addr[3],
			      a->local_port, a->remote_ipv4_addr[0],
			      a->remote_ipv4_addr[1], a->remote_ipv4_addr[2],
			      a->remote_ipv4_addr[3], a->remote_port,
			      a->protocol, a->static_ip_addr);
		break;
			     }
	case EFIDP_MSG_VENDOR: {
		struct {
			efi_guid_t guid;
			char label[40];
			ssize_t (*formatter)(char *buf, size_t size,
					     const_efidp dp);
		} subtypes[] = {
			{ EFIDP_PC_ANSI_GUID, "VenPcAnsi" },
			{ EFIDP_VT_100_GUID, "VenVt100" },
			{ EFIDP_VT_100_PLUS_GUID, "VenVt100Plus" },
			{ EFIDP_VT_UTF8_GUID, "VenUtf8" },
			{ EFIDP_MSG_DEBUGPORT_GUID, "DebugPort" },
			{ EFIDP_MSG_UART_GUID, "", format_uart },
			{ EFIDP_MSG_SAS_GUID, "", format_sas },
			{ efi_guid_empty, "" }
		};
		char *label = NULL;
		ssize_t (*formatter)(char *buf, size_t size,
				     const_efidp dp) = NULL;

		for (int i = 0; !efi_guid_is_zero(&subtypes[i].guid); i++) {
			if (efi_guid_cmp(&subtypes[i].guid,
					  &dp->msg_vendor.vendor_guid))
				continue;

			label = subtypes[i].label;
			formatter = subtypes[i].formatter;
			break;
		}

		if (!label && !formatter) {
			off += format_vendor(buf, size, off, "VenMsg", dp);
			break;
		} else if (formatter) {
			off += format_helper(formatter, buf, size, off, dp);
			break;
		}

		off += format(buf, size, off, "%s(", label);
		if (efidp_node_size(dp) >
				(ssize_t)(sizeof (efidp_header)
					  + sizeof (efi_guid_t))) {
			off += format_hex(buf, size, off,
					  dp->msg_vendor.vendor_data,
					  efidp_node_size(dp)
						- sizeof (efidp_header)
						- sizeof (efi_guid_t));
		}
		break;
			       }
	case EFIDP_MSG_IPv6: {
		efidp_ipv6_addr const *a = &dp->ipv6_addr;
		char *addr0 = NULL;
		char *addr1 = NULL;

		sz = format_ipv6_port(addr0, 0, 0, a->local_ipv6_addr,
				      a->local_port);
		if (sz < 0)
			return sz;

		addr0 = alloca(sz+1);
		sz = format_ipv6_port(addr0, sz, 0, a->local_ipv6_addr,
				      a->local_port);
		if (sz < 0)
			return sz;

		sz = format_ipv6_port(addr1, 0, 0, a->remote_ipv6_addr,
				      a->remote_port);
		if (sz < 0)
			return sz;
		addr1 = alloca(sz+1);
		sz = format_ipv6_port(addr1, sz, 0, a->remote_ipv6_addr,
				      a->remote_port);
		if (sz < 0)
			return sz;

		off += format(buf, size, off, "IPv6(%s<->%s,%hx,%hhx)",
			     addr0, addr1, a->protocol, a->ip_addr_origin);
		break;
			     }
	case EFIDP_MSG_UART: {
		int parity = dp->uart.parity;
		char parity_label[] = "DNEOMS";
		int stop_bits = dp->uart.stop_bits;
		char *sb_label[] = {"D", "1", "1.5", "2"};

		off += format(buf, size, off, "Uart(%"PRIu64",%d,",
			     dp->uart.baud_rate ? dp->uart.baud_rate : 115200,
			     dp->uart.data_bits ? dp->uart.data_bits : 8);
		off += format(buf, size, off,
			     parity > 5 ? "%d," : "%c,",
			     parity > 5 ? parity : parity_label[parity]);
		if (stop_bits > 3)
			off += format(buf, size, off, "%d)", stop_bits);
		else
			off += format(buf, size, off, "%s)",
				     sb_label[stop_bits]);
		break;
			     }
	case EFIDP_MSG_USB_CLASS:
		off += format_helper(format_usb_class, buf, size, off, dp);
		break;
	case EFIDP_MSG_USB_WWID:
		off += format(buf, size, off,
			      "UsbWwid(%"PRIx16",%"PRIx16",%d,",
			      dp->usb_wwid.vendor_id, dp->usb_wwid.product_id,
			      dp->usb_wwid.interface);
		off += format_ucs2(buf, size, off, dp->usb_wwid.serial_number,
				   (efidp_node_size(dp)
				    - offsetof(efidp_usb_wwid, serial_number))
				   / 2 + 1);
		off += format(buf, size, off, ")");
		break;
	case EFIDP_MSG_LUN:
		off += format(buf, size, off, "Unit(%d)", dp->lun.lun);
		break;
	case EFIDP_MSG_SATA:
		off += format(buf, size, off, "Sata(%d,%d,%d)",
			     dp->sata.hba_port, dp->sata.port_multiplier_port,
			     dp->sata.lun);
		break;
	case EFIDP_MSG_ISCSI: {
		size_t sz = efidp_node_size(dp)
			    - offsetof(efidp_iscsi, target_name);
		if (sz > EFIDP_ISCSI_MAX_TARGET_NAME_LEN)
			sz = EFIDP_ISCSI_MAX_TARGET_NAME_LEN;
		char target_name[sz + 1];
		memcpy(target_name, dp->iscsi.target_name, sz);
		target_name[sz] = '\0';
		uint64_t lun;

		memcpy(&lun, dp->iscsi.lun, sizeof (lun));

		off += format(buf, size, off,
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
		off += format(buf, size, off, "Vlan(%d)", dp->vlan.vlan_id);
		break;
	case EFIDP_MSG_SAS_EX:
		off += format_sas(buf, size, dp);
		break;
	case EFIDP_MSG_NVME:
		off += format(buf, size, off, "NVMe(0x%"PRIx32","
			      "%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X)",
			      dp->nvme.namespace_id, dp->nvme.ieee_eui_64[0],
			      dp->nvme.ieee_eui_64[1], dp->nvme.ieee_eui_64[2],
			      dp->nvme.ieee_eui_64[3], dp->nvme.ieee_eui_64[4],
			      dp->nvme.ieee_eui_64[5], dp->nvme.ieee_eui_64[6],
			      dp->nvme.ieee_eui_64[7]);
		break;
	case EFIDP_MSG_URI: {
		size_t sz = efidp_node_size(dp) - offsetof(efidp_uri, uri);
		char uri[sz + 1];
		memcpy(uri, dp->uri.uri, sz);
		uri[sz] = '\0';
		off += format(buf, size, off, "Uri(%s)", uri);
		break;
			    }
	case EFIDP_MSG_UFS:
		off += format(buf, size, off, "UFS(%d,0x%02x)",
			      dp->ufs.target_id, dp->ufs.lun);
		break;
	case EFIDP_MSG_SD:
		off += format(buf, size, off, "SD(%d)", dp->sd.slot_number);
		break;
	default:
		off += format(buf, size, off, "Msg(%d,", dp->subtype);
		off += format_hex(buf, size, off, (uint8_t *)dp+4,
				  efidp_node_size(dp)-4);
		off += format(buf,size,off,")");
		break;
	}
	return off;
}

ssize_t
__attribute__((__visibility__ ("default")))
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
	return sz;
}

ssize_t
__attribute__((__visibility__ ("default")))
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
		*((char *)ipv4->local_ipv4_addr) = htonl(local);
		*((char *)ipv4->remote_ipv4_addr) = htonl(remote);
		ipv4->local_port = htons(local_port);
		ipv4->remote_port = htons(remote_port);
		ipv4->protocol = htons(protocol);
		ipv4->static_ip_addr = 0;
		if (is_static)
			ipv4->static_ip_addr = 1;
		*((char *)ipv4->gateway) = htonl(gateway);
		*((char *)ipv4->netmask) = htonl(netmask);
	}
	return sz;
}

ssize_t
__attribute__((__visibility__ ("default")))
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
	return sz;
}

ssize_t
__attribute__((__visibility__ ("default")))
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
	return sz;
}

ssize_t
__attribute__((__visibility__ ("default")))
efidp_make_sata(uint8_t *buf, ssize_t size, uint16_t hba_port,
		uint16_t port_multiplier_port, uint16_t lun)
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
	return sz;
}

ssize_t
__attribute__((__visibility__ ("default")))
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
	return sz;
}


ssize_t
__attribute__((__visibility__ ("default")))
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
	return sz;
}
