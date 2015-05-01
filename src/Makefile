SRCDIR = $(realpath .)
TOPDIR = $(realpath ..)

include $(TOPDIR)/Make.defaults

LIBTARGETS = libefivar.so.$(SONAME_VERSION) libefiboot.so.$(SONAME_VERSION)
PCTARGETS = efivar.pc efiboot.pc
BINTARGETS = efivar
INCTARGETS = include/efivar/efivar-guids.h
all : $(LIBTARGETS) $(PCTARGETS) $(BINTARGETS) $(INCTARGETS) libefiboot.so
	@$(MAKE) -C test TOPDIR=$(TOPDIR) SRCDIR=$(SRCDIR)/test $@

EFIVAR_OBJECTS = dp.o dp-acpi.o dp-hw.o dp-media.o dp-message.o \
	efivarfs.o export.o guid.o guidlist.o guid-symbols.o \
	lib.o vars.o
# c files (alphabetically), then local headers (alphabetically),
# then target headers (again alphabetically)
EFIVAR_DEPS = .dp.c.P .dp-acpi.c.P .dp-hw.c.P .dp-media.c.P .dp-message.c.P \
	.efivar.c.P .efivarfs.c.P .export.c.P .guid.c.P .lib.c.P .vars.c.P \
	.dp.h.P .generics.h.P .guid.h.P .lib.h.P \
	include/efivar/.efivar.h.P include/efivar/.efivar-dp.h.P \
	include/efivar/.efivar-guids.h.P
EFIVAR_LIBS = dl

MAKEGUIDS_DEPS = .makeguids.c.P .efivar.h.P .util.h.P .guid.h.P

EFIBOOT_OBJECTS =  \
	crc32.o creator.o disk.o gpt.o linux.o loadopt.o
EFIBOOT_DEPS = \
	.crc32.c.P .crc32.h.P .creator.c.P .disk.c.P .disk.h.P \
	.gpt.c.P .gpt.h.P .linux.c.P .linux.h.P .loadopt.c.P \
	include/efivar/.efivar-creator.h.P \
	include/efivar/.efivar-loadopt.h.P

libefivar.a :: $(EFIVAR_OBJECTS)

libefivar.so.$(SONAME_VERSION) :: $(EFIVAR_OBJECTS)

libefiboot.a :: $(EFIBOOT_OBJECTS)

libefiboot.so.$(SONAME_VERSION) :: $(EFIBOOT_OBJECTS)

efivar : efivar.o libefivar.so
	$(CCLD) $(ccldflags) -L. -lefivar -o $@ $^ \
		-lpopt $(foreach lib,$(EFIVAR_LIBS),-l$(lib))

%.pc : %.pc.in
	sed -e "s,@@VERSION@@,$(VERSION),g" \
	    -e "s,@@LIBDIR@@,$(libdir),g" \
		$< > $@

include/efivar/efivar.h : include/efivar/efivar-guids.h

fakeguid.o : guid.c
	$(CC) $(cflags) -DEFIVAR_BUILD_ENVIRONMENT -c -o $@ $^

makeguids.o : makeguids.c
	$(CC) $(cflags) -DEFIVAR_BUILD_ENVIRONMENT -c -o $@ $^

makeguids : makeguids.o fakeguid.o
	$(CC) $(cflags) -o $@ $^ -ldl

include/efivar/efivar-guids.h : makeguids guids.txt
	./makeguids guids.txt guids.bin names.bin guid-symbols.S $@

guidlist.o : include/efivar/efivar-guids.h
	$(CC) $(cflags) -c -o guidlist.o guids.S

guid-symbols.o : guid-symbols.S
	$(CC) $(cflags) -c -o $@ $<

.INTERMEDIATE: guids.bin names.bin guid-symbols.S

deps : $(EFIVAR_DEPS) $(EFIBOOT_DEPS) $(MAKEGUIDS_DEPS)

-include $(EFIVAR_DEPS)
-include $(EFIBOOT_DEPS)
-include $(MAKEGUIDS_DEPS)

clean : 
	@rm -rfv *~ *.o *.a *.so *.so.$(SONAME_VERSION) .*.c.P .*.h.P $(PCTARGETS) $(BINTARGETS) $(INCTARGETS) *.bin guid-symbols.S makeguids include/efivar/.*.h.P $(LIBTARGETS)
	@$(MAKE) -C test TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/src/ $@

install : all
	$(INSTALL) -d -m 755 $(DESTDIR)$(libdir)
	$(foreach x, $(LIBTARGETS), $(INSTALL) -m 755 $(x) $(DESTDIR)$(libdir);)
	$(INSTALL) -d -m 755 $(DESTDIR)$(PCDIR)
	$(foreach x, $(PCTARGETS), $(INSTALL) -m 644 $(x) $(DESTDIR)$(PCDIR) ;)
	$(INSTALL) -d -m 755 $(DESTDIR)$(includedir)/efivar
	$(foreach x, $(wildcard $(TOPDIR)/src/include/efivar/*.h), $(INSTALL) -m 644 $(x) $(DESTDIR)$(includedir)/efivar/$(notdir $(x));)
	$(INSTALL) -d -m 755 $(DESTDIR)$(bindir)
	$(foreach x, $(BINTARGETS), $(INSTALL) -m 755 $(x) $(DESTDIR)$(bindir);)
	$(foreach x, $(wildcard *.so.$(SONAME_VERSION)), ln -fs $(x) $(patsubst %.so.$(SONAME_VERSION),%.so,$(DESTDIR)$(libdir)/$(x));)

test :all
	$(MAKE) -C test TOPDIR=$(TOPDIR) SRCDIR=$(TOPDIR)/src/ $@

.PHONY: all deps clean install test

include $(TOPDIR)/Make.rules
