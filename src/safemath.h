// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * safemath.h
 * Copyright 2016-2019 Peter Jones <pjones@redhat.com>
 */

#ifndef SAFEMATH_H_
#define SAFEMATH_H_

/*
 * I'm not actually sure when these appear, but they're present in the
 * version in front of me.
 */
#if defined(__GNUC__) && defined(__GNUC_MINOR__)
#if __GNUC__ >= 5 && __GNUC_MINOR__ >= 1
#define int_add(a, b, c) __builtin_add_overflow(a, b, c)
#define uint_add(a, b, c) __builtin_add_overflow(a, b, c)
#define long_add(a, b, c) __builtin_add_overflow(a, b, c)
#define ulong_add(a, b, c) __builtin_add_overflow(a, b, c)

#define int_mul(a, b, c) __builtin_mul_overflow(a, b, c)
#define uint_mul(a, b, c) __builtin_mul_overflow(a, b, c)
#define long_mul(a, b, c) __builtin_mul_overflow(a, b, c)
#define ulong_mul(a, b, c) __builtin_mul_overflow(a, b, c)

#define int_sub(a, b, c) __builtin_sub_overflow(a, b, c)
#define uint_sub(a, b, c) __builtin_sub_overflow(a, b, c)
#define long_sub(a, b, c) __builtin_sub_overflow(a, b, c)
#define ulong_sub(a, b, c) __builtin_sub_overflow(a, b, c)
#endif
#endif

#ifndef int_add
#define int_add(a, b, c) ({					\
		const int _limit = INT_MAX;			\
		long int _ret = _limit - (a);			\
		_ret = _ret > (b);				\
		if (!_ret)					\
			*(c) = ((a) + (b));			\
		(bool)_ret;					\
	})
#endif

#ifndef uint_add
#define uint_add(a, b, c) ({					\
		const unsigned int _limit = UINT_MAX;		\
		unsigned int _ret = _limit - (a);		\
		_ret = _ret > (b);				\
		if (!_ret)					\
			*(c) = ((a) + (b));			\
		(bool)_ret;					\
	})
#endif

#ifndef long_add
#define long_add(a, b, c) ({					\
		const long _limit = LONG_MAX;			\
		long _ret = _limit - (a);			\
		_ret = _ret > (b);				\
		if (!_ret)					\
			*(c) = ((a) + (b));			\
		(bool)_ret;					\
	})
#endif

#ifndef ulong_add
#define ulong_add(a, b, c) ({					\
		const unsigned long _limit = ULONG_MAX;		\
		unsigned long _ret = _limit - (a);		\
		_ret = _ret > (b);				\
		if (!_ret)					\
			*(c) = ((a) + (b));			\
		(bool)_ret;					\
	})
#endif

#ifndef int_mul
#define int_mul(a, b, c) ({						\
		int _ret;						\
		_ret = __builtin_popcount(a) + __builtin_popcount(b);	\
		_ret = _ret < ((sizeof(a) + sizeof(b)) << 4);		\
		if (!_ret)						\
			*(c) = ((a) * (b));				\
		(bool)_ret;						\
	})
#endif

#ifndef uint_mul
#define uint_mul(a, b, c) int_mul(a, b, c)
#endif

#ifndef long_mul
#define long_mul(a, b, c) int_mul(a, b, c)
#endif

#ifndef ulong_mul
#define ulong_mul(a, b, c) int_mul(a, b, c)
#endif

#ifndef int_sub
#define int_sub(a, b, c) ({					\
		const long _min_limit = INT_MIN;		\
		const long _max_limit = INT_MAX;		\
		int _ret;					\
		_ret = _min_limit + (b);			\
		_ret = !(_ret < (a));				\
		if (!_ret) {					\
			_ret = _max_limit - (a);		\
			_ret = _ret > (b);			\
		}						\
		if (!_ret)					\
			*(c) = ((a) - (b));			\
		(bool)_ret;					\
	})
#endif

#ifndef uint_sub
#define uint_sub(a, b, c) ({					\
		const unsigned int _limit = UINT_MAX;		\
		unsigned int _ret = _limit - (a);		\
		_ret = _ret > (b);				\
		if (!_ret)					\
			*(c) = ((a) - (b));			\
		(bool)_ret;					\
	})
#endif

#ifndef long_sub
#define long_sub(a, b, c) ({					\
		const long _min_limit = LONG_MIN;		\
		const long _max_limit = LONG_MAX;		\
		int _ret;					\
		_ret = _min_limit + (b);			\
		_ret = !(_ret < (a));				\
		if (!_ret) {					\
			_ret = _max_limit - (a);		\
			_ret = _ret > (b);			\
		}						\
		if (!_ret)					\
			*(c) = ((a) - (b));			\
		(bool)_ret;					\
	})
#endif

#ifndef ulong_sub
#define ulong_sub(a, b, c) ({					\
		const unsigned long _limit = ULONG_MAX;		\
		unsigned long _ret = _limit - (a);		\
		_ret = _ret > (b);				\
		if (!_ret)					\
			*(c) = ((a) - (b));			\
		_ret;						\
	})
#endif

#if defined(__GNUC__) && defined(__GNUC_MINOR__)
#if __GNUC__ >= 5 && __GNUC_MINOR__ >= 1
#define add(a, b, c) _Generic((c),					\
			      int *: int_add(a, b, c),			\
			      unsigned int *: uint_add(a, b, c),	\
			      long *: long_add(a, b, c),		\
			      unsigned long *: ulong_add(a, b, c))
#define sub(a, b, c) _Generic((c),					\
			      int *: int_sub(a, b, c),			\
			      unsigned int *: uint_sub(a, b, c),	\
			      long *: long_sub(a, b, c),		\
			      unsigned long *: ulong_sub(a, b, c))
#define mul(a, b, c) _Generic((c),					\
			      int *: int_sub(a, b, c),			\
			      unsigned int *: uint_mul(a, b, c),	\
			      long *: long_mul(a, b, c),		\
			      unsigned long *: ulong_mul(a, b, c))
#endif
#endif

#ifndef add
#define add(a, b, c) ({						\
		(*(c)) = ((a) + (b));				\
		})
#endif
#ifndef mul
#define mul(a, b, c) ({						\
		(*(c)) = ((a) * (b));				\
		})
#endif
#ifndef sub
#define sub(a, b, c) ({						\
		(*(c)) = ((a) - (b));				\
		})
#endif


#endif /* !SAFEMATH_H_ */
// vim:fenc=utf-8:tw=75:noet
