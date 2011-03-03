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

#include "viking.h"
#include "icons/icons.h"

#include <string.h>

#define DISCONNECT_UPDATE_SIGNAL(vl, val) g_signal_handlers_disconnect_matched(vl, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, val)

static void aggregate_layer_marshall( VikAggregateLayer *val, guint8 **data, gint *len );
static VikAggregateLayer *aggregate_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp );
static void aggregate_layer_change_coord_mode ( VikAggregateLayer *val, VikCoordMode mode );
static void aggregate_layer_drag_drop_request ( VikAggregateLayer *val_src, VikAggregateLayer *val_dest, GtkTreeIter *src_item_iter, GtkTreePath *dest_path );

VikLayerInterface vik_aggregate_layer_interface = {
  "Aggregate",
  &vikaggregatelayer_pixbuf,

  NULL,
  0,

  NULL,
  0,
  NULL,
  0,

  VIK_MENU_ITEM_ALL,

  (VikLayerFuncCreate)                  vik_aggregate_layer_create,
  (VikLayerFuncRealize)                 vik_aggregate_layer_realize,
  (VikLayerFuncPostRead)                NULL,
  (VikLayerFuncFree)                    vik_aggregate_layer_free,

  (VikLayerFuncProperties)              NULL,
  (VikLayerFuncDraw)                    vik_aggregate_layer_draw,
  (VikLayerFuncChangeCoordMode)         aggregate_layer_change_coord_mode,
  
  (VikLayerFuncSetMenuItemsSelection)	NULL,
  (VikLayerFuncGetMenuItemsSelection)	NULL,

  (VikLayerFuncAddMenuItems)            NULL,
  (VikLayerFuncSublayerAddMenuItems)    NULL,

  (VikLayerFuncSublayerRenameRequest)   NULL,
  (VikLayerFuncSublayerToggleVisible)   NULL,
  (VikLayerFuncSublayerTooltip)         NULL,
  (VikLayerFuncLayerTooltip)            NULL,
  (VikLayerFuncLayerSelected)           NULL,

  (VikLayerFuncMarshall)		aggregate_layer_marshall,
  (VikLayerFuncUnmarshall)		aggregate_layer_unmarshall,

  (VikLayerFuncSetParam)                NULL,
  (VikLayerFuncGetParam)                NULL,

  (VikLayerFuncReadFileData)            NULL,
  (VikLayerFuncWriteFileData)           NULL,

  (VikLayerFuncDeleteItem)              NULL,
  (VikLayerFuncCutItem)                 NULL,
  (VikLayerFuncCopyItem)                NULL,
  (VikLayerFuncPasteItem)               NULL,
  (VikLayerFuncFreeCopiedItem)          NULL,
  (VikLayerFuncDragDropRequest)		aggregate_layer_drag_drop_request,

  (VikLayerFuncSelectClick)             NULL,
  (VikLayerFuncSelectMove)              NULL,
  (VikLayerFuncSelectRelease)           NULL,
  (VikLayerFuncSelectedViewportMenu)    NULL,
};

struct _VikAggregateLayer {
  VikLayer vl;
  GList *children;
};

GType vik_aggregate_layer_get_type ()
{
  static GType val_type = 0;

  if (!val_type)
  {
    static const GTypeInfo val_info =
    {
      sizeof (VikAggregateLayerClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      NULL, /* class init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikAggregateLayer),
      0,
      NULL /* instance init */
    };
    val_type = g_type_register_static ( VIK_LAYER_TYPE, "VikAggregateLayer", &val_info, 0 );
  }

  return val_type;
}

VikAggregateLayer *vik_aggregate_layer_create (VikViewport *vp)
{
  VikAggregateLayer *rv = vik_aggregate_layer_new ();
  vik_layer_rename ( VIK_LAYER(rv), vik_aggregate_layer_interface.name );
  return rv;
}

static void aggregate_layer_marshall( VikAggregateLayer *val, guint8 **data, gint *datalen )
{
  GList *child = val->children;
  VikLayer *child_layer;
  guint8 *ld; 
  gint ll;
  GByteArray* b = g_byte_array_new ();
  gint len;

#define alm_append(obj, sz) 	\
  len = (sz);    		\
  g_byte_array_append ( b, (guint8 *)&len, sizeof(len) );	\
  g_byte_array_append ( b, (guint8 *)(obj), len );

  vik_layer_marshall_params(VIK_LAYER(val), &ld, &ll);
  alm_append(ld, ll);
  g_free(ld);

  while (child) {
    child_layer = VIK_LAYER(child->data);
    vik_layer_marshall ( child_layer, &ld, &ll );
    if (ld) {
      alm_append(ld, ll);
      g_free(ld);
    }
    child = child->next;
  }
  *data = b->data;
  *datalen = b->len;
  g_byte_array_free(b, FALSE);
#undef alm_append
}

static VikAggregateLayer *aggregate_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp )
{
#define alm_size (*(gint *)data)
#define alm_next \
  len -= sizeof(gint) + alm_size; \
  data += sizeof(gint) + alm_size;
  
  VikAggregateLayer *rv = vik_aggregate_layer_new();
  VikLayer *child_layer;

  vik_layer_unmarshall_params ( VIK_LAYER(rv), data+sizeof(gint), alm_size, vvp );
  alm_next;

  while (len>0) {
    child_layer = vik_layer_unmarshall ( data + sizeof(gint), alm_size, vvp );
    if (child_layer) {
      rv->children = g_list_append ( rv->children, child_layer );
      g_signal_connect_swapped ( G_OBJECT(child_layer), "update", G_CALLBACK(vik_layer_emit_update_secondary), rv );
    }
    alm_next;
  }
  //  g_print("aggregate_layer_unmarshall ended with len=%d\n", len);
  return rv;
#undef alm_size
#undef alm_next
}

VikAggregateLayer *vik_aggregate_layer_new ()
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER ( g_object_new ( VIK_AGGREGATE_LAYER_TYPE, NULL ) );
  vik_layer_init ( VIK_LAYER(val), VIK_LAYER_AGGREGATE );
  val->children = NULL;
  return val;
}

void vik_aggregate_layer_insert_layer ( VikAggregateLayer *val, VikLayer *l, GtkTreeIter *replace_iter )
{
  GtkTreeIter iter;
  VikLayer *vl = VIK_LAYER(val);

  if ( vl->realized )
  {
    vik_treeview_insert_layer ( vl->vt, &(vl->iter), &iter, l->name, val, l, l->type, l->type, replace_iter );
    if ( ! l->visible )
      vik_treeview_item_set_visible ( vl->vt, &iter, FALSE );
    vik_layer_realize ( l, vl->vt, &iter );

    if ( val->children == NULL )
      vik_treeview_expand ( vl->vt, &(vl->iter) );
  }

  if (replace_iter) {
    GList *theone = g_list_find ( val->children, vik_treeview_item_get_pointer ( vl->vt, replace_iter ) );
    val->children = g_list_insert ( val->children, l, g_list_position(val->children,theone)+1 );
  } else {
    val->children = g_list_append ( val->children, l );
  }
  g_signal_connect_swapped ( G_OBJECT(l), "update", G_CALLBACK(vik_layer_emit_update_secondary), val );
}

void vik_aggregate_layer_add_layer ( VikAggregateLayer *val, VikLayer *l )
{
  GtkTreeIter iter;
  VikLayer *vl = VIK_LAYER(val);

  if ( vl->realized )
  {
    vik_treeview_add_layer ( vl->vt, &(vl->iter), &iter, l->name, val, l, l->type, l->type);
    if ( ! l->visible )
      vik_treeview_item_set_visible ( vl->vt, &iter, FALSE );
    vik_layer_realize ( l, vl->vt, &iter );

    if ( val->children == NULL )
      vik_treeview_expand ( vl->vt, &(vl->iter) );
  }

  val->children = g_list_append ( val->children, l );
  g_signal_connect_swapped ( G_OBJECT(l), "update", G_CALLBACK(vik_layer_emit_update_secondary), val );
}

void vik_aggregate_layer_move_layer ( VikAggregateLayer *val, GtkTreeIter *child_iter, gboolean up )
{
  GList *theone, *first, *second;
  VikLayer *vl = VIK_LAYER(val);
  vik_treeview_move_item ( vl->vt, child_iter, up );

  theone = g_list_find ( val->children, vik_treeview_item_get_pointer ( vl->vt, child_iter ) );

  g_assert ( theone != NULL );

  /* the old switcheroo */
  if ( up && theone->next )
  {
    first = theone;
    second = theone->next;
  }
  else if ( !up && theone->prev )
  {
    first = theone->prev;
    second = theone;
  }
  else
    return;

  first->next = second->next;
  second->prev = first->prev;
  first->prev = second;
  second->next = first;

  /* second is now first */

  if ( second->prev )
    second->prev->next = second;
  if ( first->next )
    first->next->prev = first;

  if ( second->prev == NULL )
    val->children = second;
}

/* Draw the aggregate layer. If vik viewport is in half_drawn mode, this means we are only
 * to draw the layers above and including the trigger layer.
 * To do this we don't draw any layers if in half drawn mode, unless we find the
 * trigger layer, in which case we pull up the saved pixmap, turn off half drawn mode and
 * start drawing layers.
 * Also, if we were never in half drawn mode, we save a snapshot
 * of the pixmap before drawing the trigger layer so we can use it again
 * later.
 */
void vik_aggregate_layer_draw ( VikAggregateLayer *val, gpointer data )
{
  GList *iter = val->children;
  VikLayer *vl;
  VikViewport *viewport = VIK_VIEWPORT(data);
  VikLayer *trigger = VIK_LAYER(vik_viewport_get_trigger( viewport ));
  while ( iter ) {
    vl = VIK_LAYER(iter->data);
    if ( vl == trigger ) {
      if ( vik_viewport_get_half_drawn ( viewport ) ) {
        vik_viewport_set_half_drawn ( viewport, FALSE );
        vik_viewport_snapshot_load( viewport );
      } else {
        vik_viewport_snapshot_save( viewport );
      }
    }
    if ( vl->type == VIK_LAYER_AGGREGATE || vl->type == VIK_LAYER_GPS || ! vik_viewport_get_half_drawn( viewport ) )
      vik_layer_draw ( vl, data );
    iter = iter->next;
  }
}

static void aggregate_layer_change_coord_mode ( VikAggregateLayer *val, VikCoordMode mode )
{
  GList *iter = val->children;
  while ( iter )
  {
    vik_layer_change_coord_mode ( VIK_LAYER(iter->data), mode );
    iter = iter->next;
  }
}

static void disconnect_layer_signal ( VikLayer *vl, VikAggregateLayer *val )
{
  g_assert(DISCONNECT_UPDATE_SIGNAL(vl,val)==1);
}

void vik_aggregate_layer_free ( VikAggregateLayer *val )
{
  g_list_foreach ( val->children, (GFunc)(disconnect_layer_signal), val );
  g_list_foreach ( val->children, (GFunc)(g_object_unref), NULL );
  g_list_free ( val->children );
}

static void delete_layer_iter ( VikLayer *vl )
{
  if ( vl->realized )
    vik_treeview_item_delete ( vl->vt, &(vl->iter) );
}

void vik_aggregate_layer_clear ( VikAggregateLayer *val )
{
  g_list_foreach ( val->children, (GFunc)(disconnect_layer_signal), val );
  g_list_foreach ( val->children, (GFunc)(delete_layer_iter), NULL );
  g_list_foreach ( val->children, (GFunc)(g_object_unref), NULL );
  g_list_free ( val->children );
  val->children = NULL;
}

gboolean vik_aggregate_layer_delete ( VikAggregateLayer *val, GtkTreeIter *iter )
{
  VikLayer *l = VIK_LAYER( vik_treeview_item_get_pointer ( VIK_LAYER(val)->vt, iter ) );
  gboolean was_visible = l->visible;

  vik_treeview_item_delete ( VIK_LAYER(val)->vt, iter );
  val->children = g_list_remove ( val->children, l );
  g_assert(DISCONNECT_UPDATE_SIGNAL(l,val)==1);
  g_object_unref ( l );

  return was_visible;
}

#if 0
/* returns 0 == we're good, 1 == didn't find any layers, 2 == got rejected */
guint vik_aggregate_layer_tool ( VikAggregateLayer *val, guint16 layer_type, VikToolInterfaceFunc tool_func, GdkEventButton *event, VikViewport *vvp )
{
  GList *iter = val->children;
  gboolean found_rej = FALSE;
  if (!iter)
    return FALSE;
  while (iter->next)
    iter = iter->next;

  while ( iter )
  {
    /* if this layer "accepts" the tool call */
    if ( VIK_LAYER(iter->data)->visible && VIK_LAYER(iter->data)->type == layer_type )
    {
      if ( tool_func ( VIK_LAYER(iter->data), event, vvp ) )
        return 0;
      else
        found_rej = TRUE;
    }

    /* recursive -- try the same for the child aggregate layer. */
    else if ( VIK_LAYER(iter->data)->visible && VIK_LAYER(iter->data)->type == VIK_LAYER_AGGREGATE )
    {
      gint rv = vik_aggregate_layer_tool(VIK_AGGREGATE_LAYER(iter->data), layer_type, tool_func, event, vvp);
      if ( rv == 0 )
        return 0;
      else if ( rv == 2 )
        found_rej = TRUE;
    }
    iter = iter->prev;
  }
  return found_rej ? 2 : 1; /* no one wanted to accept the tool call in this layer */
}
#endif 

VikLayer *vik_aggregate_layer_get_top_visible_layer_of_type ( VikAggregateLayer *val, gint type )
{
  VikLayer *rv;
  GList *ls = val->children;
  if (!ls)
    return NULL;
  while (ls->next)
    ls = ls->next;

  while ( ls )
  {
    VikLayer *vl = VIK_LAYER(ls->data);
    if ( vl->visible && vl->type == type )
      return vl;
    else if ( vl->visible && vl->type == VIK_LAYER_AGGREGATE )
    {
      rv = vik_aggregate_layer_get_top_visible_layer_of_type(VIK_AGGREGATE_LAYER(vl), type);
      if ( rv )
        return rv;
    }
    ls = ls->prev;
  }
  return NULL;
}

GList *vik_aggregate_layer_get_all_layers_of_type(VikAggregateLayer *val, GList *layers, gint type, gboolean include_invisible)
{
  GList *l = layers;
  GList *children = val->children;
  VikLayer *vl;
  if (!children)
    return layers;

  // Where appropriate *don't* include non-visible layers
  while (children) {
    vl = VIK_LAYER(children->data);
    if (vl->type == VIK_LAYER_AGGREGATE ) {
      // Don't even consider invisible aggregrates, unless told to
      if (vl->visible || include_invisible)
	l = vik_aggregate_layer_get_all_layers_of_type(VIK_AGGREGATE_LAYER(children->data), l, type, include_invisible);
    }
    else if (vl->type == type) {
      if (vl->visible || include_invisible)
	l = g_list_prepend(l, children->data); /* now in top down order */
    }
    else if (type == VIK_LAYER_TRW) {
      /* GPS layers contain TRW layers. cf with usage in file.c */
      if (VIK_LAYER(children->data)->type == VIK_LAYER_GPS) {
	if (VIK_LAYER(children->data)->visible || include_invisible) {
	  if (!vik_gps_layer_is_empty(VIK_GPS_LAYER(children->data))) {
	    /*
	      can not use g_list_concat due to wrong copy method - crashes if used a couple times !!
	      l = g_list_concat (l, vik_gps_layer_get_children (VIK_GPS_LAYER(children->data)));
	    */
	    /* create own copy method instead :( */
	    GList *gps_trw_layers = (GList *)vik_gps_layer_get_children (VIK_GPS_LAYER(children->data));
	    int n_layers = g_list_length (gps_trw_layers);
	    int layer = 0;
	    for ( layer = 0; layer < n_layers; layer++) {
	      l = g_list_prepend(l, gps_trw_layers->data);
	      gps_trw_layers = gps_trw_layers->next;
	    }
	    g_list_free(gps_trw_layers);
	  }
	}
      }
    }
    children = children->next;
  }
  return l;
}

void vik_aggregate_layer_realize ( VikAggregateLayer *val, VikTreeview *vt, GtkTreeIter *layer_iter )
{
  GList *i = val->children;
  GtkTreeIter iter;
  VikLayer *vl = VIK_LAYER(val);
  VikLayer *vli;
  while ( i )
  {
    vli = VIK_LAYER(i->data);
    vik_treeview_add_layer ( vl->vt, layer_iter, &iter, vli->name, val, 
        vli, vli->type, vli->type );
    if ( ! vli->visible )
      vik_treeview_item_set_visible ( vl->vt, &iter, FALSE );
    vik_layer_realize ( vli, vl->vt, &iter );
    i = i->next;
  }
}

const GList *vik_aggregate_layer_get_children ( VikAggregateLayer *val )
{
  return val->children;
}

gboolean vik_aggregate_layer_is_empty ( VikAggregateLayer *val )
{
  if ( val->children )
    return FALSE;
  return TRUE;
}

static void aggregate_layer_drag_drop_request ( VikAggregateLayer *val_src, VikAggregateLayer *val_dest, GtkTreeIter *src_item_iter, GtkTreePath *dest_path )
{
  VikTreeview *vt = VIK_LAYER(val_src)->vt;
  VikLayer *vl = vik_treeview_item_get_pointer(vt, src_item_iter);
  GtkTreeIter dest_iter;
  gchar *dp;
  gboolean target_exists;

  dp = gtk_tree_path_to_string(dest_path);
  target_exists = vik_treeview_get_iter_from_path_str(vt, &dest_iter, dp);

  /* vik_aggregate_layer_delete unrefs, but we don't want that here.
   * we're still using the layer. */
  g_object_ref ( vl );
  vik_aggregate_layer_delete(val_src, src_item_iter);

  if (target_exists) {
    vik_aggregate_layer_insert_layer(val_dest, vl, &dest_iter);
  } else {
    vik_aggregate_layer_insert_layer(val_dest, vl, NULL); /* append */
  }
  g_free(dp);
}

