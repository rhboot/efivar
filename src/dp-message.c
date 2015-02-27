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

#include "efivar.h"
#include "dp.h"

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

ssize_t
format_message_dn(char *buf, size_t size, const_efidp dp)
{
	off_t off = 0;
	size_t sz;
	switch (dp->subtype) {
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
	case EFIDP_MSG_VENDOR:
		off += format_vendor(buf+off, size?size-off:0, "VenMsg", dp);
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
