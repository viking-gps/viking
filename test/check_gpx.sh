#!/bin/sh

# Enable running in test directory or via make distcheck when $srcdir is defined
if [ -z "$srcdir" ]; then
  srcdir=.
fi

# Avoid creator line as that now has the potential to be changed
# (either via preference setting or version number change)
grep -v "^creator=" $srcdir/SF#022.gpx > ./SF#022-creator.gpx
result=$(./gpx2gpx < $srcdir/SF#022.gpx | grep -v "^creator=" | grep -vF "<desc>Created by:" | diff ./SF#022-creator.gpx -)
if [ $? != 0 ]; then
  echo "gpx2gpx failure"
  exit 1
fi
rm ./SF#022-creator.gpx
