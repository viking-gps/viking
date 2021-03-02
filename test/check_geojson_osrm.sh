#!/bin/sh

# Enable running in test directory or via make distcheck when $srcdir is defined
if [ -z "$srcdir" ]; then
  srcdir=.
fi

# Basic check that the program has a successful run
result=$(./geojson_osrm_to_gpx $srcdir/OSRM_sample_response.txt /tmp/osrm$$.gpx)
if [ $? != 0 ]; then
  echo "$0 failure"
  exit 1
fi
rm /tmp/osrm$$.gpx
