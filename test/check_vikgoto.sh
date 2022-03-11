#!/bin/sh
# Copyright: CC0

PROG=./test_vikgotoxmltool

check_success ()
{
    value=$1
    result=$($PROG "$value")
    if [ $? != 0 ]; then
        echo "$PROG failure on $value"
        exit 1
    fi
}

# Enable running in test directory or via make distcheck when $srcdir is defined
if [ -z "$srcdir" ]; then
  srcdir=.
fi

check_success $srcdir/search-result-geonames-viking.xml
check_success $srcdir/search-result-geonames-attr-viking.xml
check_success $srcdir/search-result-nominatim-viking.xml

exit 0
