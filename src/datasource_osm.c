/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
#include <string.h>

#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"

/**
 * See http://wiki.openstreetmap.org/wiki/API_v0.6#GPS_Traces
 */
#define DOWNLOAD_URL_FMT "api.openstreetmap.org/api/0.6/trackpoints?bbox=%s,%s,%s,%s&page=%d"

typedef struct {
  GtkWidget *page_number;
  VikViewport *vvp;
} datasource_osm_widgets_t;

static gdouble last_page_number = 0;

static gpointer datasource_osm_init( );
static void datasource_osm_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, gpointer user_data );
static void datasource_osm_get_cmd_string ( datasource_osm_widgets_t *widgets, gchar **cmd, gchar **input_file_type );	
static void datasource_osm_cleanup ( gpointer data );

VikDataSourceInterface vik_datasource_osm_interface = {
  N_("OSM traces"),
  N_("OSM traces"),
  VIK_DATASOURCE_URL,
  VIK_DATASOURCE_ADDTOLAYER,
  VIK_DATASOURCE_INPUTTYPE_NONE,
  TRUE,
  TRUE,
  (VikDataSourceInitFunc)		datasource_osm_init,
  (VikDataSourceCheckExistenceFunc)	NULL,
  (VikDataSourceAddSetupWidgetsFunc)	datasource_osm_add_setup_widgets,
  (VikDataSourceGetCmdStringFunc)	datasource_osm_get_cmd_string,
  (VikDataSourceProcessFunc)		NULL,
  (VikDataSourceProgressFunc)		NULL,
  (VikDataSourceAddProgressWidgetsFunc)	NULL,
  (VikDataSourceCleanupFunc)		datasource_osm_cleanup,
  (VikDataSourceOffFunc)                NULL,
};

static gpointer datasource_osm_init ( )
{
  datasource_osm_widgets_t *widgets = g_malloc(sizeof(*widgets));
  return widgets;
}

static void datasource_osm_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, gpointer user_data )
{
  datasource_osm_widgets_t *widgets = (datasource_osm_widgets_t *)user_data;
  GtkWidget *page_number_label;
  page_number_label = gtk_label_new (_("Page number:"));
  widgets->page_number = gtk_spin_button_new_with_range(0, 100, 1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widgets->page_number), last_page_number);
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), page_number_label, FALSE, FALSE, 5 );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), widgets->page_number, FALSE, FALSE, 5 );
  gtk_widget_show_all(dialog);
  /* Keep reference to viewport */
  widgets->vvp = vvp;
}

static void datasource_osm_get_cmd_string ( datasource_osm_widgets_t *widgets, gchar **cmd, gchar **input_file_type )
{
  int page = 0;
  gdouble min_lat, max_lat, min_lon, max_lon;
  gchar sminlon[G_ASCII_DTOSTR_BUF_SIZE];
  gchar smaxlon[G_ASCII_DTOSTR_BUF_SIZE];
  gchar sminlat[G_ASCII_DTOSTR_BUF_SIZE];
  gchar smaxlat[G_ASCII_DTOSTR_BUF_SIZE];

  /* get Viewport bounding box */
  vik_viewport_get_min_max_lat_lon ( widgets->vvp, &min_lat, &max_lat, &min_lon, &max_lon );

  /* Convert as LANG=C double representation */
  g_ascii_dtostr (sminlon, G_ASCII_DTOSTR_BUF_SIZE, min_lon);
  g_ascii_dtostr (smaxlon, G_ASCII_DTOSTR_BUF_SIZE, max_lon);
  g_ascii_dtostr (sminlat, G_ASCII_DTOSTR_BUF_SIZE, min_lat);
  g_ascii_dtostr (smaxlat, G_ASCII_DTOSTR_BUF_SIZE, max_lat);

  /* Retrieve the specified page number */
  last_page_number = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widgets->page_number));
  page = last_page_number;

  *cmd = g_strdup_printf( DOWNLOAD_URL_FMT, sminlon, sminlat, smaxlon, smaxlat, page );
  *input_file_type = g_strdup("gpx");
}

static void datasource_osm_cleanup ( gpointer data )
{
  g_free ( data );
}

