// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * authenticode.h - Authenticode definitions and types
 * Copyright 2019-2020 Peter Jones <pjones@redhat.com>
 */

#ifndef EFISEC_TYPES_H_
#define EFISEC_TYPES_H_ 1

#include <stdint.h>
#include <efivar/efivar-types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Storage for specific hashes and cryptographic (pkcs1, not pkcs7)
 * signatures
 */
typedef uint8_t efi_sha1_hash_t[20];
typedef uint8_t efi_sha224_hash_t[28];
typedef uint8_t efi_sha256_hash_t[32];
typedef uint8_t efi_sha384_hash_t[48];
typedef uint8_t efi_sha512_hash_t[64];
typedef uint8_t efi_rsa2048_sig_t[256];

/*
 * Security database definitions and types
 */

#define EFI_GLOBAL_PLATFORM_KEY L"PK"
#define EFI_GLOBAL_KEY_EXCHANGE_KEY L"KEK"
#define EFI_IMAGE_SECURITY_DATABASE L"db"
#define EFI_IMAGE_SECURITY_DATABASE1 L"dbx"
#define EFI_IMAGE_SECURITY_DATABASE2 L"dbt"
#define EFI_IMAGE_SECURITY_DATABASE3 L"dbr"

typedef struct {
	efi_sha256_hash_t	to_be_signed_hash;
	efi_time_t		time_of_revocation;
} efi_cert_x509_sha256_t __attribute__((__aligned__(1)));

typedef struct {
	efi_sha384_hash_t	to_be_signed_hash;
	efi_time_t		time_of_revocation;
} efi_cert_x509_sha384_t __attribute__((__aligned__(1)));

typedef struct {
	efi_sha512_hash_t	to_be_signed_hash;
	efi_time_t		time_of_revocation;
} efi_cert_x509_sha512_t __attribute__((__aligned__(1)));

typedef struct {
	efi_guid_t		signature_owner;
	uint8_t			signature_data[];
} efi_signature_data_t __attribute__((__aligned__(1)));

typedef struct {
	efi_guid_t		signature_type;
	uint32_t		signature_list_size;
	uint32_t		signature_header_size;
	uint32_t		signature_size;
	// uint8_t		signature_header[];
	// efi_signature_data	signatures[][signature_size];
} efi_signature_list_t __attribute__((__aligned__(1)));

/**********************************************************
 * Stuff used by authenticode and authenticated variables *
 **********************************************************/

#define WIN_CERT_REVISION_1_0		((uint16_t)0x0100)
#define WIN_CERT_REVISION_2_0		((uint16_t)0x0200)

#define WIN_CERT_TYPE_PKCS_SIGNED_DATA	((uint16_t)0x0002)
#define WIN_CERT_TYPE_EFI_PKCS115	((uint16_t)0x0ef0)
#define WIN_CERT_TYPE_EFI_GUID		((uint16_t)0x0ef1)

typedef struct {
	uint32_t			length;
	uint16_t			revision;
	uint16_t			cert_type;
} win_certificate_header_t;

/*
 * The spec says:
 *
 *  This structure is the certificate header. There may be zero or more
 *  certificates.
 *  • If the wCertificateType field is set to WIN_CERT_TYPE_EFI_PKCS115,
 *    then the certificate follows the format described in
 *    WIN_CERTIFICATE_EFI_PKCS1_15.
 *  • If the wCertificateType field is set to WIN_CERT_TYPE_EFI_GUID, then
 *    the certificate follows the format described in
 *    WIN_CERTIFICATE_UEFI_GUID.
 *  • If the wCertificateType field is set to WIN_CERT_TYPE_PKCS_SIGNED_DATA
 *    then the certificate is formatted as described in the Authenticode
 *    specification.
 *
 * Which basically means we see the first two in EFI signature databases,
 * and the third one in authenticode signatures.  It goes on to say:
 *
 * Table 11.
 * PE/COFF Certificates Types and UEFI Signature Database Certificate Types
 * +---------------------------------------+-----------------------------------+
 * | Image Certificate Type                | Verified Using Signature Database |
 * |                                       | Type                              |
 * +---------------------------------------+-----------------------------------+
 * | WIN_CERT_TYPE_EFI_PKCS115             | EFI_CERT_RSA2048_GUID (public key)|
 * | ( Signature Size = 256 bytes)         |                                   |
 * +---------------------------------------+-----------------------------------+
 * | WIN_CERT_TYPE_EFI_GUID                | EFI_CERT_RSA2048_GUID (public key)|
 * | ( CertType =                          |                                   |
 * |   EFI_CERT_TYPE_RSA2048_SHA256_GUID ) |                                   |
 * +---------------------------------------+-----------------------------------+
 * | WIN_CERT_TYPE_EFI_GUID                | EFI_CERT_X509_GUID                |
 * | (CertType = EFI_CERT_TYPE_PKCS7_GUID) | EFI_CERT_RSA2048_GUID             |
 * |                                       | (when applicable)                 |
 * |                                       | EFI_CERT_X509_SHA256_GUID         |
 * |                                       | (when applicable)                 |
 * |                                       | EFI_CERT_X509_SHA384_GUID         |
 * |                                       | (when applicable)                 |
 * |                                       | EFI_CERT_X509_SHA512_GUID         |
 * |                                       | (when applicable)                 |
 * +---------------------------------------+-----------------------------------+
 * | WIN_CERT_TYPE_PKCS_SIGNED_DATA        | EFI_CERT_X509_GUID                |
 * |                                       | EFI_CERT_RSA2048_GUID             |
 * |                                       | (when applicable)                 |
 * |                                       | EFI_CERT_X509_SHA256_GUID         |
 * |                                       | (when applicable)                 |
 * |                                       | EFI_CERT_X509_SHA384_GUID         |
 * |                                       | (when applicable)                 |
 * |                                       | EFI_CERT_X509_SHA512_GUID         |
 * |                                       | (when applicable)                 |
 * +---------------------------------------+-----------------------------------+
 * |(Always applicable regardless of       | EFI_CERT_SHA1_GUID,               |
 * | whether a certificate is present or   | EFI_CERT_SHA224_GUID,             |
 * | not)                                  | EFI_CERT_SHA256_GUID,             |
 * |                                       | EFI_CERT_SHA384_GUID,             |
 * |                                       | EFI_CERT_SHA512_GUID              |
 * |                                       | In this case, the database        |
 * |                                       | contains the hash of the image.   |
 * +---------------------------------------+-----------------------------------+
 */

/*
 * hdr.cert_type = WIN_CERT_TYPE_PKCS_SIGNED_DATA
 */
typedef struct {
	win_certificate_header_t	hdr;
	uint8_t				data[]; // pkcs7 signedData
} win_certificate_pkcs_signed_data_t;

/*
 * hdr.cert_type = WIN_CERT_TYPE_EFI_PKCS115
 */
typedef struct {
	win_certificate_header_t	hdr;
	efi_guid_t			hash_alg;
	uint8_t				signature[];
} win_certificate_efi_pkcs1_15_t;

/*
 * hdr.cert_type = WIN_CERT_TYPE_EFI_GUID
 */
typedef struct {
	win_certificate_header_t	hdr;
	efi_guid_t			type;
	uint8_t				data[];
} win_certificate_uefi_guid_t;


/*
 * public_key: pubkey that may or may not be trusted
 * signature: a RSA2048 signature of the SHA256 authenticode hash
 */
typedef struct {
	efi_guid_t			hash_type;
	uint8_t				public_key[256];
	uint8_t				signature[256];
} efi_cert_rsa2048_sha256_t;

typedef struct {
	uint64_t			monotonic_count;
	win_certificate_uefi_guid_t	auth_info;
} efi_variable_authentication_t __attribute__((aligned (1)));

typedef struct {
	efi_time_t			timestamp;
	win_certificate_uefi_guid_t	auth_info;
} efi_variable_authentication_2_t __attribute__((aligned (1)));

#define EFI_VARIABLE_AUTHENTICATION_3_CERT_ID_SHA256	((uint8_t)1)

/* XXX the spec doesn't say if this is supposed to be packed/align(1) */
typedef struct {
	uint8_t				type;
	uint32_t			id_size;
	uint8_t				id[];
} efi_variable_authentication_3_cert_id_t __attribute__((aligned (1)));

#define EFI_VARIABLE_AUTHENTICATION_3_TIMESTAMP_TYPE	((uint8_t)1)
#define EFI_VARIABLE_AUTHENTICATION_3_NONCE_TYPE	((uint8_t)2)

/* XXX the spec doesn't say if this is supposed to be packed/align(1) */
typedef struct {
	uint8_t				version;
	uint8_t				type;
	uint32_t			metadata_size;	// this is everything except data[]
	uint32_t			flags;
} efi_variable_authentication_3_header_t __attribute__((aligned (1)));

#define EFI_VARIABLE_ENHANCED_AUTH_FLAG_UPDATE_CERT	((uint32_t)0x00000001)

typedef struct {
	uint32_t			nonce_size;
	uint8_t				nonce[];
} efi_variable_authentication_3_nonce_t;

/* XXX the spec sort of implies that this is supposed to be packed/align(1) */
typedef struct {
	efi_variable_authentication_3_header_t	hdr;
	efi_time_t				timestamp;
	// if EFI_VARIABLE_ENHANCED_AUTH_FLAG_UPDATE_CERT is set:
	// uint8_t				newcert[];
	// uint8_t				signing_cert[];
} efi_variable_timestamped_authentication_3 __attribute__((aligned (1)));

/* XXX the spec sort of implies that this is supposed to be packed/align(1) */
typedef struct {
	efi_variable_authentication_3_header_t	hdr;
	efi_variable_authentication_3_nonce_t	nonce;
	// if EFI_VARIABLE_ENHANCED_AUTH_FLAG_UPDATE_CERT is set:
	// uint8_t				newcert[];
	// uint8_t				signing_cert[];
} efi_variable_nonced_authentication_3 __attribute__((aligned (1)));

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* !SECURITY_H_ */
// vim:fenc=utf-8:tw=75:noet
