SRCDIR = $(realpath .)

include $(TOPDIR)/src/include/deprecated.mk
include $(TOPDIR)/src/include/version.mk
include $(TOPDIR)/src/include/rules.mk
include $(TOPDIR)/src/include/defaults.mk

LIBTARGETS=libefivar.so libefiboot.so
STATICLIBTARGETS=libefivar.a libefiboot.a
BINTARGETS=efivar thread-test
STATICBINTARGETS=efivar-static
PCTARGETS=efivar.pc efiboot.pc
TARGETS=$(LIBTARGETS) $(BINTARGETS) $(PCTARGETS)
STATICTARGETS=$(STATICLIBTARGETS) $(STATICBINTARGETS)

LIBEFIBOOT_SOURCES = crc32.c creator.c disk.c gpt.c loadopt.c path-helpers.c \
		     linux.c $(sort $(wildcard linux-*.c))
LIBEFIBOOT_OBJECTS = $(patsubst %.c,%.o,$(LIBEFIBOOT_SOURCES))
LIBEFIVAR_SOURCES = crc32.c dp.c dp-acpi.c dp-hw.c dp-media.c dp-message.c \
	efivarfs.c error.c export.c guid.c guids.S guid-symbols.c \
	lib.c vars.c time.c
LIBEFIVAR_OBJECTS = $(patsubst %.S,%.o,$(patsubst %.c,%.o,$(LIBEFIVAR_SOURCES)))
EFIVAR_SOURCES = efivar.c
EFIVAR_OBJECTS = $(patsubst %.S,%.o,$(patsubst %.c,%.o,$(EFIVAR_SOURCES)))
GENERATED_SOURCES = include/efivar/efivar-guids.h guid-symbols.c
MAKEGUIDS_SOURCES = makeguids.c
MAKEGUIDS_OBJECTS = $(patsubst %.S,%.o,$(patsubst %.c,%.o,$(MAKEGUIDS_SOURCES)))
MAKEGUIDS_OUTPUT = guids.bin names.bin $(GENERATED_SOURCES)

ALL_SOURCES=$(LIBEFIBOOT_SOURCES) $(LIBEFIVAR_SOURCES) $(MAKEGUIDS_SOURCES) \
	$(sort $(wildcard include/efivar/*.h)) $(GENERATED_SOURCES) $(EFIVAR_SOURCES)

ifneq ($(MAKECMDGOALS),clean)
$(call deps-of,$(ALL_SOURCES)) : | deps
-include $(call deps-of,$(ALL_SOURCES))
endif

all : $(TARGETS)

static : $(STATICTARGETS)

$(BINTARGETS) : | $(LIBTARGETS) $(PCTARGETS)
$(STATICTARGETS) : | $(STATICLIBTARGETS) $(PCTARGETS)

abiclean :
	@rm -vf $(patsubst %.so,%.abixml,$@)

abixml : | $(LIBTARGETS)
abixml : $(patsubst %.so,%.abixml,$(LIBTARGETS))

abidw : $(patsubst %.so,%.abixml,$(LIBTARGETS))
	git commit -s --amend $^

abicheck : $(patsubst %.so,%.abicheck,$(LIBTARGETS))

makeguids : CPPFLAGS=$(HOST_CPPFLAGS)
makeguids : LIBS=dl
makeguids : CC=$(HOSTCC)
makeguids : CCLD=$(HOSTCCLD)
makeguids : CFLAGS=$(HOST_CFLAGS)
makeguids : LDFLAGS=$(HOST_LDFLAGS)
makeguids : CCLDFLAGS=$(HOST_CCLDFLAGS)
makeguids : $(MAKEGUIDS_OBJECTS)

$(MAKEGUIDS_OUTPUT) : makeguids
$(MAKEGUIDS_OUTPUT) : guids.txt
	@set -e ; \
	missing=no ; \
	for x in $$(cat guids.txt  | awk '{ print $$2 }' | grep -v ^zz) ; do \
		if ! grep -q "efi_guid_$${x}" libefivar.map.in ; then \
			echo missing symbol "efi_guid_$${x}" ; \
			missing=yes ; \
		fi ; \
	done ; \
	if [ "$${missing}" != "no" ]; then \
		exit 1 ; \
	fi
	./makeguids guids.txt guids.bin names.bin \
		guid-symbols.c include/efivar/efivar-guids.h

guids.o : | makeguids
guids.o : guids.S guids.bin names.bin

prep : makeguids $(GENERATED_SOURCES)

$(LIBEFIVAR_OBJECTS) $(LIBEFIBOOT_OBJECTS) : prep

libefivar.a : | $(GENERATED_SOURCES)
libefivar.a : $(patsubst %.o,%.static.o,$(LIBEFIVAR_OBJECTS))

libefivar.so : $(LIBEFIVAR_OBJECTS)
libefivar.so : | $(GENERATED_SOURCES) libefivar.map
libefivar.so : LIBS=dl
libefivar.so : MAP=libefivar.map

efivar : $(EFIVAR_OBJECTS) | libefivar.so
efivar : LIBS=efivar dl

efivar-static : $(EFIVAR_OBJECTS) $(patsubst %.o,%.static.o,$(LIBEFIVAR_OBJECTS))
efivar-static : | $(GENERATED_SOURCES)
efivar-static : LIBS=dl

libefiboot.a : $(patsubst %.o,%.static.o,$(LIBEFIBOOT_OBJECTS))

libefiboot.so : $(LIBEFIBOOT_OBJECTS)
libefiboot.so : | libefiboot.map libefivar.so
libefiboot.so : LIBS=efivar
libefiboot.so : MAP=libefiboot.map

thread-test : libefivar.so
thread-test : CFLAGS=$(HOST_CFLAGS) -I$(TOPDIR)/src/include/efivar
thread-test : LIBS=pthread efivar

deps : $(ALL_SOURCES)
	@$(MAKE) -f $(SRCDIR)/include/deps.mk deps SOURCES="$(ALL_SOURCES)"

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
	$(foreach x, $(sort $(wildcard $(TOPDIR)/src/include/efivar/*.h)), $(INSTALL) -m 644 $(x) $(DESTDIR)$(INCLUDEDIR)/efivar/$(notdir $(x));)
	$(INSTALL) -d -m 755 $(DESTDIR)$(BINDIR)
	$(foreach x, $(filter-out %-test,$(BINTARGETS)), $(INSTALL) -m 755 $(x) $(DESTDIR)$(BINDIR);)

test : all
	$(MAKE) -C test $@

.PHONY: abiclean abicheck abidw abixml all
.PHONY: clean deps install test
.SECONDARY : libefivar.so.1.$(VERSION) libefivar.so.1
.SECONDARY : libefiboot.so.1.$(VERSION) libefiboot.so.1
.SECONDARY : $(GENERATED_SOURCES)
.INTERMEDIATE : guids.bin names.bin
.PRECIOUS : makeguids
