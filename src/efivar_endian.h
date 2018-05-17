/*
 * Copyright 2009-2015 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author:  Peter Jones <pjones@redhat.com>
 */
#ifndef _EFIVAR_ENDIAN_H
#define _EFIVAR_ENDIAN_H

#include <endian.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define cpu_to_le16(x) (x)
#define cpu_to_le32(x) (x)
#define cpu_to_le64(x) ((uint64_t)x)
#define le16_to_cpu(x) (x)
#define le32_to_cpu(x) (x)
#define le64_to_cpu(x) ((uint64_t)x)
#define cpu_to_be16(x) __builtin_bswap16(x)
#define cpu_to_be32(x) __builtin_bswap32(x)
#define cpu_to_be64(x) ((uint64_t)__builtin_bswap64(x))
#define be16_to_cpu(x) __builtin_bswap16(x)
#define be32_to_cpu(x) __builtin_bswap32(x)
#define be64_to_cpu(x) ((uint64_t)__builtin_bswap64(x))
#else
#define cpu_to_be16(x) (x)
#define cpu_to_be32(x) (x)
#define cpu_to_be64(x) ((uint64_t)x)
#define be16_to_cpu(x) (x)
#define be32_to_cpu(x) (x)
#define be64_to_cpu(x) ((uint64_t)x)
#define cpu_to_le16(x) __builtin_bswap16(x)
#define cpu_to_le32(x) __builtin_bswap32(x)
#define cpu_to_le64(x) ((uint64_t)__builtin_bswap64(x))
#define le16_to_cpu(x) __builtin_bswap16(x)
#define le32_to_cpu(x) __builtin_bswap32(x)
#define le64_to_cpu(x) ((uint64_t)__builtin_bswap64(x))
#endif

#endif /* _EFIVAR_ENDIAN_H */
