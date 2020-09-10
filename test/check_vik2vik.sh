#!/bin/sh

# Enable running in test directory or via make distcheck when $srcdir is defined
if [ -z "$srcdir" ]; then
  srcdir=.
fi

outfile=./testout-$$.vik
result=$(./vik2vik < $srcdir/Simple.vik $outfile)
if [ $? != 0 ]; then
  echo "vik2vik command failure"
  exit 1
fi

# Avoid maps directory as a blank input value may get saved with a user path specific default
sed -i '/^directory=/d' $outfile
grep -v "^directory=" $srcdir/Simple.vik | diff $outfile -
if [ $? != 0 ]; then
  echo "vik2vik produced different result"
  exit 1
fi
rm $outfile
