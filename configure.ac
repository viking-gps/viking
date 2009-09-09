#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(viking, 0.9.9)
AM_INIT_AUTOMAKE()
dnl AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([src/config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_RANLIB

# I18N
GETTEXT_PACKAGE=viking
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",[Gettext package name])

AM_GLIB_DEFINE_LOCALEDIR(LOCALEDIR)

AM_GLIB_GNU_GETTEXT
IT_PROG_INTLTOOL([0.35.0])

AC_SUBST([CONFIG_STATUS_DEPENDENCIES],['$(top_srcdir)/po/LINGUAS'])

dnl This will cause the automake generated makefiles to pass the
dnl correct flags to aclocal.
ACLOCAL_AMFLAGS="\${ACLOCAL_FLAGS}"
AC_SUBST(ACLOCAL_AMFLAGS)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([malloc.h netdb.h netinet/in.h stdlib.h string.h sys/param.h sys/socket.h sys/types.h sys/wait.h unistd.h math.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_MODE_T

# Checks for library functions.
AC_FUNC_STAT
AC_FUNC_STRTOD
AC_CHECK_FUNCS([floor gethostbyname memset mkdtemp pow realpath socket sqrt strcasecmp strchr strdup strncasecmp strtol strtoul])

# Expat
AM_WITH_EXPAT

# Curl
LIBCURL_CHECK_CONFIG([yes],[],[],[AC_MSG_ERROR([libcurl is needed but not found])])

# gdk-pixbuf-csource
AC_CHECK_PROG([GDK_PIXBUF_CSOURCE],gdk-pixbuf-csource,[yes])
if test $GDK_PIXBUF_CSOURCE != "yes"
then
  AC_MSG_ERROR([gdk-pixbuf-csource is needed but not found])
fi

dnl  ------------------
dnl | Pkgconfig checks |---------------------------------------
dnl  ------------------

PKG_CHECK_MODULES(PACKAGE, [
        glib-2.0          >= 2.12
        gthread-2.0       >= 2.2
        gtk+-2.0          >= 2.8
])

AC_SUBST(PACKAGE_CFLAGS)
AC_SUBST(PACKAGE_LIBS)


# Options
AC_ARG_ENABLE(alphabetized-trw, AC_HELP_STRING([--enable-alphabetized-trw],
              [enable alphabetized track & waypoints (default is enable)]),
              [ac_cv_enable_alpha_trw=$enableval],
              [ac_cv_enable_alpha_trw=yes])
AC_CACHE_CHECK([whether to enable alphabetized track & waypoint],
               [ac_cv_enable_alpha_trw], [ac_cv_enable_alpha_trw=yes])
case $ac_cv_enable_alpha_trw in
  yes)
    AC_DEFINE(VIK_CONFIG_ALPHABETIZED_TRW, [], [NO ALPHABETIZED TRW])
    ;;
esac

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
              [enable Terraserver stuff (default is enable)]),
              [ac_cv_enable_terraserver=$enableval],
              [ac_cv_enable_terraserver=yes])
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

# OpenAerial
AC_ARG_ENABLE(openaerial, AC_HELP_STRING([--enable-openaerial],
              [enable OpenAerial stuff (default is enable)]),
              [ac_cv_enable_openaerial=$enableval],
              [ac_cv_enable_openaerial=yes])
AC_CACHE_CHECK([whether to enable OpenAerial stuff],
               [ac_cv_enable_openaerial], [ac_cv_enable_openaerial=yes])
case $ac_cv_enable_openaerial in
  yes)
    AC_DEFINE(VIK_CONFIG_OPENAERIAL, [], [OPENAERIAL STUFF])
    ;;
esac
AM_CONDITIONAL([OPENAERIAL], [test x$ac_cv_enable_openaerial = xyes])

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

# Realtime GPS tracking
AC_ARG_ENABLE(realtime-gps-tracking, AC_HELP_STRING([--enable-realtime-gps-tracking],
              [enable realtime GPS tracking (default is enable)]),
              [ac_cv_enable_realtimegpstracking=$enableval],
              [ac_cv_enable_realtimegpstracking=yes])
AC_CACHE_CHECK([whether to enable Realtime GPS Tracking stuff],
               [ac_cv_enable_realtimegpstracking], [ac_cv_enable_realtimegpstracking=yes])
case $ac_cv_enable_realtimegpstracking in
  yes)
    AC_CHECK_LIB(gps,gps_poll,,AC_MSG_ERROR([libgps is needed for Realtime GPS Tracking feature[,] but not found. The feature can be disable with --disable-realtime-gps-tracking]))
    AC_DEFINE(VIK_CONFIG_REALTIME_GPS_TRACKING, [], [REALTIME GPS TRACKING STUFF])
    ;;
esac
AM_CONDITIONAL([REALTIME_GPS_TRACKING], [test x$ac_cv_enable_realtimegpstracking = xyes])

AC_ARG_WITH(search,
            [AC_HELP_STRING([--with-search],
                            [specify google or geonames for searching (default is google)])],
            [case "x$withval" in
               "xgoogle")
                 VIK_CONFIG_SEARCH=VIK_CONFIG_SEARCH_GOOGLE
                 ;;
               "xgeonames")
                 VIK_CONFIG_SEARCH=VIK_CONFIG_SEARCH_GEONAMES
                 ;;
               *)
                AC_MSG_ERROR([Please, set a value for search method])
             esac],
             [VIK_CONFIG_SEARCH=VIK_CONFIG_SEARCH_GOOGLE])
AC_DEFINE_UNQUOTED(VIK_CONFIG_SEARCH_GOOGLE, 1,
                   [Id. for searching via GOOGLE])
AC_DEFINE_UNQUOTED(VIK_CONFIG_SEARCH_GEONAMES, 2,
                   [Id. for searching via GEONAMES])
AC_DEFINE_UNQUOTED(VIK_CONFIG_SEARCH, ${VIK_CONFIG_SEARCH},
                   [Method for the search])


AC_ARG_WITH(mapcache,
            [AC_HELP_STRING([--with-mapcache],
                            [specify the size of the map cache (default is 50331648)])],
            [if test "x$withval" = "xno"; then
                VIK_CONFIG_MAPCACHE_SIZE=0;
             elif test "x$withval" = "xyes"; then
                AC_MSG_ERROR([Please, set a value for size of the map cache])
             else
                VIK_CONFIG_MAPCACHE_SIZE=${withval}
             fi],
             [VIK_CONFIG_MAPCACHE_SIZE=50331648])
AC_DEFINE_UNQUOTED(VIK_CONFIG_MAPCACHE_SIZE, ${VIK_CONFIG_MAPCACHE_SIZE},
                   [Size of the map cache])

dnl man pages processing
DB2MAN_XSL=/usr/share/sgml/docbook/stylesheet/xsl/nwalsh/manpages/docbook.xsl
AC_SUBST(DB2MAN_XSL)
AC_PATH_PROG(XP,xsltproc)
AM_CONDITIONAL([GEN_MANPAGES],[test "x$XP" != "x" && test -r "$DB2MAN_XSL"])

dnl This will cause the automake generated makefiles to pass the
dnl correct flags to aclocal.
ACLOCAL_AMFLAGS="\${ACLOCAL_FLAGS}"
AC_SUBST(ACLOCAL_AMFLAGS)

# Configuration
AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/icons/Makefile
		 po/Makefile.in
                 test/Makefile
                 doc/Makefile
                 doc/dev/Makefile])
AC_OUTPUT([
	   viking.spec
	   ])

dnl Output the configuration summary
echo ""
echo "==========================================="
echo "$PACKAGE $VERSION"
echo "-------------------------------------------"
echo "Alphabetized track & waypoints   : $ac_cv_enable_alpha_trw"
echo "Google Maps                      : $ac_cv_enable_google"
echo "Terraserver Maps                 : $ac_cv_enable_terraserver"
#echo "Expedia Maps                     : $ac_cv_enable_expedia"
echo "Open Street Map                  : $ac_cv_enable_openstreetmap"
echo "Geonames                         : $ac_cv_enable_geonames"
#echo "Geocaches Acquire                : $ac_cv_enable_geocaches"
echo "USGS 24k DEM                     : $ac_cv_enable_dem24k"
echo "Realtime GPS Tracking            : $ac_cv_enable_realtimegpstracking"
echo "Size of map cache (in memory)    : ${VIK_CONFIG_MAPCACHE_SIZE}"
echo "-------------------------------------------"
echo ""
echo "Configure finished, type 'make' to build."

