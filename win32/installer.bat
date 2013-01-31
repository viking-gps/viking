@echo OFF
echo STARTING INSTALLER PROCESS...

echo Create Icon
pushd installer\pixmaps
windres.exe viking_icon.rc -o viking_icon.o
popd

echo Remove debugging symbols
pushd ..\src
strip.exe -g viking.exe
popd

set MYCOPY=copy /y
set DESTINATION=installer\bin
echo Copying locale files into layout required by NSIS
dir ..\po\*.gmo /B > gmolist.txt
:: Create directories like de\LC_MESSAGES
for /f %%i in (gmolist.txt) do mkdir %DESTINATION%\%%~ni\LC_MESSAGES
for /f %%i in (gmolist.txt) do %MYCOPY% ..\po\%%i %DESTINATION%\%%~ni\LC_MESSAGES\viking.mo
del gmolist.txt

echo Copying other stuff
%MYCOPY% ..\src\viking.exe %DESTINATION%
%MYCOPY% installer\pixmaps\viking_icon.ico %DESTINATION%
%MYCOPY% ..\COPYING %DESTINATION%\COPYING_GPL.txt
::
:: It is assumed you've tested the code after building it :)
::  Thus GPSBabel should be here
%MYCOPY% ..\src\gpsbabel.exe %DESTINATION%
:: Otherwise install it from http://www.gpsbabel.org/download.html
::  (or get it from an old Viking Windows release)
::  and copy the command line program into ..\src
::
%MYCOPY% C:\MinGW\bin\libcurl.dll %DESTINATION%
%MYCOPY% C:\MinGW\bin\libexif-12.dll %DESTINATION%
::
%MYCOPY% installer\translations\*nsh %DESTINATION%

echo Run NSIS
pushd installer
"C:\Program Files\NSIS\makensisw.exe" viking-installer.nsi
popd

