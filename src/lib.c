// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012-2013 Red Hat, Inc.
 */

#include "fix_coverity.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "efivar.h"

static int default_probe(void)
{
	return 1;
}

struct efi_var_operations default_ops = {
		.name = "default",
		.probe = default_probe,
	};

struct efi_var_operations *ops = NULL;

VERSION(_efi_set_variable, _efi_set_variable@libefivar.so.0)
int NONNULL(2, 3) PUBLIC
_efi_set_variable(efi_guid_t guid, const char *name, const uint8_t *data,
		  size_t data_size, uint32_t attributes)
{
	int rc;
	if (!ops->set_variable) {
		efi_error("set_variable() is not implemented");
		errno = ENOSYS;
		return -1;
	}
	rc = ops->set_variable(guid, name, data, data_size, attributes, 0600);
	if (rc < 0)
		efi_error("ops->set_variable() failed");
	return rc;
}

VERSION(_efi_set_variable_variadic, efi_set_variable@libefivar.so.0)
int NONNULL(2, 3) PUBLIC
_efi_set_variable_variadic(efi_guid_t guid, const char *name, const uint8_t *data,
			   size_t data_size, uint32_t attributes, ...)
{
	int rc;
	if (!ops->set_variable) {
		efi_error("set_variable() is not implemented");
		errno = ENOSYS;
		return -1;
	}
	rc = ops->set_variable(guid, name, data, data_size, attributes, 0600);
	if (rc < 0)
		efi_error("ops->set_variable() failed");
	return rc;
}

VERSION(_efi_set_variable_mode,efi_set_variable@@LIBEFIVAR_0.24)
int NONNULL(2, 3) PUBLIC
_efi_set_variable_mode(efi_guid_t guid, const char *name, const uint8_t *data,
		       size_t data_size, uint32_t attributes, mode_t mode)
{
	int rc;
	if (!ops->set_variable) {
		efi_error("set_variable() is not implemented");
		errno = ENOSYS;
		return -1;
	}
	rc = ops->set_variable(guid, name, data, data_size, attributes, mode);
	if (rc < 0)
		efi_error("ops->set_variable() failed");
	else
		efi_error_clear();
	return rc;
}

int NONNULL(2, 3) PUBLIC
efi_set_variable(efi_guid_t guid, const char *name, const uint8_t *data,
		 size_t data_size, uint32_t attributes, mode_t mode)
	ALIAS(_efi_set_variable_mode);

int NONNULL(2, 3) PUBLIC
efi_append_variable(efi_guid_t guid, const char *name, const uint8_t *data,
			size_t data_size, uint32_t attributes)
{
	int rc;
	if (!ops->append_variable) {
		rc = generic_append_variable(guid, name, data, data_size,
					     attributes);
		if (rc < 0)
			efi_error("generic_append_variable() failed");
		else
			efi_error_clear();
		return rc;
	}
	rc = ops->append_variable(guid, name, data, data_size, attributes);
	if (rc < 0)
		efi_error("ops->append_variable() failed");
	else
		efi_error_clear();
	return rc;
}

int NONNULL(2) PUBLIC
efi_del_variable(efi_guid_t guid, const char *name)
{
	int rc;
	if (!ops->del_variable) {
		efi_error("del_variable() is not implemented");
		errno = ENOSYS;
		return -1;
	}
	rc = ops->del_variable(guid, name);
	if (rc < 0)
		efi_error("ops->del_variable() failed");
	else
		efi_error_clear();
	return rc;
}

int NONNULL(2, 3, 4, 5) PUBLIC
efi_get_variable(efi_guid_t guid, const char *name, uint8_t **data,
		  size_t *data_size, uint32_t *attributes)
{
	int rc;
	if (!ops->get_variable) {
		efi_error("get_variable() is not implemented");
		errno = ENOSYS;
		return -1;
	}
	rc = ops->get_variable(guid, name, data, data_size, attributes);
	if (rc < 0)
		efi_error("ops->get_variable failed");
	else
		efi_error_clear();
	return rc;
}

int NONNULL(2, 3) PUBLIC
efi_get_variable_attributes(efi_guid_t guid, const char *name,
			    uint32_t *attributes)
{
	int rc;
	if (!ops->get_variable_attributes) {
		efi_error("get_variable_attributes() is not implemented");
		errno = ENOSYS;
		return -1;
	}
	rc = ops->get_variable_attributes(guid, name, attributes);
	if (rc < 0)
		efi_error("ops->get_variable_attributes() failed");
	else
		efi_error_clear();
	return rc;
}

int NONNULL(2) PUBLIC
efi_get_variable_exists(efi_guid_t guid, const char *name)
{
	uint32_t unused_attributes = 0;
	return efi_get_variable_attributes(guid, name, &unused_attributes);
}

int NONNULL(2, 3) PUBLIC
efi_get_variable_size(efi_guid_t guid, const char *name, size_t *size)
{
	int rc;
	if (!ops->get_variable_size) {
		efi_error("get_variable_size() is not implemented");
		errno = ENOSYS;
		return -1;
	}
	rc = ops->get_variable_size(guid, name, size);
	if (rc < 0)
		efi_error("ops->get_variable_size() failed");
	else
		efi_error_clear();
	return rc;
}

int NONNULL(1, 2) PUBLIC
efi_get_next_variable_name(efi_guid_t **guid, char **name)
{
	int rc;
	if (!ops->get_next_variable_name) {
		efi_error("get_next_variable_name() is not implemented");
		errno = ENOSYS;
		return -1;
	}
	rc = ops->get_next_variable_name(guid, name);
	if (rc < 0)
		efi_error("ops->get_next_variable_name() failed");
	else
		efi_error_clear();
	return rc;
}

int NONNULL(2) PUBLIC
efi_chmod_variable(efi_guid_t guid, const char *name, mode_t mode)
{
	int rc;
	if (!ops->chmod_variable) {
		efi_error("chmod_variable() is not implemented");
		errno = ENOSYS;
		return -1;
	}
	rc = ops->chmod_variable(guid, name, mode);
	if (rc < 0)
		efi_error("ops->chmod_variable() failed");
	else
		efi_error_clear();
	return rc;
}

int PUBLIC
efi_variables_supported(void)
{
	if (ops == &default_ops)
		return 0;
	return 1;
}

static void CONSTRUCTOR libefivar_init(void);

static void CONSTRUCTOR
libefivar_init(void)
{
	struct efi_var_operations *ops_list[] = {
		&efivarfs_ops,
		&vars_ops,
		&default_ops,
		NULL
	};
	char *ops_name = getenv("LIBEFIVAR_OPS");
	if (ops_name && strcasestr(ops_name, "help")) {
		printf("LIBEFIVAR_OPS operations available:\n");
		for (int i = 0; ops_list[i] != NULL; i++)
			printf("\t%s\n", ops_list[i]->name);
		exit(0);
	}

	for (int i = 0; ops_list[i] != NULL; i++)
	{
		if (ops_name != NULL) {
			if (!strcmp(ops_list[i]->name, ops_name) ||
					!strcmp(ops_list[i]->name, "default")) {
				ops = ops_list[i];
				break;
			}
		} else {
			int rc = ops_list[i]->probe();
			if (rc <= 0) {
				efi_error("ops_list[%d]->probe() failed", i);
			} else {
				efi_error_clear();
				ops = ops_list[i];
				break;
			}
		}
	}
}

uint32_t PUBLIC
efi_get_libefivar_version(void)
{
	return LIBEFIVAR_VERSION;
}

// vim:fenc=utf-8:tw=75:noet
