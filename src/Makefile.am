# This is the Makefile for the viking gps viewer program.
# This file is licensed through the GPL version 2 or newer.

SUBDIRS = icons

bin_PROGRAMS = viking

noinst_LIBRARIES = libviking.a

authors.h: ../AUTHORS
	rm -f $@
	echo "/* Generated file. */" >> $@
	echo "#define AUTHORS \\" >> $@
	sed -e "s/ \*\*.*//" -e 's/^/"/' -e 's/$$/\\n"\\/' -e '$$s/\\$$//' $< >> $@

BUILT_SOURCES = authors.h

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
	dialog.c dialog.h \
	util.c util.h \
	download.c download.h \
	viktreeview.c viktreeview.h \
	viktrwlayer.c viktrwlayer.h viktrwlayer_pixmap.h \
	viklayer.c viklayer.h \
	viklayerspanel.c viklayerspanel.h \
	vikcoordlayer.c vikcoordlayer.h vikcoordlayer_pixmap.h \
	vikstatus.c vikstatus.h \
	vikwindow.c vikwindow.h \
	vikviewport.c vikviewport.h \
	vikaggregatelayer.c vikaggregatelayer.h vikaggregatelayer_pixmap.h \
	vikgpslayer.c vikgpslayer.h vikgpslayer_pixmap.h \
	vikgeoreflayer.c vikgeoreflayer.h vikgeoreflayer_pixmap.h \
	vikfileentry.c vikfileentry.h \
	viktrwlayer_tpwin.c viktrwlayer_tpwin.h \
	viktrwlayer_propwin.c viktrwlayer_propwin.h \
	thumbnails.c thumbnails.h thumbnails_pixbuf.h \
	background.c background.h \
	vikradiogroup.c vikradiogroup.h \
	vikcoord.c vikcoord.h \
	mapcache.c mapcache.h \
	vikmapslayer.c vikmapslayer.h vikmapslayer_pixmap.h \
	gtkcellrendererprogress.c gtkcellrendererprogress.h \
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
	vikdemlayer_pixmap.h \
	vikfilelist.c vikfilelist.h \
	dems.c dems.h \
	srtm_continent.c \
	uibuilder.c uibuilder.h \
	print-preview.c print-preview.h \
	print.c print.h \
	preferences.c preferences.h

if GOOGLE
libviking_a_SOURCES += \
	khmaps.c khmaps.h \
	google.c google.h \
	googlemaps.c googlemaps.h
endif

if TERRASERVER
libviking_a_SOURCES += \
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

viking_SOURCES = main.c

INCLUDES        = @GTK_CFLAGS@ @EXPAT_CFLAGS@ @LIBCURL_CPPFLAGS@
LDADD           = libviking.a @GTK_LIBS@ @EXPAT_LIBS@ @LIBCURL@ icons/libicons.a
if REALTIME_GPS_TRACKING
LDADD           += -lgps
endif
#LDADD           = -lz @GTK_LIBS@ @EXPAT_LIBS@ @LIBCURL@
AM_CFLAGS		= -Wall -g -D_GNU_SOURCE

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = viking.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
EXTRA_DIST=$(desktop_in_files)

CLEANFILES = $(BUILT_SOURCES) $(desktop_DATA)
