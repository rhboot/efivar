// SPDX-License-Identifier: GPL-v3-or-later
/*
 * sbchooser-x509.c - x509 handling
 * Copyright Peter Jones <pjones@redhat.com>
 */

#include "sbchooser.h" // IWYU pragma: keep

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
	rc = X509_NAME_cmp(cert0->issuer, cert1->issuer);
	debug("  name cmp: %d", rc);
	if (rc != 0)
		return false;

	rc = ASN1_INTEGER_cmp(cert0->serial, cert1->serial);
	debug("  serial cmp: %d", rc);
	if (!rc)
		return false;

	return true;
}

bool
is_issuing_cert(cert_data_t *subject, cert_data_t *candidate_issuer)
{
	int rc;

	rc = X509_NAME_cmp(subject->issuer, candidate_issuer->subject);
	debug("  name cmp: %d", rc);
	if (rc == 0)
		return true;
	return false;
}

int
elaborate_x509_info(cert_data_t *cert)
{
	int rc;

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

	rc = X509_get_signature_info(cert->x509, &cert->mdnid, &cert->pknid,
				     &cert->secbits, NULL);
	if (rc != 1) {
		warnx("couldn't get signature info");
		goto err;
	}

	debug("mdnid:%d pknid:%d secbits:%d",
	      cert->mdnid, cert->pknid, cert->secbits);
	debug("md:%s pk:%s secbits:%d",
	      OBJ_nid2sn(cert->mdnid), OBJ_nid2sn(cert->pknid),
	      cert->secbits);

	return 0;
err:
	X509_free(cert->x509);
	cert->x509 = NULL;
	return -1;
}

// vim:fenc=utf-8:tw=75:noet
