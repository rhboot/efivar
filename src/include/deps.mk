SRCDIR = $(realpath .)

all : deps

include $(TOPDIR)/src/include/version.mk
include $(TOPDIR)/src/include/rules.mk
include $(TOPDIR)/src/include/defaults.mk

.%.d : %.c
	$(quiet)$(CC) $(CFLAGS) $(CPPFLAGS) -MM -MG -MF $@ $^
	$(quiet)sed -i 's/:/: |/g' $@

.%.d : %.S
	$(quiet)$(CC) $(CFLAGS) $(CPPFLAGS) -MM -MG -MF $@ $^
	$(quiet)sed -i 's/:/: |/g' $@

SOURCES ?=

deps : $(call deps-of,$(filter-out %.h,$(SOURCES)))

.PHONY: deps
