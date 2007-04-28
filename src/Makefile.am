# This is the Makefile for the viking gps viewer program.
# This file is licensed through the GPL version 2 or newer.

SUBDIRS = icons

bin_PROGRAMS = viking

authors.h: ../AUTHORS
	rm -f $@
	echo "/* Generated file. */" >> $@
	echo "#define AUTHORS \\" >> $@
	sed -e "s/ \*\*.*//" -e 's/^/"/' -e 's/$$/\\n"\\/' $< >> $@

BUILT_SOURCES = authors.h

viking_SOURCES = main.c \
	modules.h modules.c \
	menu.xml.h \
    degrees_converters.c degrees_converters.h \
	viking.h globals.h mapcoord.h config.h \
	viktrack.c viktrack.h \
	vikwaypoint.c vikwaypoint.h \
	clipboard.c clipboard.h \
	coords.c coords.h \
	gpsmapper.c gpsmapper.h \
	gpspoint.c gpspoint.h \
	file.c file.h \
	authors.h \
	dialog.c dialog.h \
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
	datasources.h

if LIBCURL
viking_SOURCES += \
	curl_download.c curl_download.h
else
viking_SOURCES += \
	http.c http.h
endif

if GOOGLE
viking_SOURCES += \
	khmaps.c khmaps.h \
	google.c google.h \
	googlemaps.c googlemaps.h
endif

if TERRASERVER
viking_SOURCES += \
	terraserver.c terraserver.h
endif

if EXPEDIA
viking_SOURCES += \
	expedia.c expedia.h
endif

if OPENSTREETMAP
viking_SOURCES += \
	osm.c osm.h
endif


INCLUDES        = @GTK_CFLAGS@ @EXPAT_CFLAGS@ @LIBCURL_CPPFLAGS@
LDADD           = @GTK_LIBS@ @EXPAT_LIBS@ @LIBCURL@
AM_CFLAGS		= -Wall -g
