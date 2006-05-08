#!/bin/sh

# This file allows to 'bootstrap' the generation envir.
# It must be used the first time the project is downloaded from the CVS.

#libtoolize || exit 1
aclocal || exit 1
autoheader || exit 1
automake --add-missing || exit 1
autoconf || exit 1
