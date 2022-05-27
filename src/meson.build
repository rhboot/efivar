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


abidw = find_program('abidw', disabler : true, required : get_option('generate_abi_xml'))
abidw_command = [abidw, '--headers-dir', join_paths(meson.source_root(), 'src/include/efivar/'),
                 '--suppr', files('abignore'),
                 '--out-file', '@OUTPUT@', '@INPUT@']


c = meson.get_compiler('c')

has_ver_script = c.has_link_argument('-Wl,--version-script=' + join_paths(meson.source_root(), 'src/libefivar.map'))

abidiff = find_program('abidiff', disabler : true, required : get_option('test_abi_compat'))
abidiff_wrapper = find_program('../tests/abidiff_wrapper.py', required : get_option('test_abi_compat'))
abidiff_wrapper_args = [abidiff.full_path(), '--suppr', files('abignore'), '--headers-dir2', join_paths(meson.source_root(), 'src/include/efivar/')]

if abidiff.found()
  if has_ver_script
  else
    warning('The test_abi_compat option was enabled but the linker doesn\'t support --version-script! Skipping...')
    abidiff = disabler()
  endif
endif


if has_ver_script
  link_arg = '-Wl,--version-script=' + join_paths(meson.source_root(), 'src/lib@0@.map')
else
  link_arg = ''
endif

if meson.version().version_compare('>=0.62.0')
  dl = dependency('dl')
else
  dl = c.find_library('dl', required : false)
endif


lib_targets = {}
xml_targets = []
lib_sources = {
  'efivar' : ['crc32.c', 'dp.c', 'dp-acpi.c', 'dp-hw.c', 'dp-media.c', 'dp-message.c', 'efivarfs.c', 'error.c', 'export.c', 'guid.c', 'lib.c', 'vars.c', 'time.c'],
  'efiboot' : ['crc32.c', 'creator.c', 'disk.c', 'gpt.c', 'loadopt.c', 'path-helpers.c', 'linux.c', 'linux-acpi.c', 'linux-acpi-root.c', 'linux-ata.c', 'linux-emmc.c', 'linux-i2o.c', 'linux-md.c', 'linux-nvme.c', 'linux-pci.c', 'linux-pci-root.c', 'linux-pmem.c', 'linux-sas.c', 'linux-sata.c', 'linux-scsi.c', 'linux-soc-root.c', 'linux-virtblk.c', 'linux-virtual-root.c'],
  'efisec' : ['sec.c', 'secdb.c', 'esl-iter.c', 'util.c']
}
lib_deps = {
  'efivar' : dl
}



foreach libname : ['efivar', 'efiboot', 'efisec']
  lib = library(libname, lib_sources[libname], gen_guids, version : lib_ver,
                link_with : lib_targets.get('efivar', []), install : true,
                link_args : link_arg.format(libname), include_directories : incdir,
                dependencies : lib_deps.get(libname, []))
  lib_xml = custom_target(libname + '_xml', input : [lib], output: ['lib' + libname + '.abixml'],
                                 command : abidw_command)
  test(libname + '_abi', abidiff_wrapper, args : abidiff_wrapper_args + [files('lib' + libname + '.abixml'), lib])
  lib_targets += {libname : lib}
  xml_targets += lib_xml
endforeach



alias_target('abixml', xml_targets)


efivar_sources = ['efivar.c', 'guid.c', 'util.c']
efivar = executable('efivar', efivar_sources, gen_guids, install : true, link_with : [lib_targets['efivar']], include_directories : incdir, dependencies : dl)

efisecdb_sources = ['efisecdb.c', 'secdb-dump.c', 'util.c']
efisecdb = executable('efisecdb', efisecdb_sources, gen_guids, install : true, link_with : [lib_targets['efivar'], lib_targets['efisec']], include_directories : incdir)

tester_sources = ['test/tester.c']
tester = executable('tester', tester_sources, gen_guids, link_with : lib_targets['efivar'], include_directories : incdir)

pkg = import('pkgconfig')

pkg.generate(lib_targets['efivar'], description : 'UEFI Variable Management', subdirs : ['efivar'])
pkg.generate(lib_targets['efiboot'], description : 'UEFI Boot variable support', subdirs : ['efivar'])
pkg.generate(lib_targets['efisec'], description : 'UEFI Security Features', subdirs : ['efivar'])
