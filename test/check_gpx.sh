#!/bin/sh

# Enable running in test directory or via make distcheck when $srcdir is defined
if [ -z "$srcdir" ]; then
  srcdir=.
fi

result=$(./gpx2gpx < $srcdir/SF#022.gpx | diff $srcdir/SF#022.gpx -)
if [ $? != 0 ]; then
  echo "gpx2gpx failure"
  exit 1
fi
