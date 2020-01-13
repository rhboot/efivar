PREFIX	?= /usr
EXEC_PREFIX ?= $(PREFIX)
LIBDIR	?= $(PREFIX)/lib64
DATADIR	?= $(PREFIX)/share
MANDIR	?= $(DATADIR)/man
INCLUDEDIR ?= $(PREFIX)/include
BINDIR	?= $(EXEC_PREFIX)/bin
PCDIR	?= $(LIBDIR)/pkgconfig
DESTDIR	?=

CROSS_COMPILE	?=
COMPILER ?= gcc
$(call set-if-undefined,CC,$(CROSS_COMPILE)$(COMPILER))
$(call set-if-undefined,CCLD,$(CC))
$(call set-if-undefined,HOSTCC,$(COMPILER))
$(call set-if-undefined,HOSTCCLD,$(HOSTCC))

OPTIMIZE ?= -O2 -flto
DEBUGINFO ?= -g3
WARNINGS_GCC ?= -Wmaybe-uninitialized \
		-Wno-nonnull-compare
WARNINGS_CCC_ANALYZER ?= $(WARNINGS_GCC)
WARNINGS ?= -Wall -Wextra \
	    -Wno-address-of-packed-member \
	    -Wno-missing-field-initializers \
	    $(call family,WARNINGS)
ERRORS ?= -Werror -Wno-error=cpp $(call family,ERRORS)
CPPFLAGS ?=
override _CPPFLAGS := $(CPPFLAGS)
override CPPFLAGS = $(_CPPFLAGS) -DLIBEFIVAR_VERSION=$(VERSION) \
	    -D_GNU_SOURCE \
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
LDFLAGS_CLANG ?= -Wl,--fatal-warnings,-pie,-z,relro
LDFLAGS ?=
override _LDFLAGS := $(LDFLAGS)
override LDFLAGS = $(_LDFLAGS) \
		   -Wl,--add-needed \
		   -Wl,--build-id \
		   -Wl,--no-allow-shlib-undefined \
		   -Wl,--no-undefined-version \
		   -Wl,-z,now \
		   -Wl,-z,muldefs \
		   $(call family,LDFLAGS)
CCLDFLAGS ?=
override _CCLDFLAGS := $(CCLDFLAGS)
override CCLDFLAGS = $(CFLAGS) -L. $(_CCLDFLAGS) \
		     $(LDFLAGS) \
		     $(call pkg-config-ccldflags)
HOST_ARCH=$(shell uname -m)
ifneq ($(HOST_ARCH),ia64)
	HOST_MARCH=-march=native
else
	HOST_MARCH=
endif
HOST_CPPFLAGS ?= $(CPPFLAGS)
override _HOST_CPPFLAGS := $(HOST_CPPFLAGS)
override HOST_CPPFLAGS = $(_HOST_CPPFLAGS) \
			 -DEFIVAR_BUILD_ENVIRONMENT $(HOST_MARCH)
HOST_CFLAGS ?= $(CFLAGS)
override _HOST_CFLAGS := $(HOST_CFLAGS)
override HOST_CFLAGS = $(_HOST_CFLAGS)

PKG_CONFIG = $(shell if [ -e "$$(env $(CROSS_COMPILE)pkg-config 2>&1)" ]; then echo $(CROSS_COMPILE)pkg-config ; else echo pkg-config ; fi)
INSTALL ?= install
AR	:= $(CROSS_COMPILE)$(COMPILER)-ar
NM	:= $(CROSS_COMPILE)$(COMPILER)-nm
RANLIB	:= $(CROSS_COMPILE)$(COMPILER)-ranlib
ABIDW	:= abidw
ABIDIFF := abidiff

PKGS	=

SOFLAGS=-shared $(call family,SOFLAGS)
LDLIBS=$(foreach lib,$(LIBS),-l$(lib)) $(call pkg-config-ldlibs)

COMMIT_ID=$(shell git log -1 --pretty=%H 2>/dev/null || echo master)

NAME=efivar

# vim:ft=make
