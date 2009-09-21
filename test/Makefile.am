AM_CFLAGS        = -I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS)
LDADD           = $(PACKAGE_LIBS) @EXPAT_LIBS@ @LIBCURL@ $(top_builddir)/src/icons/libicons.a
if REALTIME_GPS_TRACKING
LDADD           += -lgps
endif

TESTS = check_degrees_conversions.sh

check_PROGRAMS = degrees_converter gpx2gpx

check_SCRIPTS = check_degrees_conversions.sh

EXTRA_DIST = check_degrees_conversions.sh
	          
degrees_converter_SOURCES = degrees_converter.c
degrees_converter_LDADD = \
  $(top_builddir)/src/degrees_converters.o \
  $(LDADD)

gpx2gpx_SOURCES = gpx2gpx.c
gpx2gpx_LDADD = \
  $(top_builddir)/src/libviking.a \
  $(LDADD)
