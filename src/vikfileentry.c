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

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "vikfileentry.h"

static void choose_file ( VikFileEntry *vfe );

struct _VikFileEntry {
  GtkHBox parent;
  GtkWidget *entry, *button;
  GtkWidget *file_selector;
  GtkFileChooserAction action;
};

GType vik_file_entry_get_type (void)
{
  static GType vs_type = 0;

  if (!vs_type)
  {
    static const GTypeInfo vs_info = 
    {
      sizeof (VikFileEntryClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      NULL, /* class init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikFileEntry),
      0,
      NULL /* instance init */
    };
    vs_type = g_type_register_static ( GTK_TYPE_HBOX, "VikFileEntry", &vs_info, 0 );
  }

  return vs_type;
}

GtkWidget *vik_file_entry_new (GtkFileChooserAction action)
{
  VikFileEntry *vfe = VIK_FILE_ENTRY ( g_object_new ( VIK_FILE_ENTRY_TYPE, NULL ) );
  vfe->entry = gtk_entry_new ();
  vfe->button = gtk_button_new_with_label ( _("Browse...") );
  vfe->action = action;
  g_signal_connect_swapped ( G_OBJECT(vfe->button), "clicked", G_CALLBACK(choose_file), vfe );

  gtk_box_pack_start ( GTK_BOX(vfe), vfe->entry, TRUE, TRUE, 3 );
  gtk_box_pack_start ( GTK_BOX(vfe), vfe->button, FALSE, FALSE, 3 );

  vfe->file_selector = NULL;

  return GTK_WIDGET(vfe);
}

G_CONST_RETURN gchar *vik_file_entry_get_filename ( VikFileEntry *vfe )
{
  return gtk_entry_get_text ( GTK_ENTRY(vfe->entry) );
}

void vik_file_entry_set_filename ( VikFileEntry *vfe, const gchar *filename )
{
  gtk_entry_set_text ( GTK_ENTRY(vfe->entry), filename );
}

static void choose_file ( VikFileEntry *vfe )
{
  if ( ! vfe->file_selector )
  {
    GtkWidget *win;
    g_assert ( (win = gtk_widget_get_toplevel(GTK_WIDGET(vfe))) );
    vfe->file_selector = gtk_file_chooser_dialog_new (_("Choose file"),
				      GTK_WINDOW(win),
				      vfe->action,   /* open file or directory */
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      NULL);
    gtk_window_set_transient_for ( GTK_WINDOW(vfe->file_selector), GTK_WINDOW(win) );
    gtk_window_set_destroy_with_parent ( GTK_WINDOW(vfe->file_selector), TRUE );
  }

  if ( gtk_dialog_run ( GTK_DIALOG(vfe->file_selector) ) == GTK_RESPONSE_ACCEPT )
    gtk_entry_set_text ( GTK_ENTRY (vfe->entry), gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER(vfe->file_selector) ) );
  gtk_widget_hide ( vfe->file_selector );
}

