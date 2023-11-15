#!/bin/sh
# Copyright: CC0

# Enable running in test directory or via make distcheck when $srcdir is defined
if [ -z "$srcdir" ]; then
  srcdir=.
fi

LOADFILE=$srcdir/Stonehenge.tcx
count=0

count=`expr $count + 1`
$(./test_file_load $LOADFILE)
result=$?
if [ $result != 0 ]; then
  echo "Part $count: result=$result"
  exit 1
fi

count=`expr $count + 1`
$(./test_file_load -e $LOADFILE)
result=$?
if [ $result != 0 ]; then
  echo "Part $count: result=$result"
  exit 1
fi
