#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(viking, 0.1.1)
AM_INIT_AUTOMAKE()
dnl AC_CONFIG_SRCDIR([src/main.c])
dnl AM_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET

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

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
