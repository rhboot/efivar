SRCDIR = $(realpath .)
TOPDIR = $(realpath ..)

SONAME = libefivar.so.$(SONAME_VERSION)

include $(TOPDIR)/Make.defaults

TARGETS = libefivar.so.0 libefivar.a efivar.pc
# TARGETS += efivar
INCTARGETS = efivar.h
all : $(TARGETS)
	@make -C test TOPDIR=$(TOPDIR) SRCDIR=$(SRCDIR)/test $@

SOURCES = lib
OBJECTS = lib.o vars.o efivarfs.o
DEPS = .lib.c.P .efivar.c.P .efivar.h.P .vars.c.P .lib.h.P \
	.generic_next_variable_name.h.P

libefivar.a :: $(OBJECTS)

libefivar.so.$(SONAME_VERSION) :: $(OBJECTS)

efivar : efivar.c libefivar.so
	$(CC) $(CFLAGS) $(CCLDFLAGS) -L. -lefivar -o $@ $^

deps : $(DEPS)

-include $(DEPS)

clean : 
	@rm -rfv *~ *.o *.a *.so *.so.$(SONAME_VERSION) .*.c.P .*.h.P
	@make -C test TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/src/ $@

install : all
	$(INSTALL) -d -m 755 $(INSTALLROOT)$(LIBDIR)
	$(foreach x, $(TARGETS), $(INSTALL) -m 755 $(x) $(INSTALLROOT)$(LIBDIR);)
	$(INSTALL) -d -m 755 $(INSTALLROOT)$(LIBDIR)pkgconfig
	sed -e "s,@@VERSION@@,$(VERSION),g" \
	    -e "s,@@LIBDIR@@,$(LIBDIR),g" \
		efivar.pc > $(INSTALLROOT)$(LIBDIR)pkgconfig/efivar.pc

	$(INSTALL) -d -m 755 $(INSTALLROOT)$(INCDIR)
	$(foreach x, $(INCTARGETS), $(INSTALL) -m 644 $(x) $(INSTALLROOT)$(INCDIR);)
	$(foreach x, $(wildcard *.so.$(SONAME_VERSION)), ln -s $(x) $(patsubst %.so.$(SONAME_VERSION),%.so,$(INSTALLROOT)$(LIBDIR)$(x));)

test :all
	make -C test TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/src/ $@

.PHONY: all deps clean install test

include $(TOPDIR)/Make.rules