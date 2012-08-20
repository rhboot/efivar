SRCDIR = .
TOPDIR = $(SRCDIR)/..

include $(TOPDIR)/Make.defaults

DOCTARGETS = efi_get_variable_size.3 efi_get_variable_attributes.3 \
	     efi_get_variable.3 efi_del_variable.3 efi_set_variable.3

install :
	$(INSTALL) -d -m 755 $(INSTALLROOT)$(DOCDIR)/man3
	$(foreach x, $(DOCTARGETS), $(INSTALL) -m 644 $(x) $(INSTALLROOT)/$(DOCDIR)/man3/;)

.PHONY: all clean install

include $(TOPDIR)/Make.rules
