/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
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
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "viking.h"
#include "acquire.h"
#include "babel.h"
#include "geojson.h"

typedef struct {
	GtkWidget *files;
	GSList *filelist;  // Files selected
} datasource_geojson_user_data_t;

// The last used directory
static gchar *last_folder_uri = NULL;

static gpointer datasource_geojson_init ( acq_vik_t *avt );
static void datasource_geojson_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, gpointer user_data );
static void datasource_geojson_get_cmd_string ( datasource_geojson_user_data_t *ud, gchar **cmd, gchar **input_file_type, DownloadMapOptions *options );
static gboolean datasource_geojson_process ( VikTrwLayer *vtl, const gchar *cmd, const gchar *extra, BabelStatusFunc status_cb, acq_dialog_widgets_t *adw, gpointer not_used );
static void datasource_geojson_cleanup ( gpointer data );

VikDataSourceInterface vik_datasource_geojson_interface = {
	N_("Acquire from GeoJSON"),
	N_("GeoJSON"),
	VIK_DATASOURCE_AUTO_LAYER_MANAGEMENT,
	VIK_DATASOURCE_INPUTTYPE_NONE,
	TRUE,
	FALSE, // We should be able to see the data on the screen so no point in keeping the dialog open
	FALSE, // Not thread method - open each file in the main loop
	(VikDataSourceInitFunc)               datasource_geojson_init,
	(VikDataSourceCheckExistenceFunc)     NULL,
	(VikDataSourceAddSetupWidgetsFunc)    datasource_geojson_add_setup_widgets,
	(VikDataSourceGetCmdStringFunc)       datasource_geojson_get_cmd_string,
	(VikDataSourceProcessFunc)            datasource_geojson_process,
	(VikDataSourceProgressFunc)           NULL,
	(VikDataSourceAddProgressWidgetsFunc) NULL,
	(VikDataSourceCleanupFunc)            datasource_geojson_cleanup,
	(VikDataSourceOffFunc)                NULL,
	NULL,
	0,
	NULL,
	NULL,
	0
};

static gpointer datasource_geojson_init ( acq_vik_t *avt )
{
	datasource_geojson_user_data_t *user_data = g_malloc(sizeof(datasource_geojson_user_data_t));
	user_data->filelist = NULL;
	return user_data;
}

static void datasource_geojson_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, gpointer user_data )
{
	datasource_geojson_user_data_t *ud = (datasource_geojson_user_data_t *)user_data;

	ud->files = gtk_file_chooser_widget_new ( GTK_FILE_CHOOSER_ACTION_OPEN );

	// try to make it a nice size - otherwise seems to default to something impractically small
	gtk_window_set_default_size ( GTK_WINDOW (dialog) , 600, 300 );

	if ( last_folder_uri )
		gtk_file_chooser_set_current_folder_uri ( GTK_FILE_CHOOSER(ud->files), last_folder_uri );

	GtkFileChooser *chooser = GTK_FILE_CHOOSER ( ud->files );

	// Add filters
	GtkFileFilter *filter;
	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name ( filter, _("All") );
	gtk_file_filter_add_pattern ( filter, "*" );
	gtk_file_chooser_add_filter ( chooser, filter );

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name ( filter, _("GeoJSON") );
	gtk_file_filter_add_pattern ( filter, "*.geojson" );
	gtk_file_chooser_add_filter ( chooser, filter );

	// Default to geojson
	gtk_file_chooser_set_filter ( chooser, filter );

	// Allow selecting more than one
	gtk_file_chooser_set_select_multiple ( chooser, TRUE );

	// Packing all widgets
	GtkBox *box = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	gtk_box_pack_start ( box, ud->files, TRUE, TRUE, 0 );

	gtk_widget_show_all ( dialog );
}

static void datasource_geojson_get_cmd_string ( datasource_geojson_user_data_t *userdata, gchar **cmd, gchar **input_file_type, DownloadMapOptions *options )
{
	// Retrieve the files selected
	userdata->filelist = gtk_file_chooser_get_filenames ( GTK_FILE_CHOOSER(userdata->files) ); // Not reusable !!

	// Memorize the directory for later reuse
	g_free ( last_folder_uri );
	last_folder_uri = gtk_file_chooser_get_current_folder_uri ( GTK_FILE_CHOOSER(userdata->files) );
	last_folder_uri = g_strdup ( last_folder_uri );

	// TODO Memorize the file filter for later reuse?
	//GtkFileFilter *filter = gtk_file_chooser_get_filter ( GTK_FILE_CHOOSER(userdata->files) );

	// return some value so *thread* processing will continue
	*cmd = g_strdup ("fake command"); // Not really used, thus no translations
}

/**
 * Process selected files and try to generate waypoints storing them in the given vtl
 */
static gboolean datasource_geojson_process ( VikTrwLayer *vtl, const gchar *cmd, const gchar *extra, BabelStatusFunc status_cb, acq_dialog_widgets_t *adw, gpointer not_used )
{
	datasource_geojson_user_data_t *user_data = (datasource_geojson_user_data_t *)adw->user_data;

	// Process selected files
	GSList *cur_file = user_data->filelist;
	while ( cur_file ) {
		gchar *filename = cur_file->data;

		gchar *gpx_filename = a_geojson_import_to_gpx ( filename );
		if ( gpx_filename ) {
			// Important that this process is run in the main thread
			vik_window_open_file ( adw->vw, gpx_filename, FALSE );
			// Delete the temporary file
			g_remove (gpx_filename);
			g_free (gpx_filename);
		}
		else {
			gchar* msg = g_strdup_printf ( _("Unable to import from: %s"), filename );
			vik_window_statusbar_update ( adw->vw, msg, VIK_STATUSBAR_INFO );
			g_free (msg);
		}

		g_free ( filename );
		cur_file = g_slist_next ( cur_file );
	}

	// Free memory
	g_slist_free ( user_data->filelist );

	// No failure
	return TRUE;
}

static void datasource_geojson_cleanup ( gpointer data )
{
	g_free ( data );
}
