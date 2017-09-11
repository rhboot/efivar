TOPDIR = $(shell echo $$PWD)

include $(TOPDIR)/Make.version
include $(TOPDIR)/Make.rules
include $(TOPDIR)/Make.defaults

SUBDIRS := src docs

all : | efivar.spec Make.version
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

$(SUBDIRS) :
	$(MAKE) -C $@

brick : all
	@set -e ; for x in $(SUBDIRS) ; do $(MAKE) -C $${x} test ; done

a :
	@if [ $${EUID} != 0 ]; then \
		echo no 1>&2 ; \
		exit 1 ; \
	fi

.PHONY: $(SUBDIRS) a brick coverity cov-upload

GITTAG = $(shell bash -c "echo $$(($(VERSION) + 1))")

efivar.spec : | Makefile Make.version

COV_EMAIL=$(call get-config,coverity.email)
COV_TOKEN=$(call get-config,coverity.token)
COV_URL=$(call get-config,coverity.url)
COV_FILE=efivar-coverity-$(GITTAG)-$(COMMIT_ID).tar.bz2
COMMIT_ID=$(shell git log -1 --pretty=%H 2>/dev/null || echo master)

cov-int : clean
	cov-build --dir cov-int make all

$(COV_FILE) : cov-int
	tar caf $@ cov-int

cov-upload :
	@if [[ -n "$(COV_URL)" ]] &&					\
	    [[ -n "$(COV_TOKEN)" ]] &&					\
	    [[ -n "$(COV_EMAIL)" ]] ;					\
	then								\
		echo curl --form token=$(COV_TOKEN) --form email="$(COV_EMAIL)" --form file=@"$(COV_FILE)" --form version=$(GITTAG).1 --form description="$(COMMIT_ID)" "$(COV_URL)" ; \
		curl --form token=$(COV_TOKEN) --form email="$(COV_EMAIL)" --form file=@"$(COV_FILE)" --form version=$(GITTAG).1 --form description="$(COMMIT_ID)" "$(COV_URL)" ; \
	else								\
		echo Coverity output is in $(COV_FILE) ;		\
	fi

coverity : $(COV_FILE) cov-upload

clean :
	@set -e ; for x in $(SUBDIRS) ; do \
		$(MAKE) -C $$x $@ ; \
	done
	@rm -vf efivar.spec
	@rm -vrf cov-int efivar-coverity-*.tar.*

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
	@echo VERSION=$(GITTAG) > Make.version
	@git add Make.version
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


