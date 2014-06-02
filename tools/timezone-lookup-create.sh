#!/bin/bash
#
# Get location and timezone information from Geonames
#
# Then simplify & mash it up into the minimal format for our usage.

# ~23K entries in 4.5M file (when uncompressed)
export CITIES=cities15000.zip
if [ ! -e $CITIES ]; then
	wget http://download.geonames.org/export/dump/$CITIES
fi

# Simplify the info - just want lat, lon and TZ id
unzip -p $CITIES | awk -F "\t" '{print $5, $6, $18}' > latlontz.txt
# Result is a ~750K file

# Store in the data directory
mv latlontz.txt ../data
