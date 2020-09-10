#!/bin/sh
# Enable running in test directory or via make distcheck when $srcdir is defined
if [ -z "$srcdir" ]; then
  srcdir=.
fi
yelp-check validate $srcdir/../help/C/index.docbook
