SRCDIR = $(realpath .)
TOPDIR = $(realpath ..)

SONAME = libefivar.so.$(SONAME_VERSION)

include $(TOPDIR)/Make.defaults

LIBTARGETS = libefivar.so.0
PCTARGETS = efivar.pc
BINTARGETS = efivar
INCTARGETS = efivar.h
all : $(LIBTARGETS) $(PCTARGETS) $(BINTARGETS) $(INCTARGETS)
	@$(MAKE) -C test TOPDIR=$(TOPDIR) SRCDIR=$(SRCDIR)/test $@

OBJECTS = lib.o vars.o efivarfs.o guid.o
DEPS = .lib.c.P .efivar.c.P .efivar.h.P .vars.c.P .lib.h.P \
	.generics.h.P .guid.h.P .guid.c.P

libefivar.a :: $(OBJECTS)

libefivar.so.$(SONAME_VERSION) :: $(OBJECTS)

efivar : efivar.o libefivar.so
	$(CC) $(CCLDFLAGS) -L. -lefivar -o $@ $^ -lpopt

efivar.pc : efivar.pc.in
	sed -e "s,@@VERSION@@,$(VERSION),g" \
	    -e "s,@@LIBDIR@@,$(libdir),g" \
		efivar.pc.in > efivar.pc

deps : $(DEPS)

-include $(DEPS)

clean : 
	@rm -rfv *~ *.o *.a *.so *.so.$(SONAME_VERSION) .*.c.P .*.h.P $(PCTARGETS) $(BINTARGETS)
	@$(MAKE) -C test TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/src/ $@

install : all
	$(INSTALL) -d -m 755 $(DESTDIR)$(libdir)
	$(foreach x, $(LIBTARGETS), $(INSTALL) -m 755 $(x) $(DESTDIR)$(libdir);)
	$(INSTALL) -d -m 755 $(DESTDIR)$(PCDIR)
	$(foreach x, $(PCTARGETS), $(INSTALL) -m 644 $(x) $(DESTDIR)$(PCDIR) ;)
	$(INSTALL) -d -m 755 $(DESTDIR)$(includedir)
	$(foreach x, $(INCTARGETS), $(INSTALL) -m 644 $(x) $(DESTDIR)$(includedir);)
	$(INSTALL) -d -m 755 $(DESTDIR)$(bindir)
	$(foreach x, $(BINTARGETS), $(INSTALL) -m 755 $(x) $(DESTDIR)$(bindir);)
	$(foreach x, $(wildcard *.so.$(SONAME_VERSION)), ln -s $(x) $(patsubst %.so.$(SONAME_VERSION),%.so,$(DESTDIR)$(libdir)/$(x));)

test :all
	$(MAKE) -C test TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/src/ $@

.PHONY: all deps clean install test

include $(TOPDIR)/Make.rules
