SRCDIR = $(realpath .)

include $(TOPDIR)/src/include/defaults.mk
include $(TOPDIR)/src/include/version.mk

CCLDFLAGS += -L$(TOPDIR)/src/ -Wl,-rpath=$(TOPDIR)/src/
LIBS=efivar

all : tester

install :

clean :
	@rm -rfv tester *.o *.E *.S

test : tester
	./tester

tester :: tester.o
	$(CC) $(cflags) $(LDFLAGS) -Wl,-rpath,$(TOPDIR)/src -L$(TOPDIR)/src -o $@ $^ -lefivar -ldl

.PHONY: all clean install test

include $(TOPDIR)/src/include/rules.mk
