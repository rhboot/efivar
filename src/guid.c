/*
 * libefivar - library for the manipulation of EFI variables
 * Copyright 2012-2014 Red Hat, Inc.
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
	return memcmp(a, b, sizeof (efi_guid_t));
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
	int rc;
	rc = text_to_guid(s, guid);
	if (rc < 0)
		efi_error("text_to_guid(\"%s\",...)", s);
	return rc;
}

int NONNULL(1) PUBLIC
efi_guid_to_str(const efi_guid_t *guid, char **sp)
{
	char *ret = NULL;
	int rc = -1;

	if (!sp) {
		rc = snprintf(NULL, 0, GUID_FORMAT,
				le32_to_cpu(guid->a),
				le16_to_cpu(guid->b),
				le16_to_cpu(guid->c),
				be16_to_cpu(guid->d),
				guid->e[0], guid->e[1], guid->e[2], guid->e[3],
				guid->e[4], guid->e[5]);
	} else if (sp && *sp) {
		rc = snprintf(*sp, GUID_LENGTH_WITH_NUL, GUID_FORMAT,
				le32_to_cpu(guid->a),
				le16_to_cpu(guid->b),
				le16_to_cpu(guid->c),
				be16_to_cpu(guid->d),
				guid->e[0], guid->e[1], guid->e[2], guid->e[3],
				guid->e[4], guid->e[5]);
	} else {
		rc = asprintf(&ret, GUID_FORMAT,
				le32_to_cpu(guid->a),
				le16_to_cpu(guid->b),
				le16_to_cpu(guid->c),
				be16_to_cpu(guid->d),
				guid->e[0], guid->e[1], guid->e[2], guid->e[3],
				guid->e[4], guid->e[5]);
		if (rc >= 0)
			*sp = ret;
	}
	if (rc < 0)
		efi_error("Could not format guid");
	return rc;
}

extern struct guidname efi_well_known_guids PUBLIC;
extern struct guidname efi_well_known_names PUBLIC;
extern struct guidname efi_well_known_guids_end PUBLIC;
extern struct guidname efi_well_known_names_end PUBLIC;

static int NONNULL(1, 2)
cmpguidp(const void *p1, const void *p2)
{
	struct guidname *gn1 = (struct guidname *)p1;
	struct guidname *gn2 = (struct guidname *)p2;

	return memcmp(&gn1->guid, &gn2->guid, sizeof (gn1->guid));
}

static int NONNULL(1, 2)
cmpnamep(const void *p1, const void *p2)
{
	struct guidname *gn1 = (struct guidname *)p1;
	struct guidname *gn2 = (struct guidname *)p2;

	return memcmp(gn1->name, gn2->name, sizeof (gn1->name));
}

static int NONNULL(1, 2)
_get_common_guidname(const efi_guid_t *guid, struct guidname **result)
{
	intptr_t end = (intptr_t)&efi_well_known_guids_end;
	intptr_t start = (intptr_t)&efi_well_known_guids;
	size_t nmemb = (end - start) / sizeof (efi_well_known_guids);

	struct guidname key;
	memset(&key, '\0', sizeof (key));
	memcpy(&key.guid, guid, sizeof (*guid));

	struct guidname *tmp;
	tmp = bsearch(&key, &efi_well_known_guids, nmemb,
			sizeof (efi_well_known_guids), cmpguidp);
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
	struct guidname *result;
	int rc = _get_common_guidname(guid, &result);
	if (rc >= 0) {
		*name = strndup(result->name, sizeof (result->name) -1);
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
	struct guidname *result;
	int rc = _get_common_guidname(guid, &result);
	if (rc >= 0) {
		*symbol = strndup(result->symbol, sizeof (result->symbol) -1);
		return *symbol ? (int)strlen(*symbol) : -1;
	}
	efi_error_clear();
	errno = EINVAL;
	return -1;
}

int NONNULL(1) PUBLIC
efi_guid_to_id_guid(const efi_guid_t *guid, char **sp)
{
	struct guidname *result = NULL;
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
				le32_to_cpu(guid->a),
				le16_to_cpu(guid->b),
				le16_to_cpu(guid->c),
				be16_to_cpu(guid->d),
				guid->e[0], guid->e[1], guid->e[2], guid->e[3],
				guid->e[4], guid->e[5]);
	} else if (sp && *sp) {
		return snprintf(*sp, GUID_LENGTH_WITH_NUL+2, "{"GUID_FORMAT"}",
				le32_to_cpu(guid->a),
				le16_to_cpu(guid->b),
				le16_to_cpu(guid->c),
				be16_to_cpu(guid->d),
				guid->e[0], guid->e[1], guid->e[2], guid->e[3],
				guid->e[4], guid->e[5]);
	}
	rc = asprintf(&ret, "{"GUID_FORMAT"}",
				le32_to_cpu(guid->a),
				le16_to_cpu(guid->b),
				le16_to_cpu(guid->c),
				be16_to_cpu(guid->d),
			guid->e[0], guid->e[1], guid->e[2], guid->e[3],
			guid->e[4], guid->e[5]);
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
	intptr_t end = (intptr_t)&efi_well_known_names_end;
	intptr_t start = (intptr_t)&efi_well_known_names;
	size_t nmemb = (end - start) / sizeof (efi_well_known_names);
	size_t namelen;

	namelen = strnlen(name, 39);
	struct guidname key;
	memset(&key, '\0', sizeof (key));
	memcpy(key.name, name, namelen);

	if (namelen > 2 && name[0] == '{' && name[namelen - 1] == '}') {
		namelen -= 2;
		memcpy(key.name, name + 1, namelen);
		key.name[namelen] = 0;
	}

	key.name[sizeof(key.name) - 1] = '\0';

	struct guidname *result;
	result = bsearch(&key, &efi_well_known_names, nmemb,
			sizeof (efi_well_known_names), cmpnamep);
	if (result != NULL) {
		memcpy(guid, &result->guid, sizeof (*guid));
		return 0;
	}

	int rc = efi_str_to_guid(key.name, guid);
	if (rc >= 0)
		return 0;

	char tmpname[sizeof(key.name) + 9];
	strcpy(tmpname, "efi_guid_");
	memmove(tmpname+9, key.name, sizeof (key.name) - 9);

	rc = efi_symbol_to_guid(tmpname, guid);
	if (rc >= 0)
		return rc;

	errno = ENOENT;
	return -1;
}

int
efi_id_guid_to_guid(const char *name, efi_guid_t *guid)
        NONNULL(1, 2) PUBLIC ALIAS(efi_name_to_guid);
