#!/bin/sh
# Copyright: CC0
if [ $(echo -n I | od -to2 | awk '{ print substr($2,6,1); exit}') != 1 ]; then
    echo "Skipping metatile test on big endian machine"
    echo " sample metatile file is only for little endian machines"
    exit 0
fi

if [ -n "$srcdir" ]; then
  ./test_metatile "$srcdir/metatile_example" && rm tilefrommeta.png
else
  ./test_metatile && rm tilefrommeta.png
fi
