// SPDX-License-Identifier: GPL-v3-or-later
/*
 * sbchooser-x509.h - x509 handling
 * Copyright Peter Jones <pjones@redhat.com>
 */

#pragma once

#include "sbchooser.h" // IWYU pragma: keep

struct cert_data {
	X509 *x509;

	/*
	 * OpenSSL owns these and they should not be individually freed
	 */
	X509_NAME *issuer;
	X509_NAME *subject;
	ASN1_INTEGER *serial;

	const ASN1_TIME *not_before;
	const ASN1_TIME *not_after;

	/*
	 * Info about the signature on the cert
	 */
	int mdnid;	// nid for the message digest algorithm
	int secbits;	// security strength of the digest
	int pknid;	// nid for the public key algorithm

	/*
	 * Are all the certs in this x509 signature trusted by db?
	 */
	bool trusted;
	cert_data_t *trust_anchor_cert; // cert it's trusted by (self or
					// issuer)
};

bool is_same_cert(cert_data_t *cert0, cert_data_t *cert1);
bool is_issuing_cert(cert_data_t *subject, cert_data_t *candidate_issuer);
int elaborate_x509_info(cert_data_t *cert);

// vim:fenc=utf-8:tw=75:noet
