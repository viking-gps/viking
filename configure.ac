#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(viking, 0.1.1)
AM_INIT_AUTOMAKE()
dnl AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([src/config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET

AC_CHECK_PROG([GDK_PIXBUF_CSOURCE],gdk-pixbuf-csource,[yes])
if test $GDK_PIXBUF_CSOURCE != "yes"
then
  AC_MSG_ERROR([gdk-pixbuf-csource is needed but not found])
fi

# Checks for libraries.
AM_PATH_GTK_2_0(2.2.0,,AC_MSG_ERROR(needs GTK+ 2.2.0),gthread)

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
                 doc/Makefile
                 doc/dev/Makefile])
AC_OUTPUT
