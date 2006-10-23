INCLUDES        = -I$(top_srcdir)/src @GTK_CFLAGS@
LDADD           = @GTK_LIBS@

TESTS = check_degrees_conversions.sh

check_PROGRAMS = degrees_converter

check_SCRIPTS = check_degrees_conversions.sh

EXTRA_DIST = check_degrees_conversions.sh
	          
degrees_converter_SOURCES = degrees_converter.c
degrees_converter_LDADD = \
  $(top_builddir)/src/degrees_converters.o \
  $(LDADD)
