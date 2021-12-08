// SPDX-License-Identifier: LGPL-2.1
/*
 * time.c - efi_time_t helper functions
 * Copyright 2020 Peter Jones <pjones@redhat.com>
 */

#include "efivar.h"

int
efi_time_to_tm(const efi_time_t * const s, struct tm *d)
{

	if (!s || !d) {
		errno = EINVAL;
		return -1;
	}

	d->tm_year = s->year - 1900;
	d->tm_mon = s->month - 1;
	d->tm_mday = s->day;
	d->tm_hour = s->hour;
	d->tm_min = s->minute;
	/*
	 * Just ignore EFI's range problem here and pretend we're in UTC
	 * not UT1.
	 */
	d->tm_sec = s->second;
	d->tm_isdst = (s->daylight & EFI_TIME_IN_DAYLIGHT) ? 1 : 0;

	return 0;
}

int
tm_to_efi_time(const struct tm * const s, efi_time_t *d, bool tzadj)
{
	if (!s || !d) {
		errno = EINVAL;
		return -1;
	}

	d->pad2 = 0;
	d->daylight = s->tm_isdst ? EFI_TIME_IN_DAYLIGHT : 0;
	d->timezone = 0;
	d->nanosecond = 0;
	d->pad1 = 0;
	d->second = s->tm_sec < 60 ? s->tm_sec : 59;
	d->minute = s->tm_min;
	d->hour = s->tm_hour;
	d->day = s->tm_mday;
	d->month = s->tm_mon + 1;
	d->year = s->tm_year + 1900;

	if (tzadj) {
		tzset();
		d->timezone = timezone / 60;
	}

	return 0;
}

static char *otz_;
static char *ntz_;

static const char *
newtz(int16_t timezone_)
{
	if (!otz_)
		otz_ = strdup(secure_getenv("TZ"));

	if (ntz_) {
		free(ntz_);
		ntz_ = NULL;
	}

	if (timezone_ == EFI_UNSPECIFIED_TIMEZONE) {
		unsetenv("TZ");
	} else {
		char tzsign = timezone_ >= 0 ? '+' : '-';
		int tzabs = tzsign == '+' ? timezone_ : -timezone_;
		int16_t tzhours = tzabs / 60;
		int16_t tzminutes = tzabs % 60;
		int rc;

		/*
		 * I have no idea what the right thing to do with DST is
		 * here, so I'm going to ignore it.
		 */
		rc = asprintf(&ntz_, "UTC%c%"PRId16":%"PRId16":00",
			  tzsign, tzhours, tzminutes);
		if (rc < 1)
			return NULL;

		setenv("TZ", ntz_, 1);
	}
	tzset();

	return ntz_;
}

static const char *
oldtz(void) {
	if (ntz_) {
		free(ntz_);
		ntz_ = NULL;

		if (otz_)
			setenv("TZ", otz_, 1);
		else
			unsetenv("TZ");
	}

	tzset();

	return otz_;
}

efi_time_t *
efi_gmtime_r(const time_t *time, efi_time_t *result)
{
	struct tm tm = { 0 };

	if (!time || !result) {
		errno = EINVAL;
		return NULL;
	}

	gmtime_r(time, &tm);
	tm_to_efi_time(&tm, result, false);

	return result;
}

efi_time_t *
efi_gmtime(const time_t *time)
{
	static efi_time_t ret;

	if (!time) {
		errno = EINVAL;
		return NULL;
	}

	efi_gmtime_r(time, &ret);

	return &ret;
}

efi_time_t *
efi_localtime_r(const time_t *time, efi_time_t *result)
{
	struct tm tm = { 0 };

	if (!time || !result) {
		errno = EINVAL;
		return NULL;
	}

	localtime_r(time, &tm);
	tm_to_efi_time(&tm, result, true);

	return result;
}

efi_time_t *
efi_localtime(const time_t *time)
{
	static efi_time_t ret;

	if (!time) {
		errno = EINVAL;
		return NULL;
	}

	efi_localtime_r(time, &ret);

	return &ret;
}

time_t
efi_mktime(const efi_time_t * const time)
{
	struct tm tm = { 0 };
	time_t ret;

	if (!time) {
		errno = EINVAL;
		return (time_t)-1;
	}

	newtz(time->timezone);

	efi_time_to_tm(time, &tm);
	ret = mktime(&tm);

	oldtz();

	return ret;
}

char *
efi_strptime(const char *s, const char *format, efi_time_t *time)
{
	struct tm tm;
	char *end;

	if (!s || !format || !time) {
		errno = EINVAL;
		return NULL;
	}

	memset(&tm, 0, sizeof(tm));
	end = strptime(s, format, &tm);
	if (end == NULL)
		return NULL;

	if (tm_to_efi_time(&tm, time, true) < 0)
		return NULL;

	return end;
}

char *
efi_asctime_r(const efi_time_t * const time, char *buf)
{
	struct tm tm = { 0, };
	char *ret;

	newtz(time->timezone);

	efi_time_to_tm(time, &tm);
	ret = asctime_r(&tm, buf);

	oldtz();

	return ret;
}

char *
efi_asctime(const efi_time_t * const time)
{
	struct tm tm;
	char *ret;

	newtz(time->timezone);

	efi_time_to_tm(time, &tm);
	ret = asctime(&tm);

	oldtz();

	return ret;
}

size_t
efi_strftime(char *s, size_t max, const char *format, const efi_time_t *time)
{
	size_t ret = 0;
	struct tm tm = { 0 };

	if (!s || !format || !time) {
		errno = EINVAL;
		return ret;
	}

	newtz(time->timezone);

	efi_time_to_tm(time, &tm);
	ret = strftime(s, max, format, &tm);

	oldtz();

	return ret;
}

// vim:fenc=utf-8:tw=75:noet
