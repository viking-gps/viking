#!/bin/bash
# NB Annoyingly pushd/popd are bashisms!
#
# You don't want to do this on a previously configured/built Linux source tree
#  as the build system will get confused so make clean and start again...
#

# Ensure a basic Windows compatible system is set up
# Use wget to get wget!
if [ ! -e ~/.wine/drive_c/Program\ Files/GnuWin32/bin/wget.exe ]; then
	if [ ! -e cache ]; then
		mkdir cache
	fi
	pushd cache
	WGET_EXE=wget-1.11.4-1-setup.exe
	if [ ! -e $WGET_EXE ]; then
		wget http://downloads.sourceforge.net/gnuwin32/$WGET_EXE
	fi
	wine $WGET_EXE \/silent
	popd
fi

wine ~/.wine/drive_c/windows/system32/cmd.exe /c prepare.bat
