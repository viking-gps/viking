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

count=0
count=`expr $count + 1`
# --- GPXv1.1 test ---
# NB also ignore the GPX name that is inserted
grep -v "^creator=" $srcdir/GPXv1.1-sample.gpx > ./GPXv1.1-sample-nocreator.gpx
result=$(./gpx2gpx < $srcdir/GPXv1.1-sample.gpx | \
             grep -v "^creator=" | \
             grep -vF "<desc>Created by:" | \
             grep -vF "<name>TrackWaypoint</name>" | \
             diff -w ./GPXv1.1-sample-nocreator.gpx -)
if [ $? != 0 ]; then
  echo "gpx2gpx failure $count"
  exit 1
fi
rm ./GPXv1.1-sample-nocreator.gpx

count=`expr $count + 1`
# --- Broken 'GPXv1.1' test ...
# e.g. from FitoTrack 12.1
# if the file that is marked as v1.0 - then it gets exported as v1.0
# (even if v1.1 extension fields are read in)
grep -v "^creator=" $srcdir/GH#137-v1.0.gpx > ./GH#137-v1.0-nocreator.gpx
result=$(./gpx2gpx < $srcdir/GH#137-v1.0.gpx | \
             grep -v "^creator=" | \
             diff ./GH#137-v1.0-nocreator.gpx -)
if [ $? != 0 ]; then
  echo "gpx2gpx failure $count"
  exit 1
fi
rm ./GH#137-v1.0-nocreator.gpx
