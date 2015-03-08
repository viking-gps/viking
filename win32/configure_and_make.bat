:: License: CC0
:: Full build
::
:: Note this assumes the 'configure' script already exists
::  either via part of the source download or via separate generation - e.g. using wine
::
set PATH=%PATH%;%SystemDrive%\Mingw\bin;%SystemDrive%\msys\1.0\bin
pushd ..
:: ATM Don't have build method for libgexiv2, so use the fallback of libexif
sh configure CFLAGS="-DWINDOWS -DWIN32 -mwindows" LIBCURL=-lcurldll LIBS=-lzdll --with-libexif --disable-realtime-gps-tracking --disable-scrollkeeper --enable-windows --disable-mapnik
popd
make.bat %*
