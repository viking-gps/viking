/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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
#define DOWNLOAD_URL_FMT "https://api.openstreetmap.org/api/0.6/trackpoints?bbox=%s,%s,%s,%s&page=%d"

typedef struct {
  GtkWidget *page_number;
  VikViewport *vvp;
} datasource_osm_widgets_t;

static gdouble last_page_number = 0;

static gpointer datasource_osm_init ( acq_vik_t *avt );
static void datasource_osm_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, gpointer user_data );
static void datasource_osm_get_process_options ( datasource_osm_widgets_t *widgets, ProcessOptions *po, DownloadFileOptions *options, const gchar *notused1, const gchar *notused2);
static void datasource_osm_cleanup ( gpointer data );

VikDataSourceInterface vik_datasource_osm_interface = {
  N_("OSM traces"),
  N_("OSM traces"),
  VIK_DATASOURCE_AUTO_LAYER_MANAGEMENT,
  VIK_DATASOURCE_INPUTTYPE_NONE,
  TRUE,
  TRUE,
  TRUE,
  (VikDataSourceInitFunc)		datasource_osm_init,
  (VikDataSourceCheckExistenceFunc)	NULL,
  (VikDataSourceAddSetupWidgetsFunc)	datasource_osm_add_setup_widgets,
  (VikDataSourceGetProcessOptionsFunc)  datasource_osm_get_process_options,
  (VikDataSourceProcessFunc)            a_babel_convert_from,
  (VikDataSourceProgressFunc)		NULL,
  (VikDataSourceAddProgressWidgetsFunc)	NULL,
  (VikDataSourceCleanupFunc)		datasource_osm_cleanup,
  (VikDataSourceOffFunc)                NULL,

  NULL,
  0,
  NULL,
  NULL,
  0
};

static gpointer datasource_osm_init ( acq_vik_t *avt )
{
  datasource_osm_widgets_t *widgets = g_malloc(sizeof(*widgets));
  /* Keep reference to viewport */
  widgets->vvp = avt->vvp;
  return widgets;
}

static void datasource_osm_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, gpointer user_data )
{
  datasource_osm_widgets_t *widgets = (datasource_osm_widgets_t *)user_data;
  GtkWidget *page_number_label;
  page_number_label = gtk_label_new (_("Page number:"));
  widgets->page_number = gtk_spin_button_new_with_range(0, 100, 1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widgets->page_number), last_page_number);

  /* Packing all widgets */
  GtkBox *box = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
  gtk_box_pack_start ( box, page_number_label, FALSE, FALSE, 5 );
  gtk_box_pack_start ( box, widgets->page_number, FALSE, FALSE, 5 );
  gtk_widget_show_all(dialog);
}

static void datasource_osm_get_process_options ( datasource_osm_widgets_t *widgets, ProcessOptions *po, DownloadFileOptions *options, const gchar *notused1, const gchar *notused2)
{
  int page = 0;
  gdouble min_lat, max_lat, min_lon, max_lon;
  gchar sminlon[COORDS_STR_BUFFER_SIZE];
  gchar smaxlon[COORDS_STR_BUFFER_SIZE];
  gchar sminlat[COORDS_STR_BUFFER_SIZE];
  gchar smaxlat[COORDS_STR_BUFFER_SIZE];

  /* get Viewport bounding box */
  vik_viewport_get_min_max_lat_lon ( widgets->vvp, &min_lat, &max_lat, &min_lon, &max_lon );

  /* Convert as LANG=C double representation */
  a_coords_dtostr_buffer ( min_lon, sminlon );
  a_coords_dtostr_buffer ( max_lon, smaxlon );
  a_coords_dtostr_buffer ( min_lat, sminlat );
  a_coords_dtostr_buffer ( max_lat, smaxlat );

  /* Retrieve the specified page number */
  last_page_number = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widgets->page_number));
  page = last_page_number;

  // NB Download is of GPX type
  po->url = g_strdup_printf( DOWNLOAD_URL_FMT, sminlon, sminlat, smaxlon, smaxlat, page );
  options = NULL; // i.e. use the default download settings
}

static void datasource_osm_cleanup ( gpointer data )
{
  g_free ( data );
}

