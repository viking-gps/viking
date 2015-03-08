#!/bin/bash
# License: CC0

# First ensure we have a configure script:
rm -rf ../src/.deps
rm -rf ../src/icons/.deps
rm -rf ../src/libjpeg/.deps
rm -rf ../src/misc/.deps
pushd ..
./autogen.sh
make distclean
popd

# Note the configure stage under wine** is really slow can easily be over 15 minutes
# make of the icons is also very slow** - can easily be over 5 minutes on a single CPU
# comparatively the make of the actual src code is not too bad

# Speed up the build by using all CPUs available.
# Note that a simple '-j' on it's own overloads the system under Wine - hence put in the specific CPU limit here
wine cmd.exe /c configure_and_make.bat -j $(grep -c ^processor /proc/cpuinfo)

# ** slowness is probably due to lots of forking going on starting many new small processes
