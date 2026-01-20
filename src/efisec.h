// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * efisec.h
 * Copyright 2018 Peter Jones <pjones@redhat.com>
 */
#pragma once

#include "fix_coverity.h"

#include <efivar/efisec.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

#include "efivar.h"
#include "esl-iter.h"
#include "secdb.h"
#include "x509.h"

// vim:fenc=utf-8:tw=75:noet
