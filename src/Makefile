SRCDIR = $(realpath .)
TOPDIR = $(realpath ..)

include $(TOPDIR)/Make.deprecated
include $(TOPDIR)/Make.version
include $(TOPDIR)/Make.rules
include $(TOPDIR)/Make.defaults

LIBTARGETS=libefivar.so libefiboot.so
STATICLIBTARGETS=libefivar.a libefiboot.a
BINTARGETS=efivar
STATICBINTARGETS=efivar-static
PCTARGETS=efivar.pc efiboot.pc
TARGETS=$(LIBTARGETS) $(BINTARGETS) $(PCTARGETS)
STATICTARGETS=$(STATICLIBTARGETS) $(STATICBINTARGETS)

LIBEFIBOOT_SOURCES = crc32.c creator.c disk.c gpt.c loadopt.c path-helpers.c \
		     linux.c $(wildcard linux-*.c)
LIBEFIBOOT_OBJECTS = $(patsubst %.c,%.o,$(LIBEFIBOOT_SOURCES))
LIBEFIVAR_SOURCES = dp.c dp-acpi.c dp-hw.c dp-media.c dp-message.c \
	efivarfs.c error.c export.c guid.c guids.S guid-symbols.c \
	lib.c vars.c
LIBEFIVAR_OBJECTS = $(patsubst %.S,%.o,$(patsubst %.c,%.o,$(LIBEFIVAR_SOURCES)))
EFIVAR_SOURCES = efivar.c
GENERATED_SOURCES = include/efivar/efivar-guids.h guid-symbols.c
MAKEGUIDS_SOURCES = makeguids.c guid.c
ALL_SOURCES=$(LIBEFIBOOT_SOURCES) $(LIBEFIVAR_SOURCES) $(MAKEGUIDS_SOURCES) \
	$(wildcard include/efivar/*.h) $(GENERATED_SOURCES) $(EFIVAR_SOURCES)

$(call deps-of,$(ALL_SOURCES)) : | deps
-include $(call deps-of,$(ALL_SOURCES))

all : $(TARGETS)

static : $(STATICTARGETS)

abiclean :
	@rm -vf $(patsubst %.so,%.abixml,$@)

abixml : | $(LIBTARGETS)
abixml : $(patsubst %.so,%.abixml,$(LIBTARGETS))

abidw : $(patsubst %.so,%.abixml,$(LIBTARGETS))
	git commit -s --amend $^

abicheck : $(patsubst %.so,%.abicheck,$(LIBTARGETS))

./guid-symbols.c : include/efivar/efivar-guids.h
./guids.bin : include/efivar/efivar-guids.h
./names.bin : include/efivar/efivar-guids.h
include/efivar/efivar-guids.h : makeguids guids.txt
	./makeguids guids.txt guids.bin names.bin \
		guid-symbols.c include/efivar/efivar-guids.h

makeguids : CPPFLAGS+=-DEFIVAR_BUILD_ENVIRONMENT
makeguids : LIBS=dl
makeguids : $(MAKEGUIDS_SOURCES)
makeguids : CCLD=$(CCLD_FOR_BUILD)

guids.o : guids.S | guids.bin names.bin

$(LIBEFIVAR_OBJECTS) $(LIBEFIBOOT_OBJECTS) : | $(GENERATED_SOURCES)

libefivar.a : | $(GENERATED_SOURCES)
libefivar.a : $(patsubst %.o,%.static.o,$(LIBEFIVAR_OBJECTS))

libefivar.so : $(LIBEFIVAR_OBJECTS)
libefivar.so : | $(GENERATED_SOURCES) libefivar.map
libefivar.so : LIBS=dl
libefivar.so : MAP=libefivar.map

efivar : efivar.c | libefivar.so
efivar : LIBS=efivar dl

efivar-static : efivar.c $(patsubst %.o,%.static.o,$(LIBEFIVAR_OBJECTS))
efivar-static : | $(GENERATED_SOURCES)
efivar-static : LIBS=dl

libefiboot.a : $(patsubst %.o,%.static.o,$(LIBEFIBOOT_OBJECTS))

libefiboot.so : $(LIBEFIBOOT_OBJECTS)
libefiboot.so : | libefiboot.map libefivar.so
libefiboot.so : LIBS=efivar
libefiboot.so : MAP=libefiboot.map

deps : $(ALL_SOURCES)
	$(MAKE) -f $(SRCDIR)/Make.deps deps SOURCES="$(ALL_SOURCES)"

clean : 
	@rm -rfv *~ *.o *.a *.E *.so *.so.* *.pc *.bin .*.d *.map \
		makeguids guid-symbols.c include/efivar/efivar-guids.h \
		$(TARGETS) $(STATICTARGETS)
	@# remove the deps files we used to create, as well.
	@rm -rfv .*.P .*.h.P *.S.P include/efivar/.*.h.P

install : all
	$(INSTALL) -d -m 755 $(DESTDIR)$(LIBDIR)
	$(foreach x,$(LIBTARGETS), \
		$(INSTALL) -m 755 $(x) $(DESTDIR)$(LIBDIR)/$(x).1.$(VERSION) ;\
		ln -fs $(x).1.$(VERSION) $(DESTDIR)$(LIBDIR)/$(x).1 ;\
		ln -fs $(x).1.$(VERSION) $(DESTDIR)$(LIBDIR)/$(x) ;\
		)
	$(INSTALL) -d -m 755 $(DESTDIR)$(PCDIR)
	$(foreach x, $(PCTARGETS), $(INSTALL) -m 644 $(x) $(DESTDIR)$(PCDIR) ;)
	$(INSTALL) -d -m 755 $(DESTDIR)$(INCLUDEDIR)/efivar
	$(foreach x, $(wildcard $(TOPDIR)/src/include/efivar/*.h), $(INSTALL) -m 644 $(x) $(DESTDIR)$(INCLUDEDIR)/efivar/$(notdir $(x));)
	$(INSTALL) -d -m 755 $(DESTDIR)$(BINDIR)
	$(foreach x, $(BINTARGETS), $(INSTALL) -m 755 $(x) $(DESTDIR)$(BINDIR);)

test : all
	$(MAKE) -C test $@

.PHONY: test deps abiclean abixml
.SECONDARY : libefivar.so.1.$(VERSION) libefivar.so.1
.SECONDARY : libefiboot.so.1.$(VERSION) libefivar.so.1
.SECONDARY : include/efivar/efivar-guids.h guid-symbols.c
.INTERMEDIATE : guids.bin names.bin
.PRECIOUS : guid-symbols.o makeguids
