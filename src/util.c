// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * util.c - utility functions and data that can't go in a header
 * Copyright Peter Jones <pjones@redhat.com>
 */

#include "efivar.h"

size_t HIDDEN page_size = 4096;

void CONSTRUCTOR
set_up_global_constants(void)
{
	page_size = sysconf(_SC_PAGE_SIZE);
}

// vim:fenc=utf-8:tw=75:noet
