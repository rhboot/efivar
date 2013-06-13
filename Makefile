TOPDIR = $(shell echo $$PWD)

SUBDIRS := src docs
VERSION := 0.1

all : $(SUBDIRS)

$(SUBDIRS) :
	$(MAKE) -C $@ TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/$@/ ARCH=$(ARCH)

clean :
	@for x in $(SUBDIRS) ; do make -C $${x} TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/$@/ ARCH=$(ARCH) $@ ; done

install :
	@for x in $(SUBDIRS) ; do make -C $${x} TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/$@/ ARCH=$(ARCH) $@ ; done

test : all
	@for x in $(SUBDIRS) ; do make -C $${x} TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/$@/ ARCH=$(ARCH) $@ ; done

.PHONY: $(SUBDIRS) all clean install test

include $(TOPDIR)/Make.defaults
include $(TOPDIR)/Make.rules

GITTAG = $(VERSION)

test-archive:
	@rm -rf /tmp/libefivar-$(VERSION) /tmp/libefivar-$(VERSION)-tmp
	@mkdir -p /tmp/libefivar-$(VERSION)-tmp
	@git archive --format=tar $(shell git branch | awk '/^*/ { print $$2 }') | ( cd /tmp/libefivar-$(VERSION)-tmp/ ; tar x )
	@git diff | ( cd /tmp/libefivar-$(VERSION)-tmp/ ; patch -s -p1 -b -z .gitdiff )
	@mv /tmp/libefivar-$(VERSION)-tmp/ /tmp/libefivar-$(VERSION)/
	@dir=$$PWD; cd /tmp; tar -c --bzip2 -f $$dir/libefivar-$(VERSION).tar.bz2 libefivar-$(VERSION)
	@rm -rf /tmp/libefivar-$(VERSION)
	@echo "The archive is in libefivar-$(VERSION).tar.bz2"

archive:
	git tag $(GITTAG) refs/heads/master
	@rm -rf /tmp/libefivar-$(VERSION) /tmp/libefivar-$(VERSION)-tmp
	@mkdir -p /tmp/libefivar-$(VERSION)-tmp
	@git archive --format=tar $(GITTAG) | ( cd /tmp/libefivar-$(VERSION)-tmp/ ; tar x )
	@mv /tmp/libefivar-$(VERSION)-tmp/ /tmp/libefivar-$(VERSION)/
	@dir=$$PWD; cd /tmp; tar -c --bzip2 -f $$dir/libefivar-$(VERSION).tar.bz2 libefivar-$(VERSION)
	@rm -rf /tmp/libefivar-$(VERSION)
	@echo "The archive is in libefivar-$(VERSION).tar.bz2"


