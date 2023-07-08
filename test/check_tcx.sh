#!/bin/sh
# Copyright: CC0

if [ -n "$srcdir" ]; then
  ./test_file_load $srcdir/Stonehenge.tcx
else
  ./test_file_load ./Stonehenge.tcx
fi
