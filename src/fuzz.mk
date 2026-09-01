# SPDX-License-Identifier: BSD-2-Clause-Patent
#
# fuzz.mk - makefile to fuzz local test programs
#

.SUFFIXES:

override CC = clang
override CFLAGS	= -I$(TOPDIR)/src/include \
		   -iquote $(TOPDIR)/src/include \
		   -D_GNU_SOURCE \
		   -ggdb -gdwarf-4 -gstrict-dwarf \
		   -fcoverage-mapping \
		   -fprofile-instr-generate \
		   -fsanitize=fuzzer,address
DEBUG_PRINTS	?= 0
FUZZ_ARGS	?=

fuzzers := $(patsubst %.c,%,$(wildcard fuzz-*.c))

fuzz-efisec_LIBS = ./libefisec.a ./libefivar.a

ifneq ($(origin FUZZ_VERBOSE),undefined)
override CFLAGS += -DFUZZ_VERBOSE
endif
ifneq ($(origin MAX_FUZZ_TIME),undefined)
override FUZZ_ARGS += -max_total_time=$(MAX_FUZZ_TIME)
endif
ifneq ($(origin MAX_FUZZ_JOBS),undefined)
override FUZZ_ARGS += -jobs=$(MAX_FUZZ_JOBS)
endif
ifneq ($(origin MAX_FUZZ_RUNS),undefined)
override FUZZ_ARGS += -runs=$(MAX_FUZZ_RUNS)
endif
ifneq ($(origin MAX_FUZZ_LEN),undefined)
override FUZZ_ARGS += -max_len=$(MAX_FUZZ_LEN)
endif
ifeq ($(origin FUZZ_LEAKS),undefined)
override FUZZ_ARGS += -detect_leaks=0
endif
ifeq ($(origin NO_REDUCE_FUZZ),undefined)
override FUZZ_ARGS += -reduce_inputs=1
endif

$(fuzzers) :: fuzz-% : |
$(fuzzers) :: fuzz-% : fuzz-%.c $(fuzz-%_FILES)
	$(quiet)$(CC) $(CFLAGS) -o $@ $(sort $^ $(wildcard $*.c) $(fuzz-$*_FILES)) \
		-L$(TOPDIR)/src $($@_OBJECTS) $($@_LIBS)
	$(quiet)mkdir -p $@-corpus
	$(quiet)cd $@-corpus ; LLVM_PROFILE_FILE="$@.profraw" LD_LIBRARY_PATH=$(TOPDIR)/src ../$@ \
		$(FUZZ_ARGS) \
		../$@-corpus

fuzz : $(fuzzers)
	$(quiet)$(MAKE) -f include/fuzz.mk fuzz-clean

fuzz-clean :
	$(quiet)rm -vf $(fuzzers) fuzz*.log *.gcda *.gcno *.gcov *.profraw

clean : fuzz-clean

all : fuzz-clean fuzz

.PHONY: $(fuzzers) all fuzz clean

