#!/bin/sh

# Enable running in test directory or via make distcheck when $srcdir is defined
if [ -z "$srcdir" ]; then
  srcdir=.
fi

outfile=./testout-$$.vik

# ATM Tests either full, no libgps or no geoclue
# Not going to try to cover all potential options, such as
#  no gps && no geoclue at the same time

if [ -z "$REALTIME_GPS_TRACKING" ]; then
    testvik=$srcdir/Simple_no-realtime-gps-tracking.vik
elif [ -z "$GEOCLUE_ENABLED" ]; then
    testvik=$srcdir/Simple_no-geoclue.vik
else
    testvik=$srcdir/Simple.vik
fi

result=$(./vik2vik < $testvik $outfile)
if [ $? != 0 ]; then
  echo "vik2vik command failure"
  exit 1
fi

# Avoid maps directory as a blank input value may get saved with a user path specific default
sed -i '/^directory=/d' $outfile
grep -v "^directory=" $testvik | diff $outfile -
if [ $? != 0 ]; then
  echo "vik2vik produced different result"
  exit 1
fi
rm $outfile
