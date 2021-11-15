#
# Makefile
# Peter Jones, 2019-06-18 11:10
#

TESTS = test.dmpstore.export \
	test.efivar.export \
	test.grubenv.var \
	test.bootorder.var \
	test.conin.var \
	test.efivar.threading

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

clean:
	$(quiet)rm $(rmverbose) -f test.*.result*

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
		grep -q '^{91376aff-cba6-42be-949d-06fde81128e8} {grub} efi_guid_grub GRUB$$'
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

.PHONY: all clean $(TESTS)

# vim:ft=make
#
