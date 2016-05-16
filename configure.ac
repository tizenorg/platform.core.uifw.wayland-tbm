AC_INIT(wayland-tbm, [0.1.0], lsj119@samsung.com)

AC_CONFIG_SRCDIR(configure.ac)
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE(1.6 foreign subdir-objects)
AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL

AC_PATH_PROG([wayland_scanner], [wayland-scanner])
if test x$wayland_scanner = x; then
      AC_MSG_ERROR([wayland-scanner is needed to compile wayland-tbm])
fi


PKG_CHECK_MODULES([WL_TBM_CLIENT], [wayland-client])
PKG_CHECK_MODULES([WL_TBM_SERVER], [wayland-server])
PKG_CHECK_MODULES( WAYLAND_SCANNER, wayland-scanner)

PKG_CHECK_EXISTS([dlog], [have_dlog="yes"], [have_dlog="no"])
AC_MSG_CHECKING([Have dlog logger])
AC_MSG_RESULT([${have_dlog}])
if test "x${have_dlog}" = "xyes"; then
    AC_DEFINE([HAVE_DLOG], [1], [Define to 1 if you have dlog])
    
    #PKG_CHECK_MODULES([DLOG], dlog)
    PKG_CHECK_MODULES([WL_TBM_COMMON], [libtbm capi-base-common dlog])
fi

AC_CONFIG_FILES([
	Makefile
    src/Makefile
    test/Makefile
    tool/Makefile
	wayland-tbm-server.pc
	wayland-tbm-client.pc
])
AC_OUTPUT
