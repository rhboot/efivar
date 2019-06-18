#
# Makefile
# Peter Jones, 2019-06-18 11:10
#

all: clean test0 test1 test2 test3 test4

GRUB_PREFIX ?= grub2
TOPDIR ?= ..
EFIVAR ?= ../src/efivar

clean:
	@rm -f *.result.env *.result.var

test0:
	@echo testing export to DMPSTORE format
	@$(GRUB_PREFIX)-editenv test.0.result.env create
	@$(GRUB_PREFIX)-editenv test.0.result.env set debug=all,-scripting,-lexer
	@truncate -s 512 test.0.result.env
	@LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -n {grub}-GRUB_ENV -f test.0.result.env -D -e test.0.result.var
	@cmp test.0.result.var test.0.goal.var
	@rm test.0.result.*
	@echo passed

test1:
	@echo testing export to libefivar format
	@$(GRUB_PREFIX)-editenv test.1.result.env create
	@$(GRUB_PREFIX)-editenv test.1.result.env set debug=all,-scripting,-lexer
	@truncate -s 512 test.1.result.env
	@LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -n {grub}-GRUB_ENV -f test.1.result.env -e test.1.result.var
	@cmp test.1.result.var test.1.goal.var
	@rm test.1.result.*
	@echo passed

test2:
	@$(GRUB_PREFIX)-editenv test.2.result.env create
	@$(GRUB_PREFIX)-editenv test.2.result.env set debug=all,-scripting,-lexer
	@truncate -s 512 test.2.result.env
	@echo test importing from DMPSTORE and exporting to DMPSTORE and symbolic guid-name validation
	@LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -n {grub}-GRUB_ENV -f test.2.result.env -D -e test.2.0.goal.var
	@LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.2.0.goal.var -e test.2.0.result.var -D
	@cmp test.2.0.result.var test.2.0.result.var
	@rm test.2.0.*
	@echo passed
	@echo test importing from DMPSTORE and exporting to libefivar
	@LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -n {grub}-GRUB_ENV -f test.2.result.env -D -e test.2.1.goal.var
	@LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.2.1.goal.var -e test.2.1.result.var
	@cmp test.2.1.result.var test.2.1.result.var
	@rm test.2.1.*
	@echo passed
	@echo test importing from libefivar and exporting to DMPSTORE and mixed-case guid-name validation
	@LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -n 91376aff-cBa6-42bE-949d-06fde81128e8-GRUB_ENV -f test.2.result.env -e test.2.2.goal.var
	@LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.2.2.goal.var -e test.2.2.result.var -D
	@cmp test.2.2.result.var test.2.2.result.var
	@rm test.2.2.*
	@echo passed
	@echo test importing from libefivar and exporting to libefivar and guid-name validation
	@LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -n 91376aff-cba6-42be-949d-06fde81128e8-GRUB_ENV -f test.2.result.env -e test.2.3.goal.var
	@LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.2.3.goal.var -e test.2.3.result.var
	@cmp test.2.3.result.var test.2.3.result.var
	@rm test.2.3.* test.2.result.env
	@echo passed
	@echo testing efivar -L
	@LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -L | \
		grep -q '^{91376aff-cba6-42be-949d-06fde81128e8} {grub} efi_guid_grub GRUB$$'
	@echo passed

test3:
	@echo testing with BootOrder variable dmpstore generated
	@LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.3.goal.var -e test.3.0.result.var
	@LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.3.0.result.var -e test.3.1.result.var -D
	@cmp test.3.goal.var test.3.1.result.var
	@rm test.3.0.result.var test.3.1.result.var
	@echo passed

test4:
	@echo testing with ConIn variable dmpstore generated
	@LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.4.goal.var -e test.4.0.result.var
	@LD_LIBRARY_PATH=$(TOPDIR)/src $(EFIVAR) -i test.4.0.result.var -e test.4.1.result.var -D
	@cmp test.4.goal.var test.4.1.result.var
	@rm test.4.0.result.var test.4.1.result.var
	@echo passed

.PHONY: all clean test0
# vim:ft=make
#
