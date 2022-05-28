mans = ['efi_append_variable.3', 'efi_del_variable.3', 'efidp_make_generic.3', 'efi_get_next_variable_name.3', 'efi_get_variable.3', 'efi_get_variable_attributes.3', 'efi_get_variable_size.3', 'efi_guid_to_id_guid.3', 'efi_guid_to_name.3', 'efi_guid_to_str.3', 'efi_guid_to_symbol.3', 'efi_id_guid_to_guid.3', 'efi_name_to_guid.3', 'efi_set_variable.3', 'efi_str_to_guid.3', 'efi_symbol_to_guid.3', 'efivar.1', 'efi_variable_alloc.3', 'efi_variable_export.3', 'efi_variable_free.3', 'efi_variable_get_attributes.3', 'efi_variable_get_data.3', 'efi_variable_get_guid.3', 'efi_variable_get_name.3', 'efi_variable_import.3', 'efi_variable_realize.3', 'efi_variable_set_attributes.3', 'efi_variable_set_data.3', 'efi_variable_set_guid.3', 'efi_variable_set_name.3', 'efi_variables_supported.3', 'efi_variable_t.3']

mdocs = ['efisecdb.1.mdoc']

foreach man : mans
  install_man(man)
endforeach

mandoc = find_program('mandoc', required : get_option('convert_mandocs'))

foreach mdoc : mdocs
  if mandoc.found()
    # Unfortunately install_man won't take target or generator output: meson bug #1550
    man = custom_target(mdoc.substring(0, -5),
                        input : mdoc,
                        output : mdoc.substring(0, -5),
                        command : [mandoc, '-mdoc', '-Tman',
                                   '-Ios=Linux', '@INPUT@'],
                        capture : true,
                        install : true,
                        install_dir : join_paths(get_option('mandir'), 'man' + mdoc[-6]))
  else
    # configure_file is used to remove .mdoc extension
    man = configure_file(input : mdoc,
                   output : mdoc.substring(0, -5),
                   copy : true)
    install_man(man)
  endif
endforeach
