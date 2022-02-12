MANDOC = @mandoc@
if HAVE_MANDOC
dist_man1_MANS =	\
	docs/efivar.1	\
	docs/efisecdb.1

dist_man3_MANS =	\
	docs/efi_append_variable.3		\
	docs/efi_del_variable.3			\
	docs/efidp_make_generic.3		\
	docs/efi_get_next_variable_name.3	\
	docs/efi_get_variable.3			\
	docs/efi_get_variable_attributes.3	\
	docs/efi_get_variable_size.3		\
	docs/efi_guid_to_id_guid.3		\
	docs/efi_guid_to_name.3			\
	docs/efi_guid_to_str.3			\
	docs/efi_guid_to_symbol.3		\
	docs/efi_id_guid_to_guid.3		\
	docs/efi_name_to_guid.3			\
	docs/efi_set_variable.3			\
	docs/efi_str_to_guid.3			\
	docs/efi_symbol_to_guid.3		\
	docs/efi_variable_alloc.3		\
	docs/efi_variable_export.3		\
	docs/efi_variable_free.3		\
	docs/efi_variable_get_attributes.3	\
	docs/efi_variable_get_data.3		\
	docs/efi_variable_get_guid.3		\
	docs/efi_variable_get_name.3		\
	docs/efi_variable_import.3		\
	docs/efi_variable_realize.3		\
	docs/efi_variable_set_attributes.3	\
	docs/efi_variable_set_data.3		\
	docs/efi_variable_set_guid.3		\
	docs/efi_variable_set_name.3		\
	docs/efi_variables_supported.3		\
	docs/efi_variable_t.3
endif

CLEANFILES +=				\
	$(builddir)/docs/.dirstamp	\
	$(builddir)/docs/efisecdb.1

docs/.dirstamp:
	$(MKDIR_P) $(@D)
	touch $@

%.1: %.1.mdoc docs/.dirstamp
	$(MANDOC) -mdoc -Tman -Ios=Linux $< > $@

%.3: %.3.mdoc docs/.dirstamp
	$(MANDOC) -mdoc -Tman -Ios=Linux $< > $@
