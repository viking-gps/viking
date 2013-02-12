include $(top_srcdir)/gnome-doc-utils.make

# Annoyingly in the XML, yelp refuses to load relative images fileref, i.e. ../../src/icons/file.png
#  whereas the PDF generation inserts these references OK
# Nevertheless we need to copy the images into figures/ so they are available for the distribution installed help
# Most tool icons are _18, except the pan one which is _22 just to be annoying
get-icons:
	-cp -f ../src/icons/*_[12][0-9].png C/figures/
# '-' ^^^^^ means it doesn't care about the exit status
#  this copy statement is not critical (especially when run under make distcheck)

dist-hook: get-icons doc-dist-hook

DOC_MODULE = viking
DOC_ENTITIES = legal.xml
DOC_INCLUDES =
DOC_FIGURES = \
    figures/addtr_18.png \
    figures/addwp_18.png \
    figures/demdl_18.png \
    figures/edtr_18.png \
    figures/edwp_18.png \
    figures/geomove_18.png \
    figures/geozoom_18.png \
    figures/map_tile_info_dialog.png \
    figures/mapdl_18.png \
    figures/mover_22.png \
    figures/properties_degree.png \
    figures/properties_units.png \
    figures/route_finder_18.png \
    figures/ruler_18.png \
    figures/select_18.png \
    figures/showpic_18.png \
    figures/trw_statusbar.png \
    figures/vik_new_route_18.png \
    figures/zoom_18.png \
    figures/zoom_status_popup.png \
    figures/Viking-OSM-CycleMap-ManyTracks.jpg
DOC_LINGUAS =
