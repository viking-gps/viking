/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef VIK_CONFIG_GEOCACHES
#include <string.h>

#include <glib/gi18n.h>

#include "viking.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"
#include "preferences.h"

// Could have an array of programs instead...
#define GC_PROGRAM1 "geo-nearest"
#define GC_PROGRAM2 "geo-html2gpx"

/* params will be geocaching.username, geocaching.password */
/* we have to make sure these don't collide. */
#define VIKING_GC_PARAMS_GROUP_KEY "geocaching"
#define VIKING_GC_PARAMS_NAMESPACE "geocaching."


typedef struct {
  GtkWidget *num_spin;
  GtkWidget *center_entry; // TODO make separate widgets for lat/lon
  GtkWidget *miles_radius_spin;

  GdkGC *circle_gc;
  VikViewport *vvp;
  gboolean circle_onscreen;
  gint circle_x, circle_y, circle_width;
} datasource_gc_widgets_t;


static gpointer datasource_gc_init ( acq_vik_t *avt );
static void datasource_gc_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, gpointer user_data );
static void datasource_gc_get_cmd_string ( datasource_gc_widgets_t *widgets, gchar **cmd, gchar **input_file_type, gpointer not_used );
static void datasource_gc_cleanup ( datasource_gc_widgets_t *widgets );
static gchar *datasource_gc_check_existence ();

#define METERSPERMILE 1609.344

VikDataSourceInterface vik_datasource_gc_interface = {
  N_("Download Geocaches"),
  N_("Geocaching.com Caches"),
  VIK_DATASOURCE_AUTO_LAYER_MANAGEMENT,
  VIK_DATASOURCE_INPUTTYPE_NONE,
  TRUE, // Yes automatically update the display - otherwise we won't see the geocache waypoints!
  TRUE,
  TRUE,
  (VikDataSourceInitFunc)		datasource_gc_init,
  (VikDataSourceCheckExistenceFunc)	datasource_gc_check_existence,
  (VikDataSourceAddSetupWidgetsFunc)	datasource_gc_add_setup_widgets,
  (VikDataSourceGetCmdStringFunc)	datasource_gc_get_cmd_string,
  (VikDataSourceProcessFunc)		a_babel_convert_from_shellcommand,
  (VikDataSourceProgressFunc)		NULL,
  (VikDataSourceAddProgressWidgetsFunc)	NULL,
  (VikDataSourceCleanupFunc)		datasource_gc_cleanup,
  (VikDataSourceOffFunc)                NULL,
};

static VikLayerParam prefs[] = {
  { VIK_LAYER_NUM_TYPES, VIKING_GC_PARAMS_NAMESPACE "username", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("geocaching.com username:"), VIK_LAYER_WIDGET_ENTRY, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_NUM_TYPES, VIKING_GC_PARAMS_NAMESPACE "password", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("geocaching.com password:"), VIK_LAYER_WIDGET_ENTRY, NULL, NULL, NULL, NULL, NULL },
};

void a_datasource_gc_init()
{
  a_preferences_register_group ( VIKING_GC_PARAMS_GROUP_KEY, "Geocaching" );

  VikLayerParamData tmp;
  tmp.s = "username";
  a_preferences_register(prefs, tmp, VIKING_GC_PARAMS_GROUP_KEY);
  tmp.s = "password";
  a_preferences_register(prefs+1, tmp, VIKING_GC_PARAMS_GROUP_KEY);
}


static gpointer datasource_gc_init ( acq_vik_t *avt )
{
  datasource_gc_widgets_t *widgets = g_malloc(sizeof(*widgets));
  return widgets;
}

static gchar *datasource_gc_check_existence ()
{
  gboolean OK1 = FALSE;
  gboolean OK2 = FALSE;

  gchar *location1 = g_find_program_in_path(GC_PROGRAM1);
  if ( location1 ) {
    g_free(location1);
    OK1 = TRUE;
  }

  gchar *location2 = g_find_program_in_path(GC_PROGRAM2);
  if ( location2 ) {
    g_free(location2);
    OK2 = TRUE;
  }

  if ( OK1 && OK2 )
    return NULL;

  return g_strdup_printf(_("Can't find %s or %s in path! Check that you have installed it correctly."), GC_PROGRAM1, GC_PROGRAM2);
}

static void datasource_gc_draw_circle ( datasource_gc_widgets_t *widgets )
{
  gdouble lat, lon;
  if ( widgets->circle_onscreen ) {
    vik_viewport_draw_arc ( widgets->vvp, widgets->circle_gc, FALSE,
		widgets->circle_x - widgets->circle_width/2,
		widgets->circle_y - widgets->circle_width/2,
		widgets->circle_width, widgets->circle_width, 0, 360*64 );
  }
  /* calculate widgets circle_x and circle_y */
  /* split up lat,lon into lat and lon */
  if ( 2 == sscanf ( gtk_entry_get_text ( GTK_ENTRY(widgets->center_entry) ), "%lf,%lf", &lat, &lon ) ) {
    struct LatLon ll;
    VikCoord c;
    gint x, y;

    ll.lat = lat; ll.lon = lon;
    vik_coord_load_from_latlon ( &c, vik_viewport_get_coord_mode ( widgets->vvp ), &ll );
    vik_viewport_coord_to_screen ( widgets->vvp, &c, &x, &y );
    /* TODO: real calculation */
    if ( x > -1000 && y > -1000 && x < (vik_viewport_get_width(widgets->vvp) + 1000) &&
	y < (vik_viewport_get_width(widgets->vvp) + 1000) ) {
      VikCoord c1, c2;
      gdouble pixels_per_meter;

      widgets->circle_x = x;
      widgets->circle_y = y;

      /* determine miles per pixel */
      vik_viewport_screen_to_coord ( widgets->vvp, 0, vik_viewport_get_height(widgets->vvp)/2, &c1 );
      vik_viewport_screen_to_coord ( widgets->vvp, vik_viewport_get_width(widgets->vvp), vik_viewport_get_height(widgets->vvp)/2, &c2 );
      pixels_per_meter = ((gdouble)vik_viewport_get_width(widgets->vvp)) / vik_coord_diff(&c1, &c2);

      /* this is approximate */
      widgets->circle_width = gtk_spin_button_get_value_as_float ( GTK_SPIN_BUTTON(widgets->miles_radius_spin) )
		* METERSPERMILE * pixels_per_meter * 2;

      vik_viewport_draw_arc ( widgets->vvp, widgets->circle_gc, FALSE,
		widgets->circle_x - widgets->circle_width/2,
		widgets->circle_y - widgets->circle_width/2,
		widgets->circle_width, widgets->circle_width, 0, 360*64 );

      widgets->circle_onscreen = TRUE;
    } else
      widgets->circle_onscreen = FALSE;
  }

  /* see if onscreen */
  /* okay */
  vik_viewport_sync ( widgets->vvp );
}

static void datasource_gc_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, gpointer user_data )
{
  datasource_gc_widgets_t *widgets = (datasource_gc_widgets_t *)user_data;
  GtkWidget *num_label, *center_label, *miles_radius_label;
  struct LatLon ll;
  gchar *s_ll;

  num_label = gtk_label_new (_("Number geocaches:"));
  widgets->num_spin = gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new( 20, 1, 1000, 10, 20, 0 )), 10, 0 );
  center_label = gtk_label_new (_("Centered around:"));
  widgets->center_entry = gtk_entry_new();
  miles_radius_label = gtk_label_new ("Miles Radius:");
  widgets->miles_radius_spin = gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new( 5, 1, 1000, 1, 20, 0 )), 25, 1 );

  vik_coord_to_latlon ( vik_viewport_get_center(vvp), &ll );
  s_ll = g_strdup_printf("%f,%f", ll.lat, ll.lon );
  gtk_entry_set_text ( GTK_ENTRY(widgets->center_entry), s_ll );
  g_free ( s_ll );


  widgets->vvp = vvp;
  widgets->circle_gc = vik_viewport_new_gc ( vvp, "#000000", 3 );
  gdk_gc_set_function ( widgets->circle_gc, GDK_INVERT );
  widgets->circle_onscreen = TRUE;
  datasource_gc_draw_circle ( widgets );

  g_signal_connect_swapped ( G_OBJECT(widgets->center_entry), "changed", G_CALLBACK(datasource_gc_draw_circle), widgets );
  g_signal_connect_swapped ( G_OBJECT(widgets->miles_radius_spin), "value-changed", G_CALLBACK(datasource_gc_draw_circle), widgets );

  /* Packing all these widgets */
  GtkBox *box = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
  gtk_box_pack_start ( box, num_label, FALSE, FALSE, 5 );
  gtk_box_pack_start ( box, widgets->num_spin, FALSE, FALSE, 5 );
  gtk_box_pack_start ( box, center_label, FALSE, FALSE, 5 );
  gtk_box_pack_start ( box, widgets->center_entry, FALSE, FALSE, 5 );
  gtk_box_pack_start ( box, miles_radius_label, FALSE, FALSE, 5 );
  gtk_box_pack_start ( box, widgets->miles_radius_spin, FALSE, FALSE, 5 );
  gtk_widget_show_all(dialog);
}

static void datasource_gc_get_cmd_string ( datasource_gc_widgets_t *widgets, gchar **cmd, gchar **input_file_type, gpointer not_used )
{
  //gchar *safe_string = g_shell_quote ( gtk_entry_get_text ( GTK_ENTRY(widgets->center_entry) ) );
  gchar *safe_user = g_shell_quote ( a_preferences_get ( VIKING_GC_PARAMS_NAMESPACE "username")->s );
  gchar *safe_pass = g_shell_quote ( a_preferences_get ( VIKING_GC_PARAMS_NAMESPACE "password")->s );
  gchar *slat, *slon;
  gdouble lat, lon;
  if ( 2 != sscanf ( gtk_entry_get_text ( GTK_ENTRY(widgets->center_entry) ), "%lf,%lf", &lat, &lon ) ) {
    g_warning (_("Broken input - using some defaults"));
    lat = a_vik_get_default_lat();
    lon = a_vik_get_default_long();
  }
  // Convert double as string in C locale
  slat = a_coords_dtostr ( lat );
  slon = a_coords_dtostr ( lon );

  // Unix specific shell commands
  // 1. Remove geocache webpages (maybe be from different location)
  // 2, Gets upto n geocaches as webpages for the specified user in radius r Miles
  // 3. Converts webpages into a single waypoint file, ignoring zero location waypoints '-z'
  //       Probably as they are premium member only geocaches and user is only a basic member
  //  Final output is piped into GPSbabel - hence removal of *html is done at beginning of the command sequence
  *cmd = g_strdup_printf( "rm -f ~/.geo/caches/*html ; %s -P -n%d -r%.1fM -u %s -p %s %s %s ; %s -z ~/.geo/caches/*html ",
			  GC_PROGRAM1,
			  gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(widgets->num_spin) ),
			  gtk_spin_button_get_value_as_float ( GTK_SPIN_BUTTON(widgets->miles_radius_spin) ),
			  safe_user,
			  safe_pass,
			  slat, slon,
			  GC_PROGRAM2 );
  *input_file_type = NULL;
  //g_free ( safe_string );
  g_free ( safe_user );
  g_free ( safe_pass );
  g_free ( slat );
  g_free ( slon );
}

static void datasource_gc_cleanup ( datasource_gc_widgets_t *widgets )
{
  if ( widgets->circle_onscreen ) {
    vik_viewport_draw_arc ( widgets->vvp, widgets->circle_gc, FALSE,
		widgets->circle_x - widgets->circle_width/2,
		widgets->circle_y - widgets->circle_width/2,
		widgets->circle_width, widgets->circle_width, 0, 360*64 );
    vik_viewport_sync( widgets->vvp );
  }
  g_free ( widgets );
}
#endif /* VIK_CONFIG_GEOCACHES */
