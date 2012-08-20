SRCDIR = .
TOPDIR = $(SRCDIR)/..

SONAME = libefivar.so.0

include $(TOPDIR)/Make.defaults

TARGETS = libefivar.so libefivar.a
DOCTARGETS = efi_get_variable_size.3 efi_get_variable_attributes.3 \
	     efi_get_variable.3 efi_del_variable.3 efi_set_variable.3
INCTARGETS = efivar.h
all : $(TARGETS)

SOURCES = $(patsubst %.c,%,$(wildcard *.c))
OBJECTS = $(foreach source,$(SOURCES),$(source).o)

libefivar.a :: $(OBJECTS)

libefivar.so :: $(OBJECTS)

clean : 
	@rm -rfv *~ *.o *.a *.so

install :
	$(INSTALL) -d -m 755 $(INSTALLROOT)$(LIBDIR)
	$(foreach x, $(TARGETS), $(INSTALL) -m 755 $(x) $(INSTALLROOT)$(LIBDIR);)
	$(INSTALL) -d -m 755 $(INSTALLROOT)$(DOCDIR)/man3
	$(foreach x, $(DOCTARGETS), ln manpage.3 $(INSTALLROOT)$(DOCDIR)man3/$x;)
	$(INSTALL) -d -m 755 $(INSTALLROOT)$(INCDIR)
	$(foreach x, $(INCTARGETS), $(INSTALL) -m 644 $(x) $(INSTALLROOT)$(INCDIR);)

.PHONY: all clean install

include $(TOPDIR)/Make.rules
