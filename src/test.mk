# SPDX-License-Identifier: BSD-2-Clause-Patent
#
# test.mk - makefile to make local test programs
#

.SUFFIXES:

CC		?= gcc
override CFLAGS	+= -I$(TOPDIR)/src/include -iquote $(TOPDIR)/src/include -D_GNU_SOURCE
VALGRIND	?=
DEBUG_PRINTS	?= 0

tests := $(patsubst %.c,%,$(wildcard test-*.c))

test-dp_OBJECTS = $(LIBEFIVAR_OBJECTS)
test-dp_LIBS = -lcrypto

$(tests) :: test-% : |

$(tests) :: test-% : test-%.c $(test-%_FILES)
	$(quiet)$(CC) $(CFLAGS) -o $@ $(sort $^ $($@_FILES)) $($@_OBJECTS) $($@_LIBS)
	$(quiet)$(if $(findstring clang,$(CC)),LLVM_PROFILE_FILE="$@.profraw") $(VALGRIND) ./$@

test : $(tests)
	$(quiet)$(MAKE) -f test.mk test-clean

test-clean :
	$(quiet)rm $(rmverbose) -f vgcore.* $(tests) *.gcda *.gcno *.gcov *.profraw

clean : test-clean

all : test-clean test

.PHONY: $(tests) all test clean

# vim:ft=make
