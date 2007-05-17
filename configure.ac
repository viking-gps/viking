#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(viking, 0.1.3)
AM_INIT_AUTOMAKE()
dnl AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([src/config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([malloc.h netdb.h netinet/in.h stdlib.h string.h sys/param.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_MODE_T

# Checks for library functions.
AC_FUNC_STAT
AC_FUNC_STRTOD
AC_CHECK_FUNCS([bzero floor gethostbyname memset mkdir pow realpath socket sqrt strcasecmp strchr strdup strncasecmp strtol strtoul])

# Expat
AM_WITH_EXPAT

# Curl
LIBCURL_CHECK_CONFIG([yes],[],[AM_CONDITIONAL([LIBCURL],[true])],[AM_CONDITIONAL([LIBCURL],[false])])

AC_CHECK_PROG([GDK_PIXBUF_CSOURCE],gdk-pixbuf-csource,[yes])
if test $GDK_PIXBUF_CSOURCE != "yes"
then
  AC_MSG_ERROR([gdk-pixbuf-csource is needed but not found])
fi

# Checks for libraries.
AM_PATH_GTK_2_0(2.2.0,,AC_MSG_ERROR(needs GTK+ 2.2.0),gthread)

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
              [enable Expedia stuff (default is enable)]),
              [ac_cv_enable_expedia=$enableval],
              [ac_cv_enable_expedia=yes])
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

# Configuration
AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/icons/Makefile
                 test/Makefile
                 doc/Makefile
                 doc/dev/Makefile])
AC_OUTPUT([
	   viking.spec
	   ])
