#
# Makefile
# Peter Jones, 2019-06-18 11:10
#

all: clean test0 test1 test2 test3 test4 test5

GRUB_PREFIX ?= grub2
EFIVAR ?= $(TOPDIR)/src/efivar
V ?= 0
ifeq ($(V),0)
        quiet=@
else
        quiet=
endif

clean:
	$(quiet)rm -f *.result.env *.result.var

test0:
	$(quiet)echo testing export to DMPSTORE format
	$(quiet)$(GRUB_PREFIX)-editenv test.0.result.env create
	$(quiet)$(GRUB_PREFIX)-editenv test.0.result.env set debug=all,-scripting,-lexer
	$(quiet)truncate -s 512 test.0.result.env
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -n {grub}-GRUB_ENV -f test.0.result.env -D -e test.0.result.var
	$(quiet)@if grep -q "Do not edit" test.0.result.env ; then \
		cmp test.0.result.var test.0.new.goal.var ; \
	else \
		cmp test.0.result.var test.0.old.goal.var ; \
	fi
	$(quiet)rm test.0.result.*
	$(quiet)echo passed

test1:
	$(quiet)echo testing export to libefivar format
	$(quiet)$(GRUB_PREFIX)-editenv test.1.result.env create
	$(quiet)$(GRUB_PREFIX)-editenv test.1.result.env set debug=all,-scripting,-lexer
	$(quiet)truncate -s 512 test.1.result.env
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -n {grub}-GRUB_ENV -f test.1.result.env -e test.1.result.var
	$(quiet)if grep -q "Do not edit" test.1.result.env ; then \
		cmp test.1.result.var test.1.new.goal.var ; \
	else \
		cmp test.1.result.var test.1.old.goal.var ; \
	fi
	$(quiet)rm test.1.result.*
	$(quiet)echo passed

test2:
	$(quiet)$(GRUB_PREFIX)-editenv test.2.result.env create
	$(quiet)$(GRUB_PREFIX)-editenv test.2.result.env set debug=all,-scripting,-lexer
	$(quiet)truncate -s 512 test.2.result.env
	$(quiet)echo test importing from DMPSTORE and exporting to DMPSTORE and symbolic guid-name validation
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -n {grub}-GRUB_ENV -f test.2.result.env -D -e test.2.0.export.var
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.2.0.export.var -e test.2.0.result.var -D
	$(quiet)if grep -q "Do not edit" test.2.result.env ; then \
		cmp test.2.0.result.var test.2.0.new.goal.var ; \
	else \
		cmp test.2.0.result.var test.2.0.old.goal.var ; \
	fi
	$(quiet)echo passed
	$(quiet)echo test importing from DMPSTORE and exporting to libefivar
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -n {grub}-GRUB_ENV -f test.2.result.env -D -e test.2.1.export.var
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.2.1.export.var -e test.2.1.result.var
	$(quiet)if grep -q "Do not edit" test.2.result.env ; then \
		cmp test.2.1.result.var test.2.1.new.goal.var ; \
	else \
		cmp test.2.1.result.var test.2.1.old.goal.var ; \
	fi
	$(quiet)echo passed
	$(quiet)echo test importing from libefivar and exporting to DMPSTORE and mixed-case guid-name validation
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -n 91376aff-cBa6-42bE-949d-06fde81128e8-GRUB_ENV -f test.2.result.env -e test.2.2.export.var
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.2.2.export.var -e test.2.2.result.var -D
	$(quiet)if grep -q "Do not edit" test.2.result.env ; then \
		cmp test.2.2.result.var test.2.2.new.goal.var ; \
	else \
		cmp test.2.2.result.var test.2.2.old.goal.var ; \
	fi
	$(quiet)echo passed
	$(quiet)echo test importing from libefivar and exporting to libefivar and guid-name validation
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -n 91376aff-cba6-42be-949d-06fde81128e8-GRUB_ENV -f test.2.result.env -e test.2.3.export.var
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.2.3.export.var -e test.2.3.result.var
	$(quiet)if grep -q "Do not edit" test.2.result.env ; then \
		cmp test.2.3.result.var test.2.3.new.goal.var ; \
	else \
		cmp test.2.3.result.var test.2.3.old.goal.var ; \
	fi
	$(quiet)rm test.2.*result.* test.2.*export.var
	$(quiet)echo passed
	$(quiet)echo testing efivar -L
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -L | \
		grep -q '^{91376aff-cba6-42be-949d-06fde81128e8} {grub} efi_guid_grub GRUB$$'
	$(quiet)echo passed

test3:
	$(quiet)echo testing with BootOrder variable dmpstore generated
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.3.goal.var -e test.3.0.result.var
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.3.0.result.var -e test.3.1.result.var -D
	$(quiet)cmp test.3.goal.var test.3.1.result.var
	$(quiet)rm test.3.0.result.var test.3.1.result.var
	$(quiet)echo passed

test4:
	$(quiet)echo testing with ConIn variable dmpstore generated
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.4.goal.var -e test.4.0.result.var
	$(quiet)LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.4.0.result.var -e test.4.1.result.var -D
	$(quiet)cmp test.4.goal.var test.4.1.result.var
	$(quiet)rm test.4.0.result.var test.4.1.result.var
	$(quiet)echo passed

test5:
	$(quiet)echo testing threading in libefivar
	$(quiet)TOPDIR=$(TOPDIR) $(TOPDIR)/tests/test-threading

.PHONY: all clean test0
# vim:ft=make
#
