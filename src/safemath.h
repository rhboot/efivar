// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * safemath.h
 * Copyright 2016-2019 Peter Jones <pjones@redhat.com>
 */

#ifndef SAFEMATH_H_
#define SAFEMATH_H_

#include "compiler.h"

#if GNUC_PREREQ(5, 1) || CLANG_PREREQ(3, 8)

#define ADD(a, b, res) ({						\
	__auto_type a_ = (a);						\
	(((res) == NULL)						\
	       ? __builtin_add_overflow((a), (b), &a_)			\
	       : __builtin_add_overflow((a), (b), (res)));		\
})
#define SUB(a, b, res) ({						\
	__auto_type a_ = (a);						\
	(((res) == NULL)						\
	       ? __builtin_sub_overflow((a), (b), &a_)			\
	       : __builtin_sub_overflow((a), (b), (res)));		\
})
#define MUL(a, b, res) ({						\
	__auto_type a_ = (a);						\
	(((res) == NULL)						\
	       ? __builtin_mul_overflow((a), (b), &a_)			\
	       : __builtin_mul_overflow((a), (b), (res)));		\
})
#define DIV(a, b, res) ({						\
	bool ret_ = true;						\
	if ((b) != 0) {							\
		if (!(res == NULL))					\
			(*(res)) = (a) / (b);				\
		ret_ = false;						\
	} else {							\
		errno = EOVERFLOW; /* no EDIVBYZERO? */			\
	}								\
	ret_;								\
})
/*
 * These really just exists for chaining results easily with || in an expr
 */
#define MOD(a, b, res) ({						\
	if (!(res == NULL))						\
		(*(res)) = (a) % (b);					\
	false;								\
})
#define DIVMOD(a, b, resd, resm) (DIV((a), (b), (resd)) || MOD((a), (b), (resm)))
#define ASSIGN(a, res) ADD((a), (typeof(a)) 0, (res))
#define ABS(val, absval) ({						\
	bool res_;							\
	if (val < 0)							\
		res_ = SUB((typeof(val)) 0, (val), absval);		\
	else								\
		res_ = ASSIGN((val), (absval));				\
	res_;								\
})

#define INCREMENT(a) ADD((a), 1, &(a))
#define DECREMENT(a) SUB((a), 1, &(a))

#define generic_sint_builtin_(op, x)					\
	_Generic((x),							\
		int: CAT(__builtin_, op)(x),				\
		long: CAT33(__builtin_, op, l)(x),			\
		long long: CAT3(__builtin_, op, ll)(x)			\
		)
#define FFS(x) generic_sint_builtin_(ffs, x)
#define CLRSB(x) generic_sint_builtin_(clrsb, x)

#define generic_uint_builtin_(op, x)					\
	_Generic((x),							\
		unsigned int: CAT(__builtin_, op)(x),			\
		unsigned long: CAT3(__builtin_, op, l)(x),		\
		unsigned long long: CAT3(__builtin_, op, ll)(x)		\
		)
#define CLZ(x) generic_uint_builtin_(clz, x)
#define CTZ(x) generic_uint_builtin_(ctz, x)
#define POPCOUNT(x) generic_uint_builtin_(popcount, x)
#define PARITY(x) generic_uint_builtin_(parity, x)

#define int_add(a, b, c) ADD(a, b, c)
#define uint_add(a, b, c) ADD(a, b, c)
#define long_add(a, b, c) ADD(a, b, c)
#define ulong_add(a, b, c) ADD(a, b, c)

#define int_sub(a, b, c) SUB(a, b, c)
#define uint_sub(a, b, c) SUB(a, b, c)
#define long_sub(a, b, c) SUB(a, b, c)
#define ulong_sub(a, b, c) SUB(a, b, c)

#define int_mul(a, b, c) MUL(a, b, c)
#define uint_mul(a, b, c) MUL(a, b, c)
#define long_mul(a, b, c) MUL(a, b, c)
#define ulong_mul(a, b, c) MUL(a, b, c)

#else
#warning gcc 5.1 or newer or clang 3.8 or newer is required
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

#ifndef ADD
#define ADD(a, b, c) ({						\
		(*(c)) = ((a) + (b));				\
		})
#endif
#ifndef MUL
#define MUL(a, b, c) ({						\
		(*(c)) = ((a) * (b));				\
		})
#endif
#ifndef SUB
#define SUB(a, b, c) ({						\
		(*(c)) = ((a) - (b));				\
		})
#endif


#endif /* !SAFEMATH_H_ */
// vim:fenc=utf-8:tw=75:noet
