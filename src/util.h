/*
 * Copyright 2011-2014 Red Hat, Inc.
 * All rights reserved.
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
 * Author(s): Peter Jones <pjones@redhat.com>
 */
#ifndef EFIVAR_UTIL_H
#define EFIVAR_UTIL_H 1

#include <alloca.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <tgmath.h>
#include <unistd.h>

#define UNUSED __attribute__((__unused__))
#define HIDDEN __attribute__((__visibility__ ("hidden")))
#define PUBLIC __attribute__((__visibility__ ("default")))
#define DESTRUCTOR __attribute__((destructor))
#define CONSTRUCTOR __attribute__((constructor))
#define ALIAS(x) __attribute__((weak, alias (#x)))
#define NONNULL(...) __attribute__((__nonnull__(__VA_ARGS__)))
#define PRINTF(...) __attribute__((__format__(printf, __VA_ARGS__)))
#define FLATTEN __attribute__((__flatten__))
#define PACKED __attribute__((__packed__))
#define VERSION(sym, ver) __asm__(".symver " # sym "," # ver)

/*
 * I'm not actually sure when these appear, but they're present in the
 * version in front of me.
 */
#if defined(__GNUC__) && defined(__GNUC_MINOR__)
#if __GNUC__ >= 5 && __GNUC_MINOR__ >= 1
#define int_add(a, b, c) __builtin_add_overflow(a, b, c)
#define long_add(a, b, c) __builtin_add_overflow(a, b, c)
#define long_mult(a, b, c) __builtin_mul_overflow(a, b, c)
#define ulong_add(a, b, c) __builtin_add_overflow(a, b, c)
#define ulong_mult(a, b, c) __builtin_mul_overflow(a, b, c)
#endif
#endif
#ifndef int_add
#define int_add(a, b, c) ({                                             \
                const int _limit = INT_MAX;                             \
                int _ret;                                               \
                _ret = _limit - ((unsigned long long)a) >               \
                          ((unsigned long long)b);                      \
                if (!_ret)                                              \
                        *(c) = ((a) + (b));                             \
                _ret;                                                   \
        })
#endif
#ifndef long_add
#define long_add(a, b, c) ({                                            \
                const long _limit = LONG_MAX;                           \
                int _ret;                                               \
                _ret = _limit - ((unsigned long long)a) >               \
                           ((unsigned long long)b);                     \
                if (!_ret)                                              \
                        *(c) = ((a) + (b));                             \
                _ret;                                                   \
        })
#endif
#ifndef long_mult
#define long_mult(a, b, c) ({                                           \
                const long _limit = LONG_MAX;                           \
                int _ret = 1;                                           \
                if ((a) == 0 || (b) == 0)                               \
                        _ret = 0;                                       \
                else                                                    \
                        _ret = _limit / (a) < (b);                      \
                if (!_ret)                                              \
                        *(c) = ((a) * (b));                             \
                _ret;                                                   \
        })
#endif
#ifndef ulong_add
#define ulong_add(a, b, c) ({                                           \
                const unsigned long _limit = ULONG_MAX;                 \
                int _ret;                                               \
                _ret = _limit - ((unsigned long long)a) >               \
                            ((unsigned long long)b);                    \
                if (!_ret)                                              \
                        *(c) = ((a) + (b));                             \
                _ret;                                                   \
        })
#endif
#ifndef ulong_mult
#define ulong_mult(a, b, c) ({                                          \
                const unsigned long _limit = ULONG_MAX;                 \
                int _ret = 1;                                           \
                if ((a) == 0 || (b) == 0)                               \
                        _ret = 0;                                       \
                else                                                    \
                        _ret = _limit / (a) < (b);                      \
                if (!_ret)                                              \
                        *(c) = ((a) * (b));                             \
                _ret;                                                   \
        })
#endif

#if defined(__GNUC__) && defined(__GNUC_MINOR__)
#if __GNUC__ >= 5 && __GNUC_MINOR__ >= 1
#define add(a, b, c) _Generic((c),                                      \
                              int *: int_add(a,b,c),                    \
                              long *: long_add(a,b,c),                  \
                              unsigned long *: ulong_add(a,b,c))
#define mult(a, b, c) _Generic((c),                                     \
                              long *: long_mult(a,b,c),                 \
                              unsigned long *: ulong_mult(a,b,c))
#endif
#endif

#ifndef add
#define add(a, b, c) ({                                                 \
                (*(c)) = ((a) + (b));                                   \
                })
#endif
#ifndef mult
#define mult(a, b, c) ({                                                \
                (*(c)) = ((a) * (b));                                   \
                })
#endif

static inline int UNUSED
read_file(int fd, uint8_t **result, size_t *bufsize)
{
        uint8_t *p;
        size_t size = 4096;
        size_t filesize = 0;
        ssize_t s = 0;
        uint8_t *buf, *newbuf;

        if (!(newbuf = calloc(size, sizeof (uint8_t)))) {
                efi_error("could not allocate memory");
                *result = buf = NULL;
                *bufsize = 0;
                return -1;
        }
        buf = newbuf;

        do {
                p = buf + filesize;
                /* size - filesize shouldn't exceed SSIZE_MAX because we're
                 * only allocating 4096 bytes at a time and we're checking that
                 * before doing so. */
                s = read(fd, p, size - filesize);
                if (s < 0 && errno == EAGAIN) {
                        /*
                         * if we got EAGAIN, there's a good chance we've hit
                         * the kernel rate limiter.  Doing more reads is just
                         * going to make it worse, so instead, give it a rest.
                         */
                        sched_yield();
                        continue;
                } else if (s < 0) {
                        int saved_errno = errno;
                        free(buf);
                        *result = buf = NULL;
                        *bufsize = 0;
                        errno = saved_errno;
                        efi_error("could not read from file");
                        return -1;
                }
                filesize += s;
                /* only exit for empty reads */
                if (s == 0)
                        break;
                if (filesize >= size) {
                        /* See if we're going to overrun and return an error
                         * instead. */
                        if (size > (size_t)-1 - 4096) {
                                free(buf);
                                *result = buf = NULL;
                                *bufsize = 0;
                                errno = ENOMEM;
                                efi_error("could not read from file");
                                return -1;
                        }
                        newbuf = realloc(buf, size + 4096);
                        if (newbuf == NULL) {
                                int saved_errno = errno;
                                free(buf);
                                *result = buf = NULL;
                                *bufsize = 0;
                                errno = saved_errno;
                                efi_error("could not allocate memory");
                                return -1;
                        }
                        buf = newbuf;
                        memset(buf + size, '\0', 4096);
                        size += 4096;
                }
        } while (1);

        newbuf = realloc(buf, filesize+1);
        if (!newbuf) {
                free(buf);
                *result = buf = NULL;
                efi_error("could not allocate memory");
                return -1;
        }
        newbuf[filesize] = '\0';
        *result = newbuf;
        *bufsize = filesize+1;
        return 0;
}

static inline uint64_t UNUSED
lcm(uint64_t x, uint64_t y)
{
        uint64_t m = x, n = y, o;
        while ((o = m % n)) {
                m = n;
                n = o;
        }
        return (x / n) * y;
}

#ifndef strndupa
#define strndupa(s, l)                                                  \
       (__extension__ ({                                                \
                const char *__in = (s);                                 \
                size_t __len = strnlen (__in, (l));                     \
                char *__out = (char *) alloca (__len + 1);              \
                strncpy(__out, __in, __len);                            \
                __out[__len] = '\0';                                    \
                __out;                                                  \
        }))
#endif

#define asprintfa(str, fmt, args...)                                    \
        ({                                                              \
                char *_tmp = NULL;                                      \
                int _rc;                                                \
                *(str) = NULL;                                          \
                _rc = asprintf((str), (fmt), ## args);                  \
                if (_rc > 0) {                                          \
                        _tmp = strdupa(*(str));                         \
                        if (!_tmp) {                                    \
                                _rc = -1;                               \
                        } else {                                        \
                                free(*(str));                           \
                                *(str) = _tmp;                          \
                        }                                               \
                } else {                                                \
                        _rc = -1;                                       \
                }                                                       \
                _rc;                                                    \
        })

#define vasprintfa(str, fmt, ap)                                        \
        ({                                                              \
                char *_tmp = NULL;                                      \
                int _rc;                                                \
                *(str) = NULL;                                          \
                _rc = vasprintf((str), (fmt), (ap));                    \
                if (_rc > 0) {                                          \
                        _tmp = strdupa(*(str));                         \
                        if (!_tmp) {                                    \
                                _rc = -1;                               \
                        } else {                                        \
                                free(*(str));                           \
                                *(str) = _tmp;                          \
                        }                                               \
                } else {                                                \
                        _rc = -1;                                       \
                }                                                       \
                _rc;                                                    \
        })

static inline ssize_t
get_file(uint8_t **result, const char * const fmt, ...)
{
        char *path;
        uint8_t *buf = NULL;
        size_t bufsize = 0;
        ssize_t rc;
        va_list ap;
        int error;
        int fd;

        if (result == NULL) {
                efi_error("invalid parameter 'result'");
                return -1;
        }

        va_start(ap, fmt);
        rc = vasprintfa(&path, fmt, ap);
        va_end(ap);
        if (rc < 0) {
                efi_error("could not allocate memory");
                return -1;
        }

        fd = open(path, O_RDONLY);
        if (fd < 0) {
                efi_error("could not open file \"%s\" for reading",
                          path);
                return -1;
        }

        rc = read_file(fd, &buf, &bufsize);
        error = errno;
        close(fd);
        errno = error;

        if (rc < 0 || bufsize < 1) {
                /*
                 * I don't think this can happen, but I can't convince
                 * cov-scan
                 */
                if (buf)
                        free(buf);
                *result = NULL;
                efi_error("could not read file \"%s\"", path);
                return -1;
        }

        *result = buf;
        return bufsize;
}

static inline void UNUSED
swizzle_guid_to_uuid(efi_guid_t *guid)
{
        uint32_t *u32;
        uint16_t *u16;

        u32 = (uint32_t *)guid;
        u32[0] = __builtin_bswap32(u32[0]);
        u16 = (uint16_t *)&u32[1];
        u16[0] = __builtin_bswap16(u16[0]);
        u16[1] = __builtin_bswap16(u16[1]);
}

#define log_(file, line, func, level, fmt, args...)                     \
        ({                                                              \
                if (efi_get_verbose() >= level) {                       \
                        FILE *logfile_ = efi_get_logfile();             \
                        int len_ = strlen(fmt);                         \
                        fprintf(logfile_, "%s:%d %s(): ",               \
                                file, line, func);                      \
                        fprintf(logfile_, fmt, ## args);                \
                        if (!len_ || fmt[len_ - 1] != '\n')             \
                                fprintf(logfile_, "\n");                \
                }                                                       \
        })

#define LOG_VERBOSE 0
#define LOG_DEBUG 1
#ifdef log
#undef log
#endif
#define log(level, fmt, args...) log_(__FILE__, __LINE__, __func__, level, fmt, ## args)
#define arrow(l,b,o,p,n,m) ({if(n==m){char c_=b[p+1]; b[o]='^'; b[p+o]='^';b[p+o+1]='\0';log(l,"%s",b);b[o]=' ';b[p+o]=' ';b[p+o+1]=c_;}})
#define debug(fmt, args...) log(LOG_DEBUG, fmt, ## args)

#endif /* EFIVAR_UTIL_H */
