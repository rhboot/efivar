SRCDIR = $(realpath .)

all : deps

include $(TOPDIR)/src/include/version.mk
include $(TOPDIR)/src/include/rules.mk
include $(TOPDIR)/src/include/defaults.mk

.%.d : %.c
	$(CC) $(cflags) $(CPPFLAGS) -MM -MG -MF $@ $^

.%.d : %.S
	$(CC) $(cflags) $(CPPFLAGS) -MM -MG -MF $@ $^

SOURCES ?=

deps : $(call deps-of,$(filter-out %.h,$(SOURCES)))

.PHONY: deps
