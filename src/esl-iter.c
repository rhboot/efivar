// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Copyright Red Hat, Inc.
 * Copyright Peter M. Jones <pjones@redhat.com>
 */

#include "efisec.h"

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
	esl_list_iter *iter;
	int line;

	efi_signature_data_t *esd;
	size_t len;

	size_t nmemb;
	unsigned int i;
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
esl_iter_next_with_size_correction(esl_iter *iter, efi_guid_t *type,
				   efi_guid_t *owner, uint8_t **data,
				   size_t *len, bool correct_size)
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
		debug("Getting next efi_signature_data_t (correct_size:%d)", correct_size);
		iter->i = 0;
		if (correct_size)
			rc = esl_list_iter_next_with_size_correction(iter->iter, type, &iter->esd, &iter->len, true);
		else
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

			asn1size = get_asn1_seq_size(iter->esd->signature_data,
				iter->len - sizeof(iter->esd->signature_owner));
			debug("iter->len:%zu sizeof(owner):%zd bufsz:%zd asn1sz:%d",
			      iter->len, sizeof(iter->esd->signature_owner),
			      iter->len - sizeof(iter->esd->signature_owner), asn1size);

			if (asn1size < 0) {
				debug("iterator data claims to be an X.509 Cert but is not valid ASN.1 DER");
			} else if ((uint32_t)asn1size != iter->len -
					sizeof(iter->esd->signature_owner)) {
				debug("X.509 Cert ASN.1 size does not match signature_list Size (%d vs %zu)",
				      asn1size, iter->len -
					sizeof(iter->esd->signature_owner));
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
			if (!correct_size)
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

esl_iter_status_t NONNULL(1, 2, 3, 4, 5)
esl_iter_next(esl_iter *iter, efi_guid_t *type,
                         efi_guid_t *owner, uint8_t **data, size_t *len)
{
	return esl_iter_next_with_size_correction(iter, type, owner, data, len, false);
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
	uint8_t *buf;
	size_t len;

	off_t offset;

	efi_signature_list_t *esl;
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

int NONNULL(1, 2, 3, 4)
esl_list_iter_next_with_size_correction(esl_list_iter *iter, efi_guid_t *type,
					efi_signature_data_t **data,
					size_t *len, bool correct_size)
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

	if (!iter->esl) {
		debug("Getting next ESL buffer (correct_size:%d)", correct_size);
		iter->esl = (efi_signature_list_t *)iter->buf;

		debug("list has %lu bytes left, element is %"PRIu32"(0x%"PRIx32") bytes",
		      iter->len - iter->offset,
		      iter->esl->signature_list_size,
		      iter->esl->signature_list_size);

		if (iter->len - iter->offset < iter->esl->signature_list_size) {
			debug("EFI_SIGNATURE_LIST is malformed: len:%zd(0x%zx) offset:%zd(0x%zx) len-off:%zd(0x%zx) esl_size:%"PRIu32"(0x%"PRIx32")",
			      iter->len, iter->len,
			      iter->offset, iter->offset,
			      iter->len - iter->offset, iter->len - iter->offset,
			      iter->esl->signature_list_size, iter->esl->signature_list_size);
			if (correct_size && (iter->len - iter->offset) > 0) {
				warnx("correcting ESL size from %d to %jd at %lx",
				      iter->esl->signature_list_size,
				      (intmax_t)(iter->len - iter->offset), (unsigned long)iter->offset);
				debug("correcting ESL size from %d to %zd at %lx",
				      iter->esl->signature_list_size,
				      iter->len - iter->offset, iter->offset);
				iter->esl->signature_list_size = iter->len - iter->offset;
			} else {
				efi_error("EFI_SIGNATURE_LIST is malformed");
				errno = EOVERFLOW;
				return -1;
			}
		}

	} else {
		debug("Getting next efi_signature_list_t");
		debug("list has %lu bytes left, element is %"PRIu32" bytes",
		      iter->len - iter->offset,
		      iter->esl->signature_list_size);
		efi_guid_t type;
		errno = 0;
		esl_list_get_type(iter, &type);
		if (iter->len - iter->offset < iter->esl->signature_list_size) {
			debug("EFI_SIGNATURE_LIST is malformed");
			if (correct_size && (iter->len - iter->offset) > 0) {
				warnx("correcting ESL size from %d to %jd at 0x%lx",
				      iter->esl->signature_list_size,
				      (intmax_t)(iter->len - iter->offset), (unsigned long)iter->offset);
				debug("correcting ESL size from %d to %zd at 0x%lx",
				      iter->esl->signature_list_size,
				      iter->len - iter->offset, iter->offset);
				iter->esl->signature_list_size = iter->len - iter->offset;
			} else {
				debug("EFI_SIGNATURE_LIST is malformed");
				efi_error("EFI_SIGNATURE_LIST is malformed");
				errno = EOVERFLOW;
				return -1;
			}
		}
		if (!efi_guid_cmp(&type, &efi_guid_x509_cert)) {
			int32_t asn1size;

			asn1size = get_asn1_seq_size(
				iter->buf + iter->offset + sizeof(efi_guid_t),
				*len - sizeof(efi_guid_t));
			if (asn1size < 0) {
				debug("iterator data claims to be an X.509 Cert but is not valid ASN.1 DER");
			} else if ((uint32_t)asn1size != iter->esl->signature_size
							 - sizeof(efi_guid_t)) {
				debug("X.509 Cert ASN.1 size does not match signature_list_size (%d vs %zu)",
				      asn1size, iter->esl->signature_size -
						sizeof(efi_guid_t));
			}
		}

		iter->offset += iter->esl->signature_list_size;
		if ((uint32_t)iter->offset >= iter->len)
			return 0;
		iter->esl = (efi_signature_list_t *)((intptr_t)iter->buf
						+ (unsigned long)iter->offset);
	}

	efi_signature_list_t esl;
	memset(&esl, '\0', sizeof(esl));
	/* if somehow we've gotten a buffer that's bigger than our
	 * real list, this will be zeros, so we've hit the end. */
	if (!memcmp(&esl, iter->esl, sizeof(esl)))
		return 0;

	debug("signature list size:%d iter->len:%zd iter->offset:%zd signature_size:%u",
	      iter->esl->signature_list_size, iter->len, iter->offset,
	      iter->esl->signature_size);
	/* if this list size is too big for our data, then it's malformed */
	if (iter->esl->signature_list_size > iter->len - iter->offset) {
		debug("EFI_SIGNATURE_LIST is malformed");
		if (correct_size && (iter->len - iter->offset) > 0) {
			warnx("correcting ESL size from %d to %jd at 0x%lx",
			      iter->esl->signature_list_size,
			      (intmax_t)(iter->len - iter->offset), (unsigned long)iter->offset);
			debug("correcting ESL size from %d to %zd at 0x%lx",
			      iter->esl->signature_list_size,
			      iter->len - iter->offset, iter->offset);
			iter->esl->signature_list_size = iter->len - iter->offset;
		} else {
			efi_error("EFI_SIGNATURE_LIST is malformed");
			errno = EOVERFLOW;
			return -1;
		}
	}

	size_t header_sz = sizeof(efi_signature_list_t)
			   + iter->esl->signature_header_size;
	debug("sizeof(esl):%zd shs:%u hdrsz:%zd", sizeof(efi_signature_list_t),
	      iter->esl->signature_header_size, header_sz);
	*type = iter->esl->signature_type;

	*data = (efi_signature_data_t *)((uint8_t *)iter->esl + header_sz);
	*len = iter->esl->signature_size;

	return 1;
}

int NONNULL(1, 2, 3, 4)
esl_list_iter_next(esl_list_iter *iter, efi_guid_t *type,
		   efi_signature_data_t **data, size_t *len)
{
	return esl_list_iter_next_with_size_correction(iter, type, data, len, false);
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
