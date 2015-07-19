TOPDIR = $(shell echo $$PWD)

include $(TOPDIR)/Make.version
include $(TOPDIR)/Make.rules
include $(TOPDIR)/Make.defaults

SUBDIRS := src docs

all clean install deps :: | Make.version
	@set -e ; for x in $(SUBDIRS) ; do \
		$(MAKE) -C $$x $@ ; \
	done

all :: efivar.spec

efivar efivar-static :
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

.PHONY: $(SUBDIRS) a brick

efivar.spec : | Makefile Make.version

clean ::
	@rm -vf efivar.spec

GITTAG = $(VERSION)

test-archive: efivar.spec
	@rm -rf /tmp/efivar-$(VERSION) /tmp/efivar-$(VERSION)-tmp
	@mkdir -p /tmp/efivar-$(VERSION)-tmp
	@git archive --format=tar $(shell git branch | awk '/^*/ { print $$2 }') | ( cd /tmp/efivar-$(VERSION)-tmp/ ; tar x )
	@git diff | ( cd /tmp/efivar-$(VERSION)-tmp/ ; patch -s -p1 -b -z .gitdiff )
	@mv /tmp/efivar-$(VERSION)-tmp/ /tmp/efivar-$(VERSION)/
	@cp efivar.spec /tmp/efivar-$(VERSION)/
	@dir=$$PWD; cd /tmp; tar -c --bzip2 -f $$dir/efivar-$(VERSION).tar.bz2 efivar-$(VERSION)
	@rm -rf /tmp/efivar-$(VERSION)
	@echo "The archive is in efivar-$(VERSION).tar.bz2"

tag:
	git tag -s $(GITTAG) refs/heads/master

archive: tag efivar.spec
	@rm -rf /tmp/efivar-$(VERSION) /tmp/efivar-$(VERSION)-tmp
	@mkdir -p /tmp/efivar-$(VERSION)-tmp
	@git archive --format=tar $(GITTAG) | ( cd /tmp/efivar-$(VERSION)-tmp/ ; tar x )
	@mv /tmp/efivar-$(VERSION)-tmp/ /tmp/efivar-$(VERSION)/
	@cp efivar.spec /tmp/efivar-$(VERSION)/
	@dir=$$PWD; cd /tmp; tar -c --bzip2 -f $$dir/efivar-$(VERSION).tar.bz2 efivar-$(VERSION)
	@rm -rf /tmp/efivar-$(VERSION)
	@echo "The archive is in efivar-$(VERSION).tar.bz2"


