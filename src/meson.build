makeguids_sources = ['makeguids.c', 'util.c']
makeguids = executable('makeguids', makeguids_sources, c_args : ['-DEFIVAR_BUILD_ENVIRONMENT'])

gen_guids = custom_target('gen-guids',
                          input : ['guids.txt'],
                          output : ['guid-symbols.c', 'efivar-guids.h'],
                          command : [makeguids, '@INPUT@', '@OUTPUT0@', '@OUTPUT1@'])


abidw = find_program('abidw', required : get_option('generate_abi_xml'))
abidw_command = [abidw, '--headers-dir', join_paths(meson.source_root(), 'src/include/efivar/'),
                 '--out-file', '@OUTPUT@', '@INPUT@']

lib_efivar_sources = ['crc32.c', 'dp.c', 'dp-acpi.c', 'dp-hw.c', 'dp-media.c', 'dp-message.c', 'efivarfs.c', 'error.c', 'export.c', 'guid.c', 'lib.c', 'vars.c', 'time.c']
lib_efivar = library('efivar', lib_efivar_sources, gen_guids, version : lib_ver)
lib_efivar_xml = custom_target('efivar_xml', input : [lib_efivar], output: ['libefivar.abixml'],
                               command : abidw_command)

lib_efiboot_sources = ['crc32.c', 'creator.c', 'disk.c', 'gpt.c', 'loadopt.c', 'path-helpers.c', 'linux.c', 'linux-acpi.c', 'linux-acpi-root.c', 'linux-ata.c', 'linux-emmc.c', 'linux-i2o.c', 'linux-md.c', 'linux-nvme.c', 'linux-pci.c', 'linux-pci-root.c', 'linux-pmem.c', 'linux-sas.c', 'linux-sata.c', 'linux-scsi.c', 'linux-soc-root.c', 'linux-virtblk.c', 'linux-virtual-root.c']
lib_efiboot = library('efiboot', lib_efiboot_sources, version : lib_ver, link_with : [lib_efivar])
lib_efiboot_xml = custom_target('efiboot_xml', input : [lib_efiboot], output: ['libefiboot.abixml'],
                               command : abidw_command)

lib_efisec_sources = ['sec.c', 'secdb.c', 'esl-iter.c', 'util.c']
lib_efisec = library('efisec', lib_efisec_sources, version : lib_ver, link_with : [lib_efivar])
lib_efisec_xml = custom_target('efisec_xml', input : [lib_efisec], output: ['libefisec.abixml'],
                               command : abidw_command)

alias_target('abixml', lib_efivar_xml, lib_efiboot_xml, lib_efisec_xml)


efivar_sources = ['efivar.c', 'guid.c', 'util.c']
efivar = executable('efivar', efivar_sources, install : true, link_with : [lib_efivar])

efisecdb_sources = ['efisecdb.c', 'secdb-dump.c', 'util.c']
efisecdb = executable('efisecdb', efisecdb_sources, install : true, link_with : [lib_efivar, lib_efisec])


pkg = import('pkgconfig')

pkg.generate(lib_efivar, description : 'UEFI Variable Management', subdirs : ['efivar'])
pkg.generate(lib_efiboot, description : 'UEFI Boot variable support', subdirs : ['efivar'])
pkg.generate(lib_efisec, description : 'UEFI Security Features', subdirs : ['efivar'])
