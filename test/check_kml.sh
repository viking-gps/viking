#!/bin/sh
# Copyright: CC0

if [ -n "$srcdir" ]; then
  ./test_file_load $srcdir/Stonehenge.kml
else
  ./test_file_load ./Stonehenge.kml
fi
