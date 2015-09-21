AC_PREREQ(2.63)

AC_INIT([gnome-power-manager],[3.19.1])
AC_CONFIG_SRCDIR(src)
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AM_INIT_AUTOMAKE([1.11 no-dist-gzip dist-xz tar-ustar foreign])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# enable nice build output on automake1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_PROG_CC
AM_PROG_CC_C_O

AC_PROG_INSTALL
LT_INIT
IT_PROG_INTLTOOL([0.50.0])

GNOME_COMPILE_WARNINGS

AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)

if test "$GCC" = "yes"; then
	WARNINGFLAGS="$WARNINGFLAGS -Wall"
	WARNINGFLAGS="$WARNINGFLAGS -Wcast-align -Wno-uninitialized"
	WARNINGFLAGS="$WARNINGFLAGS -Wmissing-declarations"
	WARNINGFLAGS="$WARNINGFLAGS -Wpointer-arith"
	WARNINGFLAGS="$WARNINGFLAGS -Wcast-align"
	WARNINGFLAGS="$WARNINGFLAGS -Wwrite-strings"
	WARNINGFLAGS="$WARNINGFLAGS -Winit-self"
	WARNINGFLAGS="$WARNINGFLAGS -Wreturn-type"
	WARNINGFLAGS="$WARNINGFLAGS -Wformat-nonliteral"
	WARNINGFLAGS="$WARNINGFLAGS -Wformat-security"
	WARNINGFLAGS="$WARNINGFLAGS -Wmissing-include-dirs"
	WARNINGFLAGS="$WARNINGFLAGS -Wmissing-format-attribute"
	WARNINGFLAGS="$WARNINGFLAGS -Wclobbered"
	WARNINGFLAGS="$WARNINGFLAGS -Wempty-body"
	WARNINGFLAGS="$WARNINGFLAGS -Wignored-qualifiers"
	WARNINGFLAGS="$WARNINGFLAGS -Wtype-limits"
	WARNINGFLAGS="$WARNINGFLAGS -Wdeclaration-after-statement"
	WARNINGFLAGS="$WARNINGFLAGS -Wshadow"
	WARNINGFLAGS="$WARNINGFLAGS -Winline"
	WARNINGFLAGS="$WARNINGFLAGS -Wmissing-parameter-type"
	WARNINGFLAGS="$WARNINGFLAGS -Woverride-init"
	WARNINGFLAGS="$WARNINGFLAGS -Wno-strict-aliasing"
else
	WARNINGFLAGS=""
fi
AC_SUBST(WARNINGFLAGS)

dnl ---------------------------------------------------------------------------
dnl - Debugging switches (uncomment this if you want to use gdb)
dnl ---------------------------------------------------------------------------
CPPFLAGS="$CPPFLAGS -g"
CPPFLAGS="$CPPFLAGS -fexceptions"

dnl ---------------------------------------------------------------------------
dnl - gettext stuff
dnl ---------------------------------------------------------------------------
GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE(GETTEXT_PACKAGE, "AC_PACKAGE_NAME", [foo])

AM_GLIB_GNU_GETTEXT
GLIB_GSETTINGS

dnl ---------------------------------------------------------------------------
dnl - Check library dependencies
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.45.8 gobject-2.0 gio-2.0 >= 2.25.9)

PKG_CHECK_MODULES(GNOME, [
 gtk+-3.0 >= 3.3.8
 gdk-3.0 >= 2.91.7
 cairo >= 1.0.0])

PKG_CHECK_MODULES(UPOWER, upower-glib >= 0.9.1)
AC_PATH_PROG(APPSTREAM_UTIL, [appstream-util], [unfound])

dnl ---------------------------------------------------------------------------
dnl - Build self tests
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(tests,
              AS_HELP_STRING([--enable-tests],
                             [Build self tests]),
              enable_tests=$enableval,enable_tests=no)

AC_MSG_CHECKING([whether to support tests])
have_tests=no
if test x$enable_tests = xyes ; then
    have_tests=yes
    AC_DEFINE(HAVE_TESTS, 1, [Define if we want to use the self tests])
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi
AM_CONDITIONAL([HAVE_TESTS], [test $have_tests = yes])

dnl ---------------------------------------------------------------------------
dnl - Is docbook2man available?
dnl ---------------------------------------------------------------------------
AC_PATH_PROG(DOCBOOK2MAN, docbook2man, no)
if test "$DOCBOOK2MAN" = "no" ; then
	AC_MSG_WARN([docbook2man not found, will not be able to build man documentation])
fi
AM_CONDITIONAL(HAVE_DOCBOOK2MAN, [test "$DOCBOOK2MAN" != "no"])

dnl ---------------------------------------------------------------------------
dnl - Makefiles, etc.
dnl ---------------------------------------------------------------------------
AC_CONFIG_FILES([
Makefile
src/Makefile
man/Makefile
po/Makefile.in
data/Makefile
data/gnome-power-statistics.desktop.in
data/appdata/Makefile
data/icons/Makefile
data/icons/hicolor/Makefile
])
AC_OUTPUT

dnl ==========================================================================
echo "
                    GNOME Power Manager $VERSION
                  =============================

        prefix:                    ${prefix}
        datadir:                   ${datadir}
        compiler:                  ${CC}
        cflags:                    ${CFLAGS}
        cppflags:                  ${CPPFLAGS}
        Self test support:         ${have_tests}
"
