dnl Process this file with autoconf to produce a configure script.
AC_INIT(src/nost_data.h)
AM_INIT_AUTOMAKE(nostromo_n50, 1.4)

dnl Checks for programs.
AC_PROG_AWK
AC_PROG_CXX
AC_PROG_CC

AC_ARG_ENABLE(
	debug,
	[  --enable-debug      enable debugging messages and logging.],
	[
		AC_MSG_RESULT(debugging ENABLED)
		DEBUG_FLAGS="-g"
	],
       [
		AC_MSG_RESULT(debugging disabled)
        ]
)
AC_SUBST(DEBUG_FLAGS)

dnl Check for FLTK...
AC_PATH_PROG(FLTKCONFIG,fltk-config)

if test "x$FLTKCONFIG" = x; then
	AC_MSG_ERROR([Sorry, the nostromo X11 tools require FLTK 1.1.x.])
else
	FLTK_CXXFLAGS="`$FLTKCONFIG --cflags`"
	FLTK_LIBS="`$FLTKCONFIG --ldflags`"
fi

AC_SUBST(FLTK_CXXFLAGS)
AC_SUBST(FLTK_LIBS)

dnl Checks for header files.
AC_PATH_X
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h limits.h sys/ioctl.h unistd.h)

dnl Checks for libraries.
PKG_CHECK_MODULES(NOSTROMO, gtk+-2.0 libxml-2.0)
AC_SUBST(NOSTROMO_CFLAGS)
AC_SUBST(NOSTROMO_LIBS)

AC_CHECK_LIB(Xtst, XTestFakeKeyEvent)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

dnl Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_TYPE_SIGNAL

AC_OUTPUT([Makefile src/Makefile doc/Makefile])

