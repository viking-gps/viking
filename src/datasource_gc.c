/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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
#include "config.h"
#ifdef VIK_CONFIG_GEOCACHES
#include <string.h>

#include "viking.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"

typedef struct {
  GtkWidget *num_spin;
  GtkWidget *center_entry;
} datasource_gc_widgets_t;


static gpointer datasource_gc_init ( );
static void datasource_gc_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, gpointer user_data );
static void datasource_gc_get_cmd_string ( datasource_gc_widgets_t *widgets, gchar **cmd, gchar **input_type );	
static void datasource_gc_cleanup ( gpointer data );
static gchar *datasource_gc_check_existence ();

VikDataSourceInterface vik_datasource_gc_interface = {
  "Download Geocaches",
  "Geocaching.com Caches",
  VIK_DATASOURCE_SHELL_CMD,
  VIK_DATASOURCE_ADDTOLAYER,
  (VikDataSourceInitFunc)		datasource_gc_init,
  (VikDataSourceCheckExistenceFunc)	datasource_gc_check_existence,
  (VikDataSourceAddSetupWidgetsFunc)	datasource_gc_add_setup_widgets,
  (VikDataSourceGetCmdStringFunc)	datasource_gc_get_cmd_string,
  (VikDataSourceProgressFunc)		NULL,
  (VikDataSourceAddProgressWidgetsFunc)	NULL,
  (VikDataSourceCleanupFunc)		datasource_gc_cleanup
};

static gpointer datasource_gc_init ( )
{
  datasource_gc_widgets_t *widgets = g_malloc(sizeof(*widgets));
  return widgets;
}

static gchar *datasource_gc_check_existence ()
{
  gchar *gcget_location = g_find_program_in_path("gcget");
  if ( gcget_location ) {
    g_free(gcget_location);
    return NULL;
  }
  return g_strdup("Can't find gcget in path! Check that you have installed gcget correctly.");
}

static void datasource_gc_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, gpointer user_data )
{
  datasource_gc_widgets_t *widgets = (datasource_gc_widgets_t *)user_data;
  GtkWidget *num_label, *center_label;
  struct LatLon ll;
  gchar *s_ll;

  num_label = gtk_label_new ("Number geocaches:");
  widgets->num_spin = gtk_spin_button_new ( GTK_ADJUSTMENT(gtk_adjustment_new( 100, 1, 1000, 10, 20, 50 )), 25, 0 );
  center_label = gtk_label_new ("Centered around:");
  widgets->center_entry = gtk_entry_new();

  vik_coord_to_latlon ( vik_viewport_get_center(vvp), &ll );
  s_ll = g_strdup_printf("%f,%f", ll.lat, ll.lon );
  gtk_entry_set_text ( GTK_ENTRY(widgets->center_entry), s_ll );
  g_free ( s_ll );

  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), num_label, FALSE, FALSE, 5 );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), widgets->num_spin, FALSE, FALSE, 5 );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), center_label, FALSE, FALSE, 5 );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), widgets->center_entry, FALSE, FALSE, 5 );
  gtk_widget_show_all(dialog);
}

static void datasource_gc_get_cmd_string ( datasource_gc_widgets_t *widgets, gchar **cmd, gchar **input_type )
{
  /* TODO: we don't actually need GPSBabel... :-) */
  gchar *safe_string = g_shell_quote ( gtk_entry_get_text ( GTK_ENTRY(widgets->center_entry) ) );
  *cmd = g_strdup_printf( "gcget %s %d", safe_string, gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(widgets->num_spin) ) );
  *input_type = g_strdup("geo");
  g_free ( safe_string );
}

static void datasource_gc_cleanup ( gpointer data )
{
  g_free ( data );
}
#endif /* VIK_CONFIG_GEOCACHES */
