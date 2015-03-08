/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2010-2015, Rob Norris <rw_norris@hotmail.com>
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

#include <string.h>

#include "viking.h"
#include "config.h"

#define TREEVIEW_GET(model,iter,what,dest) gtk_tree_model_get(GTK_TREE_MODEL(model),(iter),(what),(dest),-1)

enum {
  VT_ITEM_EDITED_SIGNAL,
  VT_ITEM_TOGGLED_SIGNAL,
  VT_LAST_SIGNAL
};

static guint treeview_signals[VT_LAST_SIGNAL] = { 0, 0 };

static GObjectClass *parent_class;

enum
{
  NAME_COLUMN = 0,
  VISIBLE_COLUMN,
  ICON_COLUMN,
  /* invisible */

  TYPE_COLUMN,
  ITEM_PARENT_COLUMN,
  ITEM_POINTER_COLUMN,
  ITEM_DATA_COLUMN,
  EDITABLE_COLUMN,
  ITEM_TIMESTAMP_COLUMN, // Date timestamp stored in tree model to enable sorting on this value
  NUM_COLUMNS
};

struct _VikTreeview {
  GtkTreeView treeview;
  GtkTreeModel *model;

  GdkPixbuf *layer_type_icons[VIK_LAYER_NUM_TYPES];

  gboolean was_a_toggle;
  gboolean editing;
};

/* TODO: find, make "static" and put up here all non-"a_" functions */
static void vik_treeview_finalize ( GObject *gob );
static void vik_treeview_add_columns ( VikTreeview *vt );

static gboolean vik_treeview_drag_data_received ( GtkTreeDragDest *drag_dest, GtkTreePath *dest, GtkSelectionData *selection_data );
static gboolean vik_treeview_drag_data_delete ( GtkTreeDragSource *drag_source, GtkTreePath *path );

G_DEFINE_TYPE (VikTreeview, vik_treeview, GTK_TYPE_TREE_VIEW)

static void vik_cclosure_marshal_VOID__POINTER_POINTER ( GClosure     *closure,
                                                         GValue       *return_value,
                                                         guint         n_param_vals,
                                                         const GValue *param_values,
                                                         gpointer      invocation_hint,
                                                         gpointer      marshal_data )
{
  typedef gboolean (*VikMarshalFunc_VOID__POINTER_POINTER) ( gpointer      data1,
                                                             gconstpointer arg_1,
                                                             gconstpointer arg_2,
                                                             gpointer      data2 );

  register VikMarshalFunc_VOID__POINTER_POINTER callback;
  register GCClosure* cc = (GCClosure*) closure;
  register gpointer data1, data2;

  g_return_if_fail (n_param_vals == 3);

  if (G_CCLOSURE_SWAP_DATA(closure)) {
    data1 = closure->data;
    data2 = g_value_peek_pointer (param_values + 0);
  }
  else {
    data1 = g_value_peek_pointer (param_values + 0);
    data2 = closure->data;
  }
  callback = (VikMarshalFunc_VOID__POINTER_POINTER) (marshal_data ? marshal_data : cc->callback);
  callback ( data1,
             g_value_get_pointer(param_values + 1),
             g_value_get_pointer(param_values + 2),
             data2 );
}

static void vik_treeview_class_init ( VikTreeviewClass *klass )
{
  /* Destructor */
  GObjectClass *object_class;
                                                                                                                                 
  object_class = G_OBJECT_CLASS (klass);
                                                                                                                                 
  object_class->finalize = vik_treeview_finalize;
                                                                                                                                 
  parent_class = g_type_class_peek_parent (klass);

  treeview_signals[VT_ITEM_EDITED_SIGNAL] = g_signal_new ( "item_edited", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (VikTreeviewClass, item_edited), NULL, NULL, 
    vik_cclosure_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

  treeview_signals[VT_ITEM_TOGGLED_SIGNAL] = g_signal_new ( "item_toggled", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (VikTreeviewClass, item_toggled), NULL, NULL,
    g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER );
}

static void vik_treeview_edited_cb (GtkCellRendererText *cell, gchar *path_str, const gchar *new_name, VikTreeview *vt)
{
  vt->editing = FALSE;
  GtkTreeIter iter;

  /* get type and data */
  vik_treeview_get_iter_from_path_str ( vt, &iter, path_str );

  g_signal_emit ( G_OBJECT(vt), treeview_signals[VT_ITEM_EDITED_SIGNAL], 0, &iter, new_name );
}

static void vik_treeview_edit_start_cb (GtkCellRenderer *cell, GtkCellEditable *editable, gchar *path, VikTreeview *vt)
{
  vt->editing = TRUE;
}

static void vik_treeview_edit_stop_cb (GtkCellRenderer *cell, VikTreeview *vt)
{
  vt->editing = FALSE;
}

static void vik_treeview_toggled_cb (GtkCellRendererToggle *cell, gchar *path_str, VikTreeview *vt)
{
  GtkTreeIter iter_toggle;
  GtkTreeIter iter_selected;

  /* get type and data */
  vik_treeview_get_iter_from_path_str ( vt, &iter_toggle, path_str );

  GtkTreePath *tp_toggle = gtk_tree_model_get_path ( vt->model, &iter_toggle );

  if ( gtk_tree_selection_get_selected ( gtk_tree_view_get_selection ( GTK_TREE_VIEW ( vt ) ), NULL, &iter_selected ) ) {
    GtkTreePath *tp_selected = gtk_tree_model_get_path ( vt->model, &iter_selected );
    if ( gtk_tree_path_compare ( tp_toggle, tp_selected ) )
      // Toggle set on different path
      // therefore prevent subsequent auto selection (otherwise no action needed)
      vt->was_a_toggle = TRUE;
    gtk_tree_path_free ( tp_selected );
  }
  else
    // Toggle set on new path
    // therefore prevent subsequent auto selection
    vt->was_a_toggle = TRUE;

  gtk_tree_path_free ( tp_toggle );

  g_signal_emit ( G_OBJECT(vt), treeview_signals[VT_ITEM_TOGGLED_SIGNAL], 0, &iter_toggle );
}

/* Inspired by GTK+ test
 * http://git.gnome.org/browse/gtk+/tree/tests/testtooltips.c
 */
static gboolean
vik_treeview_tooltip_cb (GtkWidget  *widget,
		     gint        x,
		     gint        y,
		     gboolean    keyboard_tip,
		     GtkTooltip *tooltip,
		     gpointer    data)
{
  GtkTreeIter iter;
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
  GtkTreePath *path = NULL;

  char buffer[256];

  if (!gtk_tree_view_get_tooltip_context (tree_view, &x, &y,
					  keyboard_tip,
					  &model, &path, &iter))
    return FALSE;

  /* ATM normally treeview doesn't call into layers - maybe another level of redirection required? */
  gint rv;
  gtk_tree_model_get (model, &iter, TYPE_COLUMN, &rv, -1);
  if ( rv == VIK_TREEVIEW_TYPE_SUBLAYER ) {

    gtk_tree_model_get (model, &iter, ITEM_DATA_COLUMN, &rv, -1);

    gpointer sublayer;
    gtk_tree_model_get (model, &iter, ITEM_POINTER_COLUMN, &sublayer, -1);

    gpointer parent;
    gtk_tree_model_get (model, &iter, ITEM_PARENT_COLUMN, &parent, -1);

    g_snprintf (buffer, sizeof(buffer), "%s", vik_layer_sublayer_tooltip (VIK_LAYER(parent), rv, sublayer));
  }
  else if ( rv == VIK_TREEVIEW_TYPE_LAYER ) {
    gpointer layer;
    gtk_tree_model_get (model, &iter, ITEM_POINTER_COLUMN, &layer, -1);
    g_snprintf (buffer, sizeof(buffer), "%s", vik_layer_layer_tooltip (VIK_LAYER(layer)));
  }
  else {
    gtk_tree_path_free (path);
    return FALSE;
  }

  // Don't display null strings :)
  if ( strncmp (buffer, "(null)", 6) == 0 ) {
    gtk_tree_path_free (path);
    return FALSE;
  }
  else {
    // No point in using (Pango) markup verson - gtk_tooltip_set_markup()
    //  especially as waypoint comments may well contain HTML markup which confuses the pango markup parser
    // This plain text is probably faster too.
    gtk_tooltip_set_text (tooltip, buffer);
  }

  gtk_tree_view_set_tooltip_row (tree_view, tooltip, path);

  gtk_tree_path_free (path);

  return TRUE;
}

VikTreeview *vik_treeview_new ()
{
  return VIK_TREEVIEW ( g_object_new ( VIK_TREEVIEW_TYPE, NULL ) );
}

gint vik_treeview_item_get_type ( VikTreeview *vt, GtkTreeIter *iter )
{
  gint rv;
  TREEVIEW_GET ( vt->model, iter, TYPE_COLUMN, &rv );
  return rv;
}

gchar* vik_treeview_item_get_name ( VikTreeview *vt, GtkTreeIter *iter )
{
  gchar *rv;
  TREEVIEW_GET ( vt->model, iter, NAME_COLUMN, &rv );
  return rv;
}

gint vik_treeview_item_get_data ( VikTreeview *vt, GtkTreeIter *iter )
{
  gint rv;
  TREEVIEW_GET ( vt->model, iter, ITEM_DATA_COLUMN, &rv );
  return rv;
}

gpointer vik_treeview_item_get_pointer ( VikTreeview *vt, GtkTreeIter *iter )
{
  gpointer rv;
  TREEVIEW_GET ( vt->model, iter, ITEM_POINTER_COLUMN, &rv );
  return rv;
}

void vik_treeview_item_set_pointer ( VikTreeview *vt, GtkTreeIter *iter, gpointer pointer )
{
  gtk_tree_store_set ( GTK_TREE_STORE(vt->model), iter, ITEM_POINTER_COLUMN, pointer, -1 );
}

void vik_treeview_item_set_timestamp ( VikTreeview *vt, GtkTreeIter *iter, time_t timestamp )
{
  gtk_tree_store_set ( GTK_TREE_STORE(vt->model), iter, ITEM_TIMESTAMP_COLUMN, timestamp, -1 );
}

gpointer vik_treeview_item_get_parent ( VikTreeview *vt, GtkTreeIter *iter )
{
  gpointer rv;
  TREEVIEW_GET ( vt->model, iter, ITEM_PARENT_COLUMN, &rv );
  return rv;
}

gboolean vik_treeview_get_iter_from_path_str ( VikTreeview *vt, GtkTreeIter *iter, const gchar *path_str )
{
  return gtk_tree_model_get_iter_from_string ( GTK_TREE_MODEL(vt->model), iter, path_str );
}

/**
 * Get visibility of an item considering visibility of all parents
 *  i.e. if any parent is off then this item will also be considered off
 *   (even though itself may be marked as on.)
 */
gboolean vik_treeview_item_get_visible_tree ( VikTreeview *vt, GtkTreeIter *iter )
{
  gboolean ans;
  TREEVIEW_GET ( vt->model, iter, VISIBLE_COLUMN, &ans );

  if ( !ans )
    return ans;

  GtkTreeIter parent;
  GtkTreeIter child = *iter;
  while ( gtk_tree_model_iter_parent (vt->model, &parent, &child) ) {
    // Visibility of this parent
    TREEVIEW_GET ( vt->model, &parent, VISIBLE_COLUMN, &ans );
    // If not visible, no need to check further ancestors
    if ( !ans )
      break;
    child = parent;
  }
  return ans;
}

static void vik_treeview_add_columns ( VikTreeview *vt )
{
  gint col_offset;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  /* Layer column */
  renderer = gtk_cell_renderer_text_new ();
  g_signal_connect (renderer, "edited",
		    G_CALLBACK (vik_treeview_edited_cb), vt);

  g_signal_connect (renderer, "editing-started", G_CALLBACK (vik_treeview_edit_start_cb), vt);
  g_signal_connect (renderer, "editing-canceled", G_CALLBACK (vik_treeview_edit_stop_cb), vt);

  g_object_set (G_OBJECT (renderer), "xalign", 0.0, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

  col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (vt),
							    -1, _("Layer Name"),
							    renderer, "text",
							    NAME_COLUMN,
							    "editable", EDITABLE_COLUMN,
							    NULL);

  /* ATM the minimum overall width (and starting default) of the treeview size is determined
     by the buttons added to the bottom of the layerspanel */
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (vt), col_offset - 1);
  gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (column),
                                   GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);

  /* Layer type */
  renderer = gtk_cell_renderer_pixbuf_new ();

  g_object_set (G_OBJECT (renderer), "xalign", 0.5, NULL);

  col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (vt),
							    -1, "",
							    renderer, "pixbuf",
							    ICON_COLUMN,
							    NULL);

  column = gtk_tree_view_get_column (GTK_TREE_VIEW (vt), col_offset - 1);
  gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (column),
                                   GTK_TREE_VIEW_COLUMN_AUTOSIZE);

  /* Layer visible */
  renderer = gtk_cell_renderer_toggle_new ();
  g_object_set (G_OBJECT (renderer), "xalign", 0.5, NULL);

  g_signal_connect (renderer, "toggled", G_CALLBACK (vik_treeview_toggled_cb), vt);

  col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (vt),
							    -1, "",
							    renderer,
							    "active",
							    VISIBLE_COLUMN,
							    NULL);

  column = gtk_tree_view_get_column (GTK_TREE_VIEW (vt), col_offset - 1);
  gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (column),
                                   GTK_TREE_VIEW_COLUMN_AUTOSIZE);


  g_object_set (GTK_TREE_VIEW (vt), "has-tooltip", TRUE, NULL);
  g_signal_connect (GTK_TREE_VIEW (vt), "query-tooltip", G_CALLBACK (vik_treeview_tooltip_cb), vt);
}

static void select_cb(GtkTreeSelection *selection, gpointer data)
{
  VikTreeview *vt = data;
  gint type;
  GtkTreeIter iter, parent;
  VikLayer *vl;
  VikWindow * vw;

  gpointer tmp_layer;
  VikLayer *tmp_vl = NULL;
  gint tmp_subtype = 0;
  gint tmp_type = VIK_TREEVIEW_TYPE_LAYER;

  if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) return;
  type = vik_treeview_item_get_type( vt, &iter);

  /* Find the Sublayer type if possible */
  tmp_layer = vik_treeview_item_get_pointer ( vt, &iter );
  if (tmp_layer) {
    if (type == VIK_TREEVIEW_TYPE_SUBLAYER) {
      tmp_vl = VIK_LAYER(vik_treeview_item_get_parent(vt, &iter));
      tmp_subtype = vik_treeview_item_get_data(vt, &iter);
      tmp_type = VIK_TREEVIEW_TYPE_SUBLAYER;
    }
  }
  else {
    tmp_subtype = vik_treeview_item_get_data(vt, &iter);
    tmp_type = VIK_TREEVIEW_TYPE_SUBLAYER;
  }

  /* Go up the tree to find the Vik Layer */
  while ( type != VIK_TREEVIEW_TYPE_LAYER ) {
    if ( ! vik_treeview_item_get_parent_iter ( vt, &iter, &parent ) )
      return;
    iter = parent;
    type = vik_treeview_item_get_type (vt, &iter );
  }

  vl = VIK_LAYER( vik_treeview_item_get_pointer ( vt, &iter ) );

  vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vl));
  vik_window_selected_layer(vw, vl);

  if (tmp_vl == NULL)
    tmp_vl = vl;
  /* Apply settings now we have the all details  */
  if ( vik_layer_selected ( tmp_vl,
			    tmp_subtype,
			    tmp_layer,
			    tmp_type,
			    vik_window_layers_panel(vw) ) ) {
    /* Redraw required */
    vik_layers_panel_emit_update ( vik_window_layers_panel(vw) );
  }

}

static gboolean vik_treeview_selection_filter(GtkTreeSelection *selection, GtkTreeModel *model, GtkTreePath *path, gboolean path_currently_selected, gpointer data)
{
  VikTreeview *vt = data;

  if (vt->was_a_toggle) {
    vt->was_a_toggle = FALSE;
    return FALSE;
  }

  return TRUE;
}

void vik_treeview_init ( VikTreeview *vt )
{
  vt->was_a_toggle = FALSE;
  vt->editing = FALSE;

  // ATM The dates are stored on initial creation and updated when items are deleted
  //  this should be good enough for most purposes, although it may get inaccurate if items are edited in a particular manner
  // NB implicit conversion of time_t to gint64
  vt->model = GTK_TREE_MODEL(gtk_tree_store_new ( NUM_COLUMNS,
                                                  G_TYPE_STRING,  // Name
                                                  G_TYPE_BOOLEAN, // Visibility
                                                  GDK_TYPE_PIXBUF,// The Icon
                                                  G_TYPE_INT,     // Layer Type
                                                  G_TYPE_POINTER, // pointer to TV parent
                                                  G_TYPE_POINTER, // pointer to the layer or sublayer
                                                  G_TYPE_INT,     // type of the sublayer
                                                  G_TYPE_BOOLEAN, // Editable
                                                  G_TYPE_INT64 )); // Timestamp

  /* create tree view */
  gtk_tree_selection_set_select_function(gtk_tree_view_get_selection (GTK_TREE_VIEW(vt)), vik_treeview_selection_filter, vt, NULL);

  gtk_tree_view_set_model ( GTK_TREE_VIEW(vt), vt->model );
  vik_treeview_add_columns ( vt );

  // Can not specify 'auto' sort order with a 'GtkTreeSortable' on the name since we want to control the ordering of layers
  // Thus need to create special sort to operate on a subsection of treeview (i.e. from a specific child either a layer or sublayer)
  // see vik_treeview_sort_children()

  g_object_unref (vt->model);

  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (vt), TRUE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (vt)),
                               GTK_SELECTION_SINGLE);

  /* Override treestore's dnd methods only; this is easier than deriving from GtkTreeStore. 
   * The downside is that all treestores will have this behavior, so this needs to be
   * changed if we add more treeviews in the future.  //Alex
   */
  if (1) {
    GtkTreeDragSourceIface *isrc;
    GtkTreeDragDestIface *idest;

    isrc = g_type_interface_peek (g_type_class_peek(G_OBJECT_TYPE(vt->model)), GTK_TYPE_TREE_DRAG_SOURCE);
    isrc->drag_data_delete = vik_treeview_drag_data_delete;

    idest = g_type_interface_peek (g_type_class_peek(G_OBJECT_TYPE(vt->model)), GTK_TYPE_TREE_DRAG_DEST);
    idest->drag_data_received = vik_treeview_drag_data_received;
  }      

  VikLayerTypeEnum i;
  for ( i = 0; i < VIK_LAYER_NUM_TYPES; i++ )
    vt->layer_type_icons[i] = vik_layer_load_icon ( i ); /* if icon can't be loaded, it will be null and simply not be shown. */

  gtk_tree_view_set_reorderable (GTK_TREE_VIEW(vt), TRUE);
  g_signal_connect(gtk_tree_view_get_selection (GTK_TREE_VIEW (vt)), "changed",
      G_CALLBACK(select_cb), vt);

}

gboolean vik_treeview_item_get_parent_iter ( VikTreeview *vt, GtkTreeIter *iter,  GtkTreeIter *parent )
{
  return gtk_tree_model_iter_parent ( GTK_TREE_MODEL(vt->model), parent, iter );
}

gboolean vik_treeview_move_item ( VikTreeview *vt, GtkTreeIter *iter, gboolean up )
{
  gint t = vik_treeview_item_get_type ( vt, iter );
  if ( t == VIK_TREEVIEW_TYPE_LAYER )
  {
    GtkTreeIter switch_iter;
    if (up)
    {
      /* iter to path to iter */
      GtkTreePath *path = gtk_tree_model_get_path ( vt->model, iter );
      if ( !gtk_tree_path_prev ( path ) || !gtk_tree_model_get_iter ( vt->model, &switch_iter, path ) )
      {
        gtk_tree_path_free ( path );
        return FALSE;
      }
      gtk_tree_path_free ( path );
    }
    else
    {
      switch_iter = *iter;
      if ( !gtk_tree_model_iter_next ( vt->model, &switch_iter ) )
        return FALSE;
    }
    gtk_tree_store_swap ( GTK_TREE_STORE(vt->model), iter, &switch_iter ); 
    return TRUE;
    /* now, the easy part. actually switching them, not the GUI */
  } /* if item is map */
  return FALSE;
}

gboolean vik_treeview_get_iter_at_pos ( VikTreeview *vt, GtkTreeIter *iter, gint x, gint y )
{
  GtkTreePath *path;
  gtk_tree_view_get_path_at_pos ( GTK_TREE_VIEW(vt), x, y, &path, NULL, NULL, NULL );
  if ( ! path )
    return FALSE;

  gtk_tree_model_get_iter (GTK_TREE_MODEL(vt->model), iter, path);
  gtk_tree_path_free ( path );
  return TRUE;
}

/* Option to ensure visible */
void vik_treeview_select_iter ( VikTreeview *vt, GtkTreeIter *iter, gboolean view_all )
{
  GtkTreeView *tree_view = GTK_TREE_VIEW ( vt );
  GtkTreePath *path;

  if ( view_all ) {
    path = gtk_tree_model_get_path ( gtk_tree_view_get_model (tree_view), iter );
    gtk_tree_view_expand_to_path ( tree_view, path );
  }

  gtk_tree_selection_select_iter ( gtk_tree_view_get_selection ( tree_view ), iter );

  if ( view_all ) {
    gtk_tree_view_scroll_to_cell  ( tree_view,
				    path,
				    gtk_tree_view_get_expander_column (tree_view),
				    FALSE,
				    0.0, 0.0 );
    gtk_tree_path_free ( path );
  }
}

gboolean vik_treeview_get_selected_iter ( VikTreeview *vt, GtkTreeIter *iter )
{
  return gtk_tree_selection_get_selected ( gtk_tree_view_get_selection ( GTK_TREE_VIEW ( vt ) ), NULL, iter );
}

gboolean vik_treeview_get_editing ( VikTreeview *vt )
{
  // Don't know how to get cell for the selected item
  //return GPOINTER_TO_INT(g_object_get_data ( G_OBJECT(cell), "editing" ));
  // Instead maintain our own value applying to the whole tree
  return vt->editing;
}

void vik_treeview_item_delete ( VikTreeview *vt, GtkTreeIter *iter )
{
  gtk_tree_store_remove ( GTK_TREE_STORE(vt->model), iter );
}

/* Treeview Reform Project */

void vik_treeview_item_set_icon ( VikTreeview *vt, GtkTreeIter *iter, const GdkPixbuf *icon )
{
  g_return_if_fail ( iter != NULL );
  gtk_tree_store_set ( GTK_TREE_STORE(vt->model), iter, ICON_COLUMN, icon, -1);
}

void vik_treeview_item_set_name ( VikTreeview *vt, GtkTreeIter *iter, const gchar *to )
{
  g_return_if_fail ( iter != NULL && to != NULL );
  gtk_tree_store_set ( GTK_TREE_STORE(vt->model), iter, NAME_COLUMN, to, -1);
}

void vik_treeview_item_set_visible ( VikTreeview *vt, GtkTreeIter *iter, gboolean to )
{
  g_return_if_fail ( iter != NULL );
  gtk_tree_store_set ( GTK_TREE_STORE(vt->model), iter, VISIBLE_COLUMN, to, -1 );
}

void vik_treeview_item_toggle_visible ( VikTreeview *vt, GtkTreeIter *iter )
{
  g_return_if_fail ( iter != NULL );
  gboolean to;
  TREEVIEW_GET ( vt->model, iter, VISIBLE_COLUMN, &to );
  to = !to;
  gtk_tree_store_set ( GTK_TREE_STORE(vt->model), iter, VISIBLE_COLUMN, to, -1 );
}

void vik_treeview_expand ( VikTreeview *vt, GtkTreeIter *iter )
{
  GtkTreePath *path;
  path = gtk_tree_model_get_path ( vt->model, iter );
  gtk_tree_view_expand_row ( GTK_TREE_VIEW(vt), path, FALSE );
  gtk_tree_path_free ( path );
}

void vik_treeview_item_select ( VikTreeview *vt, GtkTreeIter *iter )
{
  gtk_tree_selection_select_iter ( gtk_tree_view_get_selection ( GTK_TREE_VIEW ( vt ) ), iter );
}

void vik_treeview_item_unselect ( VikTreeview *vt, GtkTreeIter *iter )
{
  gtk_tree_selection_unselect_iter ( gtk_tree_view_get_selection ( GTK_TREE_VIEW ( vt ) ), iter );
}

void vik_treeview_add_layer ( VikTreeview *vt, GtkTreeIter *parent_iter, GtkTreeIter *iter, const gchar *name, gpointer parent, gboolean above,
                              gpointer item, gint data, VikLayerTypeEnum layer_type, time_t timestamp )
{
  g_assert ( iter != NULL );
  if ( above )
    gtk_tree_store_prepend ( GTK_TREE_STORE(vt->model), iter, parent_iter );
  else
    gtk_tree_store_append ( GTK_TREE_STORE(vt->model), iter, parent_iter );
  gtk_tree_store_set ( GTK_TREE_STORE(vt->model), iter,
    NAME_COLUMN, name,
    VISIBLE_COLUMN, TRUE,
    TYPE_COLUMN, VIK_TREEVIEW_TYPE_LAYER,
    ITEM_PARENT_COLUMN, parent,
    ITEM_POINTER_COLUMN, item,
    ITEM_DATA_COLUMN, data,
    EDITABLE_COLUMN, parent == NULL ? FALSE : TRUE,
    ICON_COLUMN, layer_type >= 0 ? vt->layer_type_icons[layer_type] : NULL,
    ITEM_TIMESTAMP_COLUMN, timestamp,
    -1 );
}

void vik_treeview_insert_layer ( VikTreeview *vt, GtkTreeIter *parent_iter, GtkTreeIter *iter, const gchar *name, gpointer parent, gboolean above,
                              gpointer item, gint data, VikLayerTypeEnum layer_type, GtkTreeIter *sibling, time_t timestamp )
{
  g_assert ( iter != NULL );
  if (sibling) {
    if (above)
      gtk_tree_store_insert_before ( GTK_TREE_STORE(vt->model), iter, parent_iter, sibling );
    else
      gtk_tree_store_insert_after ( GTK_TREE_STORE(vt->model), iter, parent_iter, sibling );
  } else {
    if (above)
      gtk_tree_store_append ( GTK_TREE_STORE(vt->model), iter, parent_iter );
    else
      gtk_tree_store_prepend ( GTK_TREE_STORE(vt->model), iter, parent_iter );
  }

  gtk_tree_store_set ( GTK_TREE_STORE(vt->model), iter,
                       NAME_COLUMN, name,
                       VISIBLE_COLUMN, TRUE,
                       TYPE_COLUMN, VIK_TREEVIEW_TYPE_LAYER,
                       ITEM_PARENT_COLUMN, parent,
                       ITEM_POINTER_COLUMN, item,
                       ITEM_DATA_COLUMN, data,
                       EDITABLE_COLUMN, TRUE,
                       ICON_COLUMN, layer_type >= 0 ? vt->layer_type_icons[layer_type] : NULL,
                       ITEM_TIMESTAMP_COLUMN, timestamp,
                       -1 );
}

void vik_treeview_add_sublayer ( VikTreeview *vt, GtkTreeIter *parent_iter, GtkTreeIter *iter, const gchar *name, gpointer parent, gpointer item,
                                 gint data, GdkPixbuf *icon, gboolean editable, time_t timestamp )
{
  g_assert ( iter != NULL );

  gtk_tree_store_append ( GTK_TREE_STORE(vt->model), iter, parent_iter );
  gtk_tree_store_set ( GTK_TREE_STORE(vt->model), iter,
                       NAME_COLUMN, name,
                       VISIBLE_COLUMN, TRUE,
                       TYPE_COLUMN, VIK_TREEVIEW_TYPE_SUBLAYER,
                       ITEM_PARENT_COLUMN, parent,
                       ITEM_POINTER_COLUMN, item,
                       ITEM_DATA_COLUMN, data,
                       EDITABLE_COLUMN, editable,
                       ICON_COLUMN, icon,
                       ITEM_TIMESTAMP_COLUMN, timestamp,
                       -1 );
}

// Inspired by the internals of GtkTreeView sorting itself
typedef struct _SortTuple
{
  gint offset;
  gchar *name;
  time_t timestamp;
} SortTuple;

/**
 *
 */
static gint sort_tuple_compare ( gconstpointer a, gconstpointer b, gpointer order )
{
  SortTuple *sa = (SortTuple *)a;
  SortTuple *sb = (SortTuple *)b;

  gint answer = 0;
  if ( GPOINTER_TO_INT(order) < VL_SO_DATE_ASCENDING ) {
    // Alphabetical comparison
    // Default ascending order
    answer = g_strcmp0 ( sa->name, sb->name );
    // Invert sort order for descending order
    if ( GPOINTER_TO_INT(order) == VL_SO_ALPHABETICAL_DESCENDING )
      answer = -answer;
  }
  else {
    // Date comparison
    answer = ( sa->timestamp > sb->timestamp );
    // Invert sort order for descending order
    if ( GPOINTER_TO_INT(order) == VL_SO_DATE_DESCENDING )
      answer = !answer;
  }
  return answer;
}

/**
 * Note: I don't believe we can sensibility use built in model sort gtk_tree_model_sort_new_with_model() on the name,
 * since that would also sort the layers - but that needs to be user controlled for ordering, such as which maps get drawn on top.
 *
 * vik_treeview_sort_children:
 * @vt:     The treeview to operate on
 * @parent: The level within the treeview to sort
 * @order:  How the items should be sorted
 *
 * Use the gtk_tree_store_reorder method as it very quick
 *
 * This ordering can be performed on demand and works for any parent iterator (i.e. both sublayer and layer levels)
 *
 * It should be called whenever an individual sublayer item is added or renamed (or after a group of sublayer items have been added).
 *
 * Previously with insertion sort on every sublayer addition: adding 10,000 items would take over 30 seconds!
 * Now sorting after simply adding all tracks takes 1 second.
 * For a KML file with over 10,000 tracks (3Mb zipped) - See 'UK Hampshire Rights of Way'
 * http://www3.hants.gov.uk/row/row-maps.htm
 */
void vik_treeview_sort_children ( VikTreeview *vt, GtkTreeIter *parent, vik_layer_sort_order_t order )
{
  if ( order == VL_SO_NONE )
    // Nothing to do
    return;

  GtkTreeModel *model = vt->model;
  GtkTreeIter child;
  if ( !gtk_tree_model_iter_children ( model, &child, parent ) )
    return;

  guint length = gtk_tree_model_iter_n_children ( model, parent );

  // Create an array to store the position offsets
  SortTuple *sort_array;
  sort_array = g_new ( SortTuple, length );

  guint ii = 0;
  do {
    sort_array[ii].offset = ii;
    gtk_tree_model_get ( model, &child, NAME_COLUMN, &(sort_array[ii].name), -1 );
    gtk_tree_model_get ( model, &child, ITEM_TIMESTAMP_COLUMN, &(sort_array[ii].timestamp), -1 );
    ii++;
  } while ( gtk_tree_model_iter_next (model, &child) );

  // Sort list...
  g_qsort_with_data (sort_array,
                     length,
                     sizeof (SortTuple),
                     sort_tuple_compare,
                     GINT_TO_POINTER(order));

  // As the sorted list contains the reordered position offsets, extract this and then apply to the treeview
  gint *positions = g_malloc ( sizeof(gdouble) * length );
  for ( ii = 0; ii < length; ii++ ) {
    positions[ii] = sort_array[ii].offset;
    g_free ( sort_array[ii].name );
  }
  g_free ( sort_array );

  // This is extremely fast compared to the old alphabetical insertion
  gtk_tree_store_reorder ( GTK_TREE_STORE(model), parent, positions );
  g_free ( positions );
}

static void vik_treeview_finalize ( GObject *gob )
{
  VikTreeview *vt = VIK_TREEVIEW ( gob );
  VikLayerTypeEnum i;
  for ( i = 0; i < VIK_LAYER_NUM_TYPES; i++ )
    if ( vt->layer_type_icons[i] != NULL )
      g_object_unref ( G_OBJECT(vt->layer_type_icons[i]) );

  G_OBJECT_CLASS(parent_class)->finalize(gob);
}

static gboolean vik_treeview_drag_data_received (GtkTreeDragDest *drag_dest, GtkTreePath *dest, GtkSelectionData *selection_data)
{
  GtkTreeModel *tree_model;
  GtkTreeModel *src_model = NULL;
  GtkTreePath *src_path = NULL, *dest_cp = NULL;
  gboolean retval = FALSE;
  GtkTreeIter src_iter, root_iter, dest_parent;
  VikTreeview *vt;
  VikLayer *vl;

  g_return_val_if_fail (GTK_IS_TREE_STORE (drag_dest), FALSE);

  tree_model = GTK_TREE_MODEL (drag_dest);

  if (gtk_tree_get_row_drag_data (selection_data, &src_model, &src_path) && src_model == tree_model) {
    /* 
     * Copy src_path to dest.  There are two subcases here, depending on what 
     * is being dragged.
     * 
     * 1. src_path is a layer. In this case, interpret the drop 
     *    as a request to move the layer to a different aggregate layer.
     *    If the destination is not an aggregate layer, use the first 
     *    ancestor that is.
     *
     * 2. src_path is a sublayer.  In this case, find ancestors of 
     *    both source and destination nodes who are full layers,
     *    and call the move method of that layer type. 
     *
     */
    if (!gtk_tree_model_get_iter (src_model, &src_iter, src_path)) {
      goto out;
    }
    if (!gtk_tree_path_compare(src_path, dest)) {
      goto out;
    }

    dest_cp = gtk_tree_path_copy (dest);

    gtk_tree_model_get_iter_first(tree_model, &root_iter);
    TREEVIEW_GET(tree_model, &root_iter, ITEM_POINTER_COLUMN, &vl);
    vt = vl->vt;


    if (gtk_tree_path_get_depth(dest_cp)>1) { /* can't be sibling of top layer */
      VikLayer *vl_src, *vl_dest;

      /* Find the first ancestor that is a full layer, and store in dest_parent. */
      do {
	gtk_tree_path_up(dest_cp);
	gtk_tree_model_get_iter (src_model, &dest_parent, dest_cp);
      } while (gtk_tree_path_get_depth(dest_cp)>1 &&
	       vik_treeview_item_get_type(vt, &dest_parent) != VIK_TREEVIEW_TYPE_LAYER);

      
      g_assert ( vik_treeview_item_get_parent(vt, &src_iter) );
      vl_src = vik_treeview_item_get_parent(vt, &src_iter);
      vl_dest = vik_treeview_item_get_pointer(vt, &dest_parent);

      /* TODO: might want to allow different types, and let the clients handle how they want */
      if (vl_src->type == vl_dest->type && vik_layer_get_interface(vl_dest->type)->drag_drop_request) {
	//	g_print("moving an item from layer '%s' into layer '%s'\n", vl_src->name, vl_dest->name);
	vik_layer_get_interface(vl_dest->type)->drag_drop_request(vl_src, vl_dest, &src_iter, dest);
      }    
    }
  }

 out:
  if (dest_cp) 
    gtk_tree_path_free(dest_cp);
  if (src_path)
    gtk_tree_path_free (src_path);

  return retval;
}

/* 
 * This may not be necessary.
 */
static gboolean vik_treeview_drag_data_delete ( GtkTreeDragSource *drag_source, GtkTreePath *path )
{
  gchar *s_dest = gtk_tree_path_to_string(path);
  g_print(_("delete data from %s\n"), s_dest);
  g_free(s_dest);
  return FALSE;
}
