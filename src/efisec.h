// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * efisec.h
 * Copyright 2018 Peter Jones <pjones@redhat.com>
 */
#pragma once

#include "fix_coverity.h" // IWYU pragma: keep

#include <err.h> // IWYU pragma: export
#include <errno.h> // IWYU pragma: export
#include <inttypes.h> // IWYU pragma: export
#include <stdbool.h> // IWYU pragma: export
#include <stddef.h> // IWYU pragma: export
#include <stdlib.h> // IWYU pragma: export
#include <stdint.h> // IWYU pragma: export
#include <stdio.h> // IWYU pragma: export
#include <string.h> // IWYU pragma: export
#include <sys/param.h> // IWYU pragma: export
#include <unistd.h> // IWYU pragma: export

#include "efivar/efivar-types.h" // IWYU pragma: export
#include "efivar/efivar-guids.h" // IWYU pragma: export
#include "efivar/efivar.h" // IWYU pragma: export

#include "efivar/efisec-types.h" // IWYU pragma: export
#include "efivar/efisec-secdb.h" // IWYU pragma: export
#include "efivar/efisec.h" // IWYU pragma: export

#include "efivar.h" // IWYU pragma: export
#include "esl-iter.h" // IWYU pragma: export
#include "secdb.h" // IWYU pragma: export
#include "x509.h" // IWYU pragma: export

// vim:fenc=utf-8:tw=75:noet
