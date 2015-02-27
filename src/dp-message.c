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
