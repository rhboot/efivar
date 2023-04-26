// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Copyright Red Hat, Inc.
 * Copyright Peter M. Jones <pjones@redhat.com>
 *
 * Author(s): Peter Jones <pjones@redhat.com>
 */
#ifndef PRIVATE_ESL_ITER_H_
#define PRIVATE_ESL_ITER_H_ 1

#include "efisec.h"

typedef struct esl_iter esl_iter;

/*
 * esl_iter_new - create a new iterator over a efi security database
 * iter: pointer to a NULL esl_iter pointer.
 * buf: security database from the file
 * len: size of the file
 *
 * returns 0 on success, negative on error, sets errno.
 */
extern int esl_iter_new(esl_iter **iter, uint8_t *buf, size_t len)
        __attribute__((__nonnull__(1, 2)));

/*
 * esl_iter_end - destroy the iterator created by esl_iter_new()
 * iter: the iterator being destroyed
 *
 * returns 0 on success, negative on error, sets errno.
 */
extern int esl_iter_end(esl_iter *iter)
        __attribute__((__nonnull__(1)));

typedef enum esl_iter_status {
	ESL_ITER_ERROR = -1,
	ESL_ITER_DONE = 0,
	ESL_ITER_NEW_DATA = 1,
	ESL_ITER_NEW_LIST = 2,
} esl_iter_status_t;

/*
 * esl_iter_next - get the next item in the list
 * iter: the iterator
 * type: the type of the entry
 * owner: the owner of the entry
 * data: the identifying data
 * len: the size of the data
 *
 * returns negative and sets errno on error,
 * ESL_ITER_ERROR (-1) on error
 * ESL_ITER_DONE if there weren't any entries (type/owner/data/len are not populated)
 * ESL_ITER_NEW_DATA if an entry was returned
 * ESL_ITER_NEW_LIST if an entry was returned and from a new efi_signature_list
 */
extern esl_iter_status_t esl_iter_next(esl_iter *iter, efi_guid_t *type,
			   efi_guid_t *owner, uint8_t **data, size_t *len)
        __attribute__((__nonnull__(1, 2, 3, 4, 5)));


extern esl_iter_status_t esl_iter_next_with_size_correction(esl_iter *iter, efi_guid_t *type,
					      efi_guid_t *owner, uint8_t **data,
					      size_t *len, bool correct_size)
        __attribute__((__nonnull__(1, 2, 3, 4, 5)));

/*
 * esl_iter_get_line - tell how many entries have been returned
 * iter: the iterator
 *
 * return value: -1 on error, with errno set, >=0 in all other cases
 */
extern int esl_iter_get_line(esl_iter *iter)
        __attribute__((__nonnull__(1)));

/*
 * get the address of the current esd in this esl buffer
 */
intptr_t
esd_get_esl_offset(esl_iter *iter)
	__attribute__((__nonnull__(1)));

#endif /* PRIVATE_ESL_ITER_H_ */
