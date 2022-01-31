// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2015 Red Hat, Inc.
 * Copyright (C) 2001 Dell Computer Corporation <Matt_Domsch@dell.com>
 */
#ifndef EFIBOOT_H
#define EFIBOOT_H 1

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <byteswap.h>

#include <efivar/efivar.h>

#include <efivar/efiboot-creator.h>
#include <efivar/efiboot-loadopt.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t efi_get_libefiboot_version(void)
	__attribute__((__visibility__("default")));

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* EFIBOOT_H */

// vim:fenc=utf-8:tw=75:noet
