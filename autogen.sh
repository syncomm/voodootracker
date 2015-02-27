#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="Voodoo Tracker"

(test -f $srcdir/configure.in \
  && test -f $srcdir/app/xm.h) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}

. $srcdir/macros/autogen.sh
