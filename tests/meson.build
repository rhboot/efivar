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

grub_wrapper = find_program('sh/_grubenv-test.sh')
grub_editenv = find_program('grub-editenv', disabler : true, required : get_option('test_with_grubenv'))
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


# build_by_default is ignored for now, see meson #2518 and #6511
efisecdb_tests = {
  'test.parse.db.var' : [
    'testing parsing db variable',
    custom_target(
      'test.parse.db.var_result', output : 'test.parse.db.var.result.txt', input : 'test.parse.db.var',
      build_by_default : false,
      command : [efisecdb, '--dump', '--annotate', '-s', 'none', '-i', '@INPUT@'], capture : true)
  ],

  'test.esl.annotation.esl' : [
    'testing ESL annotation with x509 + sha256 sums',
    custom_target(
      'test.esl.annotation.esl.var_result', output : 'test.esl.annotation.esl.result.txt', input : 'test.esl.annotation.esl',
      build_by_default : false,
      command : [efisecdb, '--dump', '--annotate', '-s', 'none', '-i', '@INPUT@'], capture : true)
  ],

  'test.esl.sha256.unsorted.esl' : [
    'testing ESL creation with sha256 sums (unsorted)',
    custom_target(
      'test.esl.sha256.unsorted.esl_result', output : 'test.esl.sha256.unsorted.esl.result',
      build_by_default : false,
      command : [
        efisecdb, '-g', '{redhat}', '-t', 'sha256', '-a',
        '-h', '87428fc522803d31065e7bce3cf03fe475096631e5e07bbd7a0fde60c4cf25c7',
        '-h', '0263829989b6fd954f72baaf2fc64bc2e2f01d692d4de72986ea808f6e99813f',
        '-h', 'a3a5e715f0cc574a73c3f9bebb6bc24f32ffd5b67b387244c2c909da779a1478',
        '-h', '8d74beec1be996322ad76813bafb92d40839895d6dd7ee808b17ca201eac98be',
        '-s', 'none', '-f', '-o', '@OUTPUT@'
      ]
    )
  ],

  'test.esl.sha256.ascending.esl' : [
    'testing ESL creation with sha256 sums (ascending)',
    custom_target(
      'test.esl.sha256.ascending.esl_result', output : 'test.esl.sha256.ascending.esl.result',
      build_by_default : false,
      command : [
        efisecdb, '-g', '{redhat}', '-t', 'sha256', '-a',
        '-h', '87428fc522803d31065e7bce3cf03fe475096631e5e07bbd7a0fde60c4cf25c7',
        '-h', '0263829989b6fd954f72baaf2fc64bc2e2f01d692d4de72986ea808f6e99813f',
        '-h', 'a3a5e715f0cc574a73c3f9bebb6bc24f32ffd5b67b387244c2c909da779a1478',
        '-h', '8d74beec1be996322ad76813bafb92d40839895d6dd7ee808b17ca201eac98be',
        '-s', 'data', '-s', 'ascending', '-f', '-o', '@OUTPUT@'
      ]
    )
  ],

  'test.esl.sha256.removal.descending.esl' : [
    'testing ESL entry removal with sha256 sums (descending)',
    custom_target(
      'test.esl.sha256.removal.descending.esl_result', output : 'test.esl.sha256.removal.descending.esl.result', input : 'test.esl.sha256.unsorted.esl.goal',
      build_by_default : false,
      command : [
        efisecdb, '-g', '{redhat}', '-t', 'sha256',
        '-i', '@INPUT@', '-r',
        '-h', 'a3a5e715f0cc574a73c3f9bebb6bc24f32ffd5b67b387244c2c909da779a1478',
        '-s', 'data', '-s', 'descending', '-f', '-o', '@OUTPUT@'
      ]
    )
  ],

  'test.esl.sha256.addition.unsorted.esl' : [
    'testing adding a sha256 sum to an existing ESL (unsorted)',
    custom_target(
      'test.esl.sha256.addition.unsorted.esl_result', output : 'test.esl.sha256.addition.unsorted.esl.result', input : 'test.esl.sha256.addition.unsorted.esl.goal',
      build_by_default : false,
      command : [
        efisecdb, '-g', '{redhat}', '-t', 'sha256',
        '-i', '@INPUT@', '-a',
        '-h', 'a3a5e715f0cc574a73c3f9bebb6bc24f32ffd5b67b387244c2c909da779a1478',
        '-s', 'none', '-f', '-o', '@OUTPUT@'
      ]
    )
  ],

  'test.esl.cert.addition.esl' : [
    'testing ESL entry addition with x509 cert',
    custom_target(
      'test.esl.cert.addition.esl_result', output : 'test.esl.cert.addition.esl.result', input : ['test.esl.sha256.unsorted.esl.goal', 'test.esl.cert.addition.cert.cer'],
      build_by_default : false,
      command : [
        efisecdb, '-g', '{redhat}', '-t', 'sha256',
		'-i', '@INPUT0@', '-a', '-c', '@INPUT1@',
		'-f', '-o', '@OUTPUT@'
      ]
    )
  ],

  'test.esl.cert.removal.esl' : [
    'testing ESL entry removal with x509 cert',
    custom_target(
      'test.esl.cert.removal.esl_result', output : 'test.esl.cert.removal.esl.result', input : ['test.esl.sha256.unsorted.esl.goal', 'test.esl.cert.addition.cert.cer'],
      build_by_default : false,
      command : [
        efisecdb, '-g', '{redhat}', '-t', 'sha256',
		'-i', '@INPUT0@', '-r', '-c', '@INPUT1@',
		'-f', '-o', '@OUTPUT@'
      ]
    )
  ]
}

secdb_wrapper = find_program('sh/_efisecdb-test.sh')
foreach efisectest, testinfo : efisecdb_tests
  test(testinfo[0], secdb_wrapper, args: [files(efisectest + '.goal'), testinfo[1], efisecdb])
endforeach


efissdfasdfecdb_tests = {
  #'test.esl.cert.addition.esl' : 'testing ESL entry addition with x509 cert',
  #'test.esl.cert.removal.esl' : 'testing ESL entry removal with x509 cert'
}

# Threading
# tester.c
