SRCDIR = .
TOPDIR = $(realpath $(SRCDIR)/../..)

include $(TOPDIR)/Make.defaults

CCLDFLAGS =-L$(TOPDIR)/src/ -Wl,-rpath=$(TOPDIR)/src/
LIBS=efivar

all :

install :

clean :
	@rm -rfv tester *.o

test : tester
	./tester

tester :: tester.o

.PHONY: all clean install test

include $(TOPDIR)/Make.rules
