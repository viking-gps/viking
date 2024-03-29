# This is the Makefile for the Viking GPS viewer program.
# This file is licensed through the GPL version 2 or newer.

SUBDIRS = icons

bin_PROGRAMS = viking

noinst_LIBRARIES = \
	libviking.a
#	libdtoa.a

authors.h: $(top_srcdir)/AUTHORS
	rm -f $@
	echo "/* Generated file. */" >> $@
	echo "const gchar *AUTHORS[] = {\\" >> $@
	sed -e "s/ \*\*.*//" -e 's/^/"/' -e 's/$$/",\\/' -e '$$s/\\$$//' $< >> $@
	echo "NULL};" >> $@

documenters.h: $(top_srcdir)/help/C/index.docbook
	$(XSLTPROC) $(srcdir)/docbook2documenters.xsl $(top_srcdir)/help/C/index.docbook > $@

# Use xxd tool as more robust than the sed commands used in the AUTHORS methods above.
# The additional sed usage here is to remove xxd's replacement of relative filename characters
#  as it does '../' --> '___' since they obviously can't be part of a C variable name.
# NB for usage in C code, one needs to manually append the NULL terminator
NEWS.h: $(top_srcdir)/NEWS
	xxd -i $< | sed -e "s/___//" > $@

BUILT_SOURCES = authors.h documenters.h NEWS.h

vikenumtypes.h: vikenumtypes.h.template $(ENUM_H_FILES) $(GLIB_MKENUMS)
	(cd $(srcdir) && $(GLIB_MKENUMS) --template vikenumtypes.h.template $(ENUM_H_FILES)) > $@

vikenumtypes.c: vikenumtypes.c.template $(ENUM_H_FILES) $(GLIB_MKENUMS)
	(cd $(srcdir) && $(GLIB_MKENUMS) --template vikenumtypes.c.template $(ENUM_H_FILES)) > $@

EXTRA_DIST = \
	misc/fpconv-license.txt \
	docbook2documenters.xsl \
	vikenumtypes.h.template \
	vikenumtypes.c.template

BUILT_SOURCES += vikenumtypes.h vikenumtypes.c

$(BUILT_SOURCES): $(srcdir)/Makefile.am

ENUM_H_FILES = \
	vikviewport.h

libviking_a_SOURCES = \
	astronomy.h astronomy.c \
	bbox.h \
	map_ids.h \
	modules.h modules.c \
	curl_download.c curl_download.h \
	compression.c compression.h \
	menu.xml.h \
	degrees_converters.c degrees_converters.h \
	globals.c globals.h \
	viking.h mapcoord.h config.h \
	viktrwlayer.c viktrwlayer.h \
	vik_compat.c vik_compat.h \
	viktrack.c viktrack.h \
	vikwaypoint.c vikwaypoint.h \
	clipboard.c clipboard.h \
	coords.c coords.h \
	gpsmapper.c gpsmapper.h \
	gpspoint.c gpspoint.h \
	geojson.c geojson.h \
	dir.c dir.h \
	file.c file.h \
	fileutils.c fileutils.h \
	file_magic.c file_magic.h \
	NEWS.h \
	authors.h \
	documenters.h \
	dialog.c dialog.h \
	util.c util.h \
	ui_util.c ui_util.h \
	download.c download.h \
	jpg.c jpg.h \
	vikenumtypes.c vikenumtypes.h \
	viktreeview.c viktreeview.h \
	viklayer.c viklayer.h \
	viklayerspanel.c viklayerspanel.h \
	vikcoordlayer.c vikcoordlayer.h \
	vikstatus.c vikstatus.h \
	vikwindow.c vikwindow.h \
	vikviewport.c vikviewport.h \
	vikaggregatelayer.c vikaggregatelayer.h \
	vikgobjectbuilder.c vikgobjectbuilder.h \
	vikgpslayer.c vikgpslayer.h \
	vikgeoreflayer.c vikgeoreflayer.h \
	vikfileentry.c vikfileentry.h \
	vikgototool.c vikgototool.h \
	vikgotoxmltool.c vikgotoxmltool.h \
	vikgoto.c vikgoto.h \
	viktrwlayer_export.c viktrwlayer_export.h \
	viktrwlayer_tpwin.c viktrwlayer_tpwin.h \
	viktrwlayer_wpwin.c viktrwlayer_wpwin.h \
	viktrwlayer_propwin.c viktrwlayer_propwin.h \
	viktrwlayer_analysis.c viktrwlayer_analysis.h \
	viktrwlayer_tracklist.c viktrwlayer_tracklist.h \
	viktrwlayer_waypointlist.c viktrwlayer_waypointlist.h \
	vikrouting.c vikrouting.h \
	vikroutingengine.c vikroutingengine.h \
	vikroutingwebengine.c vikroutingwebengine.h \
	vikutils.c vikutils.h \
	toolbar.c toolbar.h toolbar.xml.h \
	thumbnails.c thumbnails.h \
	md5_hash.c md5_hash.h \
	background.c background.h \
	logging.c logging.h \
	vikradiogroup.c vikradiogroup.h \
	vikcoord.c vikcoord.h \
	mapcache.c mapcache.h \
	maputils.c maputils.h \
	vikmapsource.c vikmapsource.h \
	vikmapsourcedefault.c vikmapsourcedefault.h \
	vikmapslayer.c vikmapslayer.h \
	vikmapslayer_compat.c vikmapslayer_compat.h \
	vikmaptype.c vikmaptype.h \
	vikslippymapsource.c vikslippymapsource.h \
	vikwmscmapsource.c vikwmscmapsource.h \
	viktmsmapsource.c viktmsmapsource.h \
	metatile.c metatile.h \
	fit.c fit.h fit_sdk.h \
	gpx.c gpx.h \
	tcx.c tcx.h \
	garminsymbols.c garminsymbols.h \
	acquire.c acquire.h \
	babel.c babel.h \
	babel_ui.c babel_ui.h \
	datasource_file.c \
	datasource_geojson.c \
	datasource_gps.c datasource_gps.h \
	datasource_routing.c \
	datasource_gc.c \
	datasource_bfilter.c \
	datasource_wikipedia.c \
	datasource_url.c \
	datasources.h \
	googlesearch.c googlesearch.h \
	dem.c dem.h \
	vikdemlayer.h vikdemlayer.c \
	vikdatetime_edit_dialog.c vikdatetime_edit_dialog.h \
	vikfilelist.c vikfilelist.h \
	vikexttool.c vikexttool.h \
	vikexttools.c vikexttools.h \
	vikwebtool.c vikwebtool.h \
	vikwebtoolcenter.c vikwebtoolcenter.h \
	vikwebtoolbounds.c vikwebtoolbounds.h \
	vikwebtoolformat.c vikwebtoolformat.h \
	vikexttool_datasources.c vikexttool_datasources.h \
	vikwebtool_datasource.c vikwebtool_datasource.h \
	dems.c dems.h \
	srtm_continent.c \
	uibuilder.c uibuilder.h \
	print-preview.c print-preview.h \
	print.c print.h \
	kml.c kml.h \
	kmz.c kmz.h \
	viklayer_defaults.c viklayer_defaults.h \
	settings.c settings.h \
	socket.c socket.h \
	preferences.c preferences.h \
	misc/heatmap.c misc/heatmap.h \
	misc/fpconv.c misc/fpconv.h misc/powers.h \
	misc/strtod.c misc/strtod.h \
	misc/kdtree.c misc/kdtree.h \
	misc/gtkhtml.c misc/gtkhtml-private.h

#libdtoa_a_SOURCES = misc/dtoa.c misc/dtoa.h
# Note especially not defining 'USE_LOCALE' to ensure dtoa() always writes a '.' for the decimal point
#libdtoa_a_CFLAGS = $(PACKAGE_CFLAGS) -DIEEE_8087

if BING
libviking_a_SOURCES += \
	bingmapsource.c bingmapsource.h \
	bing.c bing.h
endif

if GOOGLE
libviking_a_SOURCES += \
	google.c google.h
endif

if TERRASERVER
libviking_a_SOURCES += \
	terraservermapsource.c terraservermapsource.h \
	terraserver.c terraserver.h
endif

if EXPEDIA
libviking_a_SOURCES += \
	expedia.c expedia.h
endif

if OPENSTREETMAP
libviking_a_SOURCES += \
	osm.c osm.h \
	osm-traces.c osm-traces.h \
	datasource_osm_my_traces.c \
	datasource_osm.c
endif

if BLUEMARBLE
libviking_a_SOURCES += \
	bluemarble.c bluemarble.h
endif

if GEONAMES
libviking_a_SOURCES += \
	geonames.c geonames.h \
	geonamessearch.c geonamessearch.h
endif

if GEOTAG
libviking_a_SOURCES += \
	datasource_geotag.c \
	geotag_exif.c geotag_exif.h \
	viktrwlayer_geotag.c viktrwlayer_geotag.h
endif

# libexif doesn't have write support of EXIF info - so reused this code from command line exif tool
if LIBEXIF
libviking_a_SOURCES += \
	libjpeg/jpeg-data.c libjpeg/jpeg-data.h \
	libjpeg/jpeg-marker.c libjpeg/jpeg-marker.h
endif

if MAPNIK
libviking_a_SOURCES += \
	vikmapniklayer.c vikmapniklayer.h \
	mapnik_interface.cpp mapnik_interface.h
endif

if GEOCLUE
libviking_a_SOURCES += \
	vikgeocluelayer.c vikgeocluelayer.h \
	libgeoclue.c libgeoclue.h
endif

viking_SOURCES = main.c

LDADD           = icons/libicons.a $(noinst_LIBRARIES) $(PACKAGE_LIBS) $(GTK_LIBS) @EXPAT_LIBS@ @LIBCURL@
if WINDOWS
LDADD += \
	../windows/installer/pixmaps/viking_icon.o
endif

AM_CFLAGS		= -Wall -g -D_GNU_SOURCE \
	$(PACKAGE_CFLAGS) $(GTK_CFLAGS) \
	@EXPAT_CFLAGS@ @LIBCURL_CPPFLAGS@ @LIBGEOCLUE_CFLAGS@
AM_CPPFLAGS		= \
	$(PACKAGE_CFLAGS) $(GTK_CFLAGS) \
	-DLOCALEDIR=\""$(datadir)/locale"\" \
	-DVIKING_DATADIR=\""$(pkgdatadir)"\" \
	-DVIKING_SYSCONFDIR=\""$(sysconfdir)/$(PACKAGE)"\"

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = viking.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
EXTRA_DIST += $(desktop_in_files)

CLEANFILES = $(BUILT_SOURCES) $(desktop_DATA)
