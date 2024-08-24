#!/bin/sh
# Copyright: CC0

# Enable running in test directory or via make distcheck when $srcdir is defined
if [ -z "$srcdir" ]; then
  srcdir=.
fi

LOADFILE=$srcdir/Stonehenge.kml
count=0

count=`expr $count + 1`
result=$(./test_file_load $LOADFILE)
if [ $? != 0 ]; then
  echo "Part $count: result=$result"
  exit 1
fi

count=`expr $count + 1`
result=$(./test_file_load -e $LOADFILE)
if [ $? != 0 ]; then
  echo "Part $count: result=$result"
  exit 1
fi
