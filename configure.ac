#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.64)
AC_INIT(viking, 1.8, , viking, http://viking.sf.net/)

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([dist-bzip2 dist-zip subdir-objects])
dnl AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([src/config.h])

# check for gtk-doc
m4_ifdef([GTK_DOC_CHECK], [
GTK_DOC_CHECK([1.0],[--flavour no-tmpl])
],[
AM_CONDITIONAL([ENABLE_GTK_DOC], false)
])

AC_PROG_CC
AC_PROG_CC_STDC
# Checks for programs.
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PATH_PROG(GLIB_MKENUMS, glib-mkenums)

ac_mingw32=no
case $target_os in
  *mingw32*)
    ac_mingw32=yes
  ;;
  *)
  ;;
esac

# I18N
GETTEXT_PACKAGE=viking
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",[Gettext package name])

AM_GLIB_DEFINE_LOCALEDIR(LOCALEDIR)

AM_GNU_GETTEXT_VERSION([0.17])
AM_GNU_GETTEXT([external])

IT_PROG_INTLTOOL([0.35.0])

AC_SUBST([CONFIG_STATUS_DEPENDENCIES],['$(top_srcdir)/po/LINGUAS'])

dnl This will cause the automake generated makefiles to pass the
dnl correct flags to aclocal.
ACLOCAL_AMFLAGS="\${ACLOCAL_FLAGS}"
AC_SUBST(ACLOCAL_AMFLAGS)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([malloc.h stdlib.h string.h sys/param.h sys/types.h unistd.h math.h utime.h X11/Xlib.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_MODE_T

# Checks for library functions or symbols
AC_CHECK_FUNCS([floor memset mkdtemp pow realpath sqrt strcasecmp strchr strncasecmp strtol strtoul strptime fmemopen])
AC_CHECK_LIB(m, tan)
AC_CHECK_LIB(z, inflate)
AC_CHECK_LIB(X11, XSetErrorHandler)

# ATM not running automake under Windows
#AC_CHECK_PROG(USE_WINDOWS, cmd.exe, yes, no)
#AM_CONDITIONAL([WINDOWS], [test x$USE_WINDOWS = xyes])
# So pass in as an option instead
# This is only to perform slightly different build steps
#  (the code uses standard WINDOWS defines for any specific different behaviour)
AC_ARG_ENABLE(windows, AC_HELP_STRING([--enable-windows],
              [Perform specific Windows build steps (default is no)]),
              [ac_cv_enable_windows=$enableval],
              [ac_cv_enable_windows=no])
AC_CACHE_CHECK([whether to enable Windows build steps],
               [ac_cv_enable_windows], [ac_cv_enable_windows=no])
AM_CONDITIONAL([WINDOWS], [test x$ac_cv_enable_windows = xyes])

# Expat
AM_WITH_EXPAT

# Curl
LIBCURL_CHECK_CONFIG([yes],[],[],[AC_MSG_ERROR([libcurl is needed but not found])])

# gdk-pixbuf-csource
AC_CHECK_PROG([GDK_PIXBUF_CSOURCE],gdk-pixbuf-csource,[yes])
if test "x$GDK_PIXBUF_CSOURCE" != "xyes"
then
  AC_MSG_ERROR([gdk-pixbuf-csource is needed but not found])
fi

dnl  ------------------
dnl | Pkgconfig checks |---------------------------------------
dnl  ------------------

PKG_CHECK_MODULES(PACKAGE, [
        glib-2.0          >= 2.38
        gthread-2.0       >= 2.2
        gtk+-2.0          >= 2.18
        gio-2.0           >= 2.12
])

AC_SUBST(PACKAGE_CFLAGS)
AC_SUBST(PACKAGE_LIBS)

dnl  -------------
dnl | User Manual |---------------------------------------
dnl  -------------
if test "x$ac_cv_enable_windows" = "xno"; then
  YELP_HELP_INIT
fi

dnl ---------------------------------------------------------------------------
dnl - Use deprecated options (default enabled for devs, disabled in releases) 
dnl ---------------------------------------------------------------------------

AC_ARG_ENABLE(deprecations,
              [AC_HELP_STRING([--enable-deprecations],
                              [warn about deprecated usages [default=no]])],
                              [ac_cv_enable_deprecations=$enableval],
                              [ac_cv_enable_deprecations=no])
AC_CACHE_CHECK([whether to enable deprecated features],
               [ac_cv_enable_deprecations], [ac_cv_enable_deprecations=no])

if test "x$ac_cv_enable_deprecations" = "xyes"; then
   DISABLE_DEPRECATED_CFLAGS="\
-DG_DISABLE_SINGLE_INCLUDES \
-DGSEAL_ENABLE \
-DG_DISABLE_DEPRECATED \
-DGDK_DISABLE_DEPRECATED \
-DGDK_PIXBUF_DISABLE_DEPRECATED \
-DGTK_DISABLE_SINGLE_INCLUDES \
-DGTK_DISABLE_DEPRECATED"
   CPPFLAGS="$CPPFLAGS $DISABLE_DEPRECATED_CFLAGS"
fi

# Options
AC_ARG_ENABLE(bing, AC_HELP_STRING([--enable-bing],
              [enable Bing stuff (default is enable)]),
              [ac_cv_enable_bing=$enableval],
              [ac_cv_enable_bing=yes])
AC_CACHE_CHECK([whether to enable Bing stuff],
               [ac_cv_enable_bing], [ac_cv_enable_bing=yes])
case $ac_cv_enable_bing in
  yes)
    AC_DEFINE(VIK_CONFIG_BING, [], [BING STUFF])
    ;;
esac
AM_CONDITIONAL([BING], [test x$ac_cv_enable_bing = xyes])

AC_ARG_ENABLE(google, AC_HELP_STRING([--enable-google],
              [enable Google stuff (default is enable)]),
              [ac_cv_enable_google=$enableval],
              [ac_cv_enable_google=yes])
AC_CACHE_CHECK([whether to enable Google stuff],
               [ac_cv_enable_google], [ac_cv_enable_google=yes])
case $ac_cv_enable_google in
  yes)
    AC_DEFINE(VIK_CONFIG_GOOGLE, [], [GOOGLE STUFF])
    ;;
esac
AM_CONDITIONAL([GOOGLE], [test x$ac_cv_enable_google = xyes])

AC_ARG_ENABLE(terraserver, AC_HELP_STRING([--enable-terraserver],
              [enable Terraserver stuff (default is disabled as the tileserver is no longer available)]),
              [ac_cv_enable_terraserver=$enableval],
              [ac_cv_enable_terraserver=no])
AC_CACHE_CHECK([whether to enable Terraserver stuff],
               [ac_cv_enable_terraserver], [ac_cv_enable_terraserver=yes])
case $ac_cv_enable_terraserver in
  yes)
    AC_DEFINE(VIK_CONFIG_TERRASERVER, [], [TERRASERVER STUFF])
    ;;
esac
AM_CONDITIONAL([TERRASERVER], [test x$ac_cv_enable_terraserver = xyes])

AC_ARG_ENABLE(expedia, AC_HELP_STRING([--enable-expedia],
              [enable Expedia stuff (default is disable)]),
              [ac_cv_enable_expedia=$enableval],
              [ac_cv_enable_expedia=no])
AC_CACHE_CHECK([whether to enable Expedia stuff],
               [ac_cv_enable_expedia], [ac_cv_enable_expedia=yes])
case $ac_cv_enable_expedia in
  yes)
    AC_DEFINE(VIK_CONFIG_EXPEDIA, [], [EXPEDIA STUFF])
    ;;
esac
AM_CONDITIONAL([EXPEDIA], [test x$ac_cv_enable_expedia = xyes])

# OpenStreetMap http://www.openstreetmap.org/
AC_ARG_ENABLE(openstreetmap, AC_HELP_STRING([--enable-openstreetmap],
              [enable OpenStreetMap stuff (default is enable)]),
              [ac_cv_enable_openstreetmap=$enableval],
              [ac_cv_enable_openstreetmap=yes])
AC_CACHE_CHECK([whether to enable OpenStreetMap stuff],
               [ac_cv_enable_openstreetmap], [ac_cv_enable_openstreetmap=yes])
case $ac_cv_enable_openstreetmap in
  yes)
    AC_DEFINE(VIK_CONFIG_OPENSTREETMAP, [], [OPENSTREETMAP STUFF])
    ;;
esac
AM_CONDITIONAL([OPENSTREETMAP], [test x$ac_cv_enable_openstreetmap = xyes])

# BlueMarble
AC_ARG_ENABLE(bluemarble, AC_HELP_STRING([--enable-bluemarble],
              [enable BlueMarble stuff (default is enable)]),
              [ac_cv_enable_bluemarble=$enableval],
              [ac_cv_enable_bluemarble=yes])
AC_CACHE_CHECK([whether to enable BlueMarble stuff],
               [ac_cv_enable_bluemarble], [ac_cv_enable_bluemarble=yes])
case $ac_cv_enable_bluemarble in
  yes)
    AC_DEFINE(VIK_CONFIG_BLUEMARBLE, [], [BLUEMARBLE STUFF])
    ;;
esac
AM_CONDITIONAL([BLUEMARBLE], [test x$ac_cv_enable_bluemarble = xyes])

# GeoNames http://www.geonames.org/
AC_ARG_ENABLE(geonames, AC_HELP_STRING([--enable-geonames],
              [enable Geonames stuff (default is enable)]),
              [ac_cv_enable_geonames=$enableval],
              [ac_cv_enable_geonames=yes])
AC_CACHE_CHECK([whether to enable Geonames stuff],
               [ac_cv_enable_geonames], [ac_cv_enable_geonames=yes])
case $ac_cv_enable_geonames in
  yes)
    AC_DEFINE(VIK_CONFIG_GEONAMES, [], [GEONAMES STUFF])
    ;;
esac
AM_CONDITIONAL([GEONAMES], [test x$ac_cv_enable_geonames = xyes])

AC_ARG_ENABLE(geocaches, AC_HELP_STRING([--enable-geocaches],
	      [enable Geocaches Acquire (default is disable).]),
              [ac_cv_enable_geocaches=$enableval],
              [ac_cv_enable_geocaches=no])
AC_CACHE_CHECK([whether to enable Geocaches Acquire],
               [ac_cv_enable_geocaches], [ac_cv_enable_geocaches=yes])
case $ac_cv_enable_geocaches in
  yes)
    AC_DEFINE(VIK_CONFIG_GEOCACHES, [], [GEOCACHES STUFF])
    ;;
esac
AM_CONDITIONAL([GEOCACHES], [test x$ac_cv_enable_geocaches = xyes])

# GeoClue
AC_ARG_ENABLE(geoclue, AC_HELP_STRING([--enable-geoclue],
              [enable GeoClue support (default is enable).]),
              [ac_cv_enable_geoclue=$enableval],
              [ac_cv_enable_geoclue=yes])
AC_CACHE_CHECK([whether to enable GeoClue Support],
               [ac_cv_enable_geoclue], [ac_cv_enable_geoclue=yes])
case $ac_cv_enable_geoclue in
  yes)
    PKG_CHECK_MODULES( [LIBGEOCLUE], [libgeoclue-2.0 >= 2.4.4],
                       AC_CHECK_LIB(geoclue-2, main,, AC_MSG_ERROR([libgeoclue is needed but not found - you will need to install package 'libgeoclue-2-dev' or similar ])) )
    ;;
esac
AM_CONDITIONAL([GEOCLUE], [test x$ac_cv_enable_geoclue = xyes])

# Geotagging
AC_ARG_WITH(libexif, AC_HELP_STRING([--with-libexif], [Force usage of libexif instead of libgexiv2]))
AC_ARG_ENABLE(geotag, AC_HELP_STRING([--enable-geotag],
	      [enable Geotag Support (default is enable).]),
              [ac_cv_enable_geotag=$enableval],
              [ac_cv_enable_geotag=yes])
AC_CACHE_CHECK([whether to enable Geotag Support],
               [ac_cv_enable_geotag], [ac_cv_enable_geotag=yes])
case $ac_cv_enable_geotag in
  yes)
    AS_IF([test x$with_libexif = xyes],
      AC_CHECK_HEADER([libexif/exif-data.h],[],AC_MSG_ERROR([exif-data.h is needed but not found - you will need to install package 'libexif-dev' or similar]))
      AC_CHECK_LIB(exif,exif_loader_new,, AC_MSG_ERROR([libexif is not found but it has been forcibly required])),
      # gexiv2.h relies on glib so a simple compile check fails.
      #AC_CHECK_HEADER([gexiv2/gexiv2.h],,AC_MSG_ERROR([Error msg...]))
      AC_CHECK_LIB(gexiv2,gexiv2_metadata_new,, AC_MSG_ERROR([libgexiv2 is needed but not found - you will need to install package 'libgexiv2-dev' or similar. The feature can be disabled with --disable-geotag]))
      )
    AC_DEFINE(VIK_CONFIG_GEOTAG, [], [GEOTAG STUFF])
    ;;
esac
AM_CONDITIONAL([GEOTAG], [test x$ac_cv_enable_geotag = xyes])
AM_CONDITIONAL([GEXIV2], [test x$ac_cv_lib_gexiv2_gexiv2_metadata_new = xyes] )
# Tested with gexiv2 0.10.2, but probably would work with older versions; may be all of them...
#AM_COND_IF([GEXIV2], [PKG_CHECK_MODULES([GEXIV2], [gexiv2 >= 0.6.1])])
AM_CONDITIONAL([LIBEXIF], [test x$ac_cv_lib_exif_exif_loader_new = xyes] )

AC_ARG_ENABLE(dem24k, AC_HELP_STRING([--enable-dem24k],
	      [enable USGS 24k DEM (default is disable) download source. Requires dem24k.pl script in path.]),
              [ac_cv_enable_dem24k=$enableval],
              [ac_cv_enable_dem24k=no])
AC_CACHE_CHECK([whether to enable USGS DEM 24k download source],
               [ac_cv_enable_dem24k], [ac_cv_enable_dem24k=yes])
case $ac_cv_enable_dem24k in
  yes)
    AC_DEFINE(VIK_CONFIG_DEM24K, [], [DEM24K STUFF])
    ;;
esac
AM_CONDITIONAL([DEM24K], [test x$ac_cv_enable_dem24k = xyes])

# OAuth
AC_ARG_ENABLE(oauth, AC_HELP_STRING([--enable-oauth],
              [enable OAuth Support (default is enable).]),
              [ac_cv_enable_oauth=$enableval],
              [ac_cv_enable_oauth=yes])
AC_CACHE_CHECK([whether to enable OAuth Support],
               [ac_cv_enable_oauth], [ac_cv_enable_oauth=yes])
case $ac_cv_enable_oauth in
  yes)
    AC_CHECK_HEADERS([oauth.h],[],[AC_MSG_ERROR([oauth.h is needed but not found - you will need to install package 'liboauth-dev' or similar. The feature can be disabled with --disable-oauth])])
    AC_CHECK_LIB(oauth, oauth_sign_url2, [], [AC_MSG_ERROR([liboauth is needed but not found.])])
    ;;
esac
AM_CONDITIONAL([OAUTH], [test x$ac_cv_enable_oauth = xyes])

# Realtime GPS tracking
AC_ARG_ENABLE(realtime-gps-tracking, AC_HELP_STRING([--enable-realtime-gps-tracking],
              [enable realtime GPS tracking (default is enable)]),
              [ac_cv_enable_realtimegpstracking=$enableval],
              [ac_cv_enable_realtimegpstracking=yes])
AC_CACHE_CHECK([whether to enable Realtime GPS Tracking stuff],
               [ac_cv_enable_realtimegpstracking], [ac_cv_enable_realtimegpstracking=yes])
case $ac_cv_enable_realtimegpstracking in
  yes)
    AC_CHECK_HEADERS([gps.h],[],[AC_MSG_ERROR([gps.h is needed but not found - you will need to install package 'libgps-dev' or similar. The feature can be disabled with --disable-realtime-gps-tracking])])
    AC_CHECK_LIB(gps, gps_close, [],
      if test "$ac_mingw32" = "yes"; then
       [LIBS="-lgps $LIBS"
        AC_MSG_WARN([libgps not found! Forcing it anyway!])]
      else
        AC_MSG_ERROR([libgps is needed but not found.])
      fi)
    AC_DEFINE(VIK_CONFIG_REALTIME_GPS_TRACKING, [], [REALTIME GPS TRACKING STUFF])
    ;;
esac
AM_CONDITIONAL([REALTIME_GPS_TRACKING], [test x$ac_cv_enable_realtimegpstracking = xyes])

# BZIP2
AC_ARG_ENABLE(bzip2, AC_HELP_STRING([--enable-bzip2],
	      [enable bzip2 Support (default is enable).]),
              [ac_cv_enable_bzip2=$enableval],
              [ac_cv_enable_bzip2=yes])
AC_CACHE_CHECK([whether to enable bzip2 Support],
               [ac_cv_enable_bzip2], [ac_cv_enable_bzip2=yes])
case $ac_cv_enable_bzip2 in
  yes)
    AC_CHECK_HEADERS([bzlib.h],[],[AC_MSG_ERROR([bzlib.h is needed but not found - you will need to install package 'libbz2-dev' or similar. The feature can be disabled with --disable-bzip2])])
    if test "$ac_mingw32" = "yes"; then
      # Using the cross compiler it bizarrely fails to detect BZ2_bzRead during the configure stage
      # I SWEAR THIS WORKED AT SOME POINT - BUT ON A DIFFERENT DAY IT DOESN'T = MORE SWEARING
      BZ2FUNC=main
      AC_CHECK_LIB(bz2, [$BZ2FUNC], [], [LIBS="-lbz2 $LIBS"
                                         AC_MSG_WARN([libbz2 not found! Forcing it anyway!])])
    else
      BZ2FUNC=BZ2_bzRead
      AC_CHECK_LIB(bz2, [$BZ2FUNC], [], [AC_MSG_ERROR([libbz2 is needed but not found.])])
    fi
    ;;
esac
AM_CONDITIONAL([BZIP2], [test x$ac_cv_enable_bzip2 = xyes])

# FILE MAGIC
AC_ARG_ENABLE(magic, AC_HELP_STRING([--enable-magic],
	      [enable File Magic support via libmagic (default is enable).]),
              [ac_cv_enable_magic=$enableval],
              [ac_cv_enable_magic=yes])
AC_CACHE_CHECK([whether to enable Magic Support],
               [ac_cv_enable_magic], [ac_cv_enable_magic=yes])
case $ac_cv_enable_magic in
  yes)
    AC_CHECK_HEADERS([magic.h],[],[AC_MSG_ERROR([magic.h is needed but not found - you will need to install package 'libmagic-dev' or similar. The feature can be disabled with --disable-magic])])
    # Using the cross compiler it fails to find libmagic.dll during the configure stage
    AC_CHECK_LIB(magic, magic_open, [],
      if test "$ac_mingw32" = "yes"; then
        [LIBS="-lmagic $LIBS"
         AC_MSG_WARN([libmagic not found! Forcing it anyway!])]
      else
         AC_MSG_ERROR([libmagic is needed but not found.])
      fi)
    ;;
esac
AM_CONDITIONAL([MAGIC], [test x$ac_cv_enable_magic = xyes])

# SQL for MBTiles
AC_ARG_ENABLE(mbtiles, AC_HELP_STRING([--enable-mbtiles],
              [enable MBTiles support via libsqlite3 (default is enable).]),
              [ac_cv_enable_mbtiles=$enableval],
              [ac_cv_enable_mbtiles=yes])
AC_CACHE_CHECK([whether to enable MBTiles Support],
               [ac_cv_enable_mbtiles], [ac_cv_enable_mbtiles=yes])
case $ac_cv_enable_mbtiles in
  yes)
    AC_CHECK_HEADERS([sqlite3.h],[],[AC_MSG_ERROR([sqlite3.h is needed but not found - you will need to install package 'libsqlite3-dev' or similar. The feature can be disabled with --disable-mbtiles])])
    AC_CHECK_LIB(sqlite3, sqlite3_open, [],
      if test "$ac_mingw32" = "yes"; then
        [LIBS="-lsqlite3 $LIBS"
         AC_MSG_WARN([libsqlite3 not found! Forcing it anyway!])]
      else
         AC_MSG_ERROR([libsqlite3 is needed but not found.])
      fi)
    ;;
esac
AM_CONDITIONAL([SQLITE], [test x$ac_cv_enable_mbtiles = xyes])

# Standard compression is handled by libz
# libzip enables a friendlier file based interface
# libzip itself depends on libz (which is required in the Viking build ATM)
# ZIP
AC_ARG_ENABLE(zip, AC_HELP_STRING([--enable-zip],
              [enable zip support (default is enable).]),
              [ac_cv_enable_zip=$enableval],
              [ac_cv_enable_zip=yes])
AC_CACHE_CHECK([whether to enable zip Support],
               [ac_cv_enable_zip], [ac_cv_enable_zip=yes])
case $ac_cv_enable_zip in
  yes)
    # NB As far as I can tell libzip doesn't provide any versioning info within the header code itself
    # Thus supporting old versions via simple #if #else #endif directives is seemingly not possible
    # Resort to checking versioning only via pkgconfig
    PKG_CHECK_MODULES([LIBZIP], [libzip >= 0.11],
      [ AC_CHECK_HEADERS([zip.h],[],[AC_MSG_ERROR([zip.h is needed but not found - you will need to install package 'libzip-dev' or similar. The feature can be disabled with --disable-zip])])
        AC_CHECK_LIB(zip, [main], [],
          if test "$ac_mingw32" = "yes"; then
            [LIBS="-lzip $LIBS"
             AC_MSG_WARN([libzip not found! Forcing it anyway!])]
          else
             AC_MSG_ERROR([libzip is needed but not found.])
          fi)],
      [ AC_MSG_WARN([libzip version needs to be at least 0.11, use of libzip is disabled])
        ac_cv_enable_zip=no ]
  )
    ;;
esac
AM_CONDITIONAL([ZIP], [test x$ac_cv_enable_zip = xyes])

# ATM only for MD5 Hashing which is currently only used for filename of image thumbnails
AC_ARG_ENABLE(nettle, AC_HELP_STRING([--enable-nettle],
              [enable MD5 Hash support (default is enable)]),
              [ac_cv_enable_nettle=$enableval],
              [ac_cv_enable_nettle=yes])
AC_CACHE_CHECK([whether to enable MD5 Hash support],
               [ac_cv_enable_nettle], [ac_cv_enable_nettle=yes])
case $ac_cv_enable_nettle in
  yes)
    AC_CHECK_HEADER([nettle/md5-compat.h],[],[AC_MSG_ERROR([nettle/md5-compat.h is needed but not found - you will need to install package 'nettle-dev' or similar. The feature can be disabled with --disable-nettle])])
    AC_CHECK_LIB([nettle], [main], [],
      if test "$ac_mingw32" = "yes"; then
        [LIBS="-lnettle $LIBS"
         AC_MSG_WARN([libnettle not found! Forcing it anyway!])]
      else
         AC_MSG_ERROR([libnettle is needed but not found.])
      fi)
    ;;
esac
AM_CONDITIONAL([MD5_HASH], [test x$ac_cv_enable_nettle = xyes])

# Mapnik rendering layer
AC_ARG_ENABLE(mapnik, AC_HELP_STRING([--enable-mapnik],
              [enable Mapnik (default is enable)]),
              [ac_cv_enable_mapnik=$enableval],
              [ac_cv_enable_mapnik=yes])
AC_CACHE_CHECK([whether to enable Mapnik],
               [ac_cv_enable_mapnik], [ac_cv_enable_mapnik=yes])
case $ac_cv_enable_mapnik in
  yes)
    # C++
    AC_LANG_CPLUSPLUS
    AC_LANG_SAVE
    AC_PROG_CXX
    # Mapnik3 requires C++11. Viking will use C++11 as well.
    CXXFLAGS="$CXXFLAGS -std=c++11"
    # Too difficult to get check working under Windows (extra dependencies needed probably Boost)- so just skip it
    if test "x$ac_cv_enable_windows" = "xno"; then
      AC_CHECK_HEADER([mapnik/map.hpp],[],[AC_MSG_ERROR([mapnik/map.hpp is needed but not found - you will need to install package 'libmapnik-dev' or similar. The feature can be disabled with --disable-mapnik])])
    fi
    AC_CHECK_LIB([mapnik], [main], [], [AC_MSG_ERROR([libmapnik is needed but not found.])])
    AC_CHECK_LIB([stdc++], [main], [], [AC_MSG_ERROR([libstdc++ is needed but not found.])])
    # Required for Mapnik3 build (it's also in Mapnik2 but does not need to be specified for some reason)
    #  it's part of libmapnik install dependencies
    AC_CHECK_LIB([icuuc], [main], [], [AC_MSG_ERROR([libicuuc is needed but not found.])])
    AC_LANG_RESTORE
    ;;
esac
AM_CONDITIONAL([MAPNIK], [test x$ac_cv_enable_mapnik = xyes])
###

## WORK AROUND BROKEN autoconf - see http://lists.gnu.org/archive/html/bug-automake/2002-11/msg00020.html
m4_pushdef([AC_MSG_ERROR])
AC_PROG_CXX
m4_popdef([AC_MSG_ERROR])
##

dnl Compile time defined key
AC_ARG_WITH(thunderforest_apikey,
            [AC_HELP_STRING([--with-thunderforest_apikey],
	     [API Key for Thunderforest (OSM Cyclemap) tiles.
              Please register your own if you're going to distribute the
              package, as requests are limited per key.])],
	    [VIK_CONFIG_THUNDERFOREST_KEY="\"${withval}\""],
	    [VIK_CONFIG_THUNDERFOREST_KEY="\"7387c111d85642b18f63608bd4cb8b4f\""])
AC_DEFINE_UNQUOTED(VIK_CONFIG_THUNDERFOREST_KEY, ${VIK_CONFIG_THUNDERFOREST_KEY}, [Thunderforest key])

AC_ARG_WITH(geonames_username,
            [AC_HELP_STRING([--with-geonames_username],
	     [Username for the GeoNames webservice.
              Please register your own if you're going to distribute the
              package, as requests are limited per username.])],
	    [VIK_CONFIG_GEONAMES_USERNAME="\"${withval}\""],
	    [VIK_CONFIG_GEONAMES_USERNAME="\"$PACKAGE\""])
AC_DEFINE_UNQUOTED(VIK_CONFIG_GEONAMES_USERNAME, ${VIK_CONFIG_GEONAMES_USERNAME}, [geonames username])


AC_ARG_WITH(mapcache,
            [AC_HELP_STRING([--with-mapcache],
                            [specify the size of the map cache in MB (default is 256)])],
            [if test "x$withval" = "xno"; then
                VIK_CONFIG_MAPCACHE_SIZE=0;
             elif test "x$withval" = "xyes"; then
                AC_MSG_ERROR([Please, set a value for size of the map cache in MB])
             else
                VIK_CONFIG_MAPCACHE_SIZE=${withval}
             fi],
             [VIK_CONFIG_MAPCACHE_SIZE=256])
AC_DEFINE_UNQUOTED(VIK_CONFIG_MAPCACHE_SIZE, ${VIK_CONFIG_MAPCACHE_SIZE},
                   [Size of the map cache])


AC_ARG_WITH(tileage,
            [AC_HELP_STRING([--with-tileage],
                            [specify the age of a tile before checking it (default is 7 days)])],
            [if test "x$withval" = "xno"; then
                VIK_CONFIG_DEFAULT_TILE_AGE=0;
             elif test "x$withval" = "xyes"; then
                AC_MSG_ERROR([Please, set a value for age of tiles])
             else
                VIK_CONFIG_DEFAULT_TILE_AGE=${withval}
             fi],
             [VIK_CONFIG_DEFAULT_TILE_AGE=604800])
AC_DEFINE_UNQUOTED(VIK_CONFIG_DEFAULT_TILE_AGE, ${VIK_CONFIG_DEFAULT_TILE_AGE},
                   [Age of tiles before checking it (in seconds)])
AC_DEFINE(HAVE_VIKING, 1, [Enable Viking specifics in otherwise reusable code])

dnl man pages processing
AC_PATH_PROG(XSLTPROC,xsltproc)
#AM_CONDITIONAL([HAVE_XSLTPROC],[test "x$XSLTPROC" != "x"])

ISODATE="$(date --utc --date="@${SOURCE_DATE_EPOCH:-$(date +%s)}" +%Y-%m-%d)"
AC_SUBST(ISODATE)

AC_DEFINE_UNQUOTED(THEYEAR, "$(date --utc --date="@${SOURCE_DATE_EPOCH:-$(date +%s)}" +%Y)", [The Year])

# Configuration
AC_CONFIG_FILES([
		viking.spec
		mingw-viking.spec
		mingw64-viking.spec
		Makefile
		src/Makefile
		src/icons/Makefile
		po/Makefile.in
		data/Makefile
		tools/Makefile
		test/Makefile
		help/Makefile
		help/viking.xml
		windows/Makefile
		windows/installer/Makefile
		windows/installer/pixmaps/Makefile
		windows/installer/translations/Makefile
		doc/Makefile
		doc/reference/Makefile
		doc/examples/Makefile])

AC_OUTPUT

dnl Output the configuration summary
AC_MSG_NOTICE(["
===========================================
$PACKAGE $PACKAGE_URL $VERSION
-------------------------------------------
Bing Maps                        : $ac_cv_enable_bing
Google                           : $ac_cv_enable_google
Terraserver Maps                 : $ac_cv_enable_terraserver
Expedia Maps                     : $ac_cv_enable_expedia
Open Street Map                  : $ac_cv_enable_openstreetmap
BlueMarble                       : $ac_cv_enable_bluemarble
GeoClue Support                  : $ac_cv_enable_geoclue
Geonames                         : $ac_cv_enable_geonames
Geocaches Acquire                : $ac_cv_enable_geocaches
Geotag Support                   : $ac_cv_enable_geotag (libgexiv2=$ac_cv_lib_gexiv2_gexiv2_metadata_new libexif=$ac_cv_lib_exif_exif_loader_new)
USGS 24k DEM                     : $ac_cv_enable_dem24k
Realtime GPS Tracking            : $ac_cv_enable_realtimegpstracking
bzip2 Support                    : $ac_cv_enable_bzip2
File Magic Support               : $ac_cv_enable_magic
MBTiles Support (SQLite3)        : $ac_cv_enable_mbtiles
Zip File Support (with libzip)   : $ac_cv_enable_zip
MD5 Hash Support (with libnettle): $ac_cv_enable_nettle
Mapnik Rendering Support (C++)   : $ac_cv_enable_mapnik
OAuth                            : $ac_cv_enable_oauth
Size of map cache (in memory)    : ${VIK_CONFIG_MAPCACHE_SIZE}
Age of tiles (in seconds)        : ${VIK_CONFIG_DEFAULT_TILE_AGE}
GeoNames user                    : ${VIK_CONFIG_GEONAMES_USERNAME}
Documentation (+HTML)            : ${enable_gtk_doc} (HTML: ${enable_gtk_doc_html})
-------------------------------------------

Configure finished, type 'make' to build.
"])
