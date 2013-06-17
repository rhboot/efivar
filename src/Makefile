SRCDIR = $(realpath .)
TOPDIR = $(realpath ..)

SONAME = libefivar.so.$(SONAME_VERSION)

include $(TOPDIR)/Make.defaults

LIBTARGETS = libefivar.so.0
PCTARGETS = efivar.pc
BINTARGETS = efivar
INCTARGETS = efivar.h
all : $(LIBTARGETS) $(PCTARGETS) $(BINTARGETS) $(INCTARGETS)
	@make -C test TOPDIR=$(TOPDIR) SRCDIR=$(SRCDIR)/test $@

OBJECTS = lib.o vars.o efivarfs.o
DEPS = .lib.c.P .efivar.c.P .efivar.h.P .vars.c.P .lib.h.P \
	.generic_next_variable_name.h.P

libefivar.a :: $(OBJECTS)

libefivar.so.$(SONAME_VERSION) :: $(OBJECTS)

efivar : efivar.c libefivar.so
	$(CC) $(CFLAGS) $(CCLDFLAGS) -L. -lefivar -o $@ $^

efivar.pc : efivar.pc.in
	sed -e "s,@@VERSION@@,$(VERSION),g" \
	    -e "s,@@LIBDIR@@,$(LIBDIR),g" \
		efivar.pc.in > efivar.pc

deps : $(DEPS)

-include $(DEPS)

clean : 
	@rm -rfv *~ *.o *.a *.so *.so.$(SONAME_VERSION) .*.c.P .*.h.P
	@make -C test TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/src/ $@

install : all
	$(INSTALL) -d -m 755 $(INSTALLROOT)$(LIBDIR)
	$(foreach x, $(LIBTARGETS), $(INSTALL) -m 755 $(x) $(INSTALLROOT)$(LIBDIR);)
	$(INSTALL) -d -m 755 $(INSTALLROOT)$(PCDIR)
	$(foreach x, $(PCTARGETS), $(INSTALL) -m 644 $(x) $(INSTALLROOT)$(PCDIR) ;)
	$(INSTALL) -d -m 755 $(INSTALLROOT)$(INCDIR)
	$(foreach x, $(INCTARGETS), $(INSTALL) -m 644 $(x) $(INSTALLROOT)$(INCDIR);)
	$(INSTALL) -d -m 755 $(INSTALLROOT)$(BINDIR)
	$(foreach x, $(BINTARGETS), $(INSTALL) -m 644 $(x) $(INSTALLROOT)$(BINDIR);)
	$(foreach x, $(wildcard *.so.$(SONAME_VERSION)), ln -s $(x) $(patsubst %.so.$(SONAME_VERSION),%.so,$(INSTALLROOT)$(LIBDIR)$(x));)

test :all
	make -C test TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/src/ $@

.PHONY: all deps clean install test

include $(TOPDIR)/Make.rules
