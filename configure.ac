m4_define([libefivar_major], [2])
m4_define([libefivar_minor], [0])
m4_define([libefivar_patch], [0])
m4_define([efivar_release_ver], [39])
LIBEFIVAR_VERSION
AC_INIT([efivar], [efivar_release_ver], [https://github.com/rhboot/efivar/issues])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])
PKG_INSTALLDIR
AM_INIT_AUTOMAKE([subdir-objects foreign])
AC_PROG_CC
LT_INIT
AC_PATH_PROG([mandoc], [mandoc], [:])
AS_IF([test "$mandoc" = ":"],
    [AC_MSG_WARN([Unable to find mandoc utility. Manual pages cannot be built])],
    [AC_SUBST([mandoc], [$ac_cv_path_mandoc])])
AM_CONDITIONAL([HAVE_MANDOC],
    [test "x$ac_cv_path_mandoc" != "x:"])
AC_PATH_PROGS([grub_editenv], [grub2-editenv grub-editenv], [])
AS_IF([test "x$grub_editenv" = "x"],
    [AC_MSG_WARN([Unable to find grub-editenv utility.])],
    [AC_SUBST([grub_editenv], [$ac_cv_path_grub_editenv])])
AC_ARG_ENABLE([valgrind],
    AS_HELP_STRING([--enable-valgrind], [Enable running valgrind during tests (default no)]),
    [enable_valgrind=$enableval],
    [enable_valgrind="no"])
AS_IF([test "x$enable_valgrind" = "xyes"], [
    AC_PATH_PROG([valgrind], [valgrind], [])
    AS_IF([test "x$valgrind" = "x"],
        [AC_MSG_ERROR([Unable to find valgrind utility.])],
        [AC_SUBST([valgrind], [$ac_cv_path_valgrind])])
])
AM_CONDITIONAL([ENABLE_VALGRIND], [test "x$enable_valgrind" = "xyes"])
AC_DEFINE([LIBEFIVAR_VERSION], [libefivar_minor], [EFIVAR library version])
AC_SUBST(MANDOC, [$ac_cv_path_mandoc])
AC_SUBST(LIBEFIVAR_MAJOR, [libefivar_major])
AC_SUBST(LIBEFIVAR_MINOR, [libefivar_minor])
AC_SUBST(LIBEFIVAR_PATCH, [libefivar_patch])
AC_CONFIG_FILES([Makefile efivar.spec src/efiboot.pc src/efisec.pc src/efivar.pc])
AC_OUTPUT
