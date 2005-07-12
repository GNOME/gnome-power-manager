#!/bin/sh
# Run this to generate all the initial makefiles, etc. 
AUTOCONF="autoconf"
AUTOHEADER="autoheader"
AUTOMAKE="automake"
ACLOCAL="aclocal"
LIBTOOLIZE="libtoolize"

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

echo "Running $ACLOCAL..."
$ACLOCAL $ACLOCAL_INCLUDES $ACLOCAL_FLAGS || exit 1

echo "Running $AUTOHEADER..."
$AUTOHEADER || exit 1

echo "Running $AUTOCONF..."
$AUTOCONF || exit 1

echo "Running $LIBTOOLIZE --automake..."
$LIBTOOLIZE --automake || exit 1

echo "Running $AUTOMAKE..."
$AUTOMAKE -a || exit 1
$AUTOMAKE -a src/Makefile || exit 1

conf_flags=""

if test x$NOCONFIGURE = x; then
  echo Running $srcdir/configure $conf_flags "$@" ...
  $srcdir/configure $conf_flags "$@" \
  && echo Now type \`make\' to compile. || exit 1
else
  echo Skipping configure process.
fi
