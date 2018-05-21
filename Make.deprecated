#
# This is all stuff pjones should not have done the way he did initially, and
# it's deprecated and will eventually go away.
#

ifneq ($(origin prefix),undefined)
  ifeq ($(origin PREFIX),undefined)
    override PREFIX = $(prefix)
  endif
endif
ifneq ($(origin exec_prefix),undefined)
  ifeq ($(origin EXEC_PREFIX),undefined)
    override EXEC_PREFIX = $(exec_prefix)
  endif
endif
ifneq ($(origin libdir),undefined)
  ifeq ($(origin LIBDIR),undefined)
    override LIBDIR = $(libdir)
  endif
endif
ifneq ($(origin datadir),undefined)
  ifeq ($(origin DATADIR),undefined)
    override DATADIR = $(datadir)
  endif
endif
ifneq ($(origin mandir),undefined)
  ifeq ($(origin MANDIR),undefined)
    override MANDIR = $(mandir)
  endif
endif
ifneq ($(origin includedir),undefined)
  ifeq ($(origin INCLUDEDIR),undefined)
    override INCLUDEDIR = $(includedir)
  endif
endif
ifneq ($(origin bindir),undefined)
  ifeq ($(origin BINDIR),undefined)
    override BINDIR = $(bindir)
  endif
endif

# vim:ft=make
