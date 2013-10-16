TOPDIR = $(shell echo $$PWD)

SUBDIRS := src docs
VERSION := 0.7

all : $(SUBDIRS)

$(SUBDIRS) :
	$(MAKE) -C $@ TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/$@/ ARCH=$(ARCH) VERSION=$(VERSION)

clean :
	@for x in $(SUBDIRS) ; do $(MAKE) -C $${x} TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/$@/ ARCH=$(ARCH) $@ ; done

install :
	@for x in $(SUBDIRS) ; do $(MAKE) -C $${x} TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/$@/ ARCH=$(ARCH) VERSION=$(VERSION) DESTDIR=$(DESTDIR) includedir=$(includedir) bindir=$(bindir) libdir=$(libdir) PCDIR=$(PCDIR) $@ ; done

test : all
	@for x in $(SUBDIRS) ; do $(MAKE) -C $${x} TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/$@/ ARCH=$(ARCH) $@ ; done

.PHONY: $(SUBDIRS) all clean install test

include $(TOPDIR)/Make.defaults
include $(TOPDIR)/Make.rules

GITTAG = $(VERSION)

test-archive:
	@rm -rf /tmp/efivar-$(VERSION) /tmp/efivar-$(VERSION)-tmp
	@mkdir -p /tmp/efivar-$(VERSION)-tmp
	@git archive --format=tar $(shell git branch | awk '/^*/ { print $$2 }') | ( cd /tmp/efivar-$(VERSION)-tmp/ ; tar x )
	@git diff | ( cd /tmp/efivar-$(VERSION)-tmp/ ; patch -s -p1 -b -z .gitdiff )
	@mv /tmp/efivar-$(VERSION)-tmp/ /tmp/efivar-$(VERSION)/
	@dir=$$PWD; cd /tmp; tar -c --xz -f $$dir/efivar-$(VERSION).tar.xz efivar-$(VERSION)
	@rm -rf /tmp/efivar-$(VERSION)
	@echo "The archive is in efivar-$(VERSION).tar.xz"

tag:
	git tag $(GITTAG) refs/heads/master

archive: tag
	@rm -rf /tmp/efivar-$(VERSION) /tmp/efivar-$(VERSION)-tmp
	@mkdir -p /tmp/efivar-$(VERSION)-tmp
	@git archive --format=tar $(GITTAG) | ( cd /tmp/efivar-$(VERSION)-tmp/ ; tar x )
	@mv /tmp/efivar-$(VERSION)-tmp/ /tmp/efivar-$(VERSION)/
	@dir=$$PWD; cd /tmp; tar -c --xz -f $$dir/efivar-$(VERSION).tar.xz efivar-$(VERSION)
	@rm -rf /tmp/efivar-$(VERSION)
	@echo "The archive is in efivar-$(VERSION).tar.xz"


