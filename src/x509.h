// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * x509.h - X.509/ASN.1 helper functions
 * Copyright 2019-2020 Peter M. Jones <pjones@redhat.com>
 */
#ifndef EFIVAR_X509_H
#define EFIVAR_X509_H

#define SMALLEST_POSSIBLE_DER_SEQ 3

#include <inttypes.h>
#include <unistd.h>
#include "util.h"

static inline int32_t __attribute__((unused))
get_asn1_seq_size(uint8_t *location, uint32_t size)
{
	uint8_t i;
	uint8_t octets;
	uint32_t der_len = 0;

	if (size < SMALLEST_POSSIBLE_DER_SEQ)
		return -1;

	// If it's not a CONSTRUCTED SEQUENCE it's not a certificate
	if (location[0] != 0x30) {
		debug("%p: %d != 0x30", &location[0], location[0]);
		return -1;
	}

	if (!(location[1] & 0x80)) {
		// Short form, which is too small to hold a certificate.
		debug("%p: %d & 0x80 == 1", &location[1], location[1]);
		return -1;
	}

	// Long form
	octets = location[1] & 0x7;

	// There is no chance our data is more than 3GB.
	if (octets > 4 || (octets == 4 && (location[2] & 0x8))) {
		debug("octets: %" PRIu32 " %p:%d", octets, &location[2],
		      location[2] & 0x8);
		return -1;
	}

	// and if our size won't fit in the data it's wrong as well
	if (size - 2 < octets) {
		debug("size-2: %" PRIu32 " < octets %" PRIu32, size - 2,
		      octets);
		return -1;
	}

	for (i = 0; i < octets; i++) {
		der_len <<= 8;
		debug("der_len %" PRIu32 " |= location[%u] = %u = %u", der_len,
		      i + 2, location[i + 2], der_len | location[i + 2]);
		der_len |= location[i + 2];
	}

	// and if der_len is greater than what's left, it's bad too.
	if (size - 2 - octets < der_len) {
		debug("size - 2 - octets (%" PRIu32 ") < der_len (%" PRIu32 ")",
		      size - 2 - octets, der_len);
		return -1;
	}

	// or else it's a reasonable certificate from a size point of view.
	return der_len + 4;
}

#undef SMALLEST_POSSIBLE_DER_SEQ

#endif
// vim:fenc=utf-8:tw=75:noet
