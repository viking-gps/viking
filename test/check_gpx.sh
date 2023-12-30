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
# --- Broken 'GPXv1.1' tests ...
# e.g. from FitoTrack 12.1
# if the file that is marked as v1.0 - then it gets exported as v1.0
# (even if v1.1 extension fields are read in - no 'extensions' tags are written)
result=$(./gpx2gpx < $srcdir/GH#137.gpx | grep -c "extensions" )
if [ $result != 0 ]; then
  echo "gpx2gpx failure $count as result=$result"
  exit 1
fi
# Also confirm GPX version is indeed v1.0
count=`expr $count + 1`
result=$(./gpx2gpx < $srcdir/GH#137.gpx | grep -cF 'gpx version="1.0"' )
if [ $result != 1 ]; then
  echo "gpx2gpx failure $count as result=$result"
  exit 1
fi

# GPX file error whilst processing a track element:
count=`expr $count + 1`
shortfile1=./shorty1.gpx
head -n 50 $srcdir/Stonehenge.gpx > $shortfile1
# Works but returns warning code
./test_file_load $shortfile1
result=$?
if [ $result != 2 ]; then
  echo "gpx track read failure unexpected value=$result"
  exit 1
fi
rm $shortfile1

# GPX file error whilst processing a waypoint element:
count=`expr $count + 1`
shortfile2=./shorty2.gpx
head -n 50 $srcdir/WaypointSymbols.gpx > $shortfile2
# Works but returns warning code
./test_file_load $shortfile2
result=$?
if [ $result != 2 ]; then
  echo "gpx waypoint read failure unexpected value=$result"
  exit 1
fi
rm $shortfile2
