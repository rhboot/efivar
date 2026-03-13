// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * efivar.h
 * Copyright 2018 Peter Jones <pjones@redhat.com>
 */
#pragma once

#pragma GCC diagnostic warning "-Wcpp"

#include "fix_coverity.h" // IWYU pragma: keep

#include "efivar/efivar.h"

#include "compiler.h"
#include "diag.h"
#include "list.h"
#include "util.h"
#include "safemath.h"
#include "efivar_endian.h"
#include "lib.h"
#include "guid.h"
#include "generics.h"
#include "dp.h"
#include "gpt.h"
#include "disk.h"
#include "linux.h"
#include "crc32.h"
#include "hexdump.h"
#include "path-helpers.h"
#include "makeguids.h"

// vim:fenc=utf-8:tw=75:noet
