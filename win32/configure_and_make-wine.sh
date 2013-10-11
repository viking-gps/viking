#!/bin/bash
# License: CC0

# First ensure we have a configure script:
rm -rf ../src/.deps
rm -rf ../src/icons/.deps
pushd ..
./autogen.sh
make distclean
popd

# Note the configure stage under wine** is really slow can easily be over 15 minutes
# make of the icons is also very slow** - can easily over 5 minutes
# compartively the make of the actual src code is not too bad
wine cmd.exe /c configure_and_make.bat

# ** slowness is probably due to lots of forking going on starting many new small processes
