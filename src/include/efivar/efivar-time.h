// SPDX-License-Identifier: LGPL-2.1
/*
 * efivar-time.h
 * Copyright 2020 Peter Jones <pjones@redhat.com>
 */

#if defined(EFIVAR_NO_EFI_TIME_T) && EFIVAR_NO_EFI_TIME_T && \
    !defined(EFIVAR_TIME_H_)
#define EFIVAR_TIME_H_ 1
#endif

#ifndef EFIVAR_TIME_H_
#define EFIVAR_TIME_H_ 1

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int tm_to_efi_time(const struct tm *const s, efi_time_t *d, bool tzadj);
extern int efi_time_to_tm(const efi_time_t * const s, struct tm *d);

extern char *efi_asctime(const efi_time_t *const time);
extern char *efi_asctime_r(const efi_time_t *const time, char *buf);
extern efi_time_t *efi_gmtime(const time_t *time);
extern efi_time_t *efi_gmtime_r(const time_t *time, efi_time_t *result);
extern efi_time_t *efi_localtime(const time_t *time);
extern efi_time_t *efi_localtime_r(const time_t *time, efi_time_t *result);
extern time_t efi_mktime(const efi_time_t *const time);

extern char *efi_strptime(const char *s, const char *format, efi_time_t *time);
extern size_t efi_strftime(char *s, size_t max, const char *format,
			   const efi_time_t *time);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* !EFIVAR_TIME_H_ */
// vim:fenc=utf-8:tw=75:noet
