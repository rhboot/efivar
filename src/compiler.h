// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * compiler.h - compiler related macros
 * Copyright Peter Jones <pjones@redhat.com>
 */

#pragma once

/* GCC version checking borrowed from glibc. */
#if defined(__GNUC__) && defined(__GNUC_MINOR__)
#  define GNUC_PREREQ(maj,min) \
	((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
#  define GNUC_PREREQ(maj,min) 0
#endif

/* Does this compiler support compile-time error attributes? */
#if GNUC_PREREQ(4,3)
#  define ATTRIBUTE_ERROR(msg) \
	__attribute__ ((__error__ (msg)))
#else
#  define ATTRIBUTE_ERROR(msg) __attribute__ ((noreturn))
#endif

#if GNUC_PREREQ(4,4)
#  define GNU_PRINTF gnu_printf
#else
#  define GNU_PRINTF printf
#endif

#if GNUC_PREREQ(3,4)
#  define WARN_UNUSED_RESULT __attribute__ ((warn_unused_result))
#else
#  define WARN_UNUSED_RESULT
#endif

#if defined(__clang__) && defined(__clang_major__) && defined(__clang_minor__)
#  define CLANG_PREREQ(maj,min) \
          ((__clang_major__ > (maj)) || \
	   (__clang_major__ == (maj) && __clang_minor__ >= (min)))
#else
#  define CLANG_PREREQ(maj,min) 0
#endif

#define UNUSED __attribute__((__unused__))
#define HIDDEN __attribute__((__visibility__ ("hidden")))
#define PUBLIC __attribute__((__visibility__ ("default")))
#define DESTRUCTOR __attribute__((__destructor__))
#define CONSTRUCTOR __attribute__((__constructor__))
#define ALIAS(x) __attribute__((weak, alias (#x)))
#define NONNULL(...) __attribute__((__nonnull__(__VA_ARGS__)))
#define PRINTF(...) __attribute__((__format__(printf, __VA_ARGS__)))
#define FLATTEN __attribute__((__flatten__))
#define PACKED __attribute__((__packed__))
#if defined(__clang__)
# define VERSION(sym, ver)
#else
# if GNUC_PREREQ(10,0)
#  define VERSION(sym, ver) __attribute__ ((symver (# ver)))
# else
#  define VERSION(sym, ver) __asm__(".symver " # sym "," # ver);
# endif
#endif
#define NORETURN __attribute__((__noreturn__))
#define ALIGNED(n) __attribute__((__aligned__(n)))
#define CLEANUP_FUNC(x) __attribute__((__cleanup__(x)))

#define __CONCAT3(a, b, c) a ## b ## c
#define CONCATENATE(a, b) __CONCAT(a, b)
#define CAT(a, b) __CONCAT(a, b)
#define CAT3(a, b, c) __CONCAT3(a, b, c)
#define STRING(x) __STRING(x)

#define __ALIGN_MASK(x, mask)   (((x) + (mask)) & ~(mask))
#define __ALIGN(x, a)           __ALIGN_MASK(x, (typeof(x))(a) - 1)
#define ALIGN(x, a)             __ALIGN((x), (a))
#define ALIGN_DOWN(x, a)        __ALIGN((x) - ((a) - 1), (a))

#define ALIGNMENT_PADDING(value, align) ((align - (value % align)) % align)
#define ALIGN_UP(value, align) ((value) + ALIGNMENT_PADDING(value, align))

#if GNUC_PREREQ(5, 1) || CLANG_PREREQ(3, 8)
#define checked_add(addend0, addend1, sum) \
	__builtin_add_overflow(addend0, addend1, sum)
#define checked_sub(minuend, subtrahend, difference) \
	__builtin_sub_overflow(minuend, subtrahend, difference)
#define checked_mul(factor0, factor1, product) \
	__builtin_mul_overflow(factor0, factor1, product)
#else
#define checked_add(a0, a1, s)		\
	({				\
		(*s) = ((a0) + (a1));   \
		0;			\
	})
#define checked_sub(s0, s1, d)		\
	({				\
		(*d) = ((s0) - (s1));   \
		0;			\
	})
#define checked_mul(f0, f1, p)		\
	({				\
		(*p) = ((f0) * (f1));   \
		0;			\
	})
#endif

#define checked_div(dividend, divisor, quotient)                \
        ({                                                      \
                bool _ret = True;                               \
                if ((divisor) != 0) {                           \
                        _ret = False;                           \
                        (quotient) = (dividend) / (divisor);    \
                }                                               \
                _ret;                                           \
        })

// vim:fenc=utf-8:tw=75:noet
