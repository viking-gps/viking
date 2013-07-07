/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2012, Rob Norris <rw_norris@hotmail.com>
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
#include <limits.h>

#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <expat.h>

#include "viking.h"
#include "gpx.h"
#include "acquire.h"
#include "osm-traces.h"
#include "preferences.h"
#include "curl_download.h"
#include "datasource_gps.h"
#include "bbox.h"

/**
 * See http://wiki.openstreetmap.org/wiki/API_v0.6#GPS_Traces
 */
#define DS_OSM_TRACES_GPX_URL_FMT "api.openstreetmap.org/api/0.6/gpx/%d/data"
#define DS_OSM_TRACES_GPX_FILES "api.openstreetmap.org/api/0.6/user/gpx_files"

typedef struct {
	GtkWidget *user_entry;
	GtkWidget *password_entry;
	// NB actual user and password values are stored in oms-traces.c
	VikViewport *vvp;
} datasource_osm_my_traces_t;

static gpointer datasource_osm_my_traces_init( );
static void datasource_osm_my_traces_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, gpointer user_data );
static void datasource_osm_my_traces_get_cmd_string ( gpointer user_data, gchar **args, gchar **extra, DownloadMapOptions *options );
static gboolean datasource_osm_my_traces_process  ( VikTrwLayer *vtl, const gchar *cmd, const gchar *extra, BabelStatusFunc status_cb, acq_dialog_widgets_t *adw, DownloadMapOptions *options_unused );
static void datasource_osm_my_traces_cleanup ( gpointer data );

VikDataSourceInterface vik_datasource_osm_my_traces_interface = {
  N_("OSM My Traces"),
  N_("OSM My Traces"),
  VIK_DATASOURCE_MANUAL_LAYER_MANAGEMENT, // we'll do this ourselves
  VIK_DATASOURCE_INPUTTYPE_NONE,
  TRUE,
  TRUE,
  FALSE, // Don't use thread method
  (VikDataSourceInitFunc)		    datasource_osm_my_traces_init,
  (VikDataSourceCheckExistenceFunc)	NULL,
  (VikDataSourceAddSetupWidgetsFunc)datasource_osm_my_traces_add_setup_widgets,
  (VikDataSourceGetCmdStringFunc)	datasource_osm_my_traces_get_cmd_string,
  (VikDataSourceProcessFunc)		datasource_osm_my_traces_process,
  (VikDataSourceProgressFunc)		NULL,
  (VikDataSourceAddProgressWidgetsFunc)	NULL,
  (VikDataSourceCleanupFunc)		datasource_osm_my_traces_cleanup,
  (VikDataSourceOffFunc)            NULL,

  NULL,
  0,
  NULL,
  NULL,
  0
};

static gpointer datasource_osm_my_traces_init ( )
{
  datasource_osm_my_traces_t *data = g_malloc(sizeof(*data));
  // Reuse GPS functions
  // Haven't been able to get the thread method to work reliably (or get progress feedback)
  // So thread version is disabled ATM
  /*
  if ( vik_datasource_osm_my_traces_interface.is_thread ) {
	  vik_datasource_osm_my_traces_interface.progress_func = datasource_gps_progress;
	  vik_datasource_osm_my_traces_interface.add_progress_widgets_func = datasource_gps_add_progress_widgets;
  }
  */
  return data;
}

static void datasource_osm_my_traces_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, gpointer user_data )
{
	datasource_osm_my_traces_t *data = (datasource_osm_my_traces_t *)user_data;

	GtkWidget *user_label;
	GtkWidget *password_label;
	user_label = gtk_label_new(_("Username:"));
	data->user_entry = gtk_entry_new();

	gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), user_label, FALSE, FALSE, 0 );
	gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), data->user_entry, FALSE, FALSE, 0 );
	gtk_widget_set_tooltip_markup ( GTK_WIDGET(data->user_entry), _("The email or username used to login to OSM") );

	password_label = gtk_label_new ( _("Password:") );
	data->password_entry = gtk_entry_new ();

	gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), password_label, FALSE, FALSE, 0 );
	gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), data->password_entry, FALSE, FALSE, 0 );
	gtk_widget_set_tooltip_markup ( GTK_WIDGET(data->password_entry), _("The password used to login to OSM") );

	osm_login_widgets (data->user_entry, data->password_entry);
	gtk_widget_show_all ( dialog );

	/* Keep reference to viewport */
	data->vvp = vvp;
}

static void datasource_osm_my_traces_get_cmd_string ( gpointer user_data, gchar **args, gchar **extra, DownloadMapOptions *options )
{
	datasource_osm_my_traces_t *data = (datasource_osm_my_traces_t*) user_data;

    /* overwrite authentication info */
	osm_set_login ( gtk_entry_get_text ( GTK_ENTRY(data->user_entry) ),
	                gtk_entry_get_text ( GTK_ENTRY(data->password_entry) ) );

	// If going to use the values passed back into the process function parameters then these need to be set.
	// But ATM we aren't
	*args = NULL;
	*extra = NULL;
	options = NULL;
}

typedef enum {
	tt_unknown = 0,
	tt_osm,
	tt_gpx_file,
	tt_gpx_file_desc,
	tt_gpx_file_tag,
} xtag_type;

typedef struct {
	xtag_type tag_type;              /* enum from above for this tag */
	const char *tag_name;           /* xpath-ish tag name */
} xtag_mapping;

typedef struct {
	guint id;
	gchar *name;
	gchar *vis;
	gchar *desc;
	struct LatLon ll;
	gboolean in_current_view; // Is the track LatLon start within the current viewport
                              // This is useful in deciding whether to download a track or not
	// ATM Only used for display - may want to convert to a time_t for other usage
	gchar *timestamp;
	// user made up tags - not being used yet - would be nice to sort/select on these but display will get complicated
	// GList *tag_list;
} gpx_meta_data_t;

static gpx_meta_data_t *new_gpx_meta_data_t()
{
	gpx_meta_data_t *ret;

	ret = (gpx_meta_data_t *)g_malloc(sizeof(gpx_meta_data_t));
	ret->id = 0;
	ret->name = NULL;
	ret->vis  = NULL;
	ret->desc = NULL;
	ret->ll.lat = 0.0;
	ret->ll.lon = 0.0;
	ret->in_current_view = FALSE;
	ret->timestamp = NULL;

	return ret;
}

static void free_gpx_meta_data ( gpx_meta_data_t *data, gpointer userdata )
{
	g_free(data->name);
	g_free(data->vis);
	g_free(data->desc);
	g_free(data->timestamp);
}

static void free_gpx_meta_data_list (GList *list)
{
	g_list_foreach (list, (GFunc)free_gpx_meta_data, NULL);
	g_list_free (list);
}

static gpx_meta_data_t *copy_gpx_meta_data_t (gpx_meta_data_t *src)
{
	gpx_meta_data_t *dest = new_gpx_meta_data_t();

	dest->id = src->id;
	dest->name = g_strdup(src->name);
	dest->vis  = g_strdup(src->vis);
	dest->desc = g_strdup(src->desc);
	dest->ll.lat = src->ll.lat;
	dest->ll.lon = src->ll.lon;
	dest->in_current_view = src->in_current_view;
	dest->timestamp = g_strdup(src->timestamp);

	return dest;
}

typedef struct {
	//GString *xpath;
	GString *c_cdata;
	xtag_type current_tag;
	gpx_meta_data_t *current_gpx_meta_data;
	GList *list_of_gpx_meta_data;
} xml_data;

// Same as the gpx.c function
static const char *get_attr ( const char **attr, const char *key )
{
  while ( *attr ) {
    if ( strcmp(*attr,key) == 0 )
      return *(attr + 1);
    attr += 2;
  }
  return NULL;
}

// ATM don't care about actual path as tags are all unique
static xtag_mapping xtag_path_map[] = {
	{ tt_osm,           "osm" },
	{ tt_gpx_file,      "gpx_file" },
	{ tt_gpx_file_desc, "description" },
	{ tt_gpx_file_tag,  "tag" },
};

static xtag_type get_tag ( const char *t )
{
	xtag_mapping *tm;
	for (tm = xtag_path_map; tm->tag_type != 0; tm++)
		if (0 == strcmp(tm->tag_name, t))
			return tm->tag_type;
	return tt_unknown;
}

static void gpx_meta_data_start ( xml_data *xd, const char *el, const char **attr )
{
	const gchar *tmp;
	gchar buf[G_ASCII_DTOSTR_BUF_SIZE];
	buf[0] = '\0';

	// Don't need to build a path - we can use the tag directly
	//g_string_append_c ( xd->xpath, '/' );
	//g_string_append ( xd->xpath, el );
	//xd->current_tag = get_tag ( xd->xpath->str );
	xd->current_tag = get_tag ( el );
	switch ( xd->current_tag ) {
	case tt_gpx_file:
		if ( xd->current_gpx_meta_data )
			free_gpx_meta_data ( xd->current_gpx_meta_data, NULL );
		xd->current_gpx_meta_data = new_gpx_meta_data_t();

		if ( ( tmp = get_attr ( attr, "id" ) ) )
			xd->current_gpx_meta_data->id = atoi ( tmp );

		if ( ( tmp = get_attr ( attr, "name" ) ) )
			xd->current_gpx_meta_data->name = g_strdup ( tmp );

		if ( ( tmp = get_attr ( attr, "lat" ) ) ) {
			g_strlcpy ( buf, tmp, sizeof (buf) );
			xd->current_gpx_meta_data->ll.lat = g_ascii_strtod ( buf, NULL );
		}

		if ( ( tmp = get_attr ( attr, "lon" ) ) ) {
			g_strlcpy ( buf, tmp, sizeof (buf) );
			xd->current_gpx_meta_data->ll.lon = g_ascii_strtod ( buf, NULL );
		}

		if ( ( tmp = get_attr ( attr, "visibility" ) ) )
			xd->current_gpx_meta_data->vis = g_strdup ( tmp );

		if ( ( tmp = get_attr ( attr, "timestamp" ) ) )
			xd->current_gpx_meta_data->timestamp = g_strdup ( tmp );

		g_string_erase ( xd->c_cdata, 0, -1 ); // clear the cdata buffer
		break;
	case tt_gpx_file_desc:
	case tt_gpx_file_tag:
		g_string_erase ( xd->c_cdata, 0, -1 ); // clear the cdata buffer
		break;
	default:
		g_string_erase ( xd->c_cdata, 0, -1 ); // clear the cdata buffer
		break;
	}
}

static void gpx_meta_data_end ( xml_data *xd, const char *el )
{
	//g_string_truncate ( xd->xpath, xd->xpath->len - strlen(el) - 1 );
	//switch ( xd->current_tag ) {
	switch ( get_tag ( el ) ) {
	case tt_gpx_file: {
		// End of the individual file metadata, thus save what we have read in to the list
		// Copy it so we can reference it
		gpx_meta_data_t *current = copy_gpx_meta_data_t ( xd->current_gpx_meta_data );
		// Stick in the list
		xd->list_of_gpx_meta_data = g_list_prepend(xd->list_of_gpx_meta_data, current);
		g_string_erase ( xd->c_cdata, 0, -1 );
		break;
	}
	case tt_gpx_file_desc:
		// Store the description:
		if ( xd->current_gpx_meta_data ) {
			// NB Limit description size as it's displayed on a single line
			// Hopefully this will prevent the dialog getting too wide...
			xd->current_gpx_meta_data->desc = g_strndup ( xd->c_cdata->str, 63 );
		}
		g_string_erase ( xd->c_cdata, 0, -1 );
		break;
	case tt_gpx_file_tag:
		// One day do something with this...
		g_string_erase ( xd->c_cdata, 0, -1 );
		break;
	default:
		break;
	}
}

static void gpx_meta_data_cdata ( xml_data *xd, const XML_Char *s, int len )
{
	switch ( xd->current_tag ) {
    case tt_gpx_file_desc:
    case tt_gpx_file_tag:
		g_string_append_len ( xd->c_cdata, s, len );
		break;
	default: break;  // ignore cdata from other things
	}
}

static gboolean read_gpx_files_metadata_xml ( gchar *tmpname, xml_data *xd )
{
	FILE *ff = g_fopen (tmpname, "r");
	if ( !ff )
		return FALSE;

	XML_Parser parser = XML_ParserCreate(NULL);
	enum XML_Status status = XML_STATUS_ERROR;

	XML_SetElementHandler(parser, (XML_StartElementHandler) gpx_meta_data_start, (XML_EndElementHandler) gpx_meta_data_end);
	XML_SetUserData(parser, xd);
	XML_SetCharacterDataHandler(parser, (XML_CharacterDataHandler) gpx_meta_data_cdata);

	gchar buf[4096];

	int done=0, len;
	while (!done) {
		len = fread(buf, 1, sizeof(buf)-7, ff);
		done = feof(ff) || !len;
		status = XML_Parse(parser, buf, len, done);
	}

	XML_ParserFree (parser);

	fclose  ( ff );

	return status != XML_STATUS_ERROR;
}

static GList *select_from_list (GtkWindow *parent, GList *list, const gchar *title, const gchar *msg )
{
	GtkTreeIter iter;
	GtkCellRenderer *renderer;
	GtkWidget *view;
	gchar *latlon_string;
	int column_runner;

	GtkWidget *dialog = gtk_dialog_new_with_buttons (title,
													 parent,
													 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
													 GTK_STOCK_CANCEL,
													 GTK_RESPONSE_REJECT,
													 GTK_STOCK_OK,
													 GTK_RESPONSE_ACCEPT,
													 NULL);
	/* When something is selected then OK */
	gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
	GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
	/* Default to not apply - as initially nothing is selected! */
	response_w = gtk_dialog_get_widget_for_response ( GTK_DIALOG(dialog), GTK_RESPONSE_REJECT );
#endif
	GtkWidget *label = gtk_label_new ( msg );
	GtkTreeStore *store = gtk_tree_store_new ( 6, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN );
	GList *list_runner = list;
	while (list_runner) {
		gpx_meta_data_t *gpx_meta_data = (gpx_meta_data_t *)list_runner->data;
		// To keep display compact three digits of precision for lat/lon should be plenty
		latlon_string = g_strdup_printf("(%.3f,%.3f)", gpx_meta_data->ll.lat, gpx_meta_data->ll.lon);
		gtk_tree_store_append(store, &iter, NULL);
		gtk_tree_store_set ( store, &iter,
		                     0, gpx_meta_data->name,
		                     1, gpx_meta_data->desc,
		                     2, gpx_meta_data->timestamp,
		                     3, latlon_string,
		                     4, gpx_meta_data->vis,
		                     5, gpx_meta_data->in_current_view,
		                     -1 );
		list_runner = g_list_next ( list_runner );
		g_free ( latlon_string );
	}

	view = gtk_tree_view_new();
	renderer = gtk_cell_renderer_text_new();
	column_runner = 0;
	GtkTreeViewColumn *column;

	column = gtk_tree_view_column_new_with_attributes ( _("Name"), renderer, "text", column_runner, NULL);
	gtk_tree_view_column_set_sort_column_id (column, column_runner);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	column_runner++;
	column = gtk_tree_view_column_new_with_attributes ( _("Description"), renderer, "text", column_runner, NULL);
	gtk_tree_view_column_set_sort_column_id (column, column_runner);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	column_runner++;
	column = gtk_tree_view_column_new_with_attributes ( _("Time"), renderer, "text", column_runner, NULL);
	gtk_tree_view_column_set_sort_column_id (column, column_runner);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	column_runner++;
	column = gtk_tree_view_column_new_with_attributes ( _("Lat/Lon"), renderer, "text", column_runner, NULL);
	gtk_tree_view_column_set_sort_column_id (column, column_runner);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	column_runner++;
	column = gtk_tree_view_column_new_with_attributes ( _("Privacy"), renderer, "text", column_runner, NULL); // AKA Visibility
	gtk_tree_view_column_set_sort_column_id (column, column_runner);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	GtkCellRenderer *renderer_toggle = gtk_cell_renderer_toggle_new ();
	g_object_set (G_OBJECT (renderer_toggle), "activatable", FALSE, NULL); // No user action - value is just for display
	column_runner++;
	column = gtk_tree_view_column_new_with_attributes ( _("Within Current View"), renderer_toggle, "active", column_runner, NULL);
	gtk_tree_view_column_set_sort_column_id (column, column_runner);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(store));
	gtk_tree_selection_set_mode( gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), GTK_SELECTION_MULTIPLE );
	g_object_unref(store);

	GtkWidget *scrolledwindow = gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_container_add ( GTK_CONTAINER(scrolledwindow), view );

	gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), scrolledwindow, TRUE, TRUE, 0);

	// Ensure a reasonable number of items are shown, but let the width be automatically sized
	gtk_widget_set_size_request ( dialog, -1, 400) ;
	gtk_widget_show_all ( dialog );

	if ( response_w )
		gtk_widget_grab_focus ( response_w );

	while ( gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT ) {

		// Possibily not the fastest method but we don't have thousands of entries to process...
		GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
		GList *selected = NULL;

		//  because we don't store the full data in the gtk model, we have to scan & look it up
		if ( gtk_tree_model_get_iter_first( GTK_TREE_MODEL(store), &iter) ) {
			do {
				if ( gtk_tree_selection_iter_is_selected ( selection, &iter ) ) {
					// For every selected item,
					// compare the name from the displayed view to every gpx entry to find the gpx this selection represents
					gchar* name;
					gtk_tree_model_get (GTK_TREE_MODEL(store), &iter, 0, &name, -1 );
					// I believe the name of these items to be always unique
					list_runner = list;
					while (list_runner) {
						if ( !strcmp ( ((gpx_meta_data_t*)list_runner->data)->name, name ) ) {
							gpx_meta_data_t *copied = copy_gpx_meta_data_t (list_runner->data);
							selected = g_list_prepend (selected, copied);
							break;
						}
						list_runner = g_list_next ( list_runner );
					}
				}
			}
			while ( gtk_tree_model_iter_next ( GTK_TREE_MODEL(store), &iter ) );
		}

		if ( selected ) {
			gtk_widget_destroy ( dialog );
			return selected;
		}
		a_dialog_error_msg(parent, _("Nothing was selected"));
	}
	gtk_widget_destroy ( dialog );
	return NULL;
}

static void none_found ( GtkWindow *gw )
{
	GtkWidget *dialog = NULL;

	dialog = gtk_dialog_new_with_buttons ( "", gw, 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL );
	gtk_window_set_title(GTK_WINDOW(dialog), _("GPS Traces"));

	GtkWidget *search_label = gtk_label_new(_("None found!"));
	gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), search_label, FALSE, FALSE, 5 );
	gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
	gtk_widget_show_all(dialog);

	gtk_dialog_run ( GTK_DIALOG(dialog) );
	gtk_widget_destroy(dialog);
}

/**
 * For each track - mark whether the start is in within the viewport
 */
static void set_in_current_view_property ( VikTrwLayer *vtl, datasource_osm_my_traces_t *data, GList *gl )
{
	gdouble min_lat, max_lat, min_lon, max_lon;
	/* get Viewport bounding box */
	vik_viewport_get_min_max_lat_lon ( data->vvp, &min_lat, &max_lat, &min_lon, &max_lon );

	LatLonBBox bbox;
	bbox.north = max_lat;
	bbox.east = max_lon;
	bbox.south = min_lat;
	bbox.west = min_lon;

	GList *iterator = gl;
	while ( iterator ) {
		gpx_meta_data_t* gmd = (gpx_meta_data_t*)iterator->data;
		// Convert point position into a 'fake' bounding box
		// TODO - probably should have function to see if point is within bounding box
		//   rather than constructing this fake bounding box for the test
		LatLonBBox gmd_bbox;
		gmd_bbox.north = gmd->ll.lat;
		gmd_bbox.east = gmd->ll.lon;
		gmd_bbox.south = gmd->ll.lat;
		gmd_bbox.west = gmd->ll.lon;

		if ( BBOX_INTERSECT ( bbox, gmd_bbox ) )
			gmd->in_current_view = TRUE;

		iterator = g_list_next ( iterator );
	}
}

static gboolean datasource_osm_my_traces_process ( VikTrwLayer *vtl, const gchar *cmd, const gchar *extra, BabelStatusFunc status_cb, acq_dialog_widgets_t *adw, DownloadMapOptions *options_unused )
{
	//datasource_osm_my_traces_t *data = (datasource_osm_my_traces_t *)adw->user_data;

	gboolean result;

	gchar *user_pass = osm_get_login();

	DownloadMapOptions options = { FALSE, FALSE, NULL, 2, NULL, user_pass }; // Allow a couple of redirects

	xml_data *xd = g_malloc ( sizeof (xml_data) );
	//xd->xpath = g_string_new ( "" );
	xd->c_cdata = g_string_new ( "" );
	xd->current_tag = tt_unknown;
	xd->current_gpx_meta_data = new_gpx_meta_data_t();
	xd->list_of_gpx_meta_data = NULL;

	gchar *tmpname = a_download_uri_to_tmp_file ( DS_OSM_TRACES_GPX_FILES, &options );
	result = read_gpx_files_metadata_xml ( tmpname, xd );
	// Test already downloaded metadata file: eg:
	//result = read_gpx_files_metadata_xml ( "/tmp/viking-download.GI47PW", xd );

	if ( tmpname ) {
		g_remove ( tmpname );
		g_free ( tmpname );
	}

	if ( ! result )
		return FALSE;

	if ( g_list_length ( xd->list_of_gpx_meta_data ) == 0 ) {
		if (!vik_datasource_osm_my_traces_interface.is_thread)
			none_found ( GTK_WINDOW(adw->vw) );
		return FALSE;
	}

	xd->list_of_gpx_meta_data = g_list_reverse ( xd->list_of_gpx_meta_data );

	set_in_current_view_property ( vtl, adw->user_data, xd->list_of_gpx_meta_data );

    if (vik_datasource_osm_my_traces_interface.is_thread) gdk_threads_enter();
	GList *selected = select_from_list ( GTK_WINDOW(adw->vw), xd->list_of_gpx_meta_data, "Select GPS Traces", "Select the GPS traces you want to add." );
    if (vik_datasource_osm_my_traces_interface.is_thread) gdk_threads_leave();

	// If passed in on an existing layer - we will create everything into that.
	//  thus with many differing gpx's - this will combine all waypoints into this single layer!
	// Hence the preference is to create multiple layers
	//  and so this creation of the layers must be managed here

	gboolean create_new_layer = ( !vtl );

	// Only update the screen on the last layer acquired
	VikTrwLayer *vtl_last = vtl;
	gboolean got_something = FALSE;

	GList *selected_iterator = selected;
	while ( selected_iterator ) {

		VikTrwLayer *vtlX = vtl;

		if ( create_new_layer ) {
			// Have data but no layer - so create one
			vtlX = VIK_TRW_LAYER ( vik_layer_create ( VIK_LAYER_TRW, vik_window_viewport(adw->vw), NULL, FALSE ) );
			if ( ((gpx_meta_data_t*)selected_iterator->data)->name )
				vik_layer_rename ( VIK_LAYER ( vtlX ), ((gpx_meta_data_t*)selected_iterator->data)->name );
			else
				vik_layer_rename ( VIK_LAYER ( vtlX ), _("My OSM Traces") );
		}

		result = FALSE;
		gint gpx_id = ((gpx_meta_data_t*)selected_iterator->data)->id;
		if ( gpx_id ) {
			gchar *url = g_strdup_printf ( DS_OSM_TRACES_GPX_URL_FMT, gpx_id );

			result = a_babel_convert_from_url ( vtlX, url, "gpx", status_cb, adw, &options );
			// TODO investigate using a progress bar:
			// http://developer.gnome.org/gtk/2.24/GtkProgressBar.html

			got_something = got_something || result;
			// TODO feedback to UI to inform which traces failed
			if ( !result )
				g_warning ( _("Unable to get trace: %s"), url );
		}

		if ( result ) {
			// Can use the layer
			vik_aggregate_layer_add_layer ( vik_layers_panel_get_top_layer (adw->vlp), VIK_LAYER(vtlX) );
			// Move to area of the track
			vik_layer_post_read ( VIK_LAYER(vtlX), vik_window_viewport(adw->vw), TRUE );
			vik_trw_layer_auto_set_view ( vtlX, vik_window_viewport(adw->vw) );
			vtl_last = vtlX;
		}
		else if ( create_new_layer ) {
			// Layer not needed as no data has been acquired
			g_object_unref ( vtlX );
		}

		selected_iterator = g_list_next ( selected_iterator );
	}

	// Free memory
	if ( xd->current_gpx_meta_data )
		free_gpx_meta_data ( xd->current_gpx_meta_data, NULL );
	g_free ( xd->current_gpx_meta_data );
	free_gpx_meta_data_list ( xd->list_of_gpx_meta_data );
	free_gpx_meta_data_list ( selected );
	g_free ( xd );
	g_free ( user_pass );

	// Would prefer to keep the update in acquire.c,
	//  however since we may create the layer - need to do the update here
	if ( got_something )
		vik_layer_emit_update ( VIK_LAYER(vtl_last) );

	// ATM The user is only informed if all getting *all* of the traces failed
	if ( selected )
		result = got_something;
	else
		// Process was cancelled but need to return that it proceeded as expected
		result = TRUE;

	return result;
}

static void datasource_osm_my_traces_cleanup ( gpointer data )
{
	g_free ( data );
}
