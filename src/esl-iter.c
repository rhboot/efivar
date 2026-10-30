// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Copyright Red Hat, Inc.
 * Copyright Peter M. Jones <pjones@redhat.com>
 */

#include "efisec.h"

#include "secdb.h"

typedef struct esl_list_iter esl_list_iter;
extern int esl_list_iter_new(esl_list_iter **iter, uint8_t *buf, size_t len);
extern int esl_list_iter_end(esl_list_iter *iter);
extern int esl_list_iter_next(esl_list_iter *iter,
					    efi_guid_t *type,
					    efi_signature_data_t **data,
					    size_t *len);
extern int esl_list_iter_next_with_size_correction(
					esl_list_iter *iter, efi_guid_t *type,
					efi_signature_data_t **data,
					size_t *len, bool correct_size);
extern int esl_list_list_start(esl_list_iter *iter, void **buf);
extern int esl_list_list_size(esl_list_iter *iter, size_t *bufsz);
extern int esl_list_signature_list_size(esl_list_iter *iter, size_t *sls);
extern int esl_list_header_size(esl_list_iter *iter, size_t *slh);
extern int esl_list_sig_size(esl_list_iter *iter, size_t *ss);
extern int esl_list_get_type(esl_list_iter *iter, efi_guid_t *type);

struct esl_iter {
	esl_list_iter *iter;	// iterator for the buffer this is a member of
	int line;		// number of how many total ESD entries we've iterated

	efi_signature_data_t *esd; // raw sig data, points into iter->buf
	size_t len;		// iter->iter->signature_size

	size_t nmemb;		// (iter->iter->signature_list_size - sizeof(ESL) - iter->iter->signature_header_size)
				// / iter->signature_size
	unsigned int i;		// how many esd members we've parsed in this esd
};

int NONNULL(1, 2)
esl_iter_new(esl_iter **iter, uint8_t *buf, size_t len)
{
	int rc;

	if (len < sizeof(efi_signature_list_t) + sizeof(efi_signature_data_t)) {
		efi_error("buffer is too small for any EFI_SIGNATURE_LIST entries: %zd < %zd",
			  len, sizeof(efi_signature_list_t) + sizeof(efi_signature_data_t));
		errno = EINVAL;
		return -1;
	}

	*iter = calloc(1, sizeof(esl_iter));
	if (!*iter) {
		efi_error("memory allocation failed for %zd bytes", sizeof(esl_iter));
		return -1;
	}

	rc = esl_list_iter_new(&(*iter)->iter, buf, len);
	if (rc < 0) {
		int error = errno;
		free(*iter);
		errno = error;
		efi_error("esl_list_iter_new() failed");
		return -1;
	}

	(*iter)->i = -1;

	return 0;
}

int NONNULL(1)
esl_iter_end(esl_iter *iter)
{
	if (!iter) {
		errno = EINVAL;
		return -1;
	}
	if (iter->iter)
		esl_list_iter_end(iter->iter);
	free(iter);
	return 0;
}

esl_iter_status_t NONNULL(1, 2, 3, 4, 5)
esl_iter_next(esl_iter *iter, efi_guid_t *type, efi_guid_t *owner,
	      uint8_t **data, size_t *len)
{
	esl_iter_status_t status = ESL_ITER_NEW_DATA;
	int rc;
	size_t ss, sls;

	if (!iter) {
		efi_error("iter is NULL");
		errno = EINVAL;
		return -EINVAL;
	}

	if (iter->iter == NULL) {
		efi_error("iter->iter is NULL");
		errno = EINVAL;
		return -EINVAL;
	}

	iter->line += 1;

	iter->i += 1;

	if (iter->i == iter->nmemb) {
		debug("Getting next efi_signature_data_t");
		iter->i = 0;

		rc = esl_list_iter_next(iter->iter, type, &iter->esd, &iter->len);
		if (rc < 1) {
			if (rc < 0)
				efi_error("esl_list_iter_next() failed");
			return rc;
		}
		debug("type:%p data:%p len:%zd", type, iter->esd, iter->len);
		status = ESL_ITER_NEW_LIST;

		if (!efi_guid_cmp(type, &efi_guid_x509_cert)) {
			int32_t asn1size;
			size_t asn1limit;

			if (checked_sub(iter->len, sizeof(iter->esd->signature_owner), &asn1limit)) {
				asn1limit = iter->len;
			}

			asn1size = get_asn1_seq_size(iter->esd->signature_data, asn1limit);
			debug("iter->len:%zu sizeof(owner):%zd bufsz:%zd asn1sz:%d (0x%x)",
			      iter->len, sizeof(iter->esd->signature_owner), asn1limit,
			      asn1size, asn1size);

			if (asn1size < 0) {
				debug("iterator data claims to be an X.509 Cert but is not valid ASN.1 DER");
			} else if ((uint32_t)asn1size != asn1limit) {
				debug("X.509 Cert ASN.1 size does not match signature_list Size (%d vs %zu)",
				      asn1size, asn1limit);
			}
		}

		size_t slh;
		rc = esl_list_header_size(iter->iter, &slh);
		if (rc < 0) {
			efi_error("esl_list_header_size() failed");
			return rc;
		}

		rc = esl_list_sig_size(iter->iter, &ss);
		if (rc < 0) {
			efi_error("esl_list_sig_size() failed");
			return rc;
		}

		rc = esl_list_signature_list_size(iter->iter, &sls);
		if (rc < 0) {
			efi_error("esl_list_list_size() failed");
			return rc;
		}

		debug("list size:%zu header size:%zu data size:%zu", sls, slh, ss);
		/* if we'd have leftover data, then this ESD is garbage. */
		if ((sls - sizeof(efi_signature_list_t) - slh) % ss != 0) {
			efi_error("signature list size is not a multiple of the signature entry size: %zd %% %zd = %zd",
				  (sls - sizeof(efi_signature_list_t) - slh), ss,
				  (sls - sizeof(efi_signature_list_t) - slh) % ss);
			errno = EINVAL;
			return -EINVAL;
		}

		iter->nmemb = (sls - sizeof(efi_signature_list_t) - slh) / ss;
		debug("iter->nmemb:%zd", iter->nmemb);
	} else {
		uint8_t *buf = NULL;
		size_t bufsz;

		debug("Getting next esd element");
		rc = esl_list_sig_size(iter->iter, &ss);
		if (rc < 0) {
			efi_error("esl_list_sig_size() failed");
			return rc;
		}

		rc = esl_list_list_size(iter->iter, &bufsz);
		if (rc < 0) {
			efi_error("esl_list_list_size() failed");
			return rc;
		}

		rc = esl_list_list_start(iter->iter, (void **)&buf);
		if (rc < 0 || buf == NULL) {
			efi_error("esl_list_list_start() failed");
			return rc;
		}

		rc = esl_list_signature_list_size(iter->iter, &sls);
		if (rc < 0) {
			efi_error("esl_list_list_size() failed");
			return rc;
		}

		debug("signature data entry (0x%zx %c 0x%zx) (%zd %c %zd)",
		      esd_get_esl_offset(iter) + ss, LEG(esd_get_esl_offset(iter) + ss, bufsz), bufsz,
		      esd_get_esl_offset(iter) + ss, LEG(esd_get_esl_offset(iter) + ss, bufsz), bufsz);
		if (esd_get_esl_offset(iter) + ss > bufsz) {
			errno = EOVERFLOW;
			debug("EFI_SIGNATURE_LIST is malformed");
			debug("signature data entry is not within list bounds (%zd > %zd) (0x%zx > 0x%zx)",
				  esd_get_esl_offset(iter) + ss, bufsz + esd_get_esl_offset(iter) - ss,
				  (intptr_t)iter->esd + ss - (intptr_t)buf, bufsz);
			efi_error("signature data entry is not within list bounds (%p > %p) (%zd > %zd) (0x%zx > 0x%zx)",
				  (void *)((intptr_t)iter->esd + ss),
				  (void *)((intptr_t)buf + bufsz),
				  (intptr_t)iter->esd + ss - (intptr_t)buf, bufsz,
				  (intptr_t)iter->esd + ss - (intptr_t)buf, bufsz);
			return -1;
		}
		iter->esd = (efi_signature_data_t *)((intptr_t)iter->esd + ss);
	}

	rc = esl_list_get_type(iter->iter, type);
	if (rc < 0) {
		efi_error("esl_list_get_type() failed");
		return rc;
	}

	*owner = iter->esd->signature_owner;
	*data = iter->esd->signature_data;
	*len = ss - sizeof(iter->esd->signature_owner);
	return status;
}

int NONNULL(1)
esl_iter_get_line(esl_iter *iter)
{
	if (!iter) {
		errno = EINVAL;
		return -1;
	}

	return iter->line;
}

struct esl_list_iter {
	uint8_t *buf;	// the allocated buffer
	size_t len;	// size of the buffer

	off_t offset;	// where we're currently parsing

	efi_signature_list_t *esl;	// raw data (==buf)
};

intptr_t NONNULL(1)
esd_get_esl_offset(esl_iter *iter)
{
	uint64_t esd = (uintptr_t)iter->esd;
	uint64_t esl = (uintptr_t)iter->iter->buf;

	return esd - esl;
}

int NONNULL(1, 2)
esl_list_iter_new(esl_list_iter **iter, uint8_t *buf, size_t len)
{
	debug("starting new iter list");
	if (len < sizeof(efi_signature_list_t) + sizeof(efi_signature_data_t)) {
		errno = EINVAL;
		return -1;
	}

	*iter = calloc(1, sizeof(esl_list_iter));
	if (!*iter)
		return -1;

	(*iter)->buf = buf;
	(*iter)->len = len;

	return 0;
}

int NONNULL(1)
esl_list_iter_end(esl_list_iter *iter)
{
	if (!iter) {
		errno = EINVAL;
		return -1;
	}
	free(iter);
	return 0;
}

static int
check_esl_list_sanity(esl_list_iter *iter, size_t esd_len)
{
	size_t remaining = 0;
	size_t signature_data_size = 0;
	efi_guid_t type;

	remaining = iter->len - iter->offset;
	debug("list has %zu bytes left", remaining);
	if (remaining == 0)
		return 0;

	if (remaining < sizeof(efi_signature_list_t) + sizeof(efi_signature_data_t)) {
		debug("remaining %zu is too small for an EFI_SIGNATURE_LIST", remaining);
inval:
		debug("EFI_SIGNATURE_LIST is malformed.");
		errno = EINVAL;
		efi_error("EFI_SIGNATURE_LIST is malformed");
		return -1;
	}

	debug("list element is %"PRIu32"(0x%"PRIx32") bytes",
	      iter->esl->signature_list_size,
	      iter->esl->signature_list_size);

	if (checked_sub(iter->len, iter->offset, &remaining)) {
		debug("iter->offset %zd > iter->len %zu",
		      iter->offset, iter->len);
		debug("internal error");
		errno = EINVAL;
		efi_error("internal error");
		return -1;
	}

	if (iter->esl->signature_list_size == 0) {
		debug("signature_list_size == 0");
		goto inval;
	}

	if (iter->esl->signature_size == 0) {
		debug("signature_size == 0");
		goto inval;
	}

	if (iter->esl->signature_header_size != 0) {
		debug("signature_header_size == 0.  Suspicious.");
		if (iter->esl->signature_header_size >= iter->esl->signature_list_size) {
			debug("signature_header_size %"PRIu32" > signature_list_size %"PRIu32,
			      iter->esl->signature_header_size,
			      iter->esl->signature_list_size);
			goto inval;
		}
	}

	if (remaining < iter->esl->signature_list_size) {
		debug("remaining:%zu(0x%zx) signature_list_size:%"PRIu32"(0x%"PRIx32")",
		      remaining, remaining,
		      iter->esl->signature_list_size, iter->esl->signature_list_size);
		goto inval;
	}

	if (remaining < iter->esl->signature_header_size) {
		debug("remaining:%zu(0x%zx) signature_header_size:%"PRIu32"(0x%"PRIx32")",
		      remaining, remaining,
		      iter->esl->signature_header_size, iter->esl->signature_header_size);
		goto inval;
	}

	if (remaining < iter->esl->signature_size) {
		debug("remaining:%zu(0x%zx) signature_size:%"PRIu32"(0x%"PRIx32")",
		      remaining, remaining,
		      iter->esl->signature_size, iter->esl->signature_size);
		goto inval;
	}

	if (checked_sub(iter->esl->signature_list_size, iter->esl->signature_header_size,
			&signature_data_size) ||
	    checked_sub(signature_data_size, sizeof(efi_signature_list_t), &signature_data_size)) {
		debug("signature_list_size %zd(0x%zx) is smaller than signature_header_size %zd(0x%zx)",
		      iter->esl->signature_list_size, iter->esl->signature_list_size,
		      iter->esl->signature_header_size, iter->esl->signature_header_size);
		goto inval;
	}

	if (signature_data_size == 0) {
		debug("signature_data_size == 0");
		goto inval;
	}

	if (signature_data_size % iter->esl->signature_size != 0) {
		debug("signature_data_size %zd(0x%zx) is not a multiple of signature_size %zd(0x%zx)",
		      signature_data_size, signature_data_size,
		      iter->esl->signature_size, iter->esl->signature_size);
		goto inval;
	}

	esl_list_get_type(iter, &type);

	if (!efi_guid_cmp(&type, &efi_guid_x509_cert)) {
		int32_t asn1size;

		asn1size = get_asn1_seq_size(iter->buf + iter->offset
					     + sizeof(efi_signature_list_t)
					     + sizeof(efi_guid_t),
					     esd_len - sizeof(efi_guid_t));
		if (asn1size < 0) {
			debug("iterator data claims to be an X.509 Cert but is not valid ASN.1 DER");
		} else if ((uint32_t)asn1size != iter->esl->signature_size
						 - sizeof(efi_guid_t)) {
			debug("X.509 Cert ASN.1 size does not match signature_list_size (%d vs %zu)",
			      asn1size, iter->esl->signature_size -
					sizeof(efi_guid_t));
		}
	} else {
		ssize_t typesz = 0;

		typesz = secdb_entry_size_from_guid(&type);
		if (typesz < 0) {
			debug("unknown signature algorithm");
			goto inval;
		}

		if (iter->esl->signature_size != typesz) {
			debug("signature_size %zu != typesz",
			      iter->esl->signature_size, typesz);
			goto inval;
		}
	}

	return 0;
}

int NONNULL(1, 2, 3, 4)
esl_list_iter_next(esl_list_iter *iter, efi_guid_t *type,
		   efi_signature_data_t **data, size_t *len)
{
	if (!iter) {
		efi_error("iter is NULL");
		errno = EINVAL;
		return -1;
	}
	if (iter->offset < 0) {
		efi_error("iter->offset (%jd) < 0", (intmax_t)iter->offset);
		errno = EINVAL;
		return -1;
	}
	if ((uint32_t)iter->offset >= iter->len) {
		efi_error("iter->offset (%jd) >= iter->len (%zd)",
			  (intmax_t)iter->offset, iter->len);
		errno = EINVAL;
		return -1;
	}
	size_t remaining = 0;

	if (checked_sub(iter->len, iter->offset, &remaining)) {
		debug("iter->offset %zd > iter->len %zu",
		      iter->offset, iter->len);
		debug("internal error");
		efi_error("internal error");
		errno = EINVAL;
		return -1;
	}

	int rc = 0;
	if (!iter->esl) {
		debug("Getting next ESL buffer");

		iter->esl = (efi_signature_list_t *)iter->buf;
	} else {
		debug("Getting next efi_signature_list_t");
		debug("list has %zu bytes left, element is %"PRIu32" bytes",
		      remaining, iter->esl->signature_list_size);

		iter->offset += iter->esl->signature_list_size;
		if ((uint32_t)iter->offset >= iter->len) {
			debug("iter->offset %zd (0x%zx) >= iter->len %zd (0x%zx)",
			      iter->offset, iter->offset, iter->len, iter->len);
			return 0;
		}
		iter->esl = (efi_signature_list_t *)(iter->buf + (size_t)iter->offset);
	}

	rc = check_esl_list_sanity(iter, *len);
	if (rc < 0) {
		debug("esl sanity check failed");
		return rc;
	}

	efi_signature_list_t esl;
	memset(&esl, '\0', sizeof(esl));
	/* if somehow we've gotten a buffer that's bigger than our
	 * real list, this will be zeros, so we've hit the end. */
	if (!memcmp(&esl, iter->esl, sizeof(esl)))
		return 0;

	debug("signature list size:%d iter->len:%zu iter->offset:%jd signature_size:%u",
	      iter->esl->signature_list_size, iter->len, (intmax_t)iter->offset,
	      iter->esl->signature_size);

	size_t header_sz = sizeof(efi_signature_list_t)
			   + iter->esl->signature_header_size;
	debug("sizeof(esl):%zd shs:%u hdrsz:%zd", sizeof(efi_signature_list_t),
	      iter->esl->signature_header_size, header_sz);
	*type = iter->esl->signature_type;

	*data = (efi_signature_data_t *)((uint8_t *)iter->esl + header_sz);
	*len = iter->esl->signature_size;

	return 1;
}

int NONNULL(1, 2)
esl_list_list_start(esl_list_iter *iter, void **buf)
{
	if (!iter || !iter->esl || !buf) {
		errno = EINVAL;
		return -1;
	}

	*buf = iter->buf;
	return 0;
}

int NONNULL(1, 2)
esl_list_list_size(esl_list_iter *iter, size_t *bufsz)
{
	if (!iter) {
		errno = EINVAL;
		return -1;
	}

	*bufsz = iter->len;
	return 0;
}


int NONNULL(1, 2)
esl_list_signature_list_size(esl_list_iter *iter, size_t *sls)
{
	if (!iter || !iter->esl) {
		errno = EINVAL;
		return -1;
	}
	/* this has to be at least as large as its header to be valid */
	if (iter->esl->signature_list_size < sizeof(efi_signature_list_t)) {
		errno = EINVAL;
		return -1;
	}

	*sls = iter->esl->signature_list_size;
	return 0;
}

int NONNULL(1, 2)
esl_list_header_size(esl_list_iter *iter, size_t *slh)
{
	if (!iter || !iter->esl) {
		errno = EINVAL;
		return -1;
	}

	*slh = iter->esl->signature_header_size;
	return 0;
}

int NONNULL(1, 2)
esl_list_sig_size(esl_list_iter *iter, size_t *ss)
{
	if (!iter || !iter->esl) {
		errno = EINVAL;
		return -1;
	}
	/* If signature size isn't positive, there's invalid data. */
	if (iter->esl->signature_size < 1) {
		errno = EINVAL;
		return -1;
	}

	*ss = iter->esl->signature_size;
	return 0;
}

int NONNULL(1, 2)
esl_list_get_type(esl_list_iter *iter, efi_guid_t *type)
{
	if (!iter || !iter->esl) {
		errno = EINVAL;
		return -1;
	}

	memcpy(type, &iter->esl->signature_type, sizeof(*type));
	return 0;
}
