// SPDX-License-Identifier: GPL-v3-or-later
/*
 * sbchooser-x509.c - x509 handling
 * Copyright Peter Jones <pjones@redhat.com>
 */

#include "sbchooser.h" // IWYU pragma: keep

void
fmt_time(const ASN1_TIME *asn1, char buf[1024])
{
	struct tm tm;

	memset(buf, 0, 1024);
	if (!asn1) {
		strcpy(buf, "(null)");
		return;
	}
	ASN1_TIME_to_tm(asn1, &tm);
	asctime_r(&tm, buf);

	buf[1023] = '\0';
	for (size_t i = 0; i < 1024; i++) {
		if (buf[i] == '\r' || buf[i] == 'n' || !isprint(buf[i]))
			buf[i] = '\0';
	}
}

int
time_cmp(const ASN1_TIME *t0, const ASN1_TIME *t1)
{
	int rc = 0;
	char str0[1024];
	char str1[1024];

	memset(str0, 0, 1024);
	memset(str1, 0, 1024);
	fmt_time(t0, str0);
	fmt_time(t1, str1);

	if (t0 && t1)
		rc = ASN1_TIME_compare(t0, t1);
	if (t0 && !t1)
		rc = -1;
	if (t1 && !t0)
		rc = 1;
	debug("comparing \"%s\" to \"%s\": %d", str0, str1, rc);
	return rc;
}

/*
 * note that none of this checks any /cryptographic/ properties.  If you've
 * got two certs with the same issuer, and serial, we'll believe they're
 * the same, even if one of them is "fake" and has a pubkey that won't
 * verify signatures from the other one.
 */
bool
is_same_cert(cert_data_t *cert0, cert_data_t *cert1)
{
	int rc;
	char buf0[4096], buf1[4096];

	memset(buf0, 0, 4096);
	memset(buf1, 0, 4096);

	X509_NAME_oneline(cert0->issuer, buf0, 4095);
	X509_NAME_oneline(cert1->issuer, buf1, 4095);

	rc = X509_NAME_cmp(cert0->issuer, cert1->issuer);
	debug("  comparing issuers for \"%s\" and \"%s\": %d", buf0, buf1, rc);
	if (rc != 0)
		return false;

	uint64_t a, b;

	ASN1_INTEGER_get_uint64(&a, cert0->serial);
	ASN1_INTEGER_get_uint64(&b, cert1->serial);
	rc = ASN1_INTEGER_cmp(cert0->serial, cert1->serial);
	debug("  serial cmp(0x%"PRIx64",0x%"PRIx64"):%d", a, b, rc);
	if (!rc)
		return false;

	return true;
}

bool
is_issuing_cert(cert_data_t *subject, cert_data_t *candidate_issuer)
{
	int rc;
	char buf0[4096], buf1[4096];

	memset(buf0, 0, 4096);
	memset(buf1, 0, 4096);

	X509_NAME_oneline(subject->issuer, buf0, 4095);
	X509_NAME_oneline(candidate_issuer->subject, buf1, 4095);

	rc = X509_NAME_cmp(subject->issuer, candidate_issuer->subject);
	debug("  comparing issuers for \"%s\" and \"%s\": %d", buf0, buf1, rc);
	if (rc == 0)
		return true;
	return false;
}

void
free_cert(cert_data_t *cert)
{
	if (!cert)
		return;

	if (cert->free_x509 && cert->x509) {
		X509_free(cert->x509);
		cert->x509 = NULL;
	}

	if (cert->rationale) {
		free(cert->rationale);
		cert->rationale = NULL;
	}

	free(cert);
}

int
elaborate_x509_info(cert_data_t *cert)
{
	int rc;
	const char *mdsn = NULL;
	const char *pksn = NULL;

	cert->subject = X509_get_subject_name(cert->x509);
	if (!cert->subject) {
		warnx("couldn't get cert subject");
		goto err;
	}

	cert->issuer = X509_get_issuer_name(cert->x509);
	if (!cert->issuer) {
		warnx("couldn't get cert issuer");
		goto err;
	}

	cert->serial = X509_get_serialNumber(cert->x509);
	if (cert->serial == NULL) {
		warnx("couldn't get cert serial");
		goto err;
	}

	cert->not_before = X509_get0_notBefore(cert->x509);
	if (cert->not_before == NULL) {
		warnx("couldn't get not_before");
		goto err;
	}

	cert->not_after = X509_get0_notAfter(cert->x509);
	if (cert->not_after == NULL) {
		warnx("couldn't get not_after");
		goto err;
	}

	rc = X509_get_signature_info(cert->x509, &cert->md_nid, &cert->pk_nid,
				     NULL, NULL);
	if (rc != 1) {
		warnx("couldn't get signature info");
		goto err;
	}

	debug("md_nid:%d pk_nid:%d secbits:%d",
	      cert->md_nid, cert->pk_nid, cert->md_secbits);
	mdsn = OBJ_nid2sn(cert->md_nid);
	pksn = OBJ_nid2ln(cert->pk_nid);

	/*
	 * We do not believe in OpenSSL's concept of md secbits; we believe
	 * in NIST SP 800-57 Part 1 Rev. 5 page 69's.
	 */
	if (!strcmp(mdsn, "SHA512")) {
		cert->md_secbits = 256;
	} else if (!strcmp(mdsn, "SHA384")) {
		cert->md_secbits = 192;
	} else if (!strcmp(mdsn, "SHA256")) {
		cert->md_secbits = 128;
	} else if (!strcmp(mdsn, "SHA224")) {
		cert->md_secbits = 112;
	} else if (!strcmp(mdsn, "SHA1")) {
		cert->md_secbits = 80;
	}

	debug("md:%s md_secbits:%d", mdsn, cert->md_secbits);

	/*
	 * XXX:PJFIX PQC isn't implemented here yet
	 */
	while (true) {
		if (!strcmp(pksn, "rsaEncryption")) {
			X509_PUBKEY *xpk;
			ASN1_OBJECT *ppkalg = NULL;
			const unsigned char *pk = NULL;
			int pklen = 0;
			X509_ALGOR *pa = NULL;
			int rc = 0;

			xpk = X509_get_X509_PUBKEY(cert->x509);
			if (!xpk) {
				debug("This certificate has no public key.  Not scoring.");
				break;
			}

			rc = X509_PUBKEY_get0_param(&ppkalg, &pk, &pklen, &pa, xpk);
			if (!rc) {
				debug("This public key has no algorithm parameters.  Not scoring.");
				break;
			}
			/*
			 * PK here is the ASN1 encoded modulus and
			 * exponent, so pklen (in bytes) includes a few
			 * extra bytes here and there, but these values are
			 * far enough apart it shouldn't matter...
			 */
			pklen *= 8;
			if (pklen >= 15360) {
				cert->pk_secbits = 256;
			} else if (pklen >= 7690) {
				cert->pk_secbits = 192;
			} else if (pklen >= 3072) {
				cert->pk_secbits = 128;
			} else if (pklen >= 2048) {
				cert->pk_secbits = 112;
			} else if (pklen >= 1024) {
				cert->pk_secbits = 80;
			}
		}
		break;
	}
	debug("pk:%s pk_secbits:%d", pksn, cert->pk_secbits);

	return 0;
err:
	X509_free(cert->x509);
	cert->x509 = NULL;
	return -1;
}

int
cert_sec_cmp(cert_data_t *cert0, cert_data_t *cert1)
{
	int cert0_secbits = cert0->md_secbits < cert0->pk_secbits ? cert0->md_secbits : cert0->pk_secbits;
	int cert1_secbits = cert1->md_secbits < cert1->pk_secbits ? cert1->md_secbits : cert1->pk_secbits;

	return cert1_secbits - cert0_secbits;
}

// vim:fenc=utf-8:tw=75:noet
