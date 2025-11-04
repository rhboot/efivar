PREFIX	?= /usr
EXEC_PREFIX ?= $(PREFIX)
LIBDIR	?= $(PREFIX)/lib64
DATADIR	?= $(PREFIX)/share
MANDIR	?= $(DATADIR)/man
INCLUDEDIR ?= $(PREFIX)/include
BINDIR	?= $(EXEC_PREFIX)/bin
PCDIR	?= $(LIBDIR)/pkgconfig
DESTDIR	?=
PKGS	?=

CROSS_COMPILE	?=
COMPILER 	?= gcc
ifeq ($(origin CC),command line)
override COMPILER := $(CC)
override CC := $(CROSS_COMPILE)$(COMPILER)
endif
$(call set-if-undefined,CC,$(CROSS_COMPILE)$(COMPILER))
$(call set-if-undefined,CCLD,$(CC))
$(call set-if-undefined,HOSTCC,$(COMPILER))
$(call set-if-undefined,HOSTCCLD,$(HOSTCC))

# temporary, see https://sourceware.org/bugzilla/show_bug.cgi?id=28264
#OPTIMIZE_GCC = -flto
OPTIMIZE_GCC =
OPTIMIZE ?= -Og $(call family,OPTIMIZE)
DEBUGINFO ?= -g3
WARNINGS_GCC ?=
WARNINGS_CCC_ANALYZER ?= $(WARNINGS_GCC)
WARNINGS ?= -Wall -Wextra $(call family,WARNINGS)
ERRORS_GCC ?=
ERRORS ?= -Werror $(call family,ERRORS)
CPPFLAGS ?=
override _CPPFLAGS := $(CPPFLAGS)
override CPPFLAGS = $(_CPPFLAGS) -DLIBEFIVAR_VERSION=$(VERSION) \
	    -D_GNU_SOURCE \
	    -D_FILE_OFFSET_BITS=64 \
	    -I$(TOPDIR)/src/include/
CFLAGS ?= $(OPTIMIZE) $(DEBUGINFO) $(WARNINGS) $(ERRORS)
CFLAGS_GCC ?= -specs=$(TOPDIR)/src/include/gcc.specs \
	      -fno-merge-constants
override _CFLAGS := $(CFLAGS)
override CFLAGS = $(_CFLAGS) \
		  -std=gnu11 \
		  -funsigned-char \
		  -fvisibility=hidden \
		  $(call family,CFLAGS) \
		  $(call pkg-config-cflags)
LDFLAGS_CLANG ?= -rtlib=compiler-rt
CCLDFLAGS ?=
LDFLAGS ?=
override _CCLDFLAGS := $(CCLDFLAGS)
override _LDFLAGS := $(LDFLAGS)
override LDFLAGS = $(CFLAGS) -L. $(_LDFLAGS) $(_CCLDFLAGS) \
		   -Wl,--build-id \
		   -Wl,--no-allow-shlib-undefined \
		   -Wl,--no-undefined-version \
		   -Wl,-z,now \
		   -Wl,-z,muldefs \
		   -Wl,-z,relro \
		   -Wl,--fatal-warnings \
		   $(call family,LDFLAGS) $(call family,CCLDFLAGS) \
		   $(call pkg-config-ccldflags)
override CCLDFLAGS = $(LDFLAGS)
SOFLAGS_GCC =
SOFLAGS_CLANG =
SOFLAGS ?=
override _SOFLAGS := $(SOFLAGS)
override SOFLAGS = $(_SOFLAGS) \
		   -shared -Wl,-soname,$@.1 \
		   -Wl,--version-script=$(MAP) \
		   $(call family,SOFLAGS)

HOST_ARCH=$(shell uname -m)
ifneq ($(HOST_ARCH),ia64)
ifneq ($(HOST_ARCH),riscv64)
ifneq ($(HOST_ARCH),ppc64le)
	HOST_MARCH=-march=native
else
	HOST_MARCH=
endif
else
	HOST_MARCH=
endif
else
	HOST_MARCH=
endif
HOST_CPPFLAGS ?= $(CPPFLAGS)
override _HOST_CPPFLAGS := $(HOST_CPPFLAGS)
override HOST_CPPFLAGS = $(_HOST_CPPFLAGS) \
			 -DEFIVAR_BUILD_ENVIRONMENT $(HOST_MARCH)
HOST_CFLAGS_GCC ?=
HOST_CFLAGS_CLANG ?=
HOST_CFLAGS ?= $(CFLAGS) $(call family,HOST_CFLAGS)
override _HOST_CFLAGS := $(HOST_CFLAGS)
override HOST_CFLAGS = $(_HOST_CFLAGS)
HOST_LDFLAGS_CLANG ?= -Wl,--fatal-warnings,-z,relro -rtlib=compiler-rt
HOST_LDFLAGS_GCC ?= -Wl,--no-undefined-version
HOST_LDFLAGS ?=
HOST_CCLDFLAGS ?=
override _HOST_LDFLAGS := $(HOST_LDFLAGS)
override _HOST_CCLDFLAGS := $(HOST_CCLDFLAGS)
override HOST_LDFLAGS = $(HOST_CFLAGS) -L. \
			$(_HOST_LDFLAGS) $(_HOST_CCLDFLAGS) \
			-Wl,--build-id \
			-Wl,--no-allow-shlib-undefined \
			-Wl,-z,now \
			-Wl,-z,muldefs \
			$(call family,HOST_LDFLAGS) \
			$(call family,HOST_CCLDFLAGS) \
			$(call pkg-config-ccldflags)
override HOST_CCLDFLAGS = $(HOST_LDFLAGS)

PKG_CONFIG ?= $(shell if [ -e "$$(env $(CROSS_COMPILE)pkg-config 2>&1)" ]; then echo $(CROSS_COMPILE)pkg-config ; else echo pkg-config ; fi)
INSTALL ?= install
AR	:= $(CROSS_COMPILE)$(COMPILER)-ar
NM	:= $(CROSS_COMPILE)$(COMPILER)-nm
RANLIB	:= $(CROSS_COMPILE)$(COMPILER)-ranlib
ABIDW	:= abidw
ABIDIFF := abidiff
MANDOC	:= mandoc

LDLIBS=$(foreach lib,$(LIBS),-l$(lib)) $(call pkg-config-ldlibs)

COMMIT_ID=$(shell git log -1 --pretty=%H 2>/dev/null || echo master)

NAME=efivar

# Docs are enabled by default. Set ENABLE_DOCS=0 to disable
# building/installing docs.
ENABLE_DOCS ?= 1

# vim:ft=make
