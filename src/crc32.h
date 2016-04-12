/*
 * crc32.h - headers for crc32
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
 */

#ifndef _CRC32_H
#define _CRC32_H

#include <stdint.h>

/*
 * This computes a 32 bit CRC of the data in the buffer, and returns the CRC.
 * The polynomial used is 0xedb88320.
 */

extern uint32_t crc32 (const void *buf, unsigned long len, uint32_t seed);

#endif /* _CRC32_H */
