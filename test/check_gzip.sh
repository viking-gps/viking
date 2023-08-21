#!/bin/sh
# Copyright: CC0

# Create gzip file on the fly rather than storing in repository
# Hopefully safe to assume gzip program is always installed
# Enable running in test directory or via make distcheck when $srcdir is defined
if [ -z "$srcdir" ]; then
  srcdir=.
fi
zfile=./Stonehenge-$$.gz
gzip --keep --stdout $srcdir/Stonehenge.gpx > $zfile
./test_file_load $zfile
if [ $? != 0 ]; then
  echo "load gzip test failure"
  exit 1
fi
rm $zfile
