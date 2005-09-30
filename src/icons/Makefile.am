WAYPOINT_ICONS = \
  wp_camp.png \
  wp_car.png \
  wp_deer.png \
  wp_flag.png \
  wp_geocache.png \
  wp_geocache_fnd.png \
  wp_house.png \
  wp_wpt_dot.png

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

EXTRA_DIST = $(ICONS) $(WAYPOINT_ICONS)

BUILT_SOURCES = $(EXTRA_DIST:%.png=%.png_h)
CLEANFILES = $(BUILT_SOURCES)

SUFFIXES = .png_h .png

.png.png_h:
	gdk-pixbuf-csource --name="`basename $< .png`" --struct $< > $@