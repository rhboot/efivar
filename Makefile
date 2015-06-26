TOPDIR = $(shell echo $$PWD)

include $(TOPDIR)/Make.version

SUBDIRS := src docs

all : $(SUBDIRS) efivar.spec

efivar efivar-static :
	$(MAKE) -C src TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/$@/ ARCH=$(ARCH) $@

$(SUBDIRS) :
	$(MAKE) -C $@ TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/$@/ ARCH=$(ARCH)

clean :
	@set -e ; for x in $(SUBDIRS) ; do $(MAKE) -C $${x} TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/$@/ ARCH=$(ARCH) $@ ; done
	@rm -vf efivar.spec

install :
	@set -e ; for x in $(SUBDIRS) ; do $(MAKE) -C $${x} TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/$@/ ARCH=$(ARCH) DESTDIR=$(DESTDIR) includedir=$(includedir) bindir=$(bindir) libdir=$(libdir) PCDIR=$(PCDIR) $@ ; done

brick : all
	@set -e ; for x in $(SUBDIRS) ; do $(MAKE) -C $${x} TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/$@/ ARCH=$(ARCH) test ; done

a :
	@if [ $${EUID} != 0 ]; then \
		echo no 1>&2 ; \
		exit 1 ; \
	fi

.PHONY: $(SUBDIRS) all clean install a brick

include $(TOPDIR)/Make.defaults
include $(TOPDIR)/Make.rules

efivar.spec : efivar.spec.in Makefile
	@sed -e "s,@@VERSION@@,$(VERSION),g" $< > $@

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


