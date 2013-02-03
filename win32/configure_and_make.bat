:: Full build
set PATH=%PATH%;C:\Mingw\bin;C:\msys\1.0\bin
cd ..\
sh configure CFLAGS=-DWINDOWS LIBCURL=-lcurldll LIBS=-lzdll --disable-realtime-gps-tracking --disable-scrollkeeper
make
