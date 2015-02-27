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

#include <errno.h>
#include <inttypes.h>
#include <stddef.h>

#include "efivar.h"
#include "endian.h"
#include "dp.h"
#include "ucs2.h"

static ssize_t
format_ipv6_port(char *buffer, size_t buffer_size, uint8_t const *ipaddr,
		uint16_t port)
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

static ssize_t
format_uart(char *buf, size_t size, const_efidp dp)
{
	uint32_t value;
	ssize_t off = 0;
	char *labels[] = {"None", "Hardware", "XonXoff", ""};

	value = dp->uart_flow_control.flow_control_map;
	if (value > 2) {
		return pbufx(buf, size, off, "UartFlowcontrol(%d)", value);
	}
	return pbufx(buf, size, off, "UartFlowControl(%s)", labels[value]);
}

static ssize_t
format_sas(char *buf, size_t size, const_efidp dp)
{
	size_t off = 0;
	const efidp_sas const *s = &dp->sas;

	int more_info = 0;
	int sassata = 0;
	int location = 0;
	int connect = 0;
	int drive_bay = -1;

	const char const *sassata_label[] = {"NoTopology", "SAS", "SATA"};
	const char const *location_label[] = {"Internal", "External" };
	const char const *connect_label[] = {"Direct", "Expanded" };

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

	off += pbufx(buf, size, off, "SAS(%"PRIx64",%"PRIx64",%"PRIx16",%s",
		     dp->subtype == EFIDP_MSG_SAS_EX ?
			be64_to_cpu(s->sas_address) :
			le64_to_cpu(s->sas_address),
		     dp->subtype == EFIDP_MSG_SAS_EX ?
			be64_to_cpu(s->lun) :
			le64_to_cpu(s->lun),
		     s->rtp, sassata_label[sassata]);

	if (more_info)
		off += pbufx(buf, size, off, ",%s,%s",
			     location_label[location], connect_label[connect]);

	if (more_info == 2 && drive_bay >= 0)
		off += pbufx(buf, size, off, ",%d", drive_bay);

	off += pbufx(buf, size, off, ")");
	return off;
}

ssize_t
format_message_dn(char *buf, size_t size, const_efidp dp)
{
	off_t off = 0;
	size_t sz;
	switch (dp->subtype) {
	case EFIDP_MSG_ATAPI:
		off += pbufx(buf, size, off, "Ata(%d,%d,%d)",
			     dp->atapi.primary, dp->atapi.slave, dp->atapi.lun);
		break;
	case EFIDP_MSG_SCSI:
		off += pbufx(buf, size, off, "SCSI(%d,%d)",
			     dp->scsi.target, dp->scsi.lun);
		break;
	case EFIDP_MSG_FIBRECHANNEL:
		off += pbufx(buf, size, off, "Fibre(%"PRIx64",%"PRIx64")",
			     le64_to_cpu(dp->fc.wwn),
			     le64_to_cpu(dp->fc.lun));
		break;
	case EFIDP_MSG_FIBRECHANNELEX:
		off += pbufx(buf, size, off, "Fibre(%"PRIx64",%"PRIx64")",
			     be64_to_cpu(dp->fc.wwn),
			     be64_to_cpu(dp->fc.lun));
		break;
	case EFIDP_MSG_1394:
		off += pbufx(buf, size, off, "I1394(0x%"PRIx64")",
			     dp->firewire.guid);
		break;
	case EFIDP_MSG_USB:
		off += pbufx(buf, size, off, "USB(%d,%d)",
			     dp->usb.parent_port, dp->usb.interface);
		break;
	case EFIDP_MSG_I2O:
		off += pbufx(buf, size, off, "I2O(%d)", dp->i2o.target);
		break;
	case EFIDP_MSG_INFINIBAND:
		if (dp->infiniband.resource_flags &
				EFIDP_INFINIBAND_RESOURCE_IOC_SERVICE) {
			off += pbufx(buf, size, off,
				     "Infiniband(%08x,%"PRIx64"%"PRIx64",%"PRIx64",%"PRIu64",%"PRIu64")",
			     dp->infiniband.resource_flags,
			     dp->infiniband.port_gid[1],
			     dp->infiniband.port_gid[0],
			     dp->infiniband.service_id,
			     dp->infiniband.target_port_id,
			     dp->infiniband.device_id);
		} else {
			char *guidstr = NULL;
			int rc;
			rc = efi_guid_to_str(
				(efi_guid_t *)&dp->infiniband.ioc_guid,
					     &guidstr);
			if (rc < 0)
				return rc;
			guidstr = onstack(guidstr, strlen(guidstr)+1);
			off += pbufx(buf, size, off,
				     "Infiniband(%08x,%"PRIx64"%"PRIx64",%s,%"PRIu64",%"PRIu64")",
			     dp->infiniband.resource_flags,
			     dp->infiniband.port_gid[1],
			     dp->infiniband.port_gid[0],
			     guidstr,
			     dp->infiniband.target_port_id,
			     dp->infiniband.device_id);
		}
		break;

	case EFIDP_MSG_MAC_ADDR:
		off += pbufx(buf, size, off, "MAC(");
		sz = format_hex(buf+off, size?size-off:0,
			       dp->mac_addr.mac_addr,
			       dp->mac_addr.if_type < 2 ? 6
					: sizeof(dp->mac_addr.mac_addr));
		if (sz < 0)
			return sz;
		off += sz;
		off += pbufx(buf, size, off, ",%d)", dp->mac_addr.if_type);
		break;
	case EFIDP_MSG_IPv4: {
		efidp_ipv4_addr const *a = &dp->ipv4_addr;
		off += pbufx(buf, size, off,
			     "IPv4(%hhu.%hhu.%hhu.%hhu:%hu<->%hhu.%hhu.%hhu.%hhu:%hu,%hx,%hhx)",
			     a->local_ipv4_addr[0],
			     a->local_ipv4_addr[1],
			     a->local_ipv4_addr[2],
			     a->local_ipv4_addr[3],
			     a->local_port,
			     a->remote_ipv4_addr[0],
			     a->remote_ipv4_addr[1],
			     a->remote_ipv4_addr[2],
			     a->remote_ipv4_addr[3],
			     a->remote_port,
			     a->protocol,
			     a->static_ip_addr);
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
			off += format_vendor(buf+off, size?size-off:0,
					     "VenMsg", dp);
			break;
		} else if (formatter) {
			sz = formatter(buf+off, size?size-off:0, dp);
			if (sz < 0)
				return sz;
			off += sz;
			break;
		}

		off += pbufx(buf, size, off, "%s(", label);
		if (efidp_node_size(dp) >
				(ssize_t)(sizeof (efidp_header)
					  + sizeof (efi_guid_t))) {
			sz = format_hex(buf+off, size?size-off:0,
					dp->msg_vendor.vendor_data,
					efidp_node_size(dp)
					- sizeof (efidp_header)
					- sizeof (efi_guid_t));
			if (sz < 0)
				return sz;
			off += sz;
		}
		break;
			       }
	case EFIDP_MSG_IPv6: {
		efidp_ipv6_addr const *a = &dp->ipv6_addr;
		char *addr0 = NULL;
		char *addr1 = NULL;

		sz = format_ipv6_port(addr0, 0, a->local_ipv6_addr,
				      a->local_port);
		if (sz < 0)
			return sz;

		addr0 = alloca(sz+1);
		sz = format_ipv6_port(addr0, sz, a->local_ipv6_addr,
				      a->local_port);
		if (sz < 0)
			return sz;

		sz = format_ipv6_port(addr1, 0, a->remote_ipv6_addr,
				      a->remote_port);
		if (sz < 0)
			return sz;
		addr1 = alloca(sz+1);
		sz = format_ipv6_port(addr1, sz, a->remote_ipv6_addr,
				      a->remote_port);
		if (sz < 0)
			return sz;

		off += pbufx(buf, size, off, "IPv6(%s<->%s,%hx,%hhx)",
			     addr0, addr1, a->protocol, a->ip_addr_origin);
		break;
			     }
	case EFIDP_MSG_UART: {
		int parity = dp->uart.parity;
		char parity_label[] = "DNEOMS";
		int stop_bits = dp->uart.stop_bits;
		char *sb_label[] = {"D", "1", "1.5", "2"};

		off += pbufx(buf, size, off, "Uart(%"PRIu64",%d,",
			     dp->uart.baud_rate ? dp->uart.baud_rate : 115200,
			     dp->uart.data_bits ? dp->uart.data_bits : 8);
		off += pbufx(buf, size, off,
			     parity > 5 ? "%d," : "%c,",
			     parity > 5 ? parity : parity_label[parity]);
		if (stop_bits > 3)
			off += pbufx(buf, size, off, "%d)", stop_bits);
		else
			off += pbufx(buf, size, off, "%s)",
				     sb_label[stop_bits]);
		break;
			     }
	case EFIDP_MSG_USB_CLASS:
		off += pbufx(buf, size, off,
			     "UsbClass(%"PRIx16",%"PRIx16",%d,%d)",
			     dp->usb_class.vendor_id,
			     dp->usb_class.product_id,
			     dp->usb_class.device_subclass,
			     dp->usb_class.device_protocol);
		break;
	case EFIDP_MSG_USB_WWID: {
		size_t len = (efidp_node_size(dp)
			      - offsetof(efidp_usb_wwid, serial_number))
			     / 2 + 1;
		uint16_t serial16[len];

		memset(serial16, '\0', sizeof (serial16));
		memcpy(serial16, dp->file.name, sizeof (serial16)
						- sizeof (serial16[0]));
		char *serial = ucs2_to_utf8(serial16, len-1);
		serial = onstack(serial, len);

		off += pbufx(buf, size, off,
			     "UsbWwid(%"PRIx16",%"PRIx16",%d,%s)",
			     dp->usb_wwid.vendor_id, dp->usb_wwid.product_id,
			     dp->usb_wwid.interface, serial);
		break;
				 }
	case EFIDP_MSG_LUN:
		off += pbufx(buf, size, off, "Unit(%d)", dp->lun.lun);
		break;
	case EFIDP_MSG_SATA:
		off += pbufx(buf, size, off, "Sata(%d,%d,%d)",
			     dp->sata.hba_port, dp->sata.port_multiplier_port,
			     dp->sata.lun);
		break;
	case EFIDP_MSG_SAS_EX:
		off += format_sas(buf, size, dp);
		break;
	default:
		off += pbufx(buf, size, off, "MessagePath(%d,", dp->subtype);
		sz = format_hex(buf+off, size?size-off:0, (uint8_t *)dp+4,
			       efidp_node_size(dp)-4);
		if (sz < 0)
			return sz;
		off += sz;
		off += pbufx(buf,size,off,")");
		break;
	}
	return off;
}
