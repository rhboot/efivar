SRCDIR = $(realpath .)
TOPDIR = $(realpath ..)

include $(TOPDIR)/Make.version
include $(TOPDIR)/Make.rules
include $(TOPDIR)/Make.defaults
-include $(call deps-of,$(ALL_SOURCES))

LIBTARGETS=libefivar.so libefiboot.so
STATICLIBTARGETS=libefivar.a libefiboot.a
BINTARGETS=efivar efivar-static
PCTARGETS=efivar.pc efiboot.pc
TARGETS=$(LIBTARGETS) $(STATICLIBTARGETS) $(BINTARGETS) $(PCTARGETS)

LIBEFIBOOT_SOURCES = crc32.c creator.c disk.c gpt.c linux.c loadopt.c
LIBEFIBOOT_OBJECTS = $(patsubst %.c,%.o,$(LIBEFIBOOT_SOURCES))
LIBEFIVAR_SOURCES = dp.c dp-acpi.c dp-hw.c dp-media.c dp-message.c \
	efivarfs.c export.c guid.c guids.S guid-symbols.c \
	lib.c vars.c
LIBEFIVAR_OBJECTS = $(patsubst %.S,%.o,$(patsubst %.c,%.o,$(LIBEFIVAR_SOURCES)))
EFIVAR_SOURCES = efivar.c
GENERATED_SOURCES = include/efivar/efivar-guids.h guid-symbols.c
MAKEGUIDS_SOURCES = makeguids.c guid.c
ALL_SOURCES=$(LIBEFIBOOT_SOURCES) $(LIBEFIVAR_SOURCES) $(MAKEGUIDS_SOURCES) \
	$(wildcard include/efivar/*.h) $(GENERATED_SOURCES) $(EFIVAR_SOURCES)

all : deps $(TARGETS)

./guid-symbols.c : include/efivar/efivar-guids.h
./guids.bin : include/efivar/efivar-guids.h
./names.bin : include/efivar/efivar-guids.h
include/efivar/efivar-guids.h : makeguids guids.txt
	./makeguids guids.txt guids.bin names.bin \
		guid-symbols.c include/efivar/efivar-guids.h

makeguids : CPPFLAGS+=-DEFIVAR_BUILD_ENVIRONMENT
makeguids : LIBS=dl
makeguids : $(MAKEGUIDS_SOURCES)

guids.o : guids.S | guids.bin names.bin

$(LIBEFIVAR_OBJECTS) $(LIBEFIBOOT_OBJECTS) : | $(GENERATED_SOURCES)

libefivar.a : | $(GENERATED_SOURCES)
libefivar.a : $(patsubst %.o,%.static.o,$(LIBEFIVAR_OBJECTS))

libefivar.so : $(LIBEFIVAR_OBJECTS)
libefivar.so : | $(GENERATED_SOURCES)
libefivar.so : LIBS=dl

efivar : efivar.c | libefivar.so
efivar : LIBS=efivar dl
efivar : PKGS=popt

efivar-static : efivar.c $(patsubst %.o,%.static.o,$(LIBEFIVAR_OBJECTS))
efivar-static : | $(GENERATED_SOURCES)
efivar-static : LIBS=dl
efivar-static : PKGS=popt

libefiboot.a : $(patsubst %.o,%.static.o,$(LIBEFIBOOT_OBJECTS))

libefiboot.so : $(LIBEFIBOOT_OBJECTS)

deps :: $(ALL_SOURCES)
	$(MAKE) -f $(SRCDIR)/Make.deps deps SOURCES="$(ALL_SOURCES)"

clean : 
	@rm -rfv *~ *.o *.a *.E *.so *.so.* *.pc *.bin .*.d \
		makeguids guid-symbols.c include/efivar/efivar-guids.h \
		$(BINTARGETS)
	@# remove the deps files we used to create, as well.
	@rm -rfv .*.P .*.h.P *.S.P include/efivar/.*.h.P

install : all
	$(INSTALL) -d -m 755 $(DESTDIR)$(libdir)
	$(foreach x,$(LIBTARGETS), \
		$(INSTALL) -m 755 $(x) $(DESTDIR)$(libdir)/$(x).$(VERSION) ;\
		ln -fs $(x).$(VERSION) $(DESTDIR)$(libdir)/$(x).$(MAJOR_VERSION);\
		ln -fs $(x).$(VERSION) $(DESTDIR)$(libdir)/$(x); )
	$(INSTALL) -d -m 755 $(DESTDIR)$(PCDIR)
	$(foreach x, $(PCTARGETS), $(INSTALL) -m 644 $(x) $(DESTDIR)$(PCDIR) ;)
	$(INSTALL) -d -m 755 $(DESTDIR)$(includedir)/efivar
	$(foreach x, $(wildcard $(TOPDIR)/src/include/efivar/*.h), $(INSTALL) -m 644 $(x) $(DESTDIR)$(includedir)/efivar/$(notdir $(x));)
	$(INSTALL) -d -m 755 $(DESTDIR)$(bindir)
	$(foreach x, $(BINTARGETS), $(INSTALL) -m 755 $(x) $(DESTDIR)$(bindir);)

test : all
	$(MAKE) -C test $@

.PHONY: test
.SECONDARY : libefivar.so.$(VERSION) libefivar.so.$(MAJOR_VERSION)
.SECONDARY : libefiboot.so.$(VERSION) libefiboot.so.$(MAJOR_VERSION)
.SECONDARY : include/efivar/efivar-guids.h guid-symbols.c
.INTERMEDIATE : guids.bin names.bin
.PRECIOUS : guid-symbols.o makeguids
