AM_CFLAGS		= -Wall \
	-I$(top_srcdir)/src \
	$(PACKAGE_CFLAGS) $(GTK_CFLAGS)
LDADD           = $(PACKAGE_LIBS) $(GTK_LIBS) @EXPAT_LIBS@ @LIBCURL@ $(top_builddir)/src/icons/libicons.a
if REALTIME_GPS_TRACKING
LDADD           += -lgps
AM_TESTS_ENVIRONMENT = \
  REALTIME_GPS_TRACKING="YES"; export REALTIME_GPS_TRACKING;
endif

if GEOCLUE
AM_TESTS_ENVIRONMENT = \
  GEOCLUE_ENABLED=="YES"; export GEOCLUE_ENABLED;
endif

TESTS = check_degrees_conversions.sh \
	check_decimal_output.sh \
	check_parse_latlon.sh \
	check_babel.sh \
	check_vik2vik.sh \
	check_vikgoto.sh \
	check_gpx.sh \
	check_tcx.sh \
	check_zip.sh \
	check_geojson_osrm.sh \
	check_help_xml.sh \
	check_metatile.sh
if GEOTAG
TESTS += check_geotag.sh
endif
if MD5_HASH
TESTS += check_md5_hash.sh
endif

check_PROGRAMS = degrees_converter \
	geojson_osrm_to_gpx \
	gpx2gpx \
	vik2vik \
	test_vikgotoxmltool \
	test_time \
	test_decimal_output \
	test_coord_conversion \
	test_parse_latlon \
	test_babel \
	test_file_load \
	test_md5_hash \
	test_metatile

if GEOTAG
check_PROGRAMS += geotag_read geotag_write
endif

check_SCRIPTS = check_degrees_conversions.sh \
	check_decimal_output.sh \
	check_parse_latlon.sh \
	check_vik2vik.sh \
	check_vikgoto.sh \
	check_gpx.sh \
	check_tcx.sh \
	check_zip.sh \
	check_geojson_osrm.sh \
	check_help_xml.sh \
	check_metatile.sh
if GEOTAG
check_SCRIPTS += check_geotag.sh
endif
if MD5_HASH
check_SCRIPTS += check_md5_hash.sh
endif

# Scripts and the test data that they use
EXTRA_DIST = check_degrees_conversions.sh \
	check_decimal_output.sh \
	check_parse_latlon.sh \
	check_babel.sh \
	check_help_xml.sh \
	check_vik2vik.sh \
	Simple.vik \
	Simple_no-geoclue.vik \
	Simple_no-realtime-gps-tracking.vik \
	check_gpx.sh \
	check_tcx.sh \
	check_zip.sh \
	SF\#022.gpx \
	GPXv1.1-sample.gpx \
	Stonehenge.tcx \
	check_vikgoto.sh \
	search-result-geonames-viking.xml \
	search-result-geonames-attr-viking.xml \
	search-result-nominatim-viking.xml \
	check_md5_hash.sh \
	check_metatile.sh \
	metatile_example/13/0/0/250/220/0.meta \
	check_geojson_osrm.sh \
	OSRM_sample_response.txt \
	check_geotag.sh \
	Stonehenge.gpx \
	Stonehenge.jpg \
	ViewFromCribyn-Wales-GPS.jpg

degrees_converter_SOURCES = degrees_converter.c
degrees_converter_LDADD = \
  $(top_builddir)/src/degrees_converters.o \
  $(LDADD)

if GEOTAG
geotag_read_SOURCES = geotag_read.c
geotag_read_LDADD = \
  $(top_builddir)/src/libviking.a \
  $(LDADD)

geotag_write_SOURCES = geotag_write.c
geotag_write_LDADD = \
  $(top_builddir)/src/libviking.a \
  $(LDADD)
endif

gpx2gpx_SOURCES = gpx2gpx.c
gpx2gpx_LDADD = \
  $(top_builddir)/src/libviking.a \
  $(LDADD)

vik2vik_SOURCES = vik2vik.c
vik2vik_LDADD = \
  $(top_builddir)/src/libviking.a \
  $(LDADD)

test_vikgotoxmltool_SOURCES = test_vikgotoxmltool.c
test_vikgotoxmltool_LDADD = \
  $(top_builddir)/src/libviking.a \
  $(LDADD)

test_time_SOURCES = test_time.c
test_time_LDADD = \
  $(top_builddir)/src/libviking.a \
  $(LDADD)

test_decimal_output_SOURCES = test_decimal_output.c
test_decimal_output_LDADD = \
  $(top_builddir)/src/libviking.a \
  $(LDADD)

test_coord_conversion_SOURCES = test_coord_conversion.c
test_coord_conversion_LDADD = \
  $(top_builddir)/src/libviking.a \
  $(LDADD)

geojson_osrm_to_gpx_SOURCES = geojson_osrm_to_gpx.c
geojson_osrm_to_gpx_LDADD = \
  $(top_builddir)/src/libviking.a \
  $(LDADD)

test_parse_latlon_SOURCES = test_parse_latlon.c
test_parse_latlon_LDADD = \
  $(top_builddir)/src/libviking.a \
  $(LDADD)

test_babel_SOURCES = test_babel.c
test_babel_LDADD = \
  $(top_builddir)/src/libviking.a \
  $(LDADD)

test_md5_hash_SOURCES = test_md5_hash.c
test_md5_hash_LDADD = \
  $(top_builddir)/src/libviking.a \
  $(LDADD)

test_metatile_SOURCES = test_metatile.c
test_metatile_LDADD = \
  $(top_builddir)/src/libviking.a \
  $(LDADD)

test_file_load_SOURCES = test_file_load.c
test_file_load_LDADD = \
  $(top_builddir)/src/libviking.a \
  $(LDADD)
