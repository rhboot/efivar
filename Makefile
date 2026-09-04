export TOPDIR = $(realpath $(dir $(firstword $(MAKEFILE_LIST))))

include $(TOPDIR)/src/include/deprecated.mk
include $(TOPDIR)/src/include/version.mk
include $(TOPDIR)/src/include/rules.mk
include $(TOPDIR)/src/include/defaults.mk
include $(TOPDIR)/src/include/coverity.mk
include $(TOPDIR)/src/include/scan-build.mk

SUBDIRS := src
ifeq ($(ENABLE_DOCS), 1)
SUBDIRS += docs
endif

all : | efivar.spec src/include/version.mk prep
all clean install prep :
	$(quiet)set -e ; for x in $(SUBDIRS) ; do \
		$(MAKE) -C $$x $@ ; \
	done

abicheck abidw efisecdb efisecdb-static efivar efivar-static static : | all
	$(MAKE) -C src $@

abiupdate :
	$(MAKE) clean all
	$(MAKE) -C src abiclean abixml

$(SUBDIRS) :
	$(MAKE) -C $@ all

brick : all
	$(quiet)echo -n $(info this is the rule for brick PWD:$(PWD) MAKECMDGOALS:$(MAKECMDGOALS))
	$(quiet)set -e ; for x in $(SUBDIRS) ; do \
		$(MAKE) -C $${x} test ; \
	done

a :
	$(quiet)if [ $${EUID} != 0 ]; then \
		echo no 1>&2 ; \
		exit 1 ; \
	fi

GITTAG = $(shell bash -c "echo $$(($(VERSION) + 1))")

efivar.spec : | Makefile src/include/version.mk

clean : clean-toplevel
clean-toplevel:
	$(quiet)rm $(rmverbose) -f efivar.spec vgcore.* core.*
	$(quiet)$(MAKE) -C tests clean

compile_commands.json : Makefile
	$(quiet)make clean
	$(quiet)bear -- make COMPILER=clang test all

TEST_COVERAGE	?=
TEST_LTO	?=

test-clean :
	$(quiet)$(MAKE) -C tests clean
	$(quiet)$(MAKE) -C src test-clean

test : all
	$(quiet)$(MAKE) -C tests "$@"
	$(quiet)$(MAKE) -C src \
		TEST_COVERAGE="$(TEST_COVERAGE)" \
		TEST_LTO="$(TEST_LTO)" \
		test-clean test

UNIT_TESTS = $(patsubst src/%.c,%,$(wildcard src/test-*.c))

$(UNIT_TESTS) : | all
$(UNIT_TESTS) :
	$(quiet)$(MAKE) -C src \
		TEST_COVERAGE="$(TEST_COVERAGE)" \
		TEST_LTO="$(TEST_LTO)" \
		test-clean $@

unit-tests : $(UNIT_TESTS)

test-archive: abicheck efivar.spec
	$(quiet)rm $(rmverbose) -rf /tmp/efivar-$(GITTAG) /tmp/efivar-$(GITTAG)-tmp
	$(quiet)mkdir -p /tmp/efivar-$(GITTAG)-tmp
	$(quiet)git archive --format=tar $(shell git branch | awk '/^*/ { print $$2 }') | ( cd /tmp/efivar-$(GITTAG)-tmp/ ; tar x )
	$(quiet)git diff | ( cd /tmp/efivar-$(GITTAG)-tmp/ ; patch -s -p1 -b -z .gitdiff )
	$(quiet)mv /tmp/efivar-$(GITTAG)-tmp/ /tmp/efivar-$(GITTAG)/
	$(quiet)cp efivar.spec /tmp/efivar-$(GITTAG)/
	$(quiet)dir=$$PWD; cd /tmp; tar -c --bzip2 -f $$dir/efivar-$(GITTAG).tar.bz2 efivar-$(GITTAG)
	$(quiet)rm $(rmverbose) -rf /tmp/efivar-$(GITTAG)
	$(quiet)echo "The archive is in efivar-$(GITTAG).tar.bz2"

bumpver :
	$(quiet)echo VERSION=$(GITTAG) > src/include/version.mk
	$(quiet)git add src/include/version.mk
	$(quiet)git commit -m "Bump version to $(GITTAG)" -s

tag:
	$(quiet)git tag -s $(GITTAG) refs/heads/master

archive: abicheck bumpver abidw tag efivar.spec
	$(quiet)rm -rf /tmp/efivar-$(GITTAG) /tmp/efivar-$(GITTAG)-tmp
	$(quiet)mkdir -p /tmp/efivar-$(GITTAG)-tmp
	$(quiet)git archive --format=tar $(GITTAG) | ( cd /tmp/efivar-$(GITTAG)-tmp/ ; tar x )
	$(quiet)mv /tmp/efivar-$(GITTAG)-tmp/ /tmp/efivar-$(GITTAG)/
	$(quiet)cp efivar.spec /tmp/efivar-$(GITTAG)/
	$(quiet)dir=$$PWD; cd /tmp; tar -c --bzip2 -f $$dir/efivar-$(GITTAG).tar.bz2 efivar-$(GITTAG)
	$(quiet)rm $(rmverbose) -rf /tmp/efivar-$(GITTAG)
	$(quiet)echo "The archive is in efivar-$(GITTAG).tar.bz2"

.PHONY: $(SUBDIRS)
.PHONY: a abiclean abicheck abidw abiupdate all archive
.PHONY: brick bumpver clean clean-toplevel
.PHONY: efivar efivar-static
.PHONY: install prep tag test test-archive
.NOTPARALLEL:
