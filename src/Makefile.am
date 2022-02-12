AM_CPPFLAGS +=				\
	-I$(srcdir)/src			\
	-I$(srcdir)/src/include		\
	-I$(builddir)/src		\
	-I$(builddir)/src/include

libefiboot_ladir = $(includedir)/efivar
libefivar_ladir = $(includedir)/efivar
efisecdbdir = $(includedir)/efivar

EXTRA_DIST = $(srcdir)/src/guids.txt

pkgconfig_DATA =	\
	src/efiboot.pc	\
	src/efisec.pc	\
	src/efivar.pc

bin_PROGRAMS += 	\
	efisecdb	\
	efivar

noinst_PROGRAMS +=	\
	makeguids	\
	tester		\
	thread-test

lib_LTLIBRARIES +=	\
	libefiboot.la	\
	libefisec.la	\
	libefivar.la

makeguids_CPPFLAGS = $(AM_CPPFLAGS) -DEFIVAR_BUILD_ENVIRONMENT=1
makeguids_CFLAGS = $(AM_CFLAGS)
makeguids_SOURCES =				\
	src/compiler.h				\
	src/diag.h				\
	src/disk.h				\
	src/dp.h				\
	src/efivar.h				\
	src/efivar_endian.h			\
	src/fix_coverity.h			\
	src/generics.h				\
	src/gpt.h				\
	src/guid.h				\
	src/hexdump.h				\
	src/lib.h				\
	src/list.h				\
	src/linux.h				\
	src/makeguids.c				\
	src/makeguids.h				\
	src/path-helpers.h			\
	src/safemath.h				\
	src/ucs2.h				\
	src/util.c				\
	src/util.h

thread_test_SOURCES =	\
	src/thread-test.c
thread_test_LDADD = -lpthread libefivar.la

tester_SOURCES =	\
	src/test/tester.c
tester_LDADD = libefivar.la -ldl

libefiboot_la_LDFLAGS =								\
	$(AM_LDFLAGS)								\
	-no-undefined								\
	-version-number $(LIBEFIVAR_MAJOR):$(LIBEFIVAR_MINOR):$(LIBEFIVAR_PATCH)
libefiboot_la_HEADERS =				\
	src/include/efivar/efiboot-creator.h	\
	src/include/efivar/efiboot.h		\
	src/include/efivar/efiboot-loadopt.h
libefiboot_la_SOURCES =				\
	src/crc32.c				\
	src/crc32.h				\
	src/creator.c				\
	src/disk.c				\
	src/efiboot.h				\
	src/gpt.c				\
	src/include/efivar/efiboot-loadopt.h	\
	src/linux-acpi.c			\
	src/linux-acpi-root.c			\
	src/linux-ata.c				\
	src/linux.c				\
	src/linux-emmc.c			\
	src/linux-i2o.c				\
	src/linux-md.c				\
	src/linux-nvme.c			\
	src/linux-pci.c				\
	src/linux-pci-root.c			\
	src/linux-pmem.c			\
	src/linux-sas.c				\
	src/linux-sata.c			\
	src/linux-scsi.c			\
	src/linux-soc-root.c			\
	src/linux-virtblk.c			\
	src/linux-virtual-root.c		\
	src/loadopt.c				\
	src/path-helpers.c
libefiboot_la_LIBDADD = libefivar.la

libefisec_la_LDFLAGS =								\
	$(AM_LDFLAGS)								\
	-no-undefined								\
	-version-number $(LIBEFIVAR_MAJOR):$(LIBEFIVAR_MINOR):$(LIBEFIVAR_PATCH)
libefisec_la_SOURCES =	\
	src/esl-iter.c	\
	src/sec.c	\
	src/secdb.c	\
	src/secdb.h	\
	src/util.c

libefivar_la_LDFLAGS =								\
	$(AM_LDFLAGS)								\
	-no-undefined								\
	-version-number $(LIBEFIVAR_MAJOR):$(LIBEFIVAR_MINOR):$(LIBEFIVAR_PATCH)
libefivar_la_HEADERS =				\
	src/include/efivar/efivar-dp.h		\
	src/include/efivar/efivar-guids.h	\
	src/include/efivar/efivar.h		\
	src/include/efivar/efivar-time.h	\
	src/include/efivar/efivar-types.h
libefivar_la_SOURCES =				\
	src/crc32.c				\
	src/dp-acpi.c				\
	src/dp.c				\
	src/dp-hw.c				\
	src/dp-media.c				\
	src/dp-message.c			\
	src/efivarfs.c				\
	src/error.c				\
	src/export.c				\
	src/guid.c				\
	src/include/efivar/efivar-dp.h		\
	src/lib.c				\
	src/time.c				\
	src/vars.c
nodist_libefivar_la_SOURCES =			\
	src/guid-symbols.c			\
	src/include/efivar/efivar-guids.h
src/guid.lo: src/include/efivar/efivar-guids.h

efisecdb_CFLAGS = $(AM_CFLAGS)
efisecdb_HEADERS =				\
	src/include/efivar/efisec.h		\
	src/include/efivar/efisec-secdb.h	\
	src/include/efivar/efisec-types.h
efisecdb_SOURCES =				\
	src/efisecdb.c				\
	src/efisec.h				\
	src/esl-iter.h				\
	src/include/efivar/efisec.h		\
	src/include/efivar/efisec-secdb.h	\
	src/include/efivar/efisec-types.h	\
	src/secdb-dump.c			\
	src/util.c				\
	src/x509.h
efisecdb_LDADD = libefisec.la libefivar.la -ldl
nodist_efisecdb_SOURCES =			\
	src/guid-symbols.c			\
	src/include/efivar/efivar-guids.h
src/efisecdb-efisecdb.$(OBJEXT): src/include/efivar/efivar-guids.h

efivar_CFLAGS = $(AM_CFLAGS)
efivar_SOURCES =	\
	src/efivar.c	\
	src/guid.c	\
	src/util.c
efivar_LDADD = libefivar.la -ldl
src/efivar-efivar.$(OBJEXT): src/include/efivar/efivar-guids.h
src/efivar-guid.$(OBJEXT): src/include/efivar/efivar-guids.h

.PHONY: a brick
a:
	@if test $${EUID} != 0; then	\
		echo no 1>&2 ;		\
		exit 1 ;		\
	fi

brick: all tester
	./tester

src/guid-symbols.c: makeguids$(EXEEXT)
	$(AM_V_GEN)if test ! -f "$@"; then										\
		$(MKDIR_P) src/include/efivar;										\
		./makeguids$(EXEEXT) $(srcdir)/src/guids.txt src/guid-symbols.c src/include/efivar/efivar-guids.h;	\
	fi
	@touch $@

src/include/efivar/efivar-guids.h: makeguids$(EXEEXT)
	$(AM_V_GEN)if test ! -f "$@"; then										\
		$(MKDIR_P) src/include/efivar;										\
		./makeguids$(EXEEXT) $(srcdir)/src/guids.txt src/guid-symbols.c src/include/efivar/efivar-guids.h;	\
	fi
	@touch $@

CLEANFILES +=						\
	$(builddir)/src/guids.lds			\
	$(builddir)/src/guid-symbols.c			\
	$(builddir)/src/include/efivar/efivar-guids.h

