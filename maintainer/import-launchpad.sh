#!/bin/sh
cd po
tar xzf ../launchpad-export.tar.gz 
#rename -f 's/viking\/viking-//' viking/viking*.po
mv viking/*.po .
rename -f 's/viking-//' viking*.po

