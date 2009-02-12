AC_PREREQ(2.60)

AC_INIT(gnome-power-manager, 2.24.4)
AC_CONFIG_SRCDIR(src)
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AM_CONFIG_HEADER(config.h)
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CC
AM_PROG_CC_C_O

AC_PROG_INSTALL
AM_PROG_LIBTOOL
IT_PROG_INTLTOOL([0.35.0])

GNOME_COMPILE_WARNINGS
GNOME_DOC_INIT

AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)

dnl ---------------------------------------------------------------------------
dnl - Extra verbose warning switches
dnl ---------------------------------------------------------------------------
if test "$GCC" = "yes"; then
    CPPFLAGS="$CPPFLAGS -Werror -Wcast-align -Wno-uninitialized"
    CPPFLAGS="$CPPFLAGS -Wall -Wformat-security -Wno-strict-aliasing"
fi

dnl ---------------------------------------------------------------------------
dnl - Debugging switches (uncomment this if you want to use gdb)
dnl ---------------------------------------------------------------------------
CPPFLAGS="$CPPFLAGS -g"
CPPFLAGS="$CPPFLAGS -fexceptions"

dnl ---------------------------------------------------------------------------
dnl - GProf switches (do not use for normal building)
dnl ---------------------------------------------------------------------------
#CPPFLAGS="$CPPFLAGS -c -pg"
#LDFLAGS="$LDFLAGS -pg"

dnl ---------------------------------------------------------------------------
dnl - gettext stuff
dnl ---------------------------------------------------------------------------
GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE(GETTEXT_PACKAGE, "AC_PACKAGE_NAME", [foo])

AM_GLIB_GNU_GETTEXT

dnl ---------------------------------------------------------------------------
dnl - Library dependencies
dnl ---------------------------------------------------------------------------
GLIB_REQUIRED=2.13.0
GTK_REQUIRED=2.11.0
GDK_REQUIRED=2.11.0
GNOME_KEYRING_REQUIRED=0.6.0
LIBGNOME_REQUIRED=2.14.0
LIBGNOMEUI_REQUIRED=2.14.0
HAL_REQUIRED=0.5.8
DBUS_REQUIRED=0.70
LIBGLADE_REQUIRED=2.5.0
LIBNOTIFY_REQUIRED=0.4.3
LIBWNCK_REQUIRED=2.10.0
CAIRO_REQUIRED=1.0.0
UNIQUE_REQUIRED=0.9.4
LIBPANEL_REQUIRED=2.0.0
GSTREAMER_REQUIRED=0.10
XRANDR_REQUIRED=1.2.0
POLKIT_GNOME_REQUIRED=0.8

dnl ---------------------------------------------------------------------------
dnl - Make above strings available for packaging files (e.g. rpm spec files)
dnl ---------------------------------------------------------------------------
AC_SUBST(GLIB_REQUIRED)
AC_SUBST(GTK_REQUIRED)
AC_SUBST(GDK_REQUIRED)
AC_SUBST(LIBGNOME_REQUIRED)
AC_SUBST(LIBGNOMEUI_REQUIRED)
AC_SUBST(HAL_REQUIRED)
AC_SUBST(DBUS_REQUIRED)
AC_SUBST(LIBGLADE_REQUIRED)
AC_SUBST(LIBNOTIFY_REQUIRED)
AC_SUBST(LIBWNCK_REQUIRED)
AC_SUBST(CAIRO_REQUIRED)
AC_SUBST(UNIQUE_REQUIRED)
AC_SUBST(LIBPANEL_REQUIRED)
AC_SUBST(GSTREAMER_REQUIRED)
AC_SUBST(XRANDR_REQUIRED)
AC_SUBST(POLKIT_GNOME_REQUIRED)

dnl ---------------------------------------------------------------------------
dnl - Check library dependencies
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES(GLIB, glib-2.0 >= $GLIB_REQUIRED gobject-2.0)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(HAL, hal >= $HAL_REQUIRED)
AC_SUBST(HAL_CFLAGS)
AC_SUBST(HAL_LIBS)

PKG_CHECK_MODULES(GSTREAMER, gstreamer-0.10 >= $GSTREAMER_REQUIRED)
AC_SUBST(GSTREAMER_CFLAGS)
AC_SUBST(GSTREAMER_LIBS)

PKG_CHECK_MODULES(DBUS,[
 dbus-glib-1 >= $DBUS_REQUIRED
 dbus-1 >= $DBUS_REQUIRED
 gthread-2.0])
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

PKG_CHECK_MODULES(GNOME, [
 libgnomeui-2.0 >= $LIBGNOMEUI_REQUIRED
 libglade-2.0 >= $LIBGLADE_REQUIRED
 libwnck-1.0 >= $LIBWNCK_REQUIRED
 gtk+-2.0 >= $GTK_REQUIRED
 gnome-keyring-1 >= $GNOME_KEYRING_REQUIRED
 libpanelapplet-2.0 >= $LIBPANEL_REQUIRED
 cairo >= $CAIRO_REQUIRED])
AC_SUBST(GNOME_CFLAGS)
AC_SUBST(GNOME_LIBS)

PKG_CHECK_MODULES(GDK, [
 gdk-2.0 >= $GDK_REQUIRED
 gdk-x11-2.0 >= $GDK_REQUIRED])
AC_SUBST(GDK_CFLAGS)
AC_SUBST(GDK_LIBS)

PKG_CHECK_MODULES(XRANDR, [
 xrandr >= $XRANDR_REQUIRED 
 xrender x11])
AC_SUBST(XRANDR_CFLAGS)
AC_SUBST(XRANDR_LIBS)

AC_PATH_PROG(GCONFTOOL, gconftool-2)
AM_GCONF_SOURCE_2

AC_PATH_XTRA

ALL_X_LIBS="$X_LIBS $X_PRE_LIBS -lX11 $X_EXTRA_LIBS"

dnl ---------------------------------------------------------------------------
dnl - Make paths available for source files
dnl ---------------------------------------------------------------------------
AC_SUBST(SYSCONFDIR, $sysconfdir)
AC_SUBST(DATADIR, $datadir)
AC_SUBST(BINDIR, $bindir)
AC_SUBST(SBINDIR, $sbindir)

dnl ---------------------------------------------------------------------------
dnl - Where should we put documentation ?
dnl ---------------------------------------------------------------------------
AC_ARG_WITH(doc-dir,
            AS_HELP_STRING([--with-doc-dir=<dir>],
                           [directory to install documentation]))

if ! test -z "$with_doc_dir"; then
   DOCDIR="$with_doc_dir/gnome-power-manager-$VERSION"
else
   DOCDIR="$DATADIR/doc/gnome-power-manager-$VERSION"
fi
AC_SUBST(DOCDIR)

dnl ---------------------------------------------------------------------------
dnl - PolicyKit support?
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(policykit,
              AS_HELP_STRING([--enable-policykit],
                             [support PolicyKit permissions daemon (requires polkitd)]),
              enable_policykit=$enableval,enable_policykit=no)

AC_MSG_CHECKING([whether to support PolicyKit])
have_polkit=no
if test x$enable_policykit = xyes ; then
    PKG_CHECK_MODULES(POLKIT, \
                      polkit-grant >= $POLKIT_GNOME_REQUIRED\
                      polkit-dbus >= $POLKIT_GNOME_REQUIRED)
    AC_SUBST(POLKIT_CFLAGS)
    AC_SUBST(POLKIT_LIBS)
    have_polkit=yes
    AC_DEFINE(HAVE_POLKIT, 1, [Define if we use PolicyKit])
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi

dnl ---------------------------------------------------------------------------
dnl - Use check-foreground-console (Ubuntu specific) support?
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(checkfg,
              AS_HELP_STRING([--enable-checkfg],
                             [Use check-foreground-console (Ubuntu systems)]),
              enable_checkfg=$enableval,enable_checkfg=no)

AC_MSG_CHECKING([whether to use check-foreground-console])
have_checkfg=no
if test x$enable_checkfg = xyes ; then
    have_checkfg=yes
    AC_DEFINE(HAVE_CHECK_FG, 1, [Define if we want to use check-foreground-console])
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi

dnl ---------------------------------------------------------------------------
dnl - Respond to XEvents
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(xevents,
              AS_HELP_STRING([--disable-xevents],
                             [Support XEVENTS (requires a new x.org)]),
              enable_xevents=$enableval,enable_xevents=yes)

AC_MSG_CHECKING([whether to support xevents])
have_xevents=no
if test x$enable_xevents = xyes ; then
    have_xevents=yes
    AC_DEFINE(HAVE_XEVENTS, 1, [Define if we use XEVENTS])
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi

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
dnl - Build gconf default button?
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(gconf-defaults,
              AS_HELP_STRING([--disable-gconf-defaults],
                             [Allow setting gconf default settings]),
              enable_gconfdefaults=$enableval,enable_gconfdefaults=yes)

AC_MSG_CHECKING([whether to support gconf defaults])
have_gconfdefaults=no
if test x$enable_gconfdefaults = xyes && test $have_polkit = yes ; then
    PKG_CHECK_MODULES(POLKIT_GNOME, [
                      polkit-gnome >= $POLKIT_GNOME_REQUIRED])
    AC_SUBST(POLKIT_GNOME_CFLAGS)
    AC_SUBST(POLKIT_GNOME_LIBS)
    have_gconfdefaults=yes
    AC_DEFINE(HAVE_GCONF_DEFAULTS, 1, [Define if have policykit-gnome])
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi
AM_CONDITIONAL([HAVE_GCONF_DEFAULTS], [test $have_gconfdefaults = yes])

dnl ---------------------------------------------------------------------------
dnl - Build applets
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(applets,
              AS_HELP_STRING([--disable-applets],
                             [Build the extra power applets]),
              enable_applets=$enableval,enable_applets=yes)

AC_MSG_CHECKING([whether to build applet support])
have_applets=no
if test x$enable_applets = xyes ; then
    have_applets=yes
    AC_DEFINE(HAVE_APPLETS, 1, [Define if we are building applets])
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi
AM_CONDITIONAL([HAVE_APPLETS], [test $have_applets = yes])

dnl ---------------------------------------------------------------------------
dnl - DocBook Documentation
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(docbook-docs,
              AS_HELP_STRING([--enable-docbook-docs],
                             [build documentation (requires xmlto)]),
              enable_docbook_docs=$enableval,enable_docbook_docs=auto)

AC_PATH_PROG(XMLTO, xmlto, no)
AC_MSG_CHECKING([whether to build DocBook documentation])
if test x$XMLTO = xno ; then
    have_docbook=no
else
    have_docbook=yes
fi
if test x$enable_docbook_docs = xauto ; then
    if test x$have_docbook = xno ; then
        enable_docbook_docs=no
    else
        enable_docbook_docs=yes
    fi
fi
if test x$enable_docbook_docs = xyes; then
    if test x$have_docbook = xno; then
	AC_MSG_ERROR([Building DocBook docs explicitly required, but DocBook not found])
    fi
fi
AM_CONDITIONAL(DOCBOOK_DOCS_ENABLED, test x$enable_docbook_docs = xyes)
AC_MSG_RESULT(yes)

dnl ---------------------------------------------------------------------------
dnl - Are we specifying a different dbus root ?
dnl ---------------------------------------------------------------------------
AC_ARG_WITH(dbus-services,
            AS_HELP_STRING([--with-dbus-services=<dir>],
                           [where D-BUS services directory is]))

if ! test -z "$with_dbus_services" ; then
	DBUS_SERVICES_DIR="$with_dbus_services"
else
	# D-BUS 0.23 and higher use $prefix/share/dbus-1/services
	DBUS_SERVICES_DIR="$DATADIR/dbus-1/services"
fi
AC_SUBST(DBUS_SERVICES_DIR)

dnl ---------------------------------------------------------------------------
dnl - Is a new enough libnotify available?
dnl ---------------------------------------------------------------------------
if $PKG_CONFIG --atleast-version $LIBNOTIFY_REQUIRED libnotify; then
   have_libnotify=yes
   PKG_CHECK_MODULES(LIBNOTIFY, libnotify >= $LIBNOTIFY_REQUIRED)
   AC_SUBST(LIBNOTIFY_CFLAGS)
   AC_SUBST(LIBNOTIFY_LIBS)
   AC_DEFINE(HAVE_LIBNOTIFY, 1, [Building with libnotify support]) 
else
   have_libnotify=no
fi
AM_CONDITIONAL([HAVE_LIBNOTIFY], [test $have_libnotify = yes])

dnl ---------------------------------------------------------------------------
dnl - Is unique available?
dnl ---------------------------------------------------------------------------
if $PKG_CONFIG --atleast-version $UNIQUE_REQUIRED unique-1.0; then
   have_unique=yes
   PKG_CHECK_MODULES(UNIQUE, unique-1.0 >= $UNIQUE_REQUIRED)
   AC_SUBST(UNIQUE_CFLAGS)
   AC_SUBST(UNIQUE_LIBS)
   AC_DEFINE(HAVE_UNIQUE, 1, [Building with unique support]) 
else
   have_unique=no
   AC_DEFINE(HAVE_UNIQUE, 0, [Not building with unique support]) 
fi
AM_CONDITIONAL([HAVE_UNIQUE], [test $have_unique = yes])

dnl ---------------------------------------------------------------------------
dnl - Some utility functions to make checking for X things easier.
dnl ---------------------------------------------------------------------------

# Like AC_CHECK_HEADER, but it uses the already-computed -I directories.
#
AC_DEFUN(AC_CHECK_X_HEADER, [
  ac_save_CPPFLAGS="$CPPFLAGS"
  if test \! -z "$includedir" ; then 
    CPPFLAGS="$CPPFLAGS -I$includedir"
  fi
  CPPFLAGS="$CPPFLAGS $X_CFLAGS"
  AC_CHECK_HEADER([$1],[$2],[$3],[$4])
  CPPFLAGS="$ac_save_CPPFLAGS"])

# Like AC_TRY_COMPILE, but it uses the already-computed -I directories.
#
AC_DEFUN(AC_TRY_X_COMPILE, [
  ac_save_CPPFLAGS="$CPPFLAGS"
  if test \! -z "$includedir" ; then 
    CPPFLAGS="$CPPFLAGS -I$includedir"
  fi
  CPPFLAGS="$CPPFLAGS $X_CFLAGS"
  AC_TRY_COMPILE([$1], [$2], [$3], [$4])
  CPPFLAGS="$ac_save_CPPFLAGS"])


# Like AC_CHECK_LIB, but it uses the already-computed -I and -L directories.
# Use this sparingly; it probably doesn't work very well on X programs.
#
AC_DEFUN(AC_CHECK_X_LIB, [
  ac_save_CPPFLAGS="$CPPFLAGS"
  ac_save_LDFLAGS="$LDFLAGS"
#  ac_save_LIBS="$LIBS"

  if test \! -z "$includedir" ; then 
    CPPFLAGS="$CPPFLAGS -I$includedir"
  fi
  # note: $X_CFLAGS includes $x_includes
  CPPFLAGS="$CPPFLAGS $X_CFLAGS"

  if test \! -z "$libdir" ; then
    LDFLAGS="$LDFLAGS -L$libdir"
  fi
  # note: $X_LIBS includes $x_libraries
  LDFLAGS="$LDFLAGS $ALL_X_LIBS"

  AC_CHECK_LIB([$1], [$2], [$3], [$4], [$5])
  CPPFLAGS="$ac_save_CPPFLAGS"
  LDFLAGS="$ac_save_LDFLAGS"
#  LIBS="$ac_save_LIBS"
  ])


# Usage: HANDLE_X_PATH_ARG([variable_name],
#                          [--command-line-option],
#                          [descriptive string])
#
# All of the --with options take three forms:
#
#   --with-foo (or --with-foo=yes)
#   --without-foo (or --with-foo=no)
#   --with-foo=/DIR
#
# This function, HANDLE_X_PATH_ARG, deals with the /DIR case.  When it sees
# a directory (string beginning with a slash) it checks to see whether
# /DIR/include and /DIR/lib exist, and adds them to $X_CFLAGS and $X_LIBS
# as appropriate.
#
AC_DEFUN(HANDLE_X_PATH_ARG, [
   case "$[$1]" in
    yes) ;;
    no)  ;;

    /*)
     AC_MSG_CHECKING([for [$3] headers])
     d=$[$1]/include
     if test -d $d; then
       X_CFLAGS="-I$d $X_CFLAGS"
       AC_MSG_RESULT($d)
     else
       AC_MSG_RESULT(not found ($d: no such directory))
     fi

     AC_MSG_CHECKING([for [$3] libs])
     d=$[$1]/lib
     if test -d $d; then
       X_LIBS="-L$d $X_LIBS"
       AC_MSG_RESULT($d)
     else
       AC_MSG_RESULT(not found ($d: no such directory))
     fi

     # replace the directory string with "yes".
     [$1]_req="yes"
     [$1]=$[$1]_req
     ;;

    *)
     echo ""
     echo "error: argument to [$2] must be \"yes\", \"no\", or a directory."
     echo "       If it is a directory, then \`DIR/include' will be added to"
     echo "       the -I list, and \`DIR/lib' will be added to the -L list."
     exit 1
     ;;
   esac
  ])


dnl ---------------------------------------------------------------------------
dnl - Check for the DPMS server extension.
dnl ---------------------------------------------------------------------------

have_dpms=no
with_dpms_req=unspecified
GPM_EXTRA_LIBS="-lresolv"

AC_ARG_WITH(dpms-ext,
            AS_HELP_STRING([--without-dpms-ext],
                           [Include support for the DPMS extension.]),
            [with_dpms="$withval"; with_dpms_req="$withval"],[with_dpms=yes])

HANDLE_X_PATH_ARG(with_dpms, --with-dpms-ext, DPMS)

if test "$with_dpms" = yes; then

  # first check for dpms.h
  AC_CHECK_X_HEADER(X11/extensions/dpms.h, [have_dpms=yes],,
                    [#include <X11/Xlib.h>
		     #include <X11/Xmd.h>])

  # if that succeeded, then check for the DPMS code in the libraries
  if test "$have_dpms" = yes; then

    # first look in -lXext (this is where it is with XFree86 4.0)
    have_dpms=no
    AC_CHECK_X_LIB(Xext, DPMSInfo, [have_dpms=yes], [true], -lXext -lX11)

    # if that failed, look in -lXdpms (this is where it was in XFree86 3.x)
    if test "$have_dpms" = no; then
      AC_CHECK_X_LIB(Xdpms, DPMSInfo,
                    [have_dpms=yes; XDPMS_LIBS="-lXdpms"], [true],
                    -lXext -lX11)
    fi
  fi


  # if that succeeded, then we've really got it.
  if test "$have_dpms" = yes; then
    AC_DEFINE(HAVE_DPMS_EXTENSION, 1, [Define if the DPMS extension is available])
    GPM_EXTRA_LIBS="$GPM_EXTRA_LIBS -lXext"
  fi

elif test "$with_dpms" != no; then
  echo "error: must be yes or no: --with-dpms-ext=$with_dpms"
  exit 1
fi

dnl ---------------------------------------------------------------------------
dnl - Check for Solaris kstat support
dnl ---------------------------------------------------------------------------
AC_MSG_CHECKING(for Solaris kstat)
AC_CHECK_LIB(kstat, kstat_open,
	     [
	       AC_DEFINE(HAVE_KSTAT, 1, [Define if Solaris kstat is support])
	       GPM_EXTRA_LIBS="$GPM_EXTRA_LIBS -lkstat"
	     ])
AC_SUBST(GPM_EXTRA_LIBS)

dnl ---------------------------------------------------------------------------
dnl - Is docbook2man available?
dnl ---------------------------------------------------------------------------
AC_PATH_PROG(DOCBOOK2MAN, docbook2man, no)
if test "$DOCBOOK2MAN" = "no" ; then
	AC_MSG_WARN([docbook2man not found, will not be able to build man documentation])
fi
AM_CONDITIONAL(HAVE_DOCBOOK2MAN, [test "$DOCBOOK2MAN" != "no"])

dnl ------------------------------------------------------------------------------
dnl - Turn on the additional warnings last, so -Werror doesn't affect other tests.
dnl ------------------------------------------------------------------------------

AC_ARG_ENABLE(more-warnings,
              AS_HELP_STRING([--enable-more-warnings],
                             [Maximum compiler warnings]),
set_more_warnings="$enableval",[
if test -d $srcdir/CVS; then
        is_cvs_version=true
        set_more_warnings=yes
else
        set_more_warnings=no
fi
])
AC_MSG_CHECKING(for more warnings)
if test "$GCC" = "yes" -a "$set_more_warnings" != "no"; then
        AC_MSG_RESULT(yes)
        CFLAGS="\
        -Wall \
        -Wchar-subscripts -Wmissing-declarations -Wmissing-prototypes \
        -Wnested-externs -Wpointer-arith \
        -Wcast-align -Wsign-compare \
        $CFLAGS"

        for option in -Wno-strict-aliasing -Wno-sign-compare; do
                SAVE_CFLAGS="$CFLAGS"
                CFLAGS="$CFLAGS $option"
                AC_MSG_CHECKING([whether gcc understands $option])
                AC_TRY_COMPILE([], [],
                        has_option=yes,
                        has_option=no,)
                if test $has_option = no; then
                        CFLAGS="$SAVE_CFLAGS"
                fi
                AC_MSG_RESULT($has_option)
                unset has_option
                unset SAVE_CFLAGS
        done
        unset option
else
        AC_MSG_RESULT(no)
fi

dnl ---------------------------------------------------------------------------
dnl - Makefiles, etc.
dnl ---------------------------------------------------------------------------
AC_CONFIG_FILES([
Makefile
src/Makefile
src/dbus/Makefile
applets/Makefile
applets/brightness/Makefile
applets/inhibit/Makefile
libhal-glib/Makefile
libdbus-glib/Makefile
libidletime/Makefile
libunique/Makefile
docs/Makefile
man/Makefile
help/Makefile
po/Makefile.in
tools/Makefile
test/Makefile
data/Makefile
data/gnome-power-manager.desktop.in
data/gnome-power-preferences.desktop.in
data/gnome-power-statistics.desktop.in
data/sounds/Makefile
data/icons/Makefile
data/icons/16x16/Makefile
data/icons/16x16/actions/Makefile
data/icons/16x16/apps/Makefile
data/icons/16x16/status/Makefile
data/icons/22x22/Makefile
data/icons/22x22/actions/Makefile
data/icons/22x22/apps/Makefile
data/icons/22x22/status/Makefile
data/icons/24x24/Makefile
data/icons/24x24/actions/Makefile
data/icons/24x24/apps/Makefile
data/icons/24x24/status/Makefile
data/icons/48x48/Makefile
data/icons/48x48/actions/Makefile
data/icons/48x48/apps/Makefile
data/icons/48x48/status/Makefile
data/icons/scalable/Makefile
data/icons/scalable/actions/Makefile
data/icons/scalable/apps/Makefile
data/icons/scalable/status/Makefile
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
        libnotify support:         ${have_libnotify}
        unique support:            ${have_unique}
        Building extra applets:    ${enable_applets}
        DPMS support:              ${have_dpms}
        PolicyKit support:         ${have_polkit}
        XEVENTS support:           ${have_xevents}
        Self test support:         ${have_tests}
        GConf default support:     ${have_gconfdefaults}
        Docbook support:           ${enable_docbook_docs}
        documentation dir:         $DOCDIR
        dbus-1 system.d dir:       $DBUS_SYS_DIR
        dbus-1 services dir:       $DBUS_SERVICES_DIR
        gconf-schema dir:          $GCONF_SCHEMA_FILE_DIR
"

