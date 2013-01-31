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

echo Copying Viking
%MYCOPY% ..\src\viking.exe %DESTINATION%
%MYCOPY% installer\pixmaps\viking_icon.ico %DESTINATION%
%MYCOPY% ..\COPYING %DESTINATION%\COPYING_GPL.txt
::
echo Copying GPSBabel
:: It is assumed you've tested the code after building it :)
::  Thus GPSBabel should be here
%MYCOPY% ..\src\gpsbabel.exe %DESTINATION%
:: Otherwise install it from http://www.gpsbabel.org/download.html
::  (or get it from an old Viking Windows release)
::  and copy the command line program into ..\src
::
echo Copying Libraries
set LIBCURL=C:\MinGW\bin\libcurl.dll
if exist %LIBCURL% (
	%MYCOPY% %LIBCURL% %DESTINATION%
) else (
	echo %LIBCURL% does not exist
	goto Tidy
)
set LIBEXIF=C:\MinGW\bin\libexif-12.dll
if exist %LIBEXIF% (
	%MYCOPY% %LIBEXIF% %DESTINATION%
) else (
	echo %LIBEXIF% does not exist
	goto Tidy
)
::
echo Copying Translations
%MYCOPY% installer\translations\*nsh %DESTINATION%

echo Running NSIS (command line version)
pushd installer
if exist "%ProgramFiles%\NSIS" (
	"%ProgramFiles%\NSIS\makensis.exe" viking-installer.nsi
) else (
	echo NSIS Not installed in known location
)
popd

echo Tidy Up
:Tidy
rmdir /S /Q %DESTINATION%
