SRCDIR = $(realpath .)

all : deps

include $(TOPDIR)/src/include/version.mk
include $(TOPDIR)/src/include/rules.mk
include $(TOPDIR)/src/include/defaults.mk

.%.d : %.c
	@$(CC) $(CFLAGS) $(CPPFLAGS) -MM -MG -MF $@ $^
	@sed -i 's/:/: |/g' $@

.%.d : %.S
	@$(CC) $(CFLAGS) $(CPPFLAGS) -MM -MG -MF $@ $^
	@sed -i 's/:/: |/g' $@

SOURCES ?=

deps : $(call deps-of,$(filter-out %.h,$(SOURCES)))

.PHONY: deps
