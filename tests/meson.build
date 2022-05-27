grubenv_tests = {
  'test.dmpstore.export' : ['testing export to DMPSTORE format',
                            '@0@ -n {grub}-GRUB_ENV -f test.dmpstore.export.result.env -D -e test.dmpstore.export.result.var'],
  'test.efivar.export' : ['testing export to libefivar format',
                          '@0@ -n {grub}-GRUB_ENV -f test.efivar.export.result.env -e test.efivar.export.result.var'],
  'test.grubenv.var.0' : ['test importing from DMPSTORE and exporting to DMPSTORE and symbolic guid-name validation',
                        '''
                        @0@ -n {grub}-GRUB_ENV -f test.grubenv.var.0.result.env -D -e test.grubenv.var.0.export.var
                        @0@ -i test.grubenv.var.0.export.var -e test.grubenv.var.0.result.var -D
                        '''],
  'test.grubenv.var.1' : ['test importing from DMPSTORE and exporting to libefivar', 
                        '''
                        @0@ -n {grub}-GRUB_ENV -f test.grubenv.var.1.result.env -D -e test.grubenv.var.1.export.var
                        @0@ -i test.grubenv.var.1.export.var -e test.grubenv.var.1.result.var
                        '''],
  'test.grubenv.var.2' : ['test importing from libefivar and exporting to DMPSTORE and mixed-case guid-name validation',
                        '''
	                    @0@ -n 91376aff-cBa6-42bE-949d-06fde81128e8-GRUB_ENV -f test.grubenv.var.2.result.env -e test.grubenv.var.2.export.var
	                    @0@ -i test.grubenv.var.2.export.var -e test.grubenv.var.2.result.var -D
                        '''],
  'test.grubenv.var.3' : ['test importing from libefivar and exporting to libefivar and guid-name validation',
                        '''
                        @0@ -n 91376aff-cba6-42be-949d-06fde81128e8-GRUB_ENV -f test.grubenv.var.3.result.env -e test.grubenv.var.3.export.var
	                    @0@ -i test.grubenv.var.3.export.var -e test.grubenv.var.3.result.var
                        '''],
}

# TODO test grub return disabler
grub_wrapper = find_program('sh/_grubenv-test.sh')
grub_editenv = find_program('grub-editenv')
foreach grubtest, testinfo : grubenv_tests
  # TODO full_path should not be needed for either, report upstream
  # "You should not need to use this method. Passing the object itself should work in all cases."
  # and "In most cases using the object itself will do the same job as this and will also allow Meson to setup inter-target dependencies correctly. Please file a bug if that doesn't work for you."
  test(testinfo[0], grub_wrapper, args: [grub_editenv.full_path(), meson.current_source_dir(), grubtest, testinfo[1].format(efivar.full_path())])
endforeach


efivar_tests = {
  'test.efivar.l' : 'testing efivar -L',
  'test.bootorder.var' : 'testing with BootOrder variable dmpstore generated',
  'test.conin.var' : 'testing with ConIn variable dmpstore generated',
}

foreach efivartest, testname : efivar_tests
  testprog = find_program('sh/' + efivartest + '.sh')
  test(testname, testprog, args: [efivar, meson.current_source_dir()])
endforeach

efisecdb_tests = {
  'test.esl.dump.x509.sha256' : 'testing ESL dumping with x509 + sha256 sums',
  'test.parse.db' : 'testing parsing db variable',
  'test.esl.annotation' : 'testing ESL annotation with x509 + sha256 sums',
  'test.esl.sha256.unsorted' : 'testing ESL creation with sha256 sums (unsorted)',
  'test.esl.sha256.ascending' : 'testing ESL creation with sha256 sums (ascending)',
  'test.esl.sha256.removal.descending' : 'testing ESL entry removal with sha256 sums (descending)',
  'test.esl.sha256.addition.unsorted' : 'testing adding a sha256 sum to an existing ESL (unsorted)',
  'test.esl.cert.addition' : 'testing ESL entry addition with x509 cert',
  'test.esl.cert.removal' : 'testing ESL entry removal with x509 cert'
}

foreach efisectest, testname : efisecdb_tests
endforeach

# Threading
# tester.c
