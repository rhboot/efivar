// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012-2014 Red Hat, Inc.
 */

#include "fix_coverity.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>

#include "efivar.h"

#define GUID_LENGTH_WITH_NUL 37

extern const efi_guid_t efi_guid_zero;

int NONNULL(1, 2) PUBLIC
efi_guid_cmp(const efi_guid_t *a, const efi_guid_t *b)
{
	return efi_guid_cmp_(a, b);
}

int NONNULL(1) PUBLIC
efi_guid_is_zero(const efi_guid_t *guid)
{
	return !efi_guid_cmp(guid,&efi_guid_zero);
}

int
efi_guid_is_empty(const efi_guid_t *guid)
	NONNULL(1) PUBLIC ALIAS(efi_guid_is_zero);

int NONNULL(1, 2) PUBLIC
efi_str_to_guid(const char *s, efi_guid_t *guid)
{
	return efi_str_to_guid_(s, guid);
}

int NONNULL(1) PUBLIC
efi_guid_to_str(const efi_guid_t *guid, char **sp)
{
	char *ret = NULL;
	int rc = -1;

	if (!sp) {
		rc = snprintf(NULL, 0, GUID_FORMAT, GUID_FORMAT_ARGS(guid));
	} else if (sp && *sp) {
		rc = snprintf(*sp, GUID_LENGTH_WITH_NUL, GUID_FORMAT,
			      GUID_FORMAT_ARGS(guid));
	} else {
		rc = asprintf(&ret, GUID_FORMAT, GUID_FORMAT_ARGS(guid));
		if (rc >= 0)
			*sp = ret;
	}
	if (rc < 0)
		efi_error("Could not format guid");
	return rc;
}

static int NONNULL(1, 2)
cmpguidp(const void *p1, const void *p2)
{
	struct efivar_guidname *gn1 = (struct efivar_guidname *)p1;
	struct efivar_guidname *gn2 = (struct efivar_guidname *)p2;

	return efi_guid_cmp_(&gn1->guid, &gn2->guid);
}

static int NONNULL(1, 2)
cmpnamep(const void *p1, const void *p2)
{
	struct efivar_guidname *gn1 = (struct efivar_guidname *)p1;
	struct efivar_guidname *gn2 = (struct efivar_guidname *)p2;

	return strncmp(gn1->name, gn2->name, sizeof(gn1->name));
}

static int NONNULL(1, 2)
_get_common_guidname(const efi_guid_t *guid, struct efivar_guidname **result)
{
	struct efivar_guidname key;
	memset(&key, '\0', sizeof(key));
	memcpy(&key.guid, guid, sizeof(*guid));

	struct efivar_guidname *tmp;
	tmp = bsearch(&key,
		      &efi_well_known_guids[0],
		      efi_n_well_known_guids,
		      sizeof(efi_well_known_guids[0]),
		      cmpguidp);
	if (!tmp) {
		*result = NULL;
		errno = ENOENT;
		efi_error("GUID is not in common GUID list");
		return -1;
	}

	*result = tmp;
	return 0;
}

int NONNULL(1, 2) PUBLIC
efi_guid_to_name(efi_guid_t *guid, char **name)
{
	struct efivar_guidname *result;
	int rc = _get_common_guidname(guid, &result);
	if (rc >= 0) {
		*name = strndup(result->name, sizeof(result->name) -1);
		return *name ? (int)strlen(*name) : -1;
	}
	rc = efi_guid_to_str(guid, name);
	if (rc >= 0)
		efi_error_clear();
	return rc;
}

int NONNULL(1, 2) PUBLIC
efi_guid_to_symbol(efi_guid_t *guid, char **symbol)
{
	struct efivar_guidname *result;
	int rc = _get_common_guidname(guid, &result);
	if (rc >= 0) {
		*symbol = strndup(result->symbol, sizeof(result->symbol) -1);
		return *symbol ? (int)strlen(*symbol) : -1;
	}
	efi_error_clear();
	errno = EINVAL;
	return -1;
}

int NONNULL(1) PUBLIC
efi_guid_to_id_guid(const efi_guid_t *guid, char **sp)
{
	struct efivar_guidname *result = NULL;
	char *ret = NULL;
	int rc;

	rc = _get_common_guidname(guid, &result);
	if (rc >= 0) {
		if (!sp) {
			return snprintf(NULL, 0, "{%s}",
					result->symbol + strlen("efi_guid_"));
		} else if (sp && *sp) {
			return snprintf(*sp, GUID_LENGTH_WITH_NUL + 2, "{%s}",
					result->symbol + strlen("efi_guid_"));
		}

		rc = asprintf(&ret, "{%s}",
				result->symbol + strlen("efi_guid_"));
		if (rc >= 0)
			*sp = ret;
		return rc;
	}
	if (!sp) {
		return snprintf(NULL, 0, "{"GUID_FORMAT"}",
				GUID_FORMAT_ARGS(guid));
	} else if (sp && *sp) {
		return snprintf(*sp, GUID_LENGTH_WITH_NUL+2, "{"GUID_FORMAT"}",
				GUID_FORMAT_ARGS(guid));
	}
	rc = asprintf(&ret, "{"GUID_FORMAT"}", GUID_FORMAT_ARGS(guid));
	if (rc >= 0)
		*sp = ret;
	return rc;
}

int NONNULL(1, 2) PUBLIC
efi_symbol_to_guid(const char *symbol, efi_guid_t *guid)
{
	void *dlh = dlopen(NULL, RTLD_LAZY);
	if (!dlh)
		return -1;

	void *sym = dlsym(dlh, symbol);
	dlclose(dlh);
	if (!sym)
		return -1;

	memcpy(guid, sym, sizeof(*guid));
	return 0;
}

int NONNULL(1, 2) PUBLIC
efi_name_to_guid(const char *name, efi_guid_t *guid)
{
	size_t namelen;
	struct efivar_guidname key;

	namelen = strnlen(name, 39);

	memset(&key, '\0', sizeof(key));
	memcpy(key.name, name, namelen);

	if (namelen > 2 && name[0] == '{' && name[namelen - 1] == '}') {
		namelen -= 2;
		memcpy(key.name, name + 1, namelen);
		key.name[namelen] = 0;
	}

	key.name[sizeof(key.name) - 1] = '\0';

	struct efivar_guidname *result;
	result = bsearch(&key,
			 &efi_well_known_names[0],
			 efi_n_well_known_names,
			 sizeof(efi_well_known_names[0]),
			 cmpnamep);
	if (result != NULL) {
		memcpy(guid, &result->guid, sizeof(*guid));
		return 0;
	}

	int rc = efi_str_to_guid(key.name, guid);
	if (rc >= 0)
		return 0;

	char tmpname[sizeof(key.name) + 9];
	strcpy(tmpname, "efi_guid_");
	memmove(tmpname+9, key.name, sizeof(key.name) - 9);

	rc = efi_symbol_to_guid(tmpname, guid);
	if (rc >= 0)
		return rc;

	errno = ENOENT;
	return -1;
}

int
efi_id_guid_to_guid(const char *name, efi_guid_t *guid)
	NONNULL(1, 2) PUBLIC ALIAS(efi_name_to_guid);

// vim:fenc=utf-8:tw=75:noet
