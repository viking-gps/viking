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

#include <gtk/gtk.h>

#include "viking.h"
#include "babel.h"
#include "gpx.h"
#include "acquire.h"

typedef struct {
  GtkWidget *file;
  GtkWidget *type;
} datasource_file_widgets_t;

/* The last used directory */
static gchar *last_folder_uri = NULL;

/* The last used file filter */
/* Nb: we use a complex strategy for this because the UI is rebuild each
 time, so it is not possible to reuse directly the GtkFileFilter as they are
 differents. */
static BabelFile *last_file_filter = NULL;

/* The last file format selected */
static int last_type = 0;

static gpointer datasource_file_init ( acq_vik_t *avt );
static void datasource_file_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, gpointer user_data );
static void datasource_file_get_cmd_string ( datasource_file_widgets_t *widgets, gchar **cmd, gchar **input_file_type, gpointer not_used );
static void datasource_file_cleanup ( gpointer data );

VikDataSourceInterface vik_datasource_file_interface = {
  N_("Import file with GPSBabel"),
  N_("Imported file"),
  VIK_DATASOURCE_ADDTOLAYER,
  VIK_DATASOURCE_INPUTTYPE_NONE,
  TRUE,
  TRUE,
  TRUE,
  (VikDataSourceInitFunc)		datasource_file_init,
  (VikDataSourceCheckExistenceFunc)	NULL,
  (VikDataSourceAddSetupWidgetsFunc)	datasource_file_add_setup_widgets,
  (VikDataSourceGetCmdStringFunc)	datasource_file_get_cmd_string,
  (VikDataSourceProcessFunc)        a_babel_convert_from,
  (VikDataSourceProgressFunc)		NULL,
  (VikDataSourceAddProgressWidgetsFunc)	NULL,
  (VikDataSourceCleanupFunc)		datasource_file_cleanup,
  (VikDataSourceOffFunc)                NULL,
};

/* See VikDataSourceInterface */
static gpointer datasource_file_init ( acq_vik_t *avt )
{
  datasource_file_widgets_t *widgets = g_malloc(sizeof(*widgets));
  return widgets;
}

static void fill_combo_box (gpointer data, gpointer user_data)
{
  const gchar *label = ((BabelFile*) data)->label;
  vik_combo_box_text_append (GTK_WIDGET(user_data), label);
}

static void add_file_filter (gpointer data, gpointer user_data)
{
  GtkFileChooser *chooser = GTK_FILE_CHOOSER ( user_data );
  const gchar *label = ((BabelFile*) data)->label;
  const gchar *ext = ((BabelFile*) data)->ext;
  if ( ext == NULL || ext[0] == '\0' )
    /* No file extension => no filter */
	return;
  gchar *pattern = g_strdup_printf ( "*.%s", ext );

  GtkFileFilter *filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern ( filter, pattern );
  if ( strstr ( label, pattern+1 ) ) {
    gtk_file_filter_set_name ( filter, label );
  } else {
    /* Ensure displayed label contains file pattern */
	/* NB: we skip the '*' in the pattern */
	gchar *name = g_strdup_printf ( "%s (%s)", label, pattern+1 );
    gtk_file_filter_set_name ( filter, name );
	g_free ( name );
  }
  g_object_set_data ( G_OBJECT(filter), "Babel", data );
  gtk_file_chooser_add_filter ( chooser, filter );
  if ( last_file_filter == data )
    /* Previous selection used this filter */
    gtk_file_chooser_set_filter ( chooser, filter );

  g_free ( pattern );
}

/* See VikDataSourceInterface */
static void datasource_file_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, gpointer user_data )
{
  datasource_file_widgets_t *widgets = (datasource_file_widgets_t *)user_data;
  GtkWidget *filename_label, *type_label;

  /* The file selector */
  filename_label = gtk_label_new (_("File:"));
  widgets->file = gtk_file_chooser_button_new (_("File to import"), GTK_FILE_CHOOSER_ACTION_OPEN);
  if (last_folder_uri)
    gtk_file_chooser_set_current_folder_uri ( GTK_FILE_CHOOSER(widgets->file), last_folder_uri);
  /* Add filters */
  g_list_foreach ( a_babel_file_list, add_file_filter, widgets->file );
  GtkFileFilter *all_filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern ( all_filter, "*" );
  gtk_file_filter_set_name ( all_filter, _("All files") );
  gtk_file_chooser_add_filter ( GTK_FILE_CHOOSER(widgets->file), all_filter );
  if ( last_file_filter == NULL )
    /* No previously selected filter or 'All files' selected */
    gtk_file_chooser_set_filter ( GTK_FILE_CHOOSER(widgets->file), all_filter );

  /* The file format selector */
  type_label = gtk_label_new (_("File type:"));
  widgets->type = vik_combo_box_text_new ();
  g_list_foreach (a_babel_file_list, fill_combo_box, widgets->type);
  gtk_combo_box_set_active (GTK_COMBO_BOX (widgets->type), last_type);

  /* Packing all these widgets */
  GtkBox *box = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
  gtk_box_pack_start ( box, filename_label, FALSE, FALSE, 5 );
  gtk_box_pack_start ( box, widgets->file, FALSE, FALSE, 5 );
  gtk_box_pack_start ( box, type_label, FALSE, FALSE, 5 );
  gtk_box_pack_start ( box, widgets->type, FALSE, FALSE, 5 );
  gtk_widget_show_all(dialog);
}

/* See VikDataSourceInterface */
static void datasource_file_get_cmd_string ( datasource_file_widgets_t *widgets, gchar **cmd, gchar **input_file, gpointer not_used )
{
  /* Retrieve the file selected */
  gchar *filename = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER(widgets->file) );

  /* Memorize the directory for later use */
  g_free (last_folder_uri);
  last_folder_uri = gtk_file_chooser_get_current_folder_uri ( GTK_FILE_CHOOSER(widgets->file) );
  last_folder_uri = g_strdup (last_folder_uri);

  /* Memorize the file filter for later use */
  GtkFileFilter *filter = gtk_file_chooser_get_filter ( GTK_FILE_CHOOSER(widgets->file) );
  last_file_filter = g_object_get_data ( G_OBJECT(filter), "Babel" );

  /* Retrieve and memorize file format selected */
  gchar *type = NULL;
  last_type = gtk_combo_box_get_active ( GTK_COMBO_BOX (widgets->type) );
  if ( a_babel_file_list )
    type = ((BabelFile*)g_list_nth_data (a_babel_file_list, last_type))->name;

  /* Build the string */
  *cmd = g_strdup_printf( "-i %s", type);
  *input_file = g_strdup(filename);

  /* Free memory */
  g_free (filename);

  g_debug(_("using babel args '%s' and file '%s'"), *cmd, *input_file);
}

/* See VikDataSourceInterface */
static void datasource_file_cleanup ( gpointer data )
{
  g_free ( data );
}

