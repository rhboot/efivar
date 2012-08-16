SRCDIR = .
TOPDIR = $(SRCDIR)/..

SONAME = libefivar.so.0

include $(TOPDIR)/Make.defaults

TARGETS = libefivar.so libefivar.a
all : $(TARGETS)

SOURCES = $(patsubst %.c,%,$(wildcard *.c))
OBJECTS = $(foreach source,$(SOURCES),$(source).o)

libefivar.a :: $(OBJECTS)

libefivar.so :: $(OBJECTS)

clean : 
	@rm -rfv *~ *.o *.a *.so

install :
	echo "LIBDIR: $(LIBDIR)"
	$(INSTALL) -d -m 755 $(INSTALLROOT)$(LIBDIR)
	for x in $(TARGETS) ; do \
		$(INSTALL) -m 755  $$x $(INSTALLROOT)$(LIBDIR) ; \
	done

.PHONY: all clean install

include $(TOPDIR)/Make.rules
