// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012-2013 Red Hat, Inc.
 */

#ifndef LIBEFIVAR_LIB_H
#define LIBEFIVAR_LIB_H 1

#include <dirent.h>
#include <limits.h>
#include <sys/types.h>

#include <stddef.h>

#include <efivar/efivar-types.h>

struct efi_variable {
	uint64_t attrs;
	efi_guid_t *guid;
	unsigned char *name;
	uint8_t *data;
	size_t data_size;
};

struct efi_var_operations {
	char name[NAME_MAX];
	int (*probe)(void);
	int (*set_variable)(efi_guid_t guid, const char *name, const uint8_t *data,
			    size_t data_size, uint32_t attributes, mode_t mode);
	int (*del_variable)(efi_guid_t guid, const char *name);
	int (*get_variable)(efi_guid_t guid, const char *name, uint8_t **data,
			    size_t *data_size, uint32_t *attributes);
	int (*get_variable_attributes)(efi_guid_t guid, const char *name,
				       uint32_t *attributes);
	int (*get_variable_size)(efi_guid_t guid, const char *name,
				 size_t *size);
	int (*get_next_variable_name)(efi_guid_t **guid, char **name);
	int (*append_variable)(efi_guid_t guid, const char *name,
			       const uint8_t *data, size_t data_size,
			       uint32_t attributes);
	int (*chmod_variable)(efi_guid_t guid, const char *name, mode_t mode);
};

typedef unsigned long efi_status_t;

extern struct efi_var_operations vars_ops;
extern struct efi_var_operations efivarfs_ops;

#endif /* LIBEFIVAR_LIB_H */

// vim:fenc=utf-8:tw=75:noet
