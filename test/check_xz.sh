#!/bin/sh
# Copyright: CC0

# Create xz file on the fly rather than storing in repository
# Hopefully safe to assume xz program is always installed
# Note using 'kml' test file as this is larger than
#  than the block size processing in our xz handling code
# (this ensures buffer reusage part is tested)

# Enable running in test directory or via make distcheck when $srcdir is defined
if [ -z "$srcdir" ]; then
  srcdir=.
fi
zfile=./Stonehenge-$$.xz
xz --keep --stdout $srcdir/Stonehenge.kml > $zfile
./test_file_load $zfile
if [ $? != 0 ]; then
  echo "load xz test failure"
  exit 1
fi
rm $zfile
