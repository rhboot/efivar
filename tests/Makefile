#
# Makefile
# Peter Jones, 2019-06-18 11:10
#

TESTS = test.dmpstore.export \
	test.efivar.export \
	test.grubenv.var \
	test.bootorder.var \
	test.conin.var \
	test.efivar.threading \
	test.parse.db \
	test.esl.annotation \
	test.esl.sha256.unsorted \
	test.esl.sha256.ascending \
	test.esl.sha256.removal.descending \
	test.esl.sha256.addition.unsorted \
	test.esl.cert.addition \
	test.esl.cert.removal

all: clean $(TESTS)

GRUB_PREFIX ?= grub2
VALGRIND ?=

V ?= 0
ifeq ($(V),0)
        quiet=@
        rmverbose=
else
        quiet=
        rmverbose=-v
endif
ifeq ($(V),2)
        loud=-vvvv
else
        loud=
endif

EFIVAR ?= $(VALGRIND) $(TOPDIR)/src/efivar $(loud)
EFISECDB ?= $(VALGRIND) $(TOPDIR)/src/efisecdb $(loud)

EFIVAR ?= $(VALGRIND) $(TOPDIR)/src/efivar $(loud)

clean:
	$(quiet)rm $(rmverbose) -f test.*.result* \
		test.esl.annotation.esl.result \
		test.esl.cert.addition.esl.goal.txt \
		test.esl.cert.removal.esl.goal.txt \
		test.esl.sha256.addition.unsorted.esl.goal.txt \
		test.esl.sha256.ascending.esl.goal.txt \
		test.esl.sha256.removal.descending.esl.goal.txt \
		test.esl.sha256.unsorted.esl.goal.txt

test.dmpstore.export:
	$(quiet)echo testing export to DMPSTORE format
	$(quiet)$(GRUB_PREFIX)-editenv test.dmpstore.export.result.env create
	$(quiet)$(GRUB_PREFIX)-editenv test.dmpstore.export.result.env set debug=all,-scripting,-lexer
	$(quiet)truncate -s 512 test.dmpstore.export.result.env
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -n {grub}-GRUB_ENV -f test.dmpstore.export.result.env -D -e test.dmpstore.export.result.var
	$(quiet)@if grep -q "Do not edit" test.dmpstore.export.result.env ; then \
		cmp test.dmpstore.export.result.var test.dmpstore.export.new.goal.var ; \
	else \
		cmp test.dmpstore.export.result.var test.dmpstore.export.old.goal.var ; \
	fi
	$(quiet)rm -f test.dmpstore.export.result.*
	$(quiet)echo passed

test.efivar.export:
	$(quiet)echo testing export to libefivar format
	$(quiet)$(GRUB_PREFIX)-editenv test.efivar.export.result.env create
	$(quiet)$(GRUB_PREFIX)-editenv test.efivar.export.result.env set debug=all,-scripting,-lexer
	$(quiet)truncate -s 512 test.efivar.export.result.env
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -n {grub}-GRUB_ENV -f test.efivar.export.result.env -e test.efivar.export.result.var
	$(quiet)if grep -q "Do not edit" test.efivar.export.result.env ; then \
		cmp test.efivar.export.result.var test.efivar.export.new.goal.var ; \
	else \
		cmp test.efivar.export.result.var test.efivar.export.old.goal.var ; \
	fi
	$(quiet)rm -f test.efivar.export.result.*
	$(quiet)echo passed

test.grubenv.var:
	$(quiet)$(GRUB_PREFIX)-editenv test.grubenv.var.result.env create
	$(quiet)$(GRUB_PREFIX)-editenv test.grubenv.var.result.env set debug=all,-scripting,-lexer
	$(quiet)truncate -s 512 test.grubenv.var.result.env
	$(quiet)echo test importing from DMPSTORE and exporting to DMPSTORE and symbolic guid-name validation
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -n {grub}-GRUB_ENV -f test.grubenv.var.result.env -D -e test.grubenv.var.0.export.var
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.grubenv.var.0.export.var -e test.grubenv.var.0.result.var -D
	$(quiet)if grep -q "Do not edit" test.grubenv.var.result.env ; then \
		cmp test.grubenv.var.0.result.var test.grubenv.var.0.new.goal.var ; \
	else \
		cmp test.grubenv.var.0.result.var test.grubenv.var.0.old.goal.var ; \
	fi
	$(quiet)rm test.grubenv.var.0.result.*
	$(quiet)echo passed
	$(quiet)echo test importing from DMPSTORE and exporting to libefivar
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -n {grub}-GRUB_ENV -f test.grubenv.var.result.env -D -e test.grubenv.var.1.export.var
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.grubenv.var.1.export.var -e test.grubenv.var.1.result.var
	$(quiet)if grep -q "Do not edit" test.grubenv.var.result.env ; then \
		cmp test.grubenv.var.1.result.var test.grubenv.var.1.new.goal.var ; \
	else \
		cmp test.grubenv.var.1.result.var test.grubenv.var.1.old.goal.var ; \
	fi
	$(quiet)rm test.grubenv.var.1.result.*
	$(quiet)echo passed
	$(quiet)echo test importing from libefivar and exporting to DMPSTORE and mixed-case guid-name validation
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -n 91376aff-cBa6-42bE-949d-06fde81128e8-GRUB_ENV -f test.grubenv.var.result.env -e test.grubenv.var.2.export.var
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.grubenv.var.2.export.var -e test.grubenv.var.2.result.var -D
	$(quiet)if grep -q "Do not edit" test.grubenv.var.result.env ; then \
		cmp test.grubenv.var.2.result.var test.grubenv.var.2.new.goal.var ; \
	else \
		cmp test.grubenv.var.2.result.var test.grubenv.var.2.old.goal.var ; \
	fi
	$(quiet)rm test.grubenv.var.2.result.*
	$(quiet)echo passed
	$(quiet)echo test importing from libefivar and exporting to libefivar and guid-name validation
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -n 91376aff-cba6-42be-949d-06fde81128e8-GRUB_ENV -f test.grubenv.var.result.env -e test.grubenv.var.3.export.var
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.grubenv.var.3.export.var -e test.grubenv.var.3.result.var
	$(quiet)if grep -q "Do not edit" test.grubenv.var.result.env ; then \
		cmp test.grubenv.var.3.result.var test.grubenv.var.3.new.goal.var ; \
	else \
		cmp test.grubenv.var.3.result.var test.grubenv.var.3.old.goal.var ; \
	fi
	$(quiet)rm -f test.grubenv.var.3.result.* test.grubenv.var.result.env test.grubenv.var.3.goal.var
	$(quiet)echo passed
	$(quiet)echo testing efivar -L
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -L | \
		grep -q '^{91376aff-cba6-42be-949d-06fde81128e8}	{grub}	efi_guid_grub	GRUB$$'
	$(quiet)echo passed

test.bootorder.var:
	$(quiet)echo testing with BootOrder variable dmpstore generated
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.bootorder.var.goal.var -e test.bootorder.var.0.result.var
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.bootorder.var.0.result.var -e test.bootorder.var.1.result.var -D
	$(quiet)cmp test.bootorder.var.goal.var test.bootorder.var.1.result.var
	$(quiet)echo passed

test.conin.var:
	$(quiet)echo testing with ConIn variable dmpstore generated
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.conin.var.goal.var -e test.conin.var.0.result.var
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.conin.var.0.result.var -e test.conin.var.1.result.var -D
	$(quiet)cmp test.conin.var.goal.var test.conin.var.1.result.var
	$(quiet)echo passed

test.efivar.threading:
	$(quiet)echo testing threading in libefivar
	$(quiet)TOPDIR=$(TOPDIR) $(TOPDIR)/tests/test-threading

test.esl.dump.x509.sha256:
	$(quiet)echo testing ESL dumping with x509 + sha256 sums
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFISECDB) \
		-i test.esl.sha256.addition.unsorted.esl.goal -A > test.esl.dump.x509.sha256.result.txt
	$(quiet)cmp test.esl.dump.x509.sha256.goal.txt test.esl.sha256.addition.unsorted.result.txt
	$(quiet)echo passed

%.goal.txt : %.goal
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src ../src/efisecdb --dump --annotate \
		-s none -i $< > $@

%.result.txt : %.result
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src ../src/efisecdb --dump --annotate \
		-s none -i $< > $@

%.var.result.txt : %.var
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src ../src/efisecdb --dump --annotate \
		-s none -i $< > $@

test.parse.db: test.parse.db.var.result.txt
	$(quiet)echo testing parsing db variable
	$(quiet)if ! cmp test.parse.db.var.goal.txt test.parse.db.var.result.txt ; then \
		diff -U 200 test.parse.db.var.goal.txt test.parse.db.var.result.txt ; \
		exit 1 ; \
	fi
	$(quiet)echo passed

test.esl.annotation.esl.result : test.esl.annotation.esl
	$(quiet)cp $(rmverbose) $< $@

test.esl.annotation: test.esl.annotation.esl.goal.txt
test.esl.annotation: test.esl.annotation.esl.result.txt
	$(quiet)echo testing ESL annotation with x509 + sha256 sums
	$(quiet)if ! cmp $@.esl.goal.txt $@.esl.result.txt ; then \
		diff -U 200 $@.esl.goal.txt $@.esl.result.txt ; \
		exit 1 ; \
	fi
	$(quiet)echo passed

test.esl.sha256.unsorted.esl.result:
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFISECDB) -g {redhat} -t sha256 -a \
		-h 87428fc522803d31065e7bce3cf03fe475096631e5e07bbd7a0fde60c4cf25c7 \
		-h 0263829989b6fd954f72baaf2fc64bc2e2f01d692d4de72986ea808f6e99813f \
		-h a3a5e715f0cc574a73c3f9bebb6bc24f32ffd5b67b387244c2c909da779a1478 \
		-h 8d74beec1be996322ad76813bafb92d40839895d6dd7ee808b17ca201eac98be \
		-s none -f -o $@

test.esl.sha256.unsorted: test.esl.sha256.unsorted.esl.result.txt test.esl.sha256.unsorted.esl.goal.txt
	$(quiet)echo testing ESL creation with sha256 sums '(unsorted)'
	$(quiet)if ! cmp $@.esl.goal $@.esl.result ; then \
		diff -U 200 $@.esl.goal.txt $@.esl.result.txt ; \
		exit 1 ; \
	fi
	$(quiet)echo passed

test.esl.sha256.ascending.esl.result:
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFISECDB) -g {redhat} -t sha256 -a \
		-h 87428fc522803d31065e7bce3cf03fe475096631e5e07bbd7a0fde60c4cf25c7 \
		-h 0263829989b6fd954f72baaf2fc64bc2e2f01d692d4de72986ea808f6e99813f \
		-h a3a5e715f0cc574a73c3f9bebb6bc24f32ffd5b67b387244c2c909da779a1478 \
		-h 8d74beec1be996322ad76813bafb92d40839895d6dd7ee808b17ca201eac98be \
		-s data -s ascending -f -o $@

test.esl.sha256.ascending: test.esl.sha256.ascending.esl.goal.txt test.esl.sha256.ascending.esl.result.txt
	$(quiet)echo testing ESL creation with sha256 sums '(ascending)'
	$(quiet)if ! cmp $@.esl.goal $@.esl.result ; then \
		diff -U 200 $@.esl.goal.txt $@.esl.result.txt ; \
		exit 1 ; \
	fi
	$(quiet)echo passed

test.esl.sha256.removal.descending.esl.result:
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFISECDB) -g {redhat} -t sha256 \
		-i test.esl.sha256.unsorted.esl.goal -r \
		-h a3a5e715f0cc574a73c3f9bebb6bc24f32ffd5b67b387244c2c909da779a1478 \
		-s data -s descending -f -o $@

test.esl.sha256.removal.descending: test.esl.sha256.removal.descending.esl.result.txt
test.esl.sha256.removal.descending: test.esl.sha256.removal.descending.esl.goal.txt
	$(quiet)echo testing ESL entry removal with sha256 sums '(descending)'
	$(quiet)if ! cmp $@.esl.goal $@.esl.result ; then \
		diff -U 200 $@.esl.goal.txt $@.esl.result.txt ; \
		exit 1 ; \
	fi
	$(quiet)echo passed

test.esl.sha256.addition.unsorted.esl.result:
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFISECDB) -g {redhat} -t sha256 \
		-i test.esl.sha256.addition.unsorted.esl.goal -a \
		-h a3a5e715f0cc574a73c3f9bebb6bc24f32ffd5b67b387244c2c909da779a1478 \
		-s none -f -o test.esl.sha256.addition.unsorted.esl.result

test.esl.sha256.addition.unsorted: test.esl.sha256.addition.unsorted.esl.result.txt
test.esl.sha256.addition.unsorted: test.esl.sha256.addition.unsorted.esl.goal.txt
	$(quiet)echo testing adding a sha256 sum to an existing ESL '(unsorted)'
	$(quiet)if ! cmp $@.esl.goal $@.esl.result ; then \
		diff -U 200 $@.esl.goal.txt $@.esl.result.txt ; \
		exit 1 ; \
	fi
	$(quiet)echo passed

test.esl.cert.addition.esl.result:
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFISECDB) -g {redhat} -t sha256 \
		-i test.esl.sha256.unsorted.esl.goal -a -c test.esl.cert.addition.cert.cer \
		-f -o test.esl.cert.addition.esl.result

test.esl.cert.addition:
	$(quiet)echo testing ESL entry addition with x509 cert
	$(quiet)$(MAKE) test.esl.cert.addition.esl.result.txt test.esl.cert.addition.esl.goal.txt
	$(quiet)if ! cmp test.esl.cert.addition.esl.goal test.esl.cert.addition.esl.result ; then \
		diff -U 200 test.esl.cert.addition.esl.goal.txt test.esl.cert.addition.esl.result.txt ; \
		exit 1 ; \
	fi
	$(quiet)rm -f test.esl.cert.addition.esl.result test.esl.cert.addition.*.esl.txt
	$(quiet)echo passed

test.esl.cert.removal.esl.goal.txt : test.esl.cert.removal.esl.goal

test.esl.cert.removal.esl.result:
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFISECDB) -g {redhat} -t sha256 \
		-i test.esl.cert.addition.cert.esl.goal -r -c test.esl.cert.addition.cert.cer \
		-f -o test.esl.cert.removal.esl.result

test.esl.cert.removal:
	$(quiet)echo testing ESL entry removal with x509 cert
	$(quiet)$(MAKE) test.esl.cert.removal.esl.goal.txt test.esl.cert.removal.esl.result.txt
	$(quiet)if ! cmp test.esl.cert.removal.esl.goal test.esl.cert.removal.esl.result ; then \
		diff -U 200 test.esl.cert.removal.esl.goal.txt test.esl.cert.removal.esl.result.txt ; \
		exit 1 ; \
	fi
	$(quiet)cmp test.esl.cert.removal.esl.goal test.esl.cert.removal.esl.result
	$(quiet)rm -f test.esl.cert.removal.esl.result
	$(quiet)echo passed

.PHONY: all clean $(TESTS)

# vim:ft=make
#
