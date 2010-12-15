# This is the Makefile for the viking gps viewer program.
# This file is licensed through the GPL version 2 or newer.

SUBDIRS = icons

bin_PROGRAMS = viking

noinst_LIBRARIES = libviking.a

authors.h: $(top_srcdir)/AUTHORS
	rm -f $@
	echo "/* Generated file. */" >> $@
	echo "const gchar *AUTHORS[] = {\\" >> $@
	sed -e "s/ \*\*.*//" -e 's/^/"/' -e 's/$$/",\\/' -e '$$s/\\$$//' $< >> $@
	echo "NULL};" >> $@

if HAVE_XSLTPROC
XP=@XP@
documenters.h: $(top_srcdir)/help/C/viking.xml
	$(XP) $(srcdir)/docbook2documenters.xsl $(top_srcdir)/help/C/viking.xml > $@
else
documenters.h:
	rm -f $@
	echo "/* Generated file. */" >> $@
	echo "const gchar *DOCUMENTERS[] = {\\" >> $@
	echo "NULL};" >> $@
endif

BUILT_SOURCES = authors.h documenters.h

vikenumtypes.h: vikenumtypes.h.template $(ENUM_H_FILES) $(GLIB_MKENUMS)
	(cd $(srcdir) && $(GLIB_MKENUMS) --template vikenumtypes.h.template $(ENUM_H_FILES)) > $@

vikenumtypes.c: vikenumtypes.c.template $(ENUM_H_FILES) $(GLIB_MKENUMS)
	(cd $(srcdir) && $(GLIB_MKENUMS) --template vikenumtypes.c.template $(ENUM_H_FILES)) > $@

EXTRA_DIST =                         \
        docbook2documenters.xsl      \
        vikenumtypes.h.template      \
        vikenumtypes.c.template

BUILT_SOURCES += vikenumtypes.h vikenumtypes.c

$(BUILT_SOURCES): $(srcdir)/Makefile.am

ENUM_H_FILES = \
	vikviewport.h

libviking_a_SOURCES = \
	modules.h modules.c \
	curl_download.c curl_download.h \
	menu.xml.h \
    degrees_converters.c degrees_converters.h \
	globals.c globals.h \
	viking.h mapcoord.h config.h \
	viktrack.c viktrack.h \
	vikwaypoint.c vikwaypoint.h \
	clipboard.c clipboard.h \
	coords.c coords.h \
	gpsmapper.c gpsmapper.h \
	gpspoint.c gpspoint.h \
	file.c file.h \
	authors.h \
	documenters.h \
	dialog.c dialog.h \
	util.c util.h \
	download.c download.h \
	vikenumtypes.c vikenumtypes.h \
	viktreeview.c viktreeview.h \
	viktrwlayer.c viktrwlayer.h \
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
	viktrwlayer_tpwin.c viktrwlayer_tpwin.h \
	viktrwlayer_propwin.c viktrwlayer_propwin.h \
	thumbnails.c thumbnails.h \
	background.c background.h \
	vikradiogroup.c vikradiogroup.h \
	vikcoord.c vikcoord.h \
	mapcache.c mapcache.h \
	vikmapsource.c vikmapsource.h \
	vikmapsourcedefault.c vikmapsourcedefault.h \
	vikmapslayer.c vikmapslayer.h \
	vikmapslayer_compat.c vikmapslayer_compat.h \
	vikmaptype.c vikmaptype.h \
	vikslippymapsource.c vikslippymapsource.h \
	vikwmscmapsource.c vikwmscmapsource.h \
	viktmsmapsource.c viktmsmapsource.h \
        gpx.c gpx.h \
	garminsymbols.c garminsymbols.h \
	acquire.c acquire.h \
	babel.c babel.h \
	datasource_gps.c \
	datasource_google.c \
	datasource_gc.c \
	datasource_bfilter.c \
	datasources.h \
	googlesearch.c googlesearch.h \
	dem.c dem.h \
	vikdemlayer.h vikdemlayer.c \
	vikfilelist.c vikfilelist.h \
	vikexttool.c vikexttool.h \
	vikexttools.c vikexttools.h \
	vikwebtool.c vikwebtool.h \
	vikwebtoolcenter.c vikwebtoolcenter.h \
	dems.c dems.h \
	srtm_continent.c \
	uibuilder.c uibuilder.h \
	print-preview.c print-preview.h \
	print.c print.h \
	preferences.c preferences.h

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
	osm-traces.c osm-traces.h
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

#if SPOTMAPS
libviking_a_SOURCES += \
	spotmaps.c spotmaps.h
#endif

viking_SOURCES = main.c

LDADD           = libviking.a $(PACKAGE_LIBS) @EXPAT_LIBS@ @LIBCURL@ icons/libicons.a
if REALTIME_GPS_TRACKING
LDADD           += -lgps
endif
AM_CFLAGS		= -Wall -g -D_GNU_SOURCE \
	$(PACKAGE_CFLAGS) \
	@EXPAT_CFLAGS@ @LIBCURL_CPPFLAGS@

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = viking.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
EXTRA_DIST += $(desktop_in_files)

CLEANFILES = $(BUILT_SOURCES) $(desktop_DATA)
