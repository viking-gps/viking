#!/bin/bash
# License: CC0

# 'make dist' stuff
# ensure we have the latest changelog
../maintainer/git2changelog.sh > cache/ChangeLog.txt

# ensure we have the help PDF
pushd ../help/
make get-icons
pushd C
dblatex viking.xml
popd
popd

if [ ! -e installer/bin ]; then
	mkdir installer/bin
fi

wine cmd.exe /c installer.bat
