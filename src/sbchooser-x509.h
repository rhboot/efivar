// SPDX-License-Identifier: GPL-v3-or-later
/*
 * sbchooser-x509.h - x509 handling
 * Copyright Peter Jones <pjones@redhat.com>
 */

#pragma once

#include "sbchooser.h" // IWYU pragma: keep

struct cert_data {
	bool free_x509;
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
	int md_nid;	// nid for the message digest algorithm
	int md_secbits;	// security strength of the digest
	int pk_nid;	// nid for the public key algorithm
	int pk_secbits;	// security strenght of the pubkey

	/*********
	 * stuff below here is only for certs on a signature, not for certs
	 * in db/dbx
	 *********/
	/*
	 * Are all the certs in this x509 signature trusted by db?  Are any
	 * revoked?
	 */
	bool trusted;
	cert_data_t *trust_anchor_cert; // cert it's trusted by (self or
					// issuer)

	bool revoked;
	cert_data_t *revoked_cert;	// the cert that's actually in dbx

	char *rationale;		// why was this revoked or trusted
};

void free_cert(cert_data_t *cert);

int elaborate_x509_info(cert_data_t *cert);

/*
 * Returns true if these certs have the same issuer and serial number.
 * There's no cryptography here.
 */
bool is_same_cert(cert_data_t *cert0, cert_data_t *cert1);

/*
 * returns true if the subject issuer name matches the candidate issuer
 * name.  There's no cryptography here.
 */
bool is_issuing_cert(cert_data_t *subject, cert_data_t *candidate_issuer);

/*
 * Format an ASN1_TIME * into the buffer
 */
void fmt_time(const ASN1_TIME *asn1, char buf[1024]);

/*
 * Compare ASN1_TIMEs.
 * return values:
 * < 0: t0 is before t1
 *   0: t0 is the same time as t1
 * > 0: t0 is after t1
 */
int time_cmp(const ASN1_TIME *t0, const ASN1_TIME *t1);

/*
 * Compare the security strength of two certs.
 * return values:
 * < 0: cert0 has higher security strength than cert1
 *   0: they're the same
 * > 0: cert1 has higher security strength than cert0
 */
int cert_sec_cmp(cert_data_t *cert0, cert_data_t *cert1);

// vim:fenc=utf-8:tw=75:noet
