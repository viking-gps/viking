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

:: We need a program to be able to extract not only zips, but bz2 and *lzma*
set PATH=%PATH%;%ProgramFiles%\7-Zip
echo =+=+=
echo Checking 7Zip is Available...
echo =+=+=
set ZIP_INST=7z920.exe
if not exist "%ProgramFiles%\7-Zip\7z.exe" (
	call :Download %ZIP_INST% http://downloads.sourceforge.net/sevenzip/%ZIP_INST%
	%ZIP_INST% /S
	if ERRORLEVEL 1 goto Error
)

echo =+=+=
echo Checking mingw...
echo =+=+=
set MINGW_BASE_URL=http://sourceforge.net/projects/mingw/files/MinGW/Base
set BIN_UTILS_LZ=binutils-2.23.2-1-mingw32-bin.tar.lzma
set BIN_UTILS_URL=%MINGW_BASE_URL%/binutils/binutils-2.23.2-1/%BIN_UTILS_LZ%/download
::GCC dependencies::
set MPC_DEV_LZ=mpc-1.0.1-2-mingw32-dev.tar.lzma
set MPC_DEV_URL=%MINGW_BASE_URL%/mpc/mpc-1.0.1-2/%MPC_DEV_LZ%/download
set MPC_DLL_LZ=mpc-1.0.1-2-mingw32-dll.tar.lzma
set MPC_DLL_URL=%MINGW_BASE_URL%/mpc/mpc-1.0.1-2/%MPC_DLL_LZ%/download
set MPFR_DEV_LZ=mpfr-3.1.2-2-mingw32-dev.tar.lzma
set MPFR_DEV_URL=%MINGW_BASE_URL%/mpfr/mpfr-3.1.2-2/%MPFR_DEV_LZ%/download
set MPFR_DLL_LZ=mpfr-3.1.2-2-mingw32-dll.tar.lzma
set MPFR_DLL_URL=%MINGW_BASE_URL%/mpfr/mpfr-3.1.2-2/%MPFR_DLL_LZ%/download
set GMP_DEV_LZ=gmp-5.1.2-1-mingw32-dev.tar.lzma
set GMP_DEV_URL=%MINGW_BASE_URL%/gmp/gmp-5.1.2/%GMP_DEV_LZ%/download
set GMP_DLL_LZ=gmp-5.1.2-1-mingw32-dll.tar.lzma
set GMP_DLL_URL=%MINGW_BASE_URL%/gmp/gmp-5.1.2/%GMP_DLL_LZ%/download
set PTHREADS_DEV_LZ=pthreads-w32-2.9.1-1-mingw32-dev.tar.lzma
set PTHREADS_DEV_URL=%MINGW_BASE_URL%/pthreads-w32/pthreads-w32-2.9.1/%PTHREADS_DEV_LZ%/download
set PTHREADS_DLL_LZ=pthreads-w32-2.9.1-1-mingw32-dll.tar.lzma
set PTHREADS_DLL_URL=%MINGW_BASE_URL%/pthreads-w32/pthreads-w32-2.9.1/%PTHREADS_DLL_LZ%/download
set ICONV_DEV_LZ=libiconv-1.14-3-mingw32-dev.tar.lzma
set ICONV_DEV_URL=%MINGW_BASE_URL%/libiconv/libiconv-1.14-3/%ICONV_DEV_LZ%/download
set ICONV_DLL_LZ=libiconv-1.14-3-mingw32-dll.tar.lzma
set ICONV_DLL_URL=%MINGW_BASE_URL%/libiconv/libiconv-1.14-3/%ICONV_DLL_LZ%/download
set GCC_CORE_BIN_LZ=gcc-core-4.8.1-4-mingw32-bin.tar.lzma
set GCC_CORE_BIN_URL=%MINGW_BASE_URL%/gcc/Version4/gcc-4.8.1-4/%GCC_CORE_BIN_LZ%/download
set GCC_CORE_DEV_LZ=gcc-core-4.8.1-4-mingw32-dev.tar.lzma
set GCC_CORE_DEV_URL=%MINGW_BASE_URL%/gcc/Version4/gcc-4.8.1-4/%GCC_CORE_DEV_LZ%/download
set GCC_CORE_DLL_LZ=gcc-core-4.8.1-4-mingw32-dll.tar.lzma
set GCC_CORE_DLL_URL=%MINGW_BASE_URL%/gcc/Version4/gcc-4.8.1-4/%GCC_CORE_DLL_LZ%/download
set MINGWRTDLL_LZ=mingwrt-4.0.3-1-mingw32-dll.tar.lzma
set MINGWRTDLL_URL=%MINGW_BASE_URL%/mingw-rt/mingwrt-4.0.3/%MINGWRTDLL_LZ%/download
set MINGWRTDEV_LZ=mingwrt-4.0.3-1-mingw32-dev.tar.lzma
set MINGWRTDEV_URL=%MINGW_BASE_URL%/mingw-rt/mingwrt-4.0.3/%MINGWRTDEV_LZ%/download
set W32API_LZ=w32api-4.0.3-1-mingw32-dev.tar.lzma
set W32API_URL=%MINGW_BASE_URL%/w32api/w32api-4.0.3/%W32API_LZ%/download
set ZLIB_LZ=zlib-1.2.8-1-mingw32-dll.tar.lzma
set ZLIB_URL=%MINGW_BASE_URL%/zlib/zlib-1.2.8/%ZLIB_LZ%/download
set GETTEXT_LZ=gettext-0.18.3.1-1-mingw32-dll.tar.lzma
set GETTEXT_URL=%MINGW_BASE_URL%/gettext/gettext-0.18.3.1-1/%GETTEXT_LZ%/download

set GCC-CPP_BIN_LZ=gcc-c++-4.8.1-4-mingw32-bin.tar.lzma
set GCC-CPP_BIN_URL=%MINGW_BASE_URL%/gcc/Version4/gcc-4.8.1-4/%GCC-CPP_BIN_LZ%/download
set GCC-CPP_DEV_LZ=gcc-c++-4.8.1-4-mingw32-dev.tar.lzma
set GCC-CPP_DEV_URL=%MINGW_BASE_URL%/gcc/Version4/gcc-4.8.1-4/%GCC-CPP_DEV_LZ%/download
set GCC-CPP_DLL_LZ=gcc-c++-4.8.1-4-mingw32-dll.tar.lzma
set GCC-CPP_DLL_URL=%MINGW_BASE_URL%/gcc/Version4/gcc-4.8.1-4/%GCC-CPP_DLL_LZ%/download

if not exist "%MINGW_BIN%" (
	:: Here we download all default components manually in an attempt to get autoinstall to work...
	call :Download "%BIN_UTILS_LZ%" "%BIN_UTILS_URL%"
	call :InstallLZMA "%BIN_UTILS_LZ%"

	call :Download "%MPC_DLL_LZ%" "%MPC_DLL_URL%"
	call :InstallLZMA "%MPC_DLL_LZ%"

	call :Download "%MPC_DEV_LZ%" "%MPC_DEV_URL%"
	call :InstallLZMA "%MPC_DEV_LZ%"

	call :Download "%MPFR_DLL_LZ%" "%MPFR_DLL_URL%"
	call :InstallLZMA "%MPFR_DLL_LZ%"

	call :Download "%MPFR_DEV_LZ%" "%MPFR_DEV_URL%"
	call :InstallLZMA "%MPFR_DEV_LZ%"

	call :Download "%GMP_DEV_LZ%" "%GMP_DEV_URL%"
	call :InstallLZMA "%GMP_DEV_LZ%"

	call :Download "%GMP_DLL_LZ%" "%GMP_DLL_URL%"
	call :InstallLZMA "%GMP_DLL_LZ%"

	call :Download "%PTHREADS_DLL_LZ%" "%PTHREADS_DLL_URL%"
	call :InstallLZMA "%PTHREADS_DLL_LZ%"

	call :Download "%PTHREADS_DEV_LZ%" "%PTHREADS_DEV_URL%"
	call :InstallLZMA "%PTHREADS_DEV_LZ%"

	call :Download "%ICONV_DEV_LZ%" "%ICONV_DEV_URL%"
	call :InstallLZMA "%ICONV_DEV_LZ%"

	call :Download "%ICONV_DLL_LZ%" "%ICONV_DLL_URL%"
	call :InstallLZMA "%ICONV_DLL_LZ%"

	call :Download "%GCC_CORE_DEV_LZ%" "%GCC_CORE_DEV_URL%"
	call :InstallLZMA "%GCC_CORE_DEV_LZ%"

	call :Download "%GCC_CORE_DLL_LZ%" "%GCC_CORE_DLL_URL%"
	call :InstallLZMA "%GCC_CORE_DLL_LZ%"

	call :Download "%GCC_CORE_BIN_LZ%" "%GCC_CORE_BIN_URL%"
	call :InstallLZMA "%GCC_CORE_BIN_LZ%"

	call :Download "%MINGWRTDEV_LZ%" "%MINGWRTDEV_URL%"
	call :InstallLZMA "%MINGWRTDEV_LZ%"

	call :Download "%MINGWRTDLL_LZ%" "%MINGWRTDLL_URL%"
	call :InstallLZMA "%MINGWRTDLL_LZ%"

	call :Download "%W32API_LZ%" "%W32API_URL%"
	call :InstallLZMA "%W32API_LZ%"

	call :Download "%ZLIB_LZ%" "%ZLIB_URL%"
	call :InstallLZMA "%ZLIB_LZ%"

	call :Download "%GETTEXT_LZ%" "%GETTEXT_URL%"
	call :InstallLZMA "%GETTEXT_LZ%"

	REM Seems '+' in the filename screws things up for script function calls :(
	REM call :Download "%GCC_CPP_DEV_LZ%" "%GCC_CPP_DEV_URL%"
	REM call :InstallLZMA "%GCC_CPP_DEV_LZ%"

	REM call :Download "%GCC_CPP_DLL_LZ%" "%GCC_CPP_DLL_URL%"
	REM call :InstallLZMA "%GCC_CPP_DLL_LZ%"

	REM call :Download "%GCC_CPP_BIN_LZ%" "%GCC_CPP_BIN_URL%"
	REM call :InstallLZMA "%GCC_CPP_BIN_LZ%"

	REM Do it every time...
	wget http://sourceforge.net/projects/mingw/files/MinGW/Base/gcc/Version4/gcc-4.8.1-4/gcc-c++-4.8.1-4-mingw32-bin.tar.lzma/download
	wget http://sourceforge.net/projects/mingw/files/MinGW/Base/gcc/Version4/gcc-4.8.1-4/gcc-c++-4.8.1-4-mingw32-dev.tar.lzma/download
	wget http://sourceforge.net/projects/mingw/files/MinGW/Base/gcc/Version4/gcc-4.8.1-4/gcc-c++-4.8.1-4-mingw32-dll.tar.lzma/download
	7z e gcc-c++-4.8.1-4-mingw32-bin.tar.lzma
	7z e gcc-c++-4.8.1-4-mingw32-dev.tar.lzma
	7z e gcc-c++-4.8.1-4-mingw32-dll.tar.lzma
	7z x gcc-c++-4.8.1-4-mingw32-bin.tar -o"%MinGW%" -y
	7z x gcc-c++-4.8.1-4-mingw32-dev.tar -o"%MinGW%" -y
	7z x gcc-c++-4.8.1-4-mingw32-dll.tar -o"%MinGW%" -y
)

echo =+=+=
echo Checking MSYS...
echo =+=+=
set MSYS_EXE=MSYS-1.0.11.exe
if not exist "%SystemDrive%\msys" (
	if not exist %MSYS_EXE% (
		wget http://downloads.sourceforge.net/mingw/%MSYS_EXE%
	)
	if not [%WINELOADERNOEXEC%]==[] (
		echo Running under WINE - Requires MSYS install fixes: run msys-pi-wine.sh when msys install halts..."
		echo Ctrl-C to stop and then rerun the installation if necessary
	)
	%MSYS_EXE% /sp- /silent
	if ERRORLEVEL 1 goto Error
)

::
echo =+=+=
echo Checking gtk+-bundle...
echo =+=+=
set GTK_ZIP=gtk+-bundle_2.24.10-20120208_win32.zip
if not exist "%MINGW_BIN%\gtk-update-icon-cache.exe" (
	call :Download %GTK_ZIP% http://ftp.gnome.org/pub/gnome/binaries/win32/gtk+/2.24/%GTK_ZIP%
	7z x %GTK_ZIP% -o"%MinGW%" -y
	if ERRORLEVEL 1 goto Error
)

set EXPAT_ZIP=expat-dev_2.0.1-1_win32.zip
echo =+=+=
echo Checking expat-dev...
echo =+=+=
if not exist "%MINGW%\include\expat.h" (
	call :Download  %EXPAT_ZIP% http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/%EXPAT_ZIP%
	7z x %EXPAT_ZIP% -o"%MinGW%"
	if ERRORLEVEL 1 goto Error
)

::set GTT_ZIP=gettext-tools-dev_0.18.1.1-2_win32.zip
::echo =+=+=
::echo Checking gettext-tools-dev...
::echo =+=+=
::if not exist "%MINGW_BIN%\libgettextlib-0-18-1.dll" (
::	if not exist %GTT_ZIP% (
::		wget http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/%GTT_ZIP%
::	)
::	7z x -y %GTT_ZIP% -o"%MinGW%"
::	if ERRORLEVEL 1 goto Error
::)

echo =+=+=
echo Checking intltool...
echo =+=+=
set INTLTOOL_ZIP=intltool_0.40.4-1_win32.zip
if not exist "%MINGW_BIN%\intltoolize" (
	call :Download %INTLTOOL_ZIP% http://ftp.acc.umu.se/pub/GNOME/binaries/win32/intltool/0.40/%INTLTOOL_ZIP%
	7z x %INTLTOOL_ZIP% -o"%MinGW%"
	if ERRORLEVEL 1 goto Error
)

echo =+=+=
echo Checking iconv...
echo =+=+=
set ICONV_ZIP=libiconv-1.9.2-1-bin.zip
if not exist "%MINGW_BIN%\iconv.exe" (
	call :Download %ICONV_ZIP% http://sourceforge.net/projects/gnuwin32/files/libiconv/1.9.2-1/%ICONV_ZIP%
	7z x -y %ICONV_ZIP% -o"%MinGW%"
	if ERRORLEVEL 1 goto Error
)

echo =+=+=
echo Checking libintl...
echo =+=+=
:: Needed by iconv
set LIBINTL_ZIP=libintl-0.14.4-bin.zip
if not exist "%MINGW_BIN%\libintl3.dll" (
	call :Download %LIBINTL_ZIP% http://sourceforge.net/projects/gnuwin32/files/libintl/0.14.4/%LIBINTL_ZIP%
	7z x -y %LIBINTL_ZIP% -o"%MinGW%"
	if ERRORLEVEL 1 goto Error
)

echo =+=+=
echo Checking gettext tools...
echo =+=+=
:: Needed by intltool
set GETTEXT_BIN_LZ=gettext-0.18.3.1-1-mingw32-bin.tar.lzma
set GETTEXT_BIN_URL=%MINGW_BASE_URL%/gettext/gettext-0.18.3.1-1/%GETTEXT_BIN_LZ%/download
if not exist "%MINGW_BIN%\xgettext.exe" (
	call :Download %GETTEXT_BIN_LZ% %GETTEXT_BIN_URL%
	call :InstallLZMA "%GETTEXT_BIN_LZ%"
)

echo =+=+=
echo Checking libcurl...
echo =+=+=
REM Win32 - Generic - http://curl.haxx.se/download.html
set CURL=curl-7.34.0-devel-mingw32
set CURL_ZIP=%CURL%.zip
if not exist "%MINGW_BIN%\libcurl.dll" (
	call :Download %CURL_ZIP% http://curl.haxx.se/gknw.net/7.34.0/dist-w32/%CURL_ZIP%
	echo Extracting libcurl...
	7z x -y %CURL_ZIP% -o"libcurl"
	if ERRORLEVEL 1 goto Error
	@echo ON
	xcopy /S /Y libcurl\%CURL%\include\*.* "%MinGW%\include"
	xcopy /S /Y libcurl\%CURL%\bin\*.* "%MinGW_BIN%"
	xcopy /S /Y libcurl\%CURL%\lib\*.a "%MinGW%\lib"
	copy /Y libcurl\%CURL%\COPYING "%MinGW%\COPYING_curl.txt"
	rmdir /S /Q libcurl
	@echo OFF
)

echo =+=+=
echo Checking libexif...
echo =+=+=
set EXIF=libexif-0.6.21.1_winxp_mingw
set EXIF_7Z=%EXIF%.7z
if not exist "%MINGW_BIN%\libexif-12.dll" (
	:: Space in URL so function call doesn't work ATM
	::call :Download %EXIF_7Z% "http://sourceforge.net/projects/maille/files/Extern libs/%EXIF_7Z%/download"
	if not exist "%EXIF_7Z%" (
		wget "http://sourceforge.net/projects/maille/files/Extern libs/%EXIF_7Z%/download"
		if ERRORLEVEL 1 goto Error
	)

	echo Extracting libexif...
	7z x %EXIF_7Z%
	if ERRORLEVEL 1 goto Error

	echo Using xcopy to get all subdirs of libexif into place...
	@echo ON
	xcopy /Y /S %EXIF%\*.* "%MinGW%"
	rmdir /S /Q %EXIF%
	@echo OFF
)

echo =+=+=
echo Checking libstdc++...
echo =+=+=
set STDCPP_TAR=gcc-c++-4.8.1-4-mingw32-dll.tar
set STDCPP_LZ=%STDCPP_TAR%.lzma
if not exist "%MINGW_BIN%\libstdc++-6.dll" (
  call :Download "%STDCPP_LZ%" "http://sourceforge.net/projects/mingw/files/MinGW/Base/gcc/Version4/gcc-4.8.1-4/%STDCPP_LZ%/download"
  call :InstallLZMA "%STDCPP_LZ%"
)

echo =+=+=
echo Checking libbz2 header...
echo =+=+=
set BZ2_TAR=bzip2-1.0.6-4-mingw32-dev.tar
set BZ2_LZ=%BZ2_TAR%.lzma
if not exist "%MINGW%\include\bzlib.h" (
	call :Download %BZ2_LZ% "http://sourceforge.net/projects/mingw/files/MinGW/Extension/bzip2/bzip2-1.0.6-4/%BZ2_LZ%"
	call :InstallLZMA "%BZ2_LZ%"
)

echo =+=+=
echo Checking libbz2...
echo =+=+=
set BZ2DLL_TAR=libbz2-1.0.6-4-mingw32-dll-2.tar
set BZ2DLL_LZ=%BZ2DLL_TAR%.lzma
if not exist "%MINGW_BIN%\libbz2-2.dll" (
	call :Download %BZ2DLL_LZ% "http://sourceforge.net/projects/mingw/files/MinGW/Extension/bzip2/bzip2-1.0.6-4/%BZ2DLL_LZ%"
	call :InstallLZMA "%BZ2DLL_LZ%"
)

echo =+=+=
echo Checking magic dev...
echo =+=+=
set MAGIC_ZIP=file-5.03-lib.zip
if not exist "%MINGW%\include\magic.h" (
	call :Download %MAGIC_ZIP% http://downloads.sourceforge.net/gnuwin32/%MAGIC_ZIP%
	7z x %MAGIC_ZIP% -o"%MinGW%"
	if ERRORLEVEL 1 goto Error
)

echo =+=+=
echo Checking magic DLL...
echo =+=+=
set MAGICDLL_ZIP=file-5.03-bin.zip
if not exist "%MINGW_BIN%\magic1.dll" (
	call :Download %MAGICDLL_ZIP% http://downloads.sourceforge.net/gnuwin32/%MAGICDLL_ZIP%
	7z x %MAGICDLL_ZIP% -o"%MinGW%"
	if ERRORLEVEL 1 goto Error
)

echo =+=+=
echo Checking regex DLL (required by magic)...
echo =+=+=
set REGDLL_ZIP=regex-2.7-bin.zip
if not exist "%MINGW_BIN%\regex2.dll" (
	call :Download %REGDLL_ZIP% http://downloads.sourceforge.net/gnuwin32/%REGDLL_ZIP%
	7z x %REGDLL_ZIP% -o"%MinGW%"
	if ERRORLEVEL 1 goto Error
)

echo =+=+=
echo Checking SQLite dev...
echo =+=+=
set SQL_ZIP=sqlite-amalgamation-3080002.zip
if not exist "%MINGW%\include\sqlite3.h" (
	call :Download %SQL_ZIP% http://www.sqlite.org/2013/%SQL_ZIP%
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
	call :Download %SQLDLL_ZIP% http://www.sqlite.org/2013/%SQLDLL_ZIP%
	7z x %SQLDLL_ZIP% -o"%MinGW_BIN%"
	if ERRORLEVEL 1 goto Error
	REM Annoyingly SQL doesn't come with a .lib file so have to generate it ourselves:
	REM Possibly need to insert the line 'LIBRARY sqlite3.dll' at the beginning of the def file?
	REM  but this may not be needed as the --dllname option may suffice
	set PATH=%PATH%;%MinGW_BIN%
	dlltool.exe -d %MinGW_BIN%\sqlite3.def --dllname %MinGW_BIN%\sqlite3.dll -l %MinGW%\lib\sqlite3.lib
	if ERRORLEVEL 1 goto Error
)


echo =+=+=
echo Mapnik...
echo =+=+=
set MAPNIK_ZIP=mapnik-win-sdk-v2.2.0.zip
set MAPNIK_URL=http://mapnik.s3.amazonaws.com/dist/v2.2.0/%MAPNIK_ZIP%
if not exist "%MINGW_BIN%\mapnik.dll" (
	call :Download "%MAPNIK_ZIP%" "%MAPNIK_URL%"
	7z x %MAPNIK_ZIP%
	if ERRORLEVEL 1 goto Error
	copy /Y mapnik-v2.2.0\include\* "%MinGW%\include"
	copy /Y mapnik-v2.2.0\lib\*.lib "%MinGW%\lib\"
	copy /Y mapnik-v2.2.0\lib\*.dll "%MinGW%\bin"
	copy /Y mapnik-v2.2.0\lib\mapnik\input "\"
	rmdir /S /Q mapnik-v2.2.0
	REM Mapnik 2.2.0 seems to ship with a unicode copy which is missing ptypes.h
	REM Copy headers from a known good version...
	REM See fix in calling shell script when using wine
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
	call :Download %GNOME_DOC_ZIP% http://ftp.gnome.org/pub/gnome/binaries/win32/gnome-doc-utils/0.12/%GNOME_DOC_ZIP%
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
	call :Download %XLST_ZIP% ftp://ftp.zlatkovic.com/libxml/%XLST_ZIP%
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
	call :Download %XML2_ZIP% ftp://ftp.zlatkovic.com/libxml/%XML2_ZIP%
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
	call :Download %ICONV_ZIP% ftp://ftp.zlatkovic.com/libxml/%ICONV_ZIP%
	echo Extracting iconv...
	7z x %ICONV_ZIP%
	xcopy /Y /S "%ICONV%\bin\*" "%MinGW_BIN%"
	if ERRORLEVEL 1 goto Error
	rmdir /Q /S %ICONV%
)

:: Note GPSBabel can not be directly downloaded via wget
:: ATM get it manually from here:
::   http://www.gpsbabel.org/download.html
set GPSBABEL_INST=GPSBabel-1.5.2-Setup.exe
if not exist %GPSBABEL_INST% (
	echo Required %GPSBABEL_INST% not found. Exitting
	exit /b
)

echo =+=+=
echo Checking Perl Installation...
echo =+=+=
set PERL_MSI=ActivePerl-5.18.2.1802-MSWin32-x86-64int-298023.msi
if not exist "%SystemDrive%\Perl" (
	call :Download %PERL_MSI% http://downloads.activestate.com/ActivePerl/releases/5.18.2.1802/%PERL_MSI%
	echo Installing Perl takes a little time...
	msiexec /qb /i %PERL_MSI% PERL_PATH=Yes PERL_EXT=Yes
	if ERRORLEVEL 1 goto Error
)

echo =+=+=
echo Checking NSIS installed...
echo =+=+=
set NSIS_INST=nsis-2.46-setup.exe
if not exist "%ProgramFiles%\NSIS" (
	call :Download %NSIS_INST% http://prdownloads.sourceforge.net/nsis/nsis-2.46-setup.exe?download
	echo Installing NSIS...
	%NSIS_INST% /S
	if ERRORLEVEL 1 goto Error
)

echo =+=+=
echo Checking NSIS Plugins installed...
echo =+=+=
set FPDLLZIP=FindProc.zip
if not exist "%ProgramFiles%\NSIS\Plugins\FindProcDLL.dll" (
	call :Download %FPDLLZIP% http://nsis.sourceforge.net/mediawiki/images/3/3c/%FPDLLZIP%
	echo Extracting NSIS Plugins...
	7z e %FPDLLZIP% -o"%ProgramFiles%\NSIS\Plugins"
	if ERRORLEVEL 1 goto Error
)

popd

echo Fixing Perl reference

set PATH=%PATH%;C:\msys\1.0\bin

call :FixPerlRef %MINGW_BIN%\glib-mkenums s:/bin/perl:perl:
call :FixPerlRef %MINGW_BIN%\intltool-extract s:/opt/perl/bin/perl:perl:
call :FixPerlRef %MINGW_BIN%\intltool-merge s:/opt/perl/bin/perl:perl:
call :FixPerlRef %MINGW_BIN%\intltool-prepare s:/opt/perl/bin/perl:perl:
call :FixPerlRef %MINGW_BIN%\intltool-update s:/opt/perl/bin/perl:perl:

goto End

:FixPerlRef
:: Param %1 = File
:: Param %2 = sed command
:: Sadly '-i' for in place changes doesn't seem available with Windows sed 3.02
sed -e '%2' %1 > %1.tmp
if ERRORLEVEL 1 goto Error
xcopy /Y %1.tmp %1
if ERRORLEVEL 1 goto Error
del %1.tmp
:: End function
exit /b

:: Potentially Clean Up
:: If any parameters given on the command line then remove all downloaded items
:Clean
if not [%1]==[] (
	echo Removing downloaded files
	if exist cache rmdir /S /Q cache
)

goto End


::Function to try to download something via wget
:: (obviously needs 7zip to be installed first and available on the path!)
:: Param %1 = File
:: Param %2 = URL (which should retrieve %1) [ URL can't contain a space or %20:( ]
:Download
if not exist "%1" (
	wget "%2"
	if ERRORLEVEL 1 goto Error
)
:: End function
exit /b

::Function to install something via 7zip
:: (obviously needs 7zip to be installed first and available on the path!)
:: Param %1 = LZMA file
:: Param %2 = Internal file (normally the .tar file)
:InstallBy7Zip
echo Extracting "%1" from "%2"
7z e "%1"
if ERRORLEVEL 1 goto Error
7z x "%2" -o"%MinGW%"
if ERRORLEVEL 1 goto Error
del "%2"
if ERRORLEVEL 1 goto Error
:: End function
exit /b

::Function to install LZMA file containing a tar file (via 7zip)
:: (obviously needs 7zip to be installed first and available on the path!)
:: Param %1 = LZMA file
:InstallLZMA
7z e "%1"
if ERRORLEVEL 1 goto Error
set param=%1
:: Remove the .lzma extension to get the tar file
set file=%param:.lzma=%
7z x "%file%" -o"%MinGW%" -y
if ERRORLEVEL 1 goto Error
del "%file%"
if ERRORLEVEL 1 goto Error
:: End function
exit /b

:Error
echo exitting due to error: %ERRORLEVEL%
exit /b

:End
echo Finished
