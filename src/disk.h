// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2015 Red Hat, Inc.
 * Copyright (C) 2001 Dell Computer Corporation <Matt_Domsch@dell.com>
 */
#ifndef _EFIBOOT_DISK_H
#define _EFIBOOT_DISK_H

extern bool HIDDEN is_partitioned(int fd);

extern HIDDEN ssize_t make_hd_dn(uint8_t *buf, ssize_t size, int fd,
				 int32_t partition, uint32_t options);

#endif /* _EFIBOOT_DISK_H */

// vim:fenc=utf-8:tw=75:noet
