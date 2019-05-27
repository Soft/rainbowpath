AC_INIT([rainbowpath], [0.4.0], [samuel.lauren@iki.fi])
AM_INIT_AUTOMAKE([-Wall foreign])
AC_CONFIG_SRCDIR([src/rainbowpath.c])
AC_LANG([C])

AC_PROG_CC
AC_PROG_CC_C99
AC_HEADER_STDBOOL
AC_TYPE_UINT8_T

AC_CHECK_FUNC([drand48],
  [AC_DEFINE([HAVE_DRAND48], [1], [Use drand48])])

AC_CHECK_HEADER([sys/random.h],
  [AC_CHECK_DECL([getrandom],
    [AC_DEFINE([HAVE_GETRANDOM], [1], [Use getrandom])],
    [],
    [[
#include <sys/random.h>
    ]])])

AC_ARG_WITH([curses],
  [AS_HELP_STRING([--with-curses],
    [use curses for controlling terminal])],
  [],
  [with_curses=auto])

AS_CASE(["$with_curses"],
  [yes], [PKG_CHECK_MODULES([CURSES],
            [ncurses],
            [HAVE_CURSES=1])],
  [no], [HAVE_CURSES=0],
  [PKG_CHECK_MODULES([CURSES],
    [ncurses],
    [HAVE_CURSES=1],
    [HAVE_CURSES=0])])

AM_CONDITIONAL([USE_CURSES], [test "$HAVE_CURSES" -eq 1])
AS_IF([test "$HAVE_CURSES" -eq 1],
      [AC_DEFINE([HAVE_CURSES],
      [1],
      [Use curses])])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
