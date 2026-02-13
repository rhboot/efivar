// SPDX-License-Identifier: GPL-v3-or-later
/*
 * sbchooser-x509.c - x509 handling
 * Copyright Peter Jones <pjones@redhat.com>
 */

#include "sbchooser.h" // IWYU pragma: keep

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
