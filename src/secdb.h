// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * secdb.h - management of EFI security databases
 * Copyright Peter Jones <pjones@redhat.com>
 * Copyright Red Hat, Inc.
 */
#ifndef PRIVATE_SECDB_H
#define PRIVATE_SECDB_H 1

#include "efisec.h"
#include <efivar/efisec.h>

typedef enum {
	BAD,
	HASH,
	SIGNATURE,
	CERTIFICATE_HASH,
	CERTIFICATE,
} secdb_entry_class_t;

typedef struct {
	const secdb_entry_class_t class;
	const char * const name;
	const efi_guid_t *guid;
	const size_t header_size;
	const bool has_owner;
	const size_t size;
} secdb_alg_t;

struct secdb_entry {
	list_t list;
	efi_guid_t owner;
	efi_secdb_data_t data;
};
typedef struct secdb_entry secdb_entry_t;

/*****************************************************************************
 * our internal representation.  Each entry in secdb represents one distinct *
 * {owner, algorithm, size}; i.e. all efi_guid_x509_sha512 with the same     *
 * owner can go on the same entry, but each efi_guid_x509_cert of a          *
 * different size needs its own                                              *
 *****************************************************************************/
struct efi_secdb {
	list_t list;			// link to our next signature sublist

	uint64_t flags;			// bitmask of boolean flags
	efi_secdb_type_t algorithm;	// signature type
	uint32_t listsz;		// esl_size + (hdrsz + nsigs) * sigsz
	uint32_t hdrsz;			// total size of header
	uint32_t sigsz;			// size of each signature
	size_t nsigs;			// number of signatures
	void *header;			// unused
	list_t entries;			// list of signature data entries
};

#define for_each_secdb(pos, head) list_for_each(pos, head)
#define for_each_secdb_safe(pos, n, head) list_for_each_safe(pos, n, head)
#define for_each_secdb_prev(pos, head) list_for_each_prev(pos, head)
#define for_each_secdb_entry(pos, head) list_for_each(pos, head)
#define for_each_secdb_entry_safe(pos, n, head) list_for_each_safe(pos, n, head)

extern const secdb_alg_t PUBLIC efi_secdb_algs_[MAX_SECDB_TYPE];

/*********************************************************
 * some helpers to look up sizes for each algorithm type *
 *********************************************************/

/*
 * does data (and datasz) include an owner guid?
 */
static inline int
secdb_entry_has_owner_from_guid(efi_guid_t *alg_guid, bool *answer)
{
	for (efi_secdb_type_t i = 0; i < MAX_SECDB_TYPE; i++) {
		if (!memcmp(alg_guid, efi_secdb_algs_[i].guid, sizeof(*alg_guid))) {
			*answer = efi_secdb_algs_[i].has_owner;
			return 0;
		}
	}
	errno = EINVAL;
	return -1;
}

/*
 * does data (and datasz) include an owner guid?
 */
static inline int
secdb_entry_has_owner_from_type(efi_secdb_type_t secdb_type, bool *answer)
{
	if (secdb_type < 0 || secdb_type >= MAX_SECDB_TYPE) {
		errno = EINVAL;
		return -1;
	}
	*answer = efi_secdb_algs_[secdb_type].has_owner;
	return 0;
}

/*
 * get the secdb_type from the efi_guid for the algorithm
 */
static inline efi_secdb_type_t
secdb_entry_type_from_guid(const efi_guid_t * const guid)
{
	for (efi_secdb_type_t i = 0; i < MAX_SECDB_TYPE; i++) {
		if (!memcmp(guid, efi_secdb_algs_[i].guid, sizeof(*guid)))
			return i;
	}
	return -1;
}

/*
 * lookup the efi guid for a signature type from the efi_secdb_type_t type
 */
static inline efi_guid_t const *
secdb_guid_from_type(const efi_secdb_type_t secdb_type)
{
	if (secdb_type < 0 || secdb_type >= MAX_SECDB_TYPE) {
		errno = EINVAL;
		return NULL;
	}
	return efi_secdb_algs_[secdb_type].guid;
}

/*
 * find the size to store a signature from the efi guid for the signature type
 */
static inline size_t
secdb_entry_size_from_guid(const efi_guid_t * const alg_guid)
{
	efi_secdb_type_t type;

	type = secdb_entry_type_from_guid(alg_guid);
	if (type < 0)
		return type;

	return efi_secdb_algs_[type].size
	       + (efi_secdb_algs_[type].has_owner ? sizeof(efi_guid_t) : 0);
}

/*
 * find the size to store a signature from the secdb_type_t type, including
 * signature owner.  It's the size of the entire esl.Signatures array entry.
 */
static inline size_t
secdb_entry_size_from_type(const efi_secdb_type_t secdb_type)
{
	if (secdb_type < 0 || secdb_type >= MAX_SECDB_TYPE) {
		errno = EINVAL;
		return -1;
	}

	return efi_secdb_algs_[secdb_type].size
	       + (efi_secdb_algs_[secdb_type].has_owner ? sizeof(efi_guid_t) : 0);
}

/*
 * find the size of the esl header... i.e. always 0 but complicated
 */
static inline int32_t
secdb_header_size_from_type(const efi_secdb_type_t secdb_type)
{
	if (secdb_type < 0 || secdb_type >= MAX_SECDB_TYPE) {
		errno = EINVAL;
		return -1;
	}
	return efi_secdb_algs_[secdb_type].header_size;
}

/*
 * calculate secdb->listsz
 * returns 0 for lists with no signatures
 */
static inline size_t
secdb_entry_size(efi_secdb_t *secdb)
{
	size_t sz;

	if (!secdb || secdb->nsigs == 0)
		return 0;

	sz = sizeof(efi_signature_list_t)
	     + secdb->hdrsz
	     + secdb->sigsz * secdb->nsigs;
	debug("secdb:%p sz:%zd", secdb, sz);
	return sz;
}

/*
 * calculate all the secdb->listsz for a all the linked
 * efi_secdbs.
 * returns 0 for lists with no signatures
 */
static inline size_t
secdb_size(efi_secdb_t *secdb)
{
	list_t *pos = NULL;
	size_t sz = 0;

	if (!secdb)
		return 0;

	for_each_secdb(pos, &secdb->list) {
		efi_secdb_t *entry = list_entry(pos, efi_secdb_t, list);
		sz += secdb_entry_size(entry);
	}

	return sz;
}

/*
 * compare secdb_entry_t items
 */
extern int secdb_entry_cmp(const void *a, const void *b, void *state);
extern int secdb_entry_cmp_descending(const void *a, const void *b, void *state);

/*
 * compare efi_secdb_t items;
 */
extern int secdb_cmp(const void *a, const void *b, void *state);
extern int secdb_cmp_descending(const void *a, const void *b, void *state);

/*
 * hexdump with annotations
 */
extern void secdb_dump(efi_secdb_t *secdb, bool annotate);

#endif /* PRIVATE_SECDB_H */
