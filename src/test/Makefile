SRCDIR = $(realpath .)

include $(TOPDIR)/src/include/defaults.mk
include $(TOPDIR)/src/include/version.mk

CCLDFLAGS += -L$(TOPDIR)/src/ -Wl,-rpath=$(TOPDIR)/src/
LIBS=efivar

all : tester

install :

clean :
	$(quiet)rm -rf $(rmverbose) tester *.o *.E *.S

test : tester
	$(quiet)./tester

tester :: tester.o
	$(quiet)$(CC) $(cflags) $(LDFLAGS) -Wl,-rpath,$(TOPDIR)/src -L$(TOPDIR)/src -o $@ $^ -lefivar -ldl

.PHONY: all clean install test

include $(TOPDIR)/src/include/rules.mk
