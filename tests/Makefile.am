EFIVAR_V_TEST = $(EFIVAR_V_TEST_@AM_V@)
EFIVAR_V_TEST_ = $(EFIVAR_V_TEST_@AM_DEFAULT_V@)
EFIVAR_V_TEST_0 = @
EFIVAR_V_TEST_1 =
EFIVAR_V_TEST_2 =

EFIVAR_V_LOUD = $(EFIVAR_V_LOUD_@AM_V@)
EFIVAR_V_LOUD_ = $(EFIVAR_V_LOUD_@AM_DEFAULT_V@)
EFIVAR_V_LOUD_0 =
EFIVAR_V_LOUD_1 =
EFIVAR_V_LOUD_2 = -vvvv

GRUB_EDITENV = @grub_editenv@

if ENABLE_VALGRIND
VALGRIND = @valgrind@
EFISECDB = $(VALGRIND) $(builddir)/efisecdb$(EXEEXT) $(EFIVAR_V_LOUD)
EFIVAR = $(VALGRIND) $(builddir)/efivar$(EXEEXT) $(EFIVAR_V_LOUD)
else
EFISECDB = $(builddir)/efisecdb$(EXEEXT)
EFIVAR = $(builddir)/efivar$(EXEEXT)
endif

.PHONY:						\
	test.bootorder.var			\
	test.conin.var				\
	test.dmpstore.export			\
	test.efivar.export			\
	test.efivar.threading			\
	test.esl.annotation			\
	test.esl.cert.addition			\
	test.esl.cert.removal			\
	test.esl.sha256.addition.unsorted	\
	test.esl.sha256.ascending		\
	test.esl.sha256.removal.descending	\
	test.esl.sha256.unsorted		\
	test.grubenv.var			\
	test.parse.db				\
	tests					\
	tests/scratch

tests: test-clean-local
	@$(MAKE) --no-print-directory test.dmpstore.export
	@$(MAKE) --no-print-directory test.efivar.export
	@$(MAKE) --no-print-directory test.grubenv.var
	@$(MAKE) --no-print-directory test.bootorder.var
	@$(MAKE) --no-print-directory test.conin.var
	@$(MAKE) --no-print-directory test.efivar.threading
	@$(MAKE) --no-print-directory test.parse.db
	@$(MAKE) --no-print-directory test.esl.annotation
	@$(MAKE) --no-print-directory test.esl.sha256.unsorted
	@$(MAKE) --no-print-directory test.esl.sha256.ascending
	@$(MAKE) --no-print-directory test.esl.sha256.removal.descending
	@$(MAKE) --no-print-directory test.esl.sha256.addition.unsorted
	@$(MAKE) --no-print-directory test.esl.cert.addition
	@$(MAKE) --no-print-directory test.esl.cert.removal

test.bootorder.var: efivar$(EXEEXT) tests/.dirstamp
	@echo testing with BootOrder variable dmpstore generated
	$(EFIVAR_V_TEST)$(EFIVAR) -i $(srcdir)/tests/test.bootorder.var.goal.var -e tests/test.bootorder.var.0.result.var
	$(EFIVAR_V_TEST)$(EFIVAR) -i tests/test.bootorder.var.0.result.var -e tests/test.bootorder.var.1.result.var -D
	$(EFIVAR_V_TEST)cmp $(srcdir)/tests/test.bootorder.var.goal.var tests/test.bootorder.var.1.result.var
	@echo passed

test.conin.var: efivar$(EXEEXT) tests/.dirstamp
	@echo testing with ConIn variable dmpstore generated
	$(EFIVAR_V_TEST)$(EFIVAR) -i $(srcdir)/tests/test.conin.var.goal.var -e tests/test.conin.var.0.result.var
	$(EFIVAR_V_TEST)$(EFIVAR) -i tests/test.conin.var.0.result.var -e tests/test.conin.var.1.result.var -D
	$(EFIVAR_V_TEST)cmp $(srcdir)/tests/test.conin.var.goal.var tests/test.conin.var.1.result.var
	@echo passed

test.dmpstore.export: efivar$(EXEEXT) tests/.dirstamp
	@echo testing export to DMPSTORE format
	$(EFIVAR_V_TEST)$(GRUB_EDITENV) tests/test.dmpstore.export.result.env create
	$(EFIVAR_V_TEST)$(GRUB_EDITENV) tests/test.dmpstore.export.result.env set debug=all,-scripting,-lexer
	$(EFIVAR_V_TEST)truncate -s 512 tests/test.dmpstore.export.result.env
	$(EFIVAR_V_TEST)$(EFIVAR) -n {grub}-GRUB_ENV -f tests/test.dmpstore.export.result.env -D -e tests/test.dmpstore.export.result.var
	$(EFIVAR_V_TEST)grep -q "Do not edit" tests/test.dmpstore.export.result.env && cmp tests/test.dmpstore.export.result.var $(srcdir)/tests/test.dmpstore.export.new.goal.var \
		|| cmp tests/test.dmpstore.export.result.var $(srcdir)/tests/test.dmpstore.export.old.goal.var;
	$(EFIVAR_V_TEST)rm -f tests/test.dmpstore.export.result.env
	$(EFIVAR_V_TEST)rm -f tests/test.dmpstore.export.result.var
	@echo passed

test.efivar.export: efivar$(EXEEXT) tests/.dirstamp
	@echo testing export to libefivar format
	$(EFIVAR_V_TEST)$(GRUB_EDITENV) tests/test.efivar.export.result.env create
	$(EFIVAR_V_TEST)$(GRUB_EDITENV) tests/test.efivar.export.result.env set debug=all,-scripting,-lexer
	$(EFIVAR_V_TEST)truncate -s 512 tests/test.efivar.export.result.env
	$(EFIVAR_V_TEST)$(EFIVAR) -n {grub}-GRUB_ENV -f tests/test.efivar.export.result.env -e tests/test.efivar.export.result.var
	$(EFIVAR_V_TEST)grep -q "Do not edit" tests/test.efivar.export.result.env && cmp tests/test.efivar.export.result.var $(srcdir)/tests/test.efivar.export.new.goal.var \
		|| cmp tests/test.efivar.export.result.var $(srcdir)/tests/test.efivar.export.old.goal.var 
	$(EFIVAR_V_TEST)rm -f test.efivar.export.result.env
	$(EFIVAR_V_TEST)rm -f test.efivar.export.result.var
	@echo passed

test.efivar.threading: efivar$(EXEEXT) thread-test$(EXEEXT) tests/scratch
	@echo testing threading in libefivar:
	$(EFIVAR_V_TEST)export EFIVARFS_PATH=$(abs_builddir)/tests/scratch
	$(EFIVAR_V_TEST)export LIBEFIVAR_OPS=efivarfs
	$(EFIVAR_V_TEST)echo "testing  1 thread...": $$($(builddir)/thread-test$(EXEEXT) -t  1)
	$(EFIVAR_V_TEST)echo "testing  2 thread...": $$($(builddir)/thread-test$(EXEEXT) -t  2)
	$(EFIVAR_V_TEST)echo "testing  4 thread...": $$($(builddir)/thread-test$(EXEEXT) -t  4)
	$(EFIVAR_V_TEST)echo "testing  8 thread...": $$($(builddir)/thread-test$(EXEEXT) -t  8)
	$(EFIVAR_V_TEST)echo "testing 16 thread...": $$($(builddir)/thread-test$(EXEEXT) -t 16)
	$(EFIVAR_V_TEST)echo "testing 32 thread...": $$($(builddir)/thread-test$(EXEEXT) -t 32)
	$(EFIVAR_V_TEST)echo "testing 64 thread...": $$($(builddir)/thread-test$(EXEEXT) -t 64)
	$(EFIVAR_V_TEST)unset EFIVARFS_PATH
	$(EFIVAR_V_TEST)unset LIBEFIVAR_OPS

test.esl.annotation: efivar$(EXEEXT) efisecdb$(EXEEXT) tests/.dirstamp tests/test.esl.annotation.esl.goal.txt tests/test.esl.annotation.esl.result.txt
	@echo testing ESL annotation with x509 + sha256 sums
	$(EFIVAR_V_TEST)if ! cmp $(srcdir)/tests/$@.esl.goal.txt tests/$@.esl.result.txt ; then \
		diff -U 200 $(srcdir)/tests/$@.esl.goal.txt tests/$@.esl.result.txt ; \
		exit 1 ; \
	fi
	@echo passed

test.esl.cert.addition: efivar$(EXEEXT) tests/.dirstamp
	@echo testing ESL entry addition with x509 cert
	$(EFIVAR_V_TEST)$(MAKE) tests/test.esl.cert.addition.esl.result.txt tests/test.esl.cert.addition.esl.goal.txt
	$(EFIVAR_V_TEST)if ! cmp $(srcdir)/tests/test.esl.cert.addition.esl.goal tests/test.esl.cert.addition.esl.result ; then \
		diff -U 200 tests/test.esl.cert.addition.esl.goal.txt tests/test.esl.cert.addition.esl.result.txt ; \
		exit 1 ; \
	fi
	$(EFIVAR_V_TEST)rm -f test.esl.cert.addition.esl.result test.esl.cert.addition.*.esl.txt
	@echo passed

test.esl.cert.removal: efivar$(EXEEXT) tests/.dirstamp
	@echo testing ESL entry removal with x509 cert
	$(EFIVAR_V_TEST)$(MAKE) tests/test.esl.cert.removal.esl.goal.txt tests/test.esl.cert.removal.esl.result.txt
	$(EFIVAR_V_TEST)if ! cmp $(srcdir)/tests/test.esl.cert.removal.esl.goal tests/test.esl.cert.removal.esl.result ; then \
		diff -U 200 tests/test.esl.cert.removal.esl.goal.txt tests/test.esl.cert.removal.esl.result.txt ; \
		exit 1 ; \
	fi
	$(EFIVAR_V_TEST)cmp $(srcdir)/tests/test.esl.cert.removal.esl.goal tests/test.esl.cert.removal.esl.result
	$(EFIVAR_V_TEST)rm -f tests/test.esl.cert.removal.esl.result
	@echo passed

test.esl.sha256.addition.unsorted: tests/test.esl.sha256.addition.unsorted.esl.result.txt tests/test.esl.sha256.addition.unsorted.esl.goal.txt
	@echo testing adding a sha256 sum to an existing ESL '(unsorted)'
	$(EFIVAR_V_TEST)if ! cmp $(srcdir)/tests/$@.esl.goal tests/$@.esl.result ; then \
		diff -U 200 tests/$@.esl.goal.txt tests/$@.esl.result.txt ; \
		exit 1 ; \
	fi
	@echo passed

test.esl.sha256.ascending: tests/test.esl.sha256.ascending.esl.goal.txt tests/test.esl.sha256.ascending.esl.result.txt
	@echo testing ESL creation with sha256 sums '(ascending)'
	$(EFIVAR_V_TEST)if ! cmp $(srcdir)/tests/$@.esl.goal tests/$@.esl.result ; then \
		diff -U 200 tests/$@.esl.goal.txt tests/$@.esl.result.txt ; \
		exit 1 ; \
	fi
	@echo passed

test.esl.sha256.removal.descending: tests/test.esl.sha256.removal.descending.esl.result.txt tests/test.esl.sha256.removal.descending.esl.result.txt
	@echo testing ESL entry removal with sha256 sums '(descending)'
	$(EFIVAR_V_TEST)if ! cmp $(srcdir)/tests/$@.esl.goal tests/$@.esl.result ; then \
		diff -U 200 tests/$@.esl.goal.txt tests/$@.esl.result.txt ; \
		exit 1 ; \
	fi
	@echo passed

test.esl.sha256.unsorted: tests/test.esl.sha256.unsorted.esl.result.txt tests/test.esl.sha256.unsorted.esl.goal.txt
	@echo testing ESL creation with sha256 sums '(unsorted)'
	$(EFIVAR_V_TEST)if ! cmp $(srcdir)/tests/$@.esl.goal tests/$@.esl.result ; then \
		diff -U 200 tests/$@.esl.goal.txt tests/$@.esl.result.txt ; \
		exit 1 ; \
	fi
	@echo passed

test.grubenv.var: efivar$(EXEEXT) tests/.dirstamp
	$(EFIVAR_V_TEST)$(GRUB_EDITENV) tests/test.grubenv.var.result.env create
	$(EFIVAR_V_TEST)$(GRUB_EDITENV) tests/test.grubenv.var.result.env set debug=all,-scripting,-lexer
	$(EFIVAR_V_TEST)truncate -s 512 tests/test.grubenv.var.result.env
	@echo test importing from DMPSTORE and exporting to DMPSTORE and symbolic guid-name validation
	$(EFIVAR_V_TEST)$(EFIVAR) -n {grub}-GRUB_ENV -f tests/test.grubenv.var.result.env -D -e tests/test.grubenv.var.0.export.var
	$(EFIVAR_V_TEST)$(EFIVAR) -i tests/test.grubenv.var.0.export.var -e tests/test.grubenv.var.0.result.var -D
	$(EFIVAR_V_TEST)if grep -q "Do not edit" tests/test.grubenv.var.result.env ; then \
		cmp tests/test.grubenv.var.0.result.var $(srcdir)/tests/test.grubenv.var.0.new.goal.var ; \
	else \
		cmp tests/test.grubenv.var.0.result.var $(srcdir)/tests/test.grubenv.var.0.old.goal.var ; \
	fi
	$(EFIVAR_V_TEST)rm tests/test.grubenv.var.0.result.*
	@echo passed
	@echo test importing from DMPSTORE and exporting to libefivar
	$(EFIVAR_V_TEST)$(EFIVAR) -n {grub}-GRUB_ENV -f tests/test.grubenv.var.result.env -D -e tests/test.grubenv.var.1.export.var
	$(EFIVAR_V_TEST)$(EFIVAR) -i tests/test.grubenv.var.1.export.var -e tests/test.grubenv.var.1.result.var
	$(EFIVAR_V_TEST)if grep -q "Do not edit" tests/test.grubenv.var.result.env ; then \
		cmp tests/test.grubenv.var.1.result.var $(srcdir)/tests/test.grubenv.var.1.new.goal.var ; \
	else \
		cmp tests/test.grubenv.var.1.result.var $(srcdir)/tests/test.grubenv.var.1.old.goal.var ; \
	fi
	$(EFIVAR_V_TEST)rm tests/test.grubenv.var.1.result.*
	@echo passed
	@echo test importing from libefivar and exporting to DMPSTORE and mixed-case guid-name validation
	$(EFIVAR_V_TEST)$(EFIVAR) -n 91376aff-cBa6-42bE-949d-06fde81128e8-GRUB_ENV -f tests/test.grubenv.var.result.env -e tests/test.grubenv.var.2.export.var
	$(EFIVAR_V_TEST)$(EFIVAR) -i tests/test.grubenv.var.2.export.var -e tests/test.grubenv.var.2.result.var -D
	$(EFIVAR_V_TEST)if grep -q "Do not edit" tests/test.grubenv.var.result.env ; then \
		cmp tests/test.grubenv.var.2.result.var $(srcdir)/tests/test.grubenv.var.2.new.goal.var ; \
	else \
		cmp tests/test.grubenv.var.2.result.var $(srcdir)/tests/test.grubenv.var.2.old.goal.var ; \
	fi
	$(EFIVAR_V_TEST)rm tests/test.grubenv.var.2.result.*
	@echo passed
	@echo test importing from libefivar and exporting to libefivar and guid-name validation
	$(EFIVAR_V_TEST)$(EFIVAR) -n 91376aff-cba6-42be-949d-06fde81128e8-GRUB_ENV -f tests/test.grubenv.var.result.env -e tests/test.grubenv.var.3.export.var
	$(EFIVAR_V_TEST)$(EFIVAR) -i tests/test.grubenv.var.3.export.var -e tests/test.grubenv.var.3.result.var
	$(EFIVAR_V_TEST)if grep -q "Do not edit" tests/test.grubenv.var.result.env ; then \
		cmp tests/test.grubenv.var.3.result.var $(srcdir)/tests/test.grubenv.var.3.new.goal.var ; \
	else \
		cmp tests/test.grubenv.var.3.result.var $(srcdir)/tests/test.grubenv.var.3.old.goal.var ; \
	fi
	$(EFIVAR_V_TEST)rm -f tests/test.grubenv.var.3.result.* tests/test.grubenv.var.result.env tests/test.grubenv.var.3.goal.var
	@echo passed
	@echo testing efivar -L
	$(EFIVAR_V_TEST)$(EFIVAR) -L | \
		grep -q '^{91376aff-cba6-42be-949d-06fde81128e8}	{grub}	efi_guid_grub	GRUB$$'
	@echo passed

test.parse.db: efisecdb$(EXEEXT) tests/test.parse.db.var.result.txt
	@echo testing parsing db variable
	$(EFIVAR_V_TEST)if ! cmp $(srcdir)/tests/test.parse.db.var.goal.txt tests/test.parse.db.var.result.txt ; then \
		diff -U 200 $(srcdir)/tests/test.parse.db.var.goal.txt tests/test.parse.db.var.result.txt ; \
		exit 1 ; \
	fi
	@echo passed

tests/.dirstamp:
	$(MKDIR_P) $(@D)
	touch $@

tests/scratch:
	@rm -rf $@
	$(MKDIR_P) $@

tests/test.esl.annotation.esl.result : tests/test.esl.annotation.esl tests/.dirstamp
	$(EFIVAR_V_TEST)cp $< $@

tests/test.esl.cert.addition.esl.result: efisecdb$(EXEEXT) tests/.dirstamp
	$(EFIVAR_V_TEST)$(EFISECDB) -g {redhat} -t sha256 \
		-i $(srcdir)/tests/test.esl.sha256.unsorted.esl.goal -a -c $(srcdir)/tests/test.esl.cert.addition.cert.cer \
		-f -o $@

tests/test.esl.cert.removal.esl.result: efisecdb$(EXEEXT) tests/.dirstamp
	$(EFIVAR_V_TEST)$(EFISECDB) -g {redhat} -t sha256 \
		-i $(srcdir)/tests/test.esl.cert.addition.cert.esl.goal -r -c $(srcdir)/tests/test.esl.cert.addition.cert.cer \
		-f -o $@

tests/test.esl.cert.removal.esl.goal.txt : $(srcdir)/tests/test.esl.cert.removal.esl.goal

tests/test.esl.sha256.addition.unsorted.esl.result: efisecdb$(EXEEXT) tests/.dirstamp
	$(EFIVAR_V_TEST)$(EFISECDB) -g {redhat} -t sha256 \
		-i $(srcdir)/tests/test.esl.sha256.addition.unsorted.esl.goal -a \
		-h a3a5e715f0cc574a73c3f9bebb6bc24f32ffd5b67b387244c2c909da779a1478 \
		-s none -f -o $@

tests/test.esl.sha256.ascending.esl.result: efisecdb$(EXEEXT) tests/.dirstamp
	$(EFIVAR_V_TEST)$(EFISECDB) -g {redhat} -t sha256 -a \
		-h 87428fc522803d31065e7bce3cf03fe475096631e5e07bbd7a0fde60c4cf25c7 \
		-h 0263829989b6fd954f72baaf2fc64bc2e2f01d692d4de72986ea808f6e99813f \
		-h a3a5e715f0cc574a73c3f9bebb6bc24f32ffd5b67b387244c2c909da779a1478 \
		-h 8d74beec1be996322ad76813bafb92d40839895d6dd7ee808b17ca201eac98be \
		-s data -s ascending -f -o $@

tests/test.esl.sha256.removal.descending.esl.result: efisecdb$(EXEEXT) tests/.dirstamp
	$(EFIVAR_V_TEST)$(EFISECDB) -g {redhat} -t sha256 \
		-i $(srcdir)/tests/test.esl.sha256.unsorted.esl.goal -r \
		-h a3a5e715f0cc574a73c3f9bebb6bc24f32ffd5b67b387244c2c909da779a1478 \
		-s data -s descending -f -o $@

tests/test.esl.sha256.unsorted.esl.result: efisecdb$(EXEEXT) tests/.dirstamp
	$(EFIVAR_V_TEST)$(EFISECDB) -g {redhat} -t sha256 -a \
		-h 87428fc522803d31065e7bce3cf03fe475096631e5e07bbd7a0fde60c4cf25c7 \
		-h 0263829989b6fd954f72baaf2fc64bc2e2f01d692d4de72986ea808f6e99813f \
		-h a3a5e715f0cc574a73c3f9bebb6bc24f32ffd5b67b387244c2c909da779a1478 \
		-h 8d74beec1be996322ad76813bafb92d40839895d6dd7ee808b17ca201eac98be \
		-s none -f -o $@

tests/test.parse.db.var.result.txt: efisecdb$(EXEEXT) tests/.dirstamp

%.goal.txt : %.goal
	$(EFIVAR_V_TEST)$(EFISECDB) --dump --annotate \
		-s none -i $< > $@

%.result.txt : %.result
	$(EFIVAR_V_TEST)$(EFISECDB) --dump --annotate \
		-s none -i $< > $@

%.var.result.txt : %.var
	$(EFIVAR_V_TEST)$(EFISECDB) --dump --annotate \
		-s none -i $< > $@

clean-local: test-clean-local
	-rm -f tests/.dirstamp

test-clean-local:
	$(EFIVAR_V_TEST)rm -f tests/test.bootorder.var.0.result.var
	$(EFIVAR_V_TEST)rm -f tests/test.bootorder.var.1.result.var
	$(EFIVAR_V_TEST)rm -f tests/test.conin.var.0.result.var
	$(EFIVAR_V_TEST)rm -f tests/test.conin.var.1.result.var
	$(EFIVAR_V_TEST)rm -f tests/test.efivar.export.result.env
	$(EFIVAR_V_TEST)rm -f tests/test.efivar.export.result.var
	$(EFIVAR_V_TEST)rm -f tests/test.esl.annotation.esl.result
	$(EFIVAR_V_TEST)rm -f tests/test.esl.annotation.esl.result.txt
	$(EFIVAR_V_TEST)rm -f tests/test.esl.cert.addition.esl.goal.txt
	$(EFIVAR_V_TEST)rm -f tests/test.esl.cert.addition.esl.result
	$(EFIVAR_V_TEST)rm -f tests/test.esl.cert.addition.esl.result.txt
	$(EFIVAR_V_TEST)rm -f tests/test.esl.cert.removal.esl.goal.txt
	$(EFIVAR_V_TEST)rm -f tests/test.esl.cert.removal.esl.result.txt
	$(EFIVAR_V_TEST)rm -f tests/test.esl.sha256.addition.unsorted.esl.goal.txt
	$(EFIVAR_V_TEST)rm -f tests/test.esl.sha256.addition.unsorted.esl.result
	$(EFIVAR_V_TEST)rm -f tests/test.esl.sha256.addition.unsorted.esl.result.txt
	$(EFIVAR_V_TEST)rm -f tests/test.esl.sha256.ascending.esl.goal.txt
	$(EFIVAR_V_TEST)rm -f tests/test.esl.sha256.ascending.esl.result
	$(EFIVAR_V_TEST)rm -f tests/test.esl.sha256.ascending.esl.result.txt
	$(EFIVAR_V_TEST)rm -f tests/test.esl.sha256.removal.descending.esl.result
	$(EFIVAR_V_TEST)rm -f tests/test.esl.sha256.removal.descending.esl.result.txt
	$(EFIVAR_V_TEST)rm -f tests/test.esl.sha256.unsorted.esl.goal.txt
	$(EFIVAR_V_TEST)rm -f tests/test.esl.sha256.unsorted.esl.result
	$(EFIVAR_V_TEST)rm -f tests/test.esl.sha256.unsorted.esl.result.txt
	$(EFIVAR_V_TEST)rm -f tests/test.grubenv.var.0.export.var
	$(EFIVAR_V_TEST)rm -f tests/test.grubenv.var.1.export.var
	$(EFIVAR_V_TEST)rm -f tests/test.grubenv.var.2.export.var
	$(EFIVAR_V_TEST)rm -f tests/test.grubenv.var.3.export.var
	$(EFIVAR_V_TEST)rm -f tests/test.parse.db.var.result.txt
