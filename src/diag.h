// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * diag.h - Set up appropriate -W flags based on which compiler is in use
 * Copyright Peter Jones <pjones@redhat.com>
 */

#ifndef PRIVATE_DIAG_H_
#define PRIVATE_DIAG_H_

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wpointer-bool-conversion"
/*
 * -Wmissing-field-initializers just encourages you to write worse code,
 *  and that's all it's ever done.  It vaguely made sense pre-C99, before
 *  named initializers, but at this point it's just completely nonsense.
 */
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic warning "-Wcpp"
#endif

#if defined(__GNUC__) && !defined(__clang__)
/*
 * nonnull compare is more or less completetly defective; rather than
 * warning that you're *calling* something without ensuring an argument
 * can't be NULL, it complains in the implementation of the callee that
 * you're *checking* for NULL.  Useful behavior would be to complain in the
 * caller if an argument hasn't been properly constrained, and to complain
 * in the callee if either the visibility is public and the variable
 * /isn't/ checked, or the visibility is hidden and it is - i.e. only
 * complain about checking if you can verify every caller.
 */
#if __GNUC__ > 6
#pragma GCC diagnostic ignored "-Wnonnull-compare"
#endif
/*
 * -Wmissing-field-initializers just encourages you to write worse code,
 *  and that's all it's ever done.  It vaguely made sense pre-C99, before
 *  named initializers, but at this point it's just completely nonsense.
 */
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#if defined(EFIVAR_SYNTAX_CHECKING)
#pragma GCC diagnostic ignored "-Wcpp"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-result"
#pragma GCC diagnostic ignored "-Wunused-variable"
#else
#pragma GCC diagnostic warning "-Wcpp"
#pragma GCC diagnostic error "-Wunused-but-set-variable"
#pragma GCC diagnostic error "-Wunused-function"
#pragma GCC diagnostic error "-Wunused-parameter"
#pragma GCC diagnostic error "-Wunused-result"
#pragma GCC diagnostic error "-Wunused-variable"
#endif /* !defined(EFIVAR_SYNTAX_CHECKING) */
#endif

#endif /* !PRIVATE_DIAG_H_ */
// vim:fenc=utf-8:tw=75:noet
