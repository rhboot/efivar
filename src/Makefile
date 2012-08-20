SRCDIR = $(realpath .)
TOPDIR = $(realpath ..)

SONAME := libefivar.so.$(SONAME_VERSION)

include $(TOPDIR)/Make.defaults

TARGETS = libefivar.so.0 libefivar.a
INCTARGETS = efivar.h
all : $(TARGETS)

SOURCES = $(patsubst %.c,%,$(wildcard *.c))
OBJECTS = $(foreach source,$(SOURCES),$(source).o)

libefivar.a :: $(OBJECTS)

libefivar.so.$(SONAME_VERSION) :: $(OBJECTS)

clean : 
	@rm -rfv *~ *.o *.a *.so *.so.$(SONAME_VERSION)
	@make -C test TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/src/ $@

install : all
	$(INSTALL) -d -m 755 $(INSTALLROOT)$(LIBDIR)
	$(foreach x, $(TARGETS), $(INSTALL) -m 755 $(x) $(INSTALLROOT)$(LIBDIR);)
	$(INSTALL) -d -m 755 $(INSTALLROOT)$(INCDIR)
	$(foreach x, $(INCTARGETS), $(INSTALL) -m 644 $(x) $(INSTALLROOT)$(INCDIR);)
	$(foreach x, $(wildcard *.so.$(SONAME_VERSION)), ln -s $(x) $(patsubst %.so.$(SONAME_VERSION),%.so,$(INSTALLROOT)$(LIBDIR)$(x));)

test :all
	make -C test TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/src/ $@

.PHONY: all clean install test

include $(TOPDIR)/Make.rules
