#!/bin/sh

# Quick script to convert the given file and upload load it to an attached GPS device
# As this modifies the given file, it is ideally designed for temporary files.

#
# Some basic checks
#
if [ -z "$1" ]; then
	echo "Need a file to process"
	exit 1
fi

if [ ! -e "$1" ]; then
	echo "Specified file >$1< does not exist"
	exit 1
fi

# Delete lines with track segments - there is no equivalent in routes
# Then simply replace track and trackpoints with route types

sed -i \
-e '/trkseg/d' \
-e 's:<trkpt:<rtept:g' \
-e 's:</trkpt>:</rtept>:g' \
-e 's:<trk>:<rte>:g' \
-e 's:</trk>:</rte>:g' \
"$1"

# Ths is obviously setup for a USB Garmin device
gpsbabel -r -i gpx,gpxver=1.1 -f "$1" -o garmin -F /dev/ttyUSB0
# One may need to change this for your configuration

