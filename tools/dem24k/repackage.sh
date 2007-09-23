#!/bin/bash
TEMPDIR=`tempfile -p dem24`
rm -f $TEMPDIR
mkdir $TEMPDIR
(cd $TEMPDIR; tar -xzf $1; X=`basename \`ls [0-9][0-9][0-9][0-9]CATD.DDF\` CATD.DDF`; echo $X; sdts2dem $X $2)
mv $TEMPDIR/$2.dem .
cd ..
rm -rf $TEMPDIR

