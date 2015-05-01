TOPDIR = $(shell echo $$PWD)

SUBDIRS := src docs
VERSION := 0.18

all : $(SUBDIRS) efivar.spec

$(SUBDIRS) :
	$(MAKE) -C $@ TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/$@/ ARCH=$(ARCH) VERSION=$(VERSION)

clean :
	@for x in $(SUBDIRS) ; do $(MAKE) -C $${x} TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/$@/ ARCH=$(ARCH) $@ ; done
	@rm -vf efivar.spec

install :
	@for x in $(SUBDIRS) ; do $(MAKE) -C $${x} TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/$@/ ARCH=$(ARCH) VERSION=$(VERSION) DESTDIR=$(DESTDIR) includedir=$(includedir) bindir=$(bindir) libdir=$(libdir) PCDIR=$(PCDIR) $@ ; done

test : all
	@for x in $(SUBDIRS) ; do $(MAKE) -C $${x} TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/$@/ ARCH=$(ARCH) $@ ; done

.PHONY: $(SUBDIRS) all clean install test

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


