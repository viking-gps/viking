:: License: CC0
:: Standard build
set PATH=%PATH%;%SystemDrive%\MinGW\bin;%SystemDrive%\msys\1.0\bin

:: The icon needs creating first as it will get embedded in the generated executable
echo Create Icon
pushd installer\pixmaps
windres.exe viking_icon.rc -o viking_icon.o
popd

:: Change to root directory and build
cd ..
make %*
