@YELP_HELP_RULES@

# Annoyingly in the XML, yelp refuses to load relative images fileref, i.e. ../../src/icons/file.png
#  whereas the PDF generation inserts these references OK
# Nevertheless we need to copy the images into figures/ so they are available for the distribution installed help
C/figures/%.png:
	-cp -f ../src/icons/$(notdir $@) C/figures/
# '-' ^^^^^ means it doesn't care about the exit status
#  this copy statement is not critical (especially when run under make distcheck)

HELP_ID = viking
HELP_FILES = index.docbook
HELP_EXTRA = legal.xml \
    attribution.xml \
    commandline.xml \
    commandline_details.xml \
    edit_view_menus.xml \
    dem_layer.xml \
    geoclue_layer.xml \
    georef_layer.xml \
    mapnik_rendering_layer.xml \
    recommends.xml \
    refsynopsis.xml
HELP_MEDIA = \
    figures/Aggregate_heatmap.png \
    figures/Aggregate_statistics.png \
    figures/Aggregate_statistics_years.png \
    figures/addtr_18.png \
    figures/addwp_18.png \
    figures/demdl_18.png \
    figures/edtr_18.png \
    figures/edwp_18.png \
    figures/geomove_18.png \
    figures/geozoom_18.png \
    figures/LayersTracksList.png \
    figures/LayersTracksStatistics.png \
    figures/Maps_download_region_dialog.png \
    figures/Maps_Cache_Status_Overlay.png \
    figures/map_tile_info_dialog.png \
    figures/mapdl_18.png \
    figures/mover_22.png \
    figures/properties_degree.png \
    figures/properties_units.png \
    figures/route_finder_18.png \
    figures/ruler_18.png \
    figures/select_18.png \
    figures/showpic_18.png \
    figures/DEM_Example.png \
    figures/GeoRefLayer_Properties.png \
    figures/Goto_Main_Window.png \
    figures/Panel_Stats_Tab.png \
    figures/Trackpoint_edit_dialog.png \
    figures/Track_elevation_graph.png \
    figures/Track_speed_time_graph_WinterSolstice2021.png \
    figures/Track_properties.png \
    figures/Track_statistics.png \
    figures/Track_time_splits.png \
    figures/trw_statusbar.png \
    figures/vik_new_route_18.png \
    figures/WaypointProperties.png \
    figures/WaypointProperties-Extra.png \
    figures/WaypointsList.png \
    figures/zoom_18.png \
    figures/zoom_status_popup.png \
    figures/Viking-OSM-CycleMap-ManyTracks.jpg \
    figures/Viking-TAC.png \
    figures/Viking-TAC-Lines.png \
    figures/DEM_file_info_dialog.png \
    figures/customize_toolbar_dialog.png
HELP_LINGUAS =

EXTRA_DIST = viking.xml.in

CLEANFILES=

# man pages processing
MANPAGES=viking.1
man_MANS = $(MANPAGES)
CLEANFILES+=$(MANPAGES)
XSLTPROC=@XSLTPROC@ --nonet --xinclude

# man pages now shares some detail with main Help
%.1: %.xml C/refsynopsis.xml C/commandline_details.xml
	$(XSLTPROC) -o $@ http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<
