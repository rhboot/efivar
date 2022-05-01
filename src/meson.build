incdir = include_directories('include')


makeguids_sources = ['makeguids.c', 'util.c']
makeguids = executable('makeguids', makeguids_sources, c_args : ['-DEFIVAR_BUILD_ENVIRONMENT'], native : true, include_directories : incdir)

headers = files('include/efivar/efiboot-creator.h', 'include/efivar/efiboot.h', 'include/efivar/efiboot-loadopt.h', 'include/efivar/efisec.h', 'include/efivar/efisec-secdb.h', 'include/efivar/efisec-types.h', 'include/efivar/efivar-dp.h', 'include/efivar/efivar.h', 'include/efivar/efivar-time.h', 'include/efivar/efivar-types.h')

gen_guids = custom_target('gen-guids',
                          input : ['guids.txt'],
                          output : ['guid-symbols.c', 'efivar-guids.h'],
                          command : [makeguids, '@INPUT@', '@OUTPUT0@', '@OUTPUT1@'],
                          install : true,
                          install_dir : [false, join_paths(get_option('includedir'), 'efivar')])


install_headers(headers, subdir : 'efivar')


abidw = find_program('abidw', required : get_option('generate_abi_xml'))
abidw_command = [abidw, '--headers-dir', join_paths(meson.source_root(), 'src/include/efivar/'),
                 '--out-file', '@OUTPUT@', '@INPUT@']

abidiff = find_program('abidiff', required : get_option('test_abi_compat'))
abidiff_args = ['--suppr', files('abignore'), '--headers-dir2', join_paths(meson.source_root(), 'src/include/efivar/')]


c = meson.get_compiler('c')

has_ver_script = c.has_link_argument('-Wl,--version-script=' + join_paths(meson.source_root(), 'src/libefivar.map'))
can_test_abi = has_ver_script and abidiff.found()

if has_ver_script
  link_arg = '-Wl,--version-script=' + join_paths(meson.source_root(), 'src/lib@0@.map')
else
  link_arg = ''
endif

if abidiff.found() and not has_ver_script
  warning('The test_abi_compat option was enabled but the linker doesn\'t support --version-script! Skipping...')
endif

lib_efivar_sources = ['crc32.c', 'dp.c', 'dp-acpi.c', 'dp-hw.c', 'dp-media.c', 'dp-message.c', 'efivarfs.c', 'error.c', 'export.c', 'guid.c', 'lib.c', 'vars.c', 'time.c']
lib_efivar = library('efivar', lib_efivar_sources, gen_guids, version : lib_ver, install : true, link_args : link_arg.format('efivar'), include_directories : incdir)
lib_efivar_xml = custom_target('efivar_xml', input : [lib_efivar], output: ['libefivar.abixml'],
                               command : abidw_command)
if can_test_abi
  test('efivar_abi', abidiff, args : abidiff_args + [files('libefivar.abixml'), lib_efivar])
endif

lib_efiboot_sources = ['crc32.c', 'creator.c', 'disk.c', 'gpt.c', 'loadopt.c', 'path-helpers.c', 'linux.c', 'linux-acpi.c', 'linux-acpi-root.c', 'linux-ata.c', 'linux-emmc.c', 'linux-i2o.c', 'linux-md.c', 'linux-nvme.c', 'linux-pci.c', 'linux-pci-root.c', 'linux-pmem.c', 'linux-sas.c', 'linux-sata.c', 'linux-scsi.c', 'linux-soc-root.c', 'linux-virtblk.c', 'linux-virtual-root.c']
lib_efiboot = library('efiboot', lib_efiboot_sources, gen_guids, version : lib_ver, link_with : [lib_efivar], install : true, link_args : link_arg.format('efiboot'), include_directories : incdir)
lib_efiboot_xml = custom_target('efiboot_xml', input : [lib_efiboot], output: ['libefiboot.abixml'],
                               command : abidw_command)
if can_test_abi
  test('efiboot_abi', abidiff, args : abidiff_args + [files('libefiboot.abixml'), lib_efiboot])
endif

lib_efisec_sources = ['sec.c', 'secdb.c', 'esl-iter.c', 'util.c']
lib_efisec = library('efisec', lib_efisec_sources, gen_guids, version : lib_ver, link_with : [lib_efivar], install : true, link_args : link_arg.format('efisec'), include_directories : incdir)
lib_efisec_xml = custom_target('efisec_xml', input : [lib_efisec], output: ['libefisec.abixml'],
                               command : abidw_command)
if can_test_abi
  test('efisec_abi', abidiff, args : abidiff_args + [files('libefisec.abixml'), lib_efisec])
endif



alias_target('abixml', lib_efivar_xml, lib_efiboot_xml, lib_efisec_xml)


efivar_sources = ['efivar.c', 'guid.c', 'util.c']
efivar = executable('efivar', efivar_sources, gen_guids, install : true, link_with : [lib_efivar], include_directories : incdir)

efisecdb_sources = ['efisecdb.c', 'secdb-dump.c', 'util.c']
efisecdb = executable('efisecdb', efisecdb_sources, gen_guids, install : true, link_with : [lib_efivar, lib_efisec], include_directories : incdir)


pkg = import('pkgconfig')

pkg.generate(lib_efivar, description : 'UEFI Variable Management', subdirs : ['efivar'])
pkg.generate(lib_efiboot, description : 'UEFI Boot variable support', subdirs : ['efivar'])
pkg.generate(lib_efisec, description : 'UEFI Security Features', subdirs : ['efivar'])
