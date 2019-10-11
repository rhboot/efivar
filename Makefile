export TOPDIR = $(realpath $(dir $(firstword $(MAKEFILE_LIST))))

include $(TOPDIR)/src/include/deprecated.mk
include $(TOPDIR)/src/include/version.mk
include $(TOPDIR)/src/include/rules.mk
include $(TOPDIR)/src/include/defaults.mk
include $(TOPDIR)/src/include/coverity.mk
include $(TOPDIR)/src/include/scan-build.mk

SUBDIRS := src docs

all : | efivar.spec src/include/version.mk
all :
	@set -e ; for x in $(SUBDIRS) ; do \
		$(MAKE) -C $$x $@ ; \
	done

install :
	@set -e ; for x in $(SUBDIRS) ; do \
		$(MAKE) -C $$x $@ ; \
	done

abidw abicheck efivar efivar-static static:
	$(MAKE) -C src $@

abiupdate :
	$(MAKE) clean all
	$(MAKE) -C src abiclean abixml

$(SUBDIRS) :
	$(MAKE) -C $@

brick : all
	@set -e ; for x in $(SUBDIRS) ; do $(MAKE) -C $${x} test ; done

a :
	@if [ $${EUID} != 0 ]; then \
		echo no 1>&2 ; \
		exit 1 ; \
	fi

.PHONY: $(SUBDIRS) a brick abiupdate

GITTAG = $(shell bash -c "echo $$(($(VERSION) + 1))")

efivar.spec : | Makefile src/include/version.mk

clean :
	@set -e ; for x in $(SUBDIRS) ; do \
		$(MAKE) -C $$x $@ ; \
	done
	@rm -vf efivar.spec vgcore.* core.*

test : all
	@$(MAKE) -C tests

test-archive: abicheck efivar.spec
	@rm -rf /tmp/efivar-$(GITTAG) /tmp/efivar-$(GITTAG)-tmp
	@mkdir -p /tmp/efivar-$(GITTAG)-tmp
	@git archive --format=tar $(shell git branch | awk '/^*/ { print $$2 }') | ( cd /tmp/efivar-$(GITTAG)-tmp/ ; tar x )
	@git diff | ( cd /tmp/efivar-$(GITTAG)-tmp/ ; patch -s -p1 -b -z .gitdiff )
	@mv /tmp/efivar-$(GITTAG)-tmp/ /tmp/efivar-$(GITTAG)/
	@cp efivar.spec /tmp/efivar-$(GITTAG)/
	@dir=$$PWD; cd /tmp; tar -c --bzip2 -f $$dir/efivar-$(GITTAG).tar.bz2 efivar-$(GITTAG)
	@rm -rf /tmp/efivar-$(GITTAG)
	@echo "The archive is in efivar-$(GITTAG).tar.bz2"

bumpver :
	@echo VERSION=$(GITTAG) > src/include/version.mk
	@git add src/include/version.mk
	git commit -m "Bump version to $(GITTAG)" -s

tag:
	git tag -s $(GITTAG) refs/heads/master

archive: abicheck bumpver abidw tag efivar.spec
	@rm -rf /tmp/efivar-$(GITTAG) /tmp/efivar-$(GITTAG)-tmp
	@mkdir -p /tmp/efivar-$(GITTAG)-tmp
	@git archive --format=tar $(GITTAG) | ( cd /tmp/efivar-$(GITTAG)-tmp/ ; tar x )
	@mv /tmp/efivar-$(GITTAG)-tmp/ /tmp/efivar-$(GITTAG)/
	@cp efivar.spec /tmp/efivar-$(GITTAG)/
	@dir=$$PWD; cd /tmp; tar -c --bzip2 -f $$dir/efivar-$(GITTAG).tar.bz2 efivar-$(GITTAG)
	@rm -rf /tmp/efivar-$(GITTAG)
	@echo "The archive is in efivar-$(GITTAG).tar.bz2"

.NOTPARALLEL:
