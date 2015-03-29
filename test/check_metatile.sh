#!/bin/sh

if [ -n "$srcdir" ]; then
  ./test_metatile "$srcdir/metatile_example" && rm tilefrommeta.png
else
  ./test_metatile && rm tilefrommeta.png
fi
