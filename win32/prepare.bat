@echo OFF
:: License: CC0
::
:: Setup wget first - this has to be done manually
:: http://gnuwin32.sourceforge.net/packages/wget.htm
:: http://downloads.sourceforge.net/gnuwin32/wget-1.11.4-1-setup.exe
::
:: Simple script to check required built environment in default locations
::  Versions downloaded are specified absolutely as otherwise it's tricky to try and work out the latest/stable version
:: In principal the idea is to try and automate the process as much as possible
::
set PATH=%PATH%;%ProgramFiles%\GnuWin32\bin

set MINGW=%SystemDrive%\MinGW
set MINGW_BIN=%MinGW%\bin

set ERRORLEVEL=0

if not exist cache mkdir cache
pushd cache

echo =+=+=
echo Checking mingw...
echo =+=+=
set MINGW_EXE=MinGW-5.1.6.exe
set BIN_UTILS=binutils-2.19.1-mingw32-bin.tar.gz
set GCC_CORE=gcc-core-3.4.5-20060117-3.tar.gz
set GCC_GPP=gcc-g++-3.4.5-20060117-3.tar.gz
set MINGWRTDLL=mingwrt-3.15.2-mingw32-dll.tar.gz
set MINGWRTDEV=mingwrt-3.15.2-mingw32-dev.tar.gz
set W32API=w32api-3.13-mingw32-dev.tar.gz

if not exist "%MINGW_BIN%" (
	:: Here we download all default components manually in an attempt to get autoinstall to work...
	if not exist %MINGW_EXE% (
		wget "http://sourceforge.net/projects/mingw/files/OldFiles/MinGW 5.1.6/%MINGW_EXE%"
	)
	if not exist %BIN_UTILS% (
		wget "http://sourceforge.net/projects/mingw/files/MinGW/Base/binutils/binutils-2.19.1/%BIN_UTILS%/download"
	)
	if not exist %GCC_CORE% (
		wget "http://sourceforge.net/projects/mingw/files/MinGW/Base/gcc/Version3/Current Release_ gcc-3.4.5-20060117-3/%GCC_CORE%/download"
	)
	if not exist %GCC_GPP% (
		wget "http://sourceforge.net/projects/mingw/files/MinGW/Base/gcc/Version3/Current Release_ gcc-3.4.5-20060117-3/%GCC_GPP%/download"
	)
	if not exist %MINGWRTDEV% (
		wget http://sourceforge.net/projects/mingw/files/MinGW/Base/mingw-rt/mingwrt-3.15.2/%MINGWRTDEV%/download
	)
	if not exist %MINGWRTDLL% (
		wget http://sourceforge.net/projects/mingw/files/MinGW/Base/mingw-rt/mingwrt-3.15.2/%MINGWRTDLL%/download
	)
	if not exist %W32API% (
		wget http://sourceforge.net/projects/mingw/files/MinGW/Base/w32api/w32api-3.13/%W32API%/download
	)
	:: Can't get it to silent install. As a NSIS installer it supports /S, but it doesn't seem to work - it just hangs
	:: Have to click through manually
	%MINGW_EXE%
	if ERRORLEVEL 1 goto Error
)

echo =+=+=
echo Checking MSYS...
echo =+=+=
set MSYS_EXE=MSYS-1.0.11.exe
if not exist "%SystemDrive%\msys" (
	if not exist %MSYS_EXE% (
		wget http://downloads.sourceforge.net/mingw/%MSYS_EXE%
	)
	if not [%WINELOADER%]==[] (
		echo Running under WINE - Requires MSYS install fixes: run msys-pi-wine.sh when msys install halts..."
		echo Ctrl-C to stop and then rerun the installation if necessary
	)
	%MSYS_EXE% /sp- /silent
	if ERRORLEVEL 1 goto Error
)

:: We need a program to be able to extract not only zips, but bz2 and *lzma*
set PATH=%PATH%;%ProgramFiles%\7-Zip
echo =+=+=
echo Checking 7Zip is Available...
echo =+=+=
set ZIP_INST=7z920.exe
if not exist "%ProgramFiles%\7-Zip\7z.exe" (
	if not exist %ZIP_INST% (
		wget http://downloads.sourceforge.net/sevenzip/%ZIP_INST%
	)
	%ZIP_INST% /S
	if ERRORLEVEL 1 goto Error
)

::
echo =+=+=
echo Checking gtk+-bundle...
echo =+=+=
set GTK_ZIP=gtk+-bundle_2.24.10-20120208_win32.zip
if not exist "%MINGW_BIN%\gtk-update-icon-cache.exe" (
	if not exist %GTK_ZIP% (
		wget http://ftp.gnome.org/pub/gnome/binaries/win32/gtk+/2.24/%GTK_ZIP%
	)
	7z x %GTK_ZIP% -o"%MinGW%"
	if ERRORLEVEL 1 goto Error
)

set EXPAT_ZIP=expat-dev_2.0.1-1_win32.zip
echo =+=+=
echo Checking expat-dev...
echo =+=+=
if not exist "%MINGW%\include\expat.h" (
	if not exist %EXPAT_ZIP% (
		wget http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/%EXPAT_ZIP%
	)
	7z x %EXPAT_ZIP% -o"%MinGW%"
	if ERRORLEVEL 1 goto Error
)

set GTT_ZIP=gettext-tools-dev_0.18.1.1-2_win32.zip
echo =+=+=
echo Checking gettext-tools-dev...
echo =+=+=
if not exist "%MINGW_BIN%\libgettextlib-0-18-1.dll" (
	if not exist %GTT_ZIP% (
		wget http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/%GTT_ZIP%
	)
	7z x -y %GTT_ZIP% -o"%MinGW%"
	if ERRORLEVEL 1 goto Error
)

echo =+=+=
echo Checking intltool...
echo =+=+=
set INTLTOOL_ZIP=intltool_0.40.4-1_win32.zip
if not exist "%MINGW_BIN%\intltoolize" (
	if not exist %INTLTOOL_ZIP% (
		wget http://ftp.acc.umu.se/pub/GNOME/binaries/win32/intltool/0.40/%INTLTOOL_ZIP%
	)
	7z x %INTLTOOL_ZIP% -o"%MinGW%"
	if ERRORLEVEL 1 goto Error
)

echo =+=+=
echo Checking iconv...
echo =+=+=
set ICONV_ZIP=libiconv-1.9.2-1-bin.zip
if not exist "%MINGW_BIN%\iconv.exe" (
	if not exist %ICONV_ZIP% (
		wget http://sourceforge.net/projects/gnuwin32/files/libiconv/1.9.2-1/%ICONV_ZIP%
	)
	7z x -y %ICONV_ZIP% -o"%MinGW%"
	if ERRORLEVEL 1 goto Error
)

echo =+=+=
echo Checking libintl...
echo =+=+=
:: Needed by iconv
set LIBINTL_ZIP=libintl-0.14.4-bin.zip
if not exist "%MINGW_BIN%\libintl3.dll" (
	if not exist %LIBINTL_ZIP% (
		wget http://sourceforge.net/projects/gnuwin32/files/libintl/0.14.4/%LIBINTL_ZIP%
	)
	7z x -y %LIBINTL_ZIP% -o"%MinGW%"
	if ERRORLEVEL 1 goto Error
)

echo =+=+=
echo Checking libcurl...
echo =+=+=
set CURL_TAR=libcurl-7.14.0_nossl-1sid.tar
set CURL_BZ2=%CURL_TAR%.bz2
if not exist "%MINGW_BIN%\libcurl.dll" (
	if not exist %CURL_BZ2% (
		wget http://downloads.sourceforge.net/devpaks/libcurl-7.14.0_nossl-1sid.DevPak?download
		move libcurl-7.14.0_nossl-1sid.DevPak %CURL_BZ2%
	)
	echo Extracting libcurl...
	7z e %CURL_BZ2%
	7z x %CURL_TAR% -o"libcurl"
	if ERRORLEVEL 1 goto Error
	@echo ON
	move libcurl\include "%MinGW%\include\curl
	copy /Y libcurl\bin\*.* "%MinGW_BIN%"
	copy /Y libcurl\lib\*.* "%MinGW%\lib"
	copy /Y libcurl\docs\*.* "%MinGW%\doc"
	copy /Y COPYING.txt "%MinGW%\COPYING_curl.txt"
	rmdir /S /Q libcurl
	del %CURL_TAR%
	@echo OFF
)

echo =+=+=
echo Checking libexif...
echo =+=+=
set EXIF=libexif-0.6.20_winxp_mingw
set EXIF_7Z=%EXIF%.7z
if not exist "%MINGW_BIN%\libexif-12.dll" (
	if not exist %EXIF_7Z% (
		wget "http://sourceforge.net/projects/maille/files/Extern libs/%EXIF_7Z%/download"
	)
	echo Extracting libexif...
	7z x %EXIF_7Z%
	if ERRORLEVEL 1 goto Error

	echo Using *xcopy* (to get all subdirs) libexif into place...
	@echo ON
	xcopy /Y /S %EXIF%\*.* "%MinGW%"
	rmdir /S /Q %EXIF%
	@echo OFF
)

echo =+=+=
echo Checking libstdc++...
echo =+=+=
set STDCPP_TAR=libstdc++-4.6.2-1-mingw32-dll-6.tar
set STDCPP_LZ=%STDCPP_TAR%.lzma
if not exist "%MINGW_BIN%\libstdc++-6.dll" (
	if not exist %STDCPP_LZ% (
		wget http://sourceforge.net/projects/mingw/files/MinGW/Base/gcc/Version4/gcc-4.6.2-1/%STDCPP_LZ%
	)
	echo Extracting lidstdc++...
	7z e %STDCPP_LZ%
	7z x %STDCPP_TAR% -o"%MinGW%"
	if ERRORLEVEL 1 goto Error
	del %STDCPP_TAR%
)

echo =+=+=
echo Checking libbz2 header...
echo =+=+=
set BZ2_TAR=bzip2-1.0.6-4-mingw32-dev.tar
set BZ2_LZ=%BZ2_TAR%.lzma
if not exist "%MINGW%\include\bzlib.h" (
	if not exist %BZ2_LZ% (
		wget "http://sourceforge.net/projects/mingw/files/MinGW/Extension/bzip2/bzip2-1.0.6-4/%BZ2_LZ%"
	)
	echo Extracting libbz2 header...
	7z e %BZ2_LZ%
	7z x %BZ2_TAR% -o"%MinGW%"
	if ERRORLEVEL 1 goto Error
	del %BZ2_TAR%
)

echo =+=+=
echo Checking libbz2...
echo =+=+=
set BZ2DLL_TAR=libbz2-1.0.6-4-mingw32-dll-2.tar
set BZ2DLL_LZ=%BZ2DLL_TAR%.lzma
if not exist "%MINGW_BIN%\libbz2-2.dll" (
	if not exist %BZ2DLL_LZ% (
		wget "http://sourceforge.net/projects/mingw/files/MinGW/Extension/bzip2/bzip2-1.0.6-4/%BZ2DLL_LZ%"
	)
	echo Extracting libbz2...
	7z e %BZ2DLL_LZ%
	7z x %BZ2DLL_TAR% -o"%MinGW%"
	if ERRORLEVEL 1 goto Error
	del %BZ2DLL_TAR%
)

echo =+=+=
echo Checking magic dev...
echo =+=+=
set MAGIC_ZIP=file-5.03-lib.zip
if not exist "%MINGW%\include\magic.h" (
	if not exist %MAGIC_ZIP% (
		wget http://downloads.sourceforge.net/gnuwin32/%MAGIC_ZIP%
	)
	7z x %MAGIC_ZIP% -o"%MinGW%"
	if ERRORLEVEL 1 goto Error
)

echo =+=+=
echo Checking magic DLL...
echo =+=+=
set MAGICDLL_ZIP=file-5.03-bin.zip
if not exist "%MINGW_BIN%\magic1.dll" (
	if not exist %MAGICDLL_ZIP% (
		wget http://downloads.sourceforge.net/gnuwin32/%MAGICDLL_ZIP%
	)
	7z x %MAGICDLL_ZIP% -o"%MinGW%"
	if ERRORLEVEL 1 goto Error
)

echo =+=+=
echo Checking regex DLL (required by magic)...
echo =+=+=
set REGDLL_ZIP=regex-2.7-bin.zip
if not exist "%MINGW_BIN%\regex2.dll" (
	if not exist %REGDLL_ZIP% (
		wget http://downloads.sourceforge.net/gnuwin32/%REGDLL_ZIP%
	)
	7z x %REGDLL_ZIP% -o"%MinGW%"
	if ERRORLEVEL 1 goto Error
)

echo =+=+=
echo Checking SQLite dev...
echo =+=+=
set SQL_ZIP=sqlite-amalgamation-3080002.zip
if not exist "%MINGW%\include\sqlite3.h" (
	if not exist %SQL_ZIP% (
		wget http://www.sqlite.org/2013/%SQL_ZIP%
	)
	7z x %SQL_ZIP%
	if ERRORLEVEL 1 goto Error
	copy /Y sqlite-amalgamation-3080002\s* "%MinGW%\include"
	rmdir /S /Q sqlite-amalgamation-3080002
)

echo =+=+=
echo Checking SQL DLL...
echo =+=+=
set SQLDLL_ZIP=sqlite-dll-win32-x86-3080002.zip
if not exist "%MINGW_BIN%\sqlite3.dll" (
	if not exist %SQLDLL_ZIP% (
		wget http://www.sqlite.org/2013/%SQLDLL_ZIP%
	)
	7z x %SQLDLL_ZIP% -o"%MinGW_BIN%"
	if ERRORLEVEL 1 goto Error
	REM Annoyingly SQL doesn't come with a .lib file so have to generate it ourselves:
	REM Possibly need to insert the line 'LIBRARY sqlite3.dll' at the beginning of the def file?
	REM  but this may not be needed as the --dllname option may suffice
	popd %MinGW_BIN%
	dlltool -d sqlite3.def --dllname sqlite3.dll -l ..\lib\sqlite3.lib
	pushd
)

::
:: Ideally building the code on Windows shouldn't need Doc Utils or the Help processor stuff
:: But ATM it's too hard to avoid.
::
echo =+=+=
echo Checking Gnome Doc Utils...
echo =+=+=
set GNOME_DOC_ZIP=gnome-doc-utils-0.12.0.zip
if not exist "%MINGW_BIN%\gnome-doc-prepare" (
	if not exist %GNOME_DOC_ZIP% (
		wget http://ftp.gnome.org/pub/gnome/binaries/win32/gnome-doc-utils/0.12/%GNOME_DOC_ZIP%
	)
	echo Extracting Gnome Doc Utils...
	7z x %GNOME_DOC_ZIP% -o"%MinGW%"
	if ERRORLEVEL 1 goto Error
)

echo =+=+=
echo Checking xsltproc...
echo =+=+=
set XLST=libxslt-1.1.26.win32
set XLST_ZIP=%XLST%.zip
if not exist "%MINGW_BIN%\xsltproc.exe" (
	if not exist %XLST_ZIP% (
		wget ftp://ftp.zlatkovic.com/libxml/%XLST_ZIP%
	)
	echo Extracting XLST...
	7z x %XLST_ZIP%
	xcopy /Y /S "%XLST%\bin\*" "%MinGW_BIN%"
	if ERRORLEVEL 1 goto Error
	rmdir /Q /S %XLST%
)

echo =+=+=
echo Checking xmllint...
echo =+=+=
set XML2=libxml2-2.7.8.win32
set XML2_ZIP=%XML2%.zip
if not exist "%MINGW_BIN%\xmllint.exe" (
	if not exist %XML2_ZIP% (
		wget ftp://ftp.zlatkovic.com/libxml/%XML2_ZIP%
	)
	echo Extracting xmllint...
	7z x %XML2_ZIP%
	xcopy /Y /S "%XML2%\bin\*" "%MinGW_BIN%"
	if ERRORLEVEL 1 goto Error
	rmdir /Q /S %XML2%
)

echo =+=+=
echo Checking iconv...
echo =+=+=
set ICONV=iconv-1.9.2.win32
set ICONV_ZIP=%ICONV%.zip
if not exist "%MINGW_BIN%\iconv.dll" (
	if not exist %ICONV_ZIP% (
		wget ftp://ftp.zlatkovic.com/libxml/%ICONV_ZIP%
	)
	echo Extracting iconv...
	7z x %ICONV_ZIP%
	xcopy /Y /S "%ICONV%\bin\*" "%MinGW_BIN%"
	if ERRORLEVEL 1 goto Error
	rmdir /Q /S %ICONV%
)

:: Note GPSBabel can not be directly downloaded via wget
:: ATM get it manually from here:
::   http://www.gpsbabel.org/download.html
set GPSBABEL_INST=GPSBabel-1.4.4-Setup.exe
if not exist "%ProgramFiles%\GPSBabel" (
	echo Installing GPSBabel...
	if exist %GPSBABEL_INST% (
		%GPSBABEL_INST% /silent
		if ERRORLEVEL 1 goto Error
	)
)

echo =+=+=
echo Checking Perl Installation...
echo =+=+=
set PERL_MSI=ActivePerl-5.14.3.1404-MSWin32-x86-296513.msi
if not exist "%SystemDrive%\Perl" (
	if not exist %PERL_MSI% (
		wget http://downloads.activestate.com/ActivePerl/releases/5.14.3.1404/%PERL_MSI%
	)
	echo Installing Perl takes a little time...
	msiexec /qb /i %PERL_MSI% PERL_PATH=Yes PERL_EXT=Yes
	if ERRORLEVEL 1 goto Error
)

echo =+=+=
echo Checking NSIS installed...
echo =+=+=
set NSIS_INST=nsis-2.46-setup.exe
if not exist "%ProgramFiles%\NSIS" (
	if not exist %NSIS_INST% (
		wget http://prdownloads.sourceforge.net/nsis/nsis-2.46-setup.exe?download
	)
	echo Installing NSIS...
	%NSIS_INST% /S
	if ERRORLEVEL 1 goto Error
)

echo =+=+=
echo Checking NSIS Plugins installed...
echo =+=+=
set FPDLLZIP=FindProc.zip
if not exist "%ProgramFiles%\NSIS\Plugins\FindProcDLL.dll" (
	if not exist %FPDLLZIP% (
		wget http://nsis.sourceforge.net/mediawiki/images/3/3c/%FPDLLZIP%
	)
	echo Extracting NSIS Plugins...
	7z e %FPDLLZIP% -o"%ProgramFiles%\NSIS\Plugins"
	if ERRORLEVEL 1 goto Error
)

popd

echo Fixing Perl reference
REM Sadly '-i' for in place changes doesn't seem available with Windows sed 3.02
set PATH=%PATH%;C:\msys\1.0\bin
sed -e 's:#! /bin/perl:#! /opt/perl/bin/perl:' %MINGW_BIN%\glib-mkenums > tmp.enums
if ERRORLEVEL 1 goto Error
xcopy /Y tmp.enums %MINGW_BIN%\glib-mkenums
if ERRORLEVEL 1 goto Error
del tmp.enums

:: Potentially Clean Up
:: If any parameters given on the command line then remove all downloaded items
:Clean
if not [%1]==[] (
	echo Removing downloaded files
	if exist cache rmdir /S /Q cache
)

goto End

:Error
echo exitting due to error: %ERRORLEVEL%
exit

:End
echo Finished
