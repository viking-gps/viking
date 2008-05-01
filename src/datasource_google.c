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

#define GOOGLE_DIRECTIONS_STRING "maps.google.com/maps?q=from:%s+to:%s&output=js"

typedef struct {
  GtkWidget *from_entry, *to_entry;
} datasource_google_widgets_t;

static gchar *last_from_str = NULL;
static gchar *last_to_str = NULL;

static gpointer datasource_google_init( );
static void datasource_google_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, gpointer user_data );
static void datasource_google_get_cmd_string ( datasource_google_widgets_t *widgets, gchar **cmd, gchar **input_file_type );	
static void datasource_google_cleanup ( gpointer data );

VikDataSourceInterface vik_datasource_google_interface = {
  N_("Google Directions"),
  N_("Google Directions"),
  VIK_DATASOURCE_URL,
  VIK_DATASOURCE_ADDTOLAYER,
  VIK_DATASOURCE_INPUTTYPE_NONE,
  TRUE,
  (VikDataSourceInitFunc)		datasource_google_init,
  (VikDataSourceCheckExistenceFunc)	NULL,
  (VikDataSourceAddSetupWidgetsFunc)	datasource_google_add_setup_widgets,
  (VikDataSourceGetCmdStringFunc)	datasource_google_get_cmd_string,
  (VikDataSourceProgressFunc)		NULL,
  (VikDataSourceAddProgressWidgetsFunc)	NULL,
  (VikDataSourceCleanupFunc)		datasource_google_cleanup,
};

static gpointer datasource_google_init ( )
{
  datasource_google_widgets_t *widgets = g_malloc(sizeof(*widgets));
  return widgets;
}

static void datasource_google_add_setup_widgets ( GtkWidget *dialog, VikViewport *vvp, gpointer user_data )
{
  datasource_google_widgets_t *widgets = (datasource_google_widgets_t *)user_data;
  GtkWidget *from_label, *to_label;
  from_label = gtk_label_new (_("From:"));
  widgets->from_entry = gtk_entry_new();
  to_label = gtk_label_new (_("To:"));
  widgets->to_entry = gtk_entry_new();
  if (last_from_str)
    gtk_entry_set_text(GTK_ENTRY(widgets->from_entry), last_from_str);
  if (last_to_str)
    gtk_entry_set_text(GTK_ENTRY(widgets->to_entry), last_to_str);
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), from_label, FALSE, FALSE, 5 );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), widgets->from_entry, FALSE, FALSE, 5 );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), to_label, FALSE, FALSE, 5 );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), widgets->to_entry, FALSE, FALSE, 5 );
  gtk_widget_show_all(dialog);
}

static void datasource_google_get_cmd_string ( datasource_google_widgets_t *widgets, gchar **cmd, gchar **input_file_type )
{
  /* TODO: special characters handling!!! */
  gchar *from_quoted, *to_quoted;
  gchar **from_split, **to_split;
  from_quoted = g_shell_quote ( gtk_entry_get_text ( GTK_ENTRY(widgets->from_entry) ) );
  to_quoted = g_shell_quote ( gtk_entry_get_text ( GTK_ENTRY(widgets->to_entry) ) );

  from_split = g_strsplit( from_quoted, " ", 0);
  to_split = g_strsplit( to_quoted, " ", 0);
  from_quoted = g_strjoinv( "%20", from_split);
  to_quoted = g_strjoinv( "%20", to_split);

  *cmd = g_strdup_printf( GOOGLE_DIRECTIONS_STRING, from_quoted, to_quoted );
  *input_file_type = g_strdup("google");

  g_free(last_from_str);
  g_free(last_to_str);

  last_from_str = g_strdup( gtk_entry_get_text ( GTK_ENTRY(widgets->from_entry) ));
  last_to_str = g_strdup( gtk_entry_get_text ( GTK_ENTRY(widgets->to_entry) ));

  g_free(from_quoted);
  g_free(to_quoted);
  g_strfreev(from_split);
  g_strfreev(to_split);

}

static void datasource_google_cleanup ( gpointer data )
{
  g_free ( data );
}
