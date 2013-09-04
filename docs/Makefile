SRCDIR = .
TOPDIR = $(SRCDIR)/..

include $(TOPDIR)/Make.defaults

MAN1TARGETS = efivar.1
MAN3TARGETS = efi_get_variable_size.3 efi_get_variable_attributes.3 \
	     efi_get_variable.3 efi_del_variable.3 efi_set_variable.3 \
	     efi_append_variable.3 efi_get_next_variable_name.3 \
	     efi_str_to_guid.3 efi_guid_to_str.3

all :

clean :

install :
	$(INSTALL) -d -m 755 $(DESTDIR)$(mandir)/man1
	$(foreach x, $(MAN1TARGETS), $(INSTALL) -m 644 $(x) $(DESTDIR)/$(mandir)/man1/;)
	$(INSTALL) -d -m 755 $(DESTDIR)$(mandir)/man3
	$(foreach x, $(MAN3TARGETS), $(INSTALL) -m 644 $(x) $(DESTDIR)/$(mandir)/man3/;)

.PHONY: all clean install

include $(TOPDIR)/Make.rules
