// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * sec.c
 * Copyright 2020 Peter Jones <pjones@redhat.com>
 *
 */

#include "efivar.h"

uint32_t PUBLIC
efi_get_libefisec_version(void)
{
	return LIBEFIVAR_VERSION;
}

// vim:fenc=utf-8:tw=75:noet
