#!/bin/bash

# Currently on my machine all tests takes about 10 minutes

check_build ()
{
    make clean > /dev/null
    echo "Building with '$1' ..."
    ./configure $1 > /dev/null && make -j > /dev/null
    if [ $? != 0 ]; then
        echo "==================="
        echo "Build for $1 failed"
        echo "==================="
        exit 1
    fi
}

# Options not supported anymore but might work:
#check_build "--enable-gtk2"

# Check for mostly single build option failures
# Could check (some) combinations - but too many variants
check_build "--disable-geotag --disable-geoclue"
check_build "--disable-bing"
check_build "--disable-google"
check_build "--enable-terraserver"
check_build "--enable-expedia"
check_build "--disable-openstreetmap"
check_build "--disable-bluemarble"
check_build "--disable-geonames"
check_build "--enable-geocaches"
check_build "--disable-geoclue"
check_build "--disable-geotag"
check_build "--with-libexif"
check_build "--enable-dem24k"
check_build "--disable-oauth"
check_build "--disable-realtime-gps-tracking"
check_build "--disable-bzip2"
check_build "--disable-magic"
check_build "--disable-mbtiles"
check_build "--disable-zip"
check_build "--disable-nettle"
check_build "--disable-nova"
check_build "--disable-mapnik"
check_build "--disable-xz"
# Restore back to defaults
check_build ""
