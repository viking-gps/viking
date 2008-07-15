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
  wp_cap_cty.png \
  wp_bell.png \
  wp_diamond_grn.png \
  wp_diamond_red.png \
  wp_dive1.png \
  wp_dive2.png \
  wp_fish.png \
  wp_fuel.png \
  wp_horn.png \
  wp_knife.png \
  wp_light.png \
  wp_mug.png \
  wp_skull.png \
  wp_square_grn.png \
  wp_square_red.png \
  wp_wbuoy.png \
  wp_wreck.png \
  wp_null.png \
  wp_mine.png \
  wp_school.png \
  wp_cemetery.png \
  wp_church.png \
  wp_military.png \
  wp_mob.png \
  wp_buoy_ambr.png \
  wp_buoy_blck.png \
  wp_buoy_blue.png \
  wp_buoy_grn.png \
  wp_buoy_grn_red.png \
  wp_buoy_grn_wht.png \
  wp_buoy_orng.png \
  wp_buoy_red.png \
  wp_buoy_red_grn.png \
  wp_buoy_red_wht.png \
  wp_buoy_violet.png \
  wp_buoy_wht.png \
  wp_buoy_wht_grn.png \
  wp_buoy_wht_red.png \
  wp_dot.png \
  wp_rbcn.png \
  wp_boat_ramp.png \
  wp_restroom.png \
  wp_shower.png \
  wp_drinking_wtr.png \
  wp_phone.png \
  wp_1st_aid.png \
  wp_info.png \
  wp_park.png \
  wp_picnic.png \
  wp_scenic.png \
  wp_skiing.png \
  wp_swimming.png \
  wp_dam.png \
  wp_controlled.png \
  wp_danger.png \
  wp_restricted.png \
  wp_ball.png \
  wp_rv_park.png \
  wp_bridge.png \
  wp_building.png \
  wp_tunnel.png \
  wp_beach.png \
  wp_airplane.png

ICONS = \
  viking_icon.png \
  addtr_18.png \
  begintr_18.png \
  addwp_18.png \
  edtr_18.png \
  edwp_18.png \
  geomove_18.png \
  geozoom_18.png \
  mapdl_18.png \
  demdl_18.png \
  mover_22.png \
  ruler_18.png \
  showpic_18.png \
  zoom_18.png \
  iscissors_18.png

CURSORS = \
  cursor_addtr.png \
  cursor_begintr.png \
  cursor_edtr.png \
  cursor_geozoom.png \
  cursor_ruler.png \
  cursor_addwp.png \
  cursor_edwp.png \
  cursor_iscissors.png \
  cursor_showpic.png \
  cursor_demdl.png \
  cursor_geomove.png \
  cursor_mapdl.png \
  cursor_zoom.png

ALL_ICONS = $(ICONS) $(WAYPOINT_ICONS) $(CURSORS)
EXTRA_DIST = $(ALL_ICONS)

BUILT_SOURCES = $(EXTRA_DIST:%.png=%.png_h) icons.c icons.h
CLEANFILES = $(BUILT_SOURCES)

SUFFIXES = .png_h .png

noinst_LIBRARIES = libicons.a
libicons_a_SOURCES = icons.c icons.h

icons.h: $(ALL_ICONS:%.png=%.png_h)
	echo "/* Generated file */" > $@
	sed -n "s/^\(const.*\) = {/extern \1;/p" *_h >> $@

icons.c: $(ALL_ICONS:%.png=%.png_h)
	echo "/* Generated file */" > $@
	echo "#include <gdk-pixbuf/gdk-pixdata.h>" >> $@
	for file in *_h ; do echo "#include \"$$file\"" >> $@ ; done

.png.png_h:
	gdk-pixbuf-csource --name="`basename $< .png`" --struct --extern $< > $@
	cat $@ | sed -e 's/\(pixel_data: ..\)/\1 (unsigned char \*)/' > temp.h
	mv temp.h $@

INCLUDES        = @GTK_CFLAGS@
AM_CFLAGS       = -Wall -g -D_GNU_SOURCE

iconsdir = $(datadir)/icons
icons_DATA = viking_icon.png
