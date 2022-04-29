makeguids_sources = ['makeguids.c', 'util.c']
makeguids = executable('makeguids', makeguids_sources, c_args : ['-DEFIVAR_BUILD_ENVIRONMENT'])

gen_guids = custom_target('gen-guids',
                          input : ['guids.txt'],
                          output : ['guid-symbols.c', 'efivar-guids.h'],
                          command : [makeguids, '@INPUT@', '@OUTPUT0@', '@OUTPUT1@'])


lib_efivar_sources = ['crc32.c', 'dp.c', 'dp-acpi.c', 'dp-hw.c', 'dp-media.c', 'dp-message.c', 'efivarfs.c', 'error.c', 'export.c', 'guid.c', 'lib.c', 'vars.c', 'time.c']
lib_efivar = library('efivar', lib_efivar_sources, gen_guids, version : '1.@0@'.format(meson.project_version()), soversion : '1')

lib_efiboot_sources = ['crc32.c', 'creator.c', 'disk.c', 'gpt.c', 'loadopt.c', 'path-helpers.c', 'linux.c', 'linux-acpi.c', 'linux-acpi-root.c', 'linux-ata.c', 'linux-emmc.c', 'linux-i2o.c', 'linux-md.c', 'linux-nvme.c', 'linux-pci.c', 'linux-pci-root.c', 'linux-pmem.c', 'linux-sas.c', 'linux-sata.c', 'linux-scsi.c', 'linux-soc-root.c', 'linux-virtblk.c', 'linux-virtual-root.c']
lib_efiboot = library('efiboot', lib_efiboot_sources, version : so_ver, soversion : '1', link_with : [lib_efivar])

lib_efisec_sources = ['sec.c', 'secdb.c', 'esl-iter.c', 'util.c']
lib_efisec = library('efisec', lib_efisec_sources, version : so_ver, soversion : '1', link_with : [lib_efivar])

efivar_sources = ['efivar.c', 'guid.c', 'util.c']
efivar = executable('efivar', efivar_sources, install : true, link_with : [lib_efivar])

efisecdb_sources = ['efisecdb.c', 'secdb-dump.c', 'util.c']
efisecdb = executable('efisecdb', efisecdb_sources, install : true, link_with : [lib_efivar, lib_efisec])


pkg = import('pkgconfig')

pkg.generate(lib_efivar, description : 'UEFI Variable Management', subdirs : ['efivar'])
pkg.generate(lib_efiboot, description : 'UEFI Boot variable support', subdirs : ['efivar'])
pkg.generate(lib_efisec, description : 'UEFI Security Features', subdirs : ['efivar'])
