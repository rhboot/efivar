// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * efisec-secdb.h - interfaces for EFI security databases
 * Copyright Peter Jones <pjones@redhat.com>
 */

#ifndef EFISEC_SECDB_H_
#define EFISEC_SECDB_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct efi_secdb efi_secdb_t;

typedef union {
	efi_sha1_hash_t		sha1;
	efi_sha224_hash_t	sha224;
	efi_sha256_hash_t	sha256;
	efi_sha384_hash_t	sha384;
	efi_sha512_hash_t	sha512;
	efi_rsa2048_sig_t	rsa2048;
	efi_rsa2048_sig_t	rsa2048_sha1;
	efi_rsa2048_sig_t	rsa2048_sha256;
	efi_cert_x509_sha256_t	x509_sha256_t;
	efi_cert_x509_sha384_t	x509_sha384_t;
	efi_cert_x509_sha512_t	x509_sha512_t;
	uint8_t			raw[0];
} efi_secdb_data_t;

typedef enum efi_secdb_type {
	EFI_SECDB_TYPE_X509_CERT,      // a raw x509 cert
	EFI_SECDB_TYPE_X509_SHA256,    // SHA-256 hash of the TBSData
	EFI_SECDB_TYPE_SHA256,         // SHA-256 hash
	EFI_SECDB_TYPE_X509_SHA512,    // SHA-512 hash of the TBSData
	EFI_SECDB_TYPE_SHA512,         // SHA-512 hash
	EFI_SECDB_TYPE_X509_SHA384,    // SHA-384 hash of the TBSData
	EFI_SECDB_TYPE_SHA224,         // SHA-224 hash
	EFI_SECDB_TYPE_SHA384,         // SHA-384 hash
	EFI_SECDB_TYPE_SHA1,           // SHA-1 hash
	EFI_SECDB_TYPE_RSA2048,        // RSA-2048 pubkey (m, e=0x10001)
	EFI_SECDB_TYPE_RSA2048_SHA1,   // RSA-2048 signature of a SHA-1 hash
	EFI_SECDB_TYPE_RSA2048_SHA256, // RSA-2048 signature of a SHA-256 hash
	EFI_SECDB_TYPE_MAX
} efi_secdb_type_t;

typedef enum efi_secdb_flag {
	EFI_SECDB_SORT,
	EFI_SECDB_SORT_DATA,
	EFI_SECDB_SORT_DESCENDING,
} efi_secdb_flag_t;

extern efi_secdb_t *efi_secdb_new(void);
extern int efi_secdb_set_bool(efi_secdb_t *secdb,
			      efi_secdb_flag_t flag,
			      bool value);
extern int efi_secdb_parse(uint8_t *data,
			   size_t datasz,
			   efi_secdb_t **secdbp);
extern int efi_secdb_add_entry(efi_secdb_t *secdb,
			       const efi_guid_t *owner,
			       efi_secdb_type_t algorithm,
			       efi_secdb_data_t *data,
			       size_t datasz);
extern int efi_secdb_del_entry(efi_secdb_t *secdb,
			       const efi_guid_t *owner,
			       efi_secdb_type_t algorithm,
			       efi_secdb_data_t *data,
			       size_t datasz);
extern int efi_secdb_realize(efi_secdb_t *secdb,
			     /* caller owns out */
			     void **out,
			     size_t *outsize);
extern void efi_secdb_free(efi_secdb_t *secdb);

typedef enum {
	EFI_SECDB_VISITOR_ERROR = -1,
	EFI_SECDB_VISITOR_BREAK = 0,
	EFI_SECDB_VISITOR_CONTINUE = 1,
} efi_secdb_visitor_status_t;

typedef efi_secdb_visitor_status_t
	(efi_secdb_visitor_t)(unsigned int listnum,
			      unsigned int signum,
			      const efi_guid_t * const owner,
			      const efi_secdb_type_t algorithm,
			      const void * const header,
			      const size_t headersz,
			      const efi_secdb_data_t * const data,
			      const size_t datasz,
			      void *closure);

extern int efi_secdb_visit_entries(efi_secdb_t *secdb,
				   efi_secdb_visitor_t *visitor,
				   void *closure);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* !EFISEC_SECDB_H_ */
// vim:fenc=utf-8:tw=75:noet
