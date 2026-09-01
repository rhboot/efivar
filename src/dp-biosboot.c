// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * dp-biosboot.c - BIOS boot helpers
 * Copyright Peter Jones <pjones@redhat.com>
 */

#include "fix_coverity.h" // IWYU pragma: keep

#include "efivar.h" // IWYU pragma: keep

ssize_t PUBLIC
efidp_make_bios_boot(uint8_t *buf, ssize_t size,
		     uint16_t device_type, uint16_t status,
		     uint8_t *description)
{
	efidp_bios_boot *bbs = (efidp_bios_boot *)buf;
	size_t descsz = strlen((char *)description) + 1;

	ssize_t sz = efidp_make_generic(buf, size, EFIDP_BIOS_BOOT_TYPE,
					EFIDP_BIOS_BOOT, sizeof(*bbs) + descsz);
	ssize_t req = sizeof(*bbs) + strlen((char *)description) + 1;
	if (size && sz == req) {
		bbs->device_type = device_type;
		bbs->status = status;
		memcpy(bbs->description, description, descsz);
	}

	if (sz < 0)
		efi_error("efidp_make_generic failed");

	return sz;
}

ssize_t PUBLIC
_format_bios_boot_dn(unsigned char *buf, size_t size, const_efidp dp)
{
	ssize_t off = 0;
	char *types[] = {"", "Floppy", "HD", "CDROM", "PCMCIA", "USB", "Network", "" };

	if (dp->subtype != EFIDP_BIOS_BOOT) {
		size_t sz = efidp_node_size(dp) - sizeof(efidp_header);
		format(buf, size, off, "BbsPath", "BbsPath(%d", dp->subtype);
		if (sz > 0) {
			format(buf, size, off, "BbsPath", ",");
			format_hex(buf, size, off, "BbsPath", (uint8_t *)dp+4, sz);
		}
		format(buf, size, off, "BbsPath", ")");
		return off;
	}

	if (dp->bios_boot.device_type >= EFIDP_BIOS_BOOT_DEVICE_TYPE_FLOPPY &&
	    dp->bios_boot.device_type <= EFIDP_BIOS_BOOT_DEVICE_TYPE_EMBEDDED_NET) {
		format(buf, size, off, "BBS", "BBS(%s,%s,0x%"PRIx32")",
		       types[dp->bios_boot.device_type],
		       dp->bios_boot.description,
		       dp->bios_boot.status);
	} else {
		format(buf, size, off, "BBS", "BBS(%d,%s,0x%"PRIx32")",
		       dp->bios_boot.device_type,
		       dp->bios_boot.description,
		       dp->bios_boot.status);
	}
	return off;
}

// vim:fenc=utf-8:tw=75:noet
