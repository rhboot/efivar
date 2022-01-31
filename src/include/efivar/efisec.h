// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libefisec - library for the manipulation of EFI boot variables
 * Copyright 2020 Peter M. Jones <pjones@redhat.com>
 * Copyright 2020 Red Hat, Inc.
 */
#ifndef EFISEC_H
#define EFISEC_H 1

#include <efivar/efivar.h>

#include <efivar/efisec-types.h>
#include <efivar/efisec-secdb.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t efi_get_libefisec_version(void)
	__attribute__((__visibility__("default")));

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* EFISEC_H */

// vim:fenc=utf-8:tw=75:noet
