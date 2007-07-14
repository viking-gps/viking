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

#include <gtk/gtk.h>

#include "vikfilelist.h"
#include "vikfileentry.h"

struct _VikFileList {
  GtkVBox parent;
  VikFileEntry *file_entry;
  GtkWidget *treeview;
  GtkTreeModel *model;
};

static void file_list_add ( VikFileList *vfl )
{
  GtkTreeIter iter;
  gtk_list_store_append ( GTK_LIST_STORE(vfl->model), &iter );
  gtk_list_store_set ( GTK_LIST_STORE(vfl->model), &iter, 0, vik_file_entry_get_filename(vfl->file_entry), -1 );
}

static void file_list_del ( VikFileList *vfl )
{
  GtkTreeSelection *ts = gtk_tree_view_get_selection (GTK_TREE_VIEW(vfl->treeview));
  GtkTreeIter iter;
  if ( gtk_tree_selection_get_selected(ts, NULL, &iter) )
    gtk_list_store_remove(GTK_LIST_STORE(vfl->model), &iter);
}

GType vik_file_list_get_type (void)
{
  static GType vs_type = 0;

  if (!vs_type)
  {
    static const GTypeInfo vs_info = 
    {
      sizeof (VikFileListClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      NULL, /* class init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikFileList),
      0,
      NULL /* instance init */
    };
    vs_type = g_type_register_static ( GTK_TYPE_VBOX, "VikFileList", &vs_info, 0 );
  }

  return vs_type;
}

GtkWidget *vik_file_list_new ( const gchar *title )
{
  GtkWidget *add_btn, *del_btn;
  GtkWidget *hbox, *scrolledwindow;
  VikFileList *vfl = VIK_FILE_LIST ( g_object_new ( VIK_FILE_LIST_TYPE, NULL ) );

  GtkTreeViewColumn *column;

  vfl->model = GTK_TREE_MODEL ( gtk_list_store_new(1, G_TYPE_STRING) );

  vfl->treeview = gtk_tree_view_new ( );
  gtk_tree_view_set_model ( GTK_TREE_VIEW(vfl->treeview), vfl->model );
  column = gtk_tree_view_column_new_with_attributes ( title, gtk_cell_renderer_text_new (), "text", 0, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (vfl->treeview), column);

  gtk_widget_set_size_request ( vfl->treeview, 200, 100);

  vfl->file_entry = VIK_FILE_ENTRY(vik_file_entry_new());
  add_btn = gtk_button_new_with_label("Add");
  del_btn = gtk_button_new_with_label("Delete");

  g_signal_connect_swapped ( G_OBJECT(add_btn), "clicked", G_CALLBACK(file_list_add), vfl );
  g_signal_connect_swapped ( G_OBJECT(del_btn), "clicked", G_CALLBACK(file_list_del), vfl );

  hbox = gtk_hbox_new(FALSE, 2);

  scrolledwindow = gtk_scrolled_window_new ( NULL, NULL );
  gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
  gtk_container_add ( GTK_CONTAINER(scrolledwindow), GTK_WIDGET(vfl->treeview) );

  gtk_box_pack_start ( GTK_BOX(vfl), scrolledwindow, TRUE, TRUE, 3 );


  gtk_box_pack_start ( GTK_BOX(hbox), GTK_WIDGET(vfl->file_entry), TRUE, TRUE, 3 );
  gtk_box_pack_start ( GTK_BOX(hbox), add_btn, TRUE, TRUE, 3 );
  gtk_box_pack_start ( GTK_BOX(hbox), del_btn, TRUE, TRUE, 3 );
  gtk_box_pack_start ( GTK_BOX(vfl), hbox, FALSE, FALSE, 3 );
  gtk_widget_show_all(GTK_WIDGET(vfl));


  return GTK_WIDGET(vfl);
}

static gboolean get_file_name(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, GList **list)
{
  gchar *str;
  gtk_tree_model_get ( model, iter, 0, &str, -1 );
  g_debug("get_file_name: %s", str);
  (*list) = g_list_append((*list), g_strdup(str));
  return FALSE;
}

GList *vik_file_list_get_files ( VikFileList *vfl )
{
  GList *list = NULL;
  gtk_tree_model_foreach (vfl->model, (GtkTreeModelForeachFunc) get_file_name, &list);
  return list;
}

void vik_file_list_set_files ( VikFileList *vfl, GList *files )
{
  while (files) {
    GtkTreeIter iter;
    gtk_list_store_append ( GTK_LIST_STORE(vfl->model), &iter );
    gtk_list_store_set ( GTK_LIST_STORE(vfl->model), &iter, 0, files->data, -1 );
    files = files->next;
  }
}
