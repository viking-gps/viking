WAYPOINT_ICONS = \
  wp_camp.png \
  wp_car.png \
  wp_deer.png \
  wp_flag.png \
  wp_geocache.png \
  wp_geocache_fnd.png \
  wp_house.png \
  wp_wpt_dot.png \
  wp_dollar.png \
  wp_golf.png \
  wp_summit.png \
  wp_anchor.png \
  wp_forest.png \
  wp_parking.png \
  wp_sml_cty.png \
  wp_med_cty.png \
  wp_lrg_cty.png \
  wp_cap_cty.png 


ICONS = \
  addtr_18.png \
  addwp_18.png \
  edtr_18.png \
  edwp_18.png \
  geomove_18.png \
  geozoom_18.png \
  mapdl_18.png \
  ruler_18.png \
  showpic_18.png \
  zoom_18.png


ALL_ICONS = $(ICONS) $(WAYPOINT_ICONS)
EXTRA_DIST = $(ALL_ICONS)

BUILT_SOURCES = $(EXTRA_DIST:%.png=%.png_h) icons.h
CLEANFILES = $(BUILT_SOURCES)

SUFFIXES = .png_h .png

icons.h: $(ALL_ICONS:%.png=%.png_h)
	echo "/* Generated file */" > icons.h
	for file in *_h ; do echo "#include \"icons/$$file\"" >> icons.h ; done

.png.png_h:
	gdk-pixbuf-csource --name="`basename $< .png`" --struct $< > $@
	cat $@ | sed -e 's/\(pixel_data: ..\)/\1 (unsigned char \*)/' > temp.h
	mv temp.h $@
