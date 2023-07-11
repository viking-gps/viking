#!/bin/sh
# Copyright: CC0

# Create zip file on the fly rather than storing in repository
# Hopefully safe to assume zip program is always installed
# Enable running in test directory or via make distcheck when $srcdir is defined
if [ -z "$srcdir" ]; then
  srcdir=.
fi
zfile=./Stonehenge-$$.zip
zip $zfile $srcdir/Stonehenge.gpx
./test_file_load $zfile
if [ $? != 0 ]; then
  echo "load zip test failure"
  exit 1
fi
rm $zfile
