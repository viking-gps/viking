/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013-2020, Rob Norris <rw_norris@hotmail.com>
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
#include "vikgpslayer.h"
#include "vikgeocluelayer.h"
#include "viktrwlayer_analysis.h"
#include "viktrwlayer_tracklist.h"
#include "viktrwlayer_waypointlist.h"
#include "icons/icons.h"
#include "maputils.h"
#include "background.h"

static void aggregate_layer_marshall( VikAggregateLayer *val, guint8 **data, guint *len );
static VikAggregateLayer *aggregate_layer_unmarshall( guint8 *data, guint len, VikViewport *vvp );
static void aggregate_layer_change_coord_mode ( VikAggregateLayer *val, VikCoordMode mode );
static void aggregate_layer_drag_drop_request ( VikAggregateLayer *val_src, VikAggregateLayer *val_dest, GtkTreeIter *src_item_iter, GtkTreePath *dest_path );
static const gchar* aggregate_layer_tooltip ( VikAggregateLayer *val );
static void aggregate_layer_add_menu_items ( VikAggregateLayer *val, GtkMenu *menu, gpointer vlp );
static gboolean aggregate_layer_set_param ( VikAggregateLayer *val, VikLayerSetParam *vlsp );
static VikLayerParamData aggregate_layer_get_param ( VikAggregateLayer *val, guint16 id, gboolean is_file_operation );
static void aggregate_layer_change_param ( GtkWidget *widget, ui_change_values values );
static void aggregate_layer_post_read ( VikAggregateLayer *val, VikViewport *vvp, gboolean from_file );
static void tac_calculate ( VikAggregateLayer *val );

static gchar *params_tile_area_levels[] = { "15", "14", "13", "12", "11", "10", "9", "8", "7", "6", "5", "4", NULL };

static VikLayerParamScale params_scales[] = {
 // min, max, step, digits (decimal places)
 { 0, 255, 3, 0 }, // alpha
};

static VikLayerParamData color_default ( void ) {
  VikLayerParamData data; gdk_color_parse ( "orange", &data.c ); return data;
}
static VikLayerParamData alpha_default ( void ) { return VIK_LPD_UINT ( 100 ); }
static VikLayerParamData tile_area_level_default ( void ) { return VIK_LPD_UINT ( 1 ); }

static VikLayerParamData color_default_max_sqr ( void ) {
  VikLayerParamData data; gdk_color_parse ( "purple", &data.c ); return data;
}
static VikLayerParamData color_default_contig ( void ) {
  VikLayerParamData data; gdk_color_parse ( "lightgreen", &data.c ); return data;
}
static VikLayerParamData color_default_cluster ( void ) {
  VikLayerParamData data; gdk_color_parse ( "darkgreen", &data.c ); return data;
}

static gchar *params_groups[] = { N_("Tracks Area Coverage"), "TAC Advanced" };
enum { GROUP_TAC, GROUP_TAC_ADV };

VikLayerParam aggregate_layer_params[] = {
  { VIK_LAYER_AGGREGATE, "tac_on", VIK_LAYER_PARAM_BOOLEAN, GROUP_TAC, N_("On:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_false_default, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "alpha", VIK_LAYER_PARAM_UINT, GROUP_TAC, N_("Alpha:"), VIK_LAYER_WIDGET_HSCALE, params_scales, NULL,
    N_("Control the Alpha value for transparency effects"), alpha_default, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "color", VIK_LAYER_PARAM_COLOR, GROUP_TAC, N_("Color:"), VIK_LAYER_WIDGET_COLOR, NULL, NULL, NULL, color_default, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "max_sqr_on", VIK_LAYER_PARAM_BOOLEAN, GROUP_TAC_ADV, N_("Max Square On:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_false_default, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "max_sqr_alpha", VIK_LAYER_PARAM_UINT, GROUP_TAC_ADV, N_("Max Square Alpha:"), VIK_LAYER_WIDGET_HSCALE, params_scales, NULL,
    N_("Control the Alpha value for transparency effects"), alpha_default, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "max_sqr_color", VIK_LAYER_PARAM_COLOR, GROUP_TAC_ADV, N_("Max Square Color:"), VIK_LAYER_WIDGET_COLOR, NULL, NULL, NULL, color_default_max_sqr, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "contig_on", VIK_LAYER_PARAM_BOOLEAN, GROUP_TAC_ADV, N_("Contiguous On:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "contig_alpha", VIK_LAYER_PARAM_UINT, GROUP_TAC_ADV, N_("Contiguous Alpha:"), VIK_LAYER_WIDGET_HSCALE, params_scales, NULL,
    N_("Control the Alpha value for transparency effects"), alpha_default, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "contig_color", VIK_LAYER_PARAM_COLOR, GROUP_TAC_ADV, N_("Contiguous Color:"), VIK_LAYER_WIDGET_COLOR, NULL, NULL, NULL, color_default_contig, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "cluster_on", VIK_LAYER_PARAM_BOOLEAN, GROUP_TAC_ADV, N_("Cluster On:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_false_default, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "cluster_alpha", VIK_LAYER_PARAM_UINT, GROUP_TAC_ADV, N_("Cluster Alpha:"), VIK_LAYER_WIDGET_HSCALE, params_scales, NULL,
    N_("Control the Alpha value for transparency effects"), alpha_default, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "cluster_color", VIK_LAYER_PARAM_COLOR, GROUP_TAC_ADV, N_("Cluster Color:"), VIK_LAYER_WIDGET_COLOR, NULL, NULL, NULL, color_default_cluster, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "drawgrid", VIK_LAYER_PARAM_BOOLEAN, GROUP_TAC, N_("Draw Grid:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "tilearealevel", VIK_LAYER_PARAM_UINT, GROUP_TAC, N_("Tile Area Level:"), VIK_LAYER_WIDGET_COMBOBOX, params_tile_area_levels, NULL,
    N_("Area size. A higher level means a smaller grid."), tile_area_level_default, NULL, NULL },
};

typedef enum { BASIC, MAX_SQR, CONTIG, CLUSTER, CP_NUM } common_property_types;

enum {
      PARAM_DO_TAC=0,
      PARAM_ALPHA,
      PARAM_COLOR,
      PARAM_MAX_SQR_ON,
      PARAM_MAX_SQR_ALPHA,
      PARAM_MAX_SQR_COLOR,
      PARAM_CONTIG_ON,
      PARAM_CONTIG_ALPHA,
      PARAM_CONTIG_COLOR,
      PARAM_CLUSTER_ON,
      PARAM_CLUSTER_ALPHA,
      PARAM_CLUSTER_COLOR,
      PARAM_DRAW_GRID,
      PARAM_TILE_AREA_LEVEL,
      NUM_PARAMS
};

VikLayerInterface vik_aggregate_layer_interface = {
  "Aggregate",
  N_("Aggregate"),
  "<control><shift>A",
  &vikaggregatelayer_pixbuf,

  NULL,
  0,

  aggregate_layer_params,
  NUM_PARAMS,
  params_groups,
  G_N_ELEMENTS(params_groups),

  VIK_MENU_ITEM_ALL,

  (VikLayerFuncCreate)                  vik_aggregate_layer_create,
  (VikLayerFuncRealize)                 vik_aggregate_layer_realize,
  (VikLayerFuncPostRead)                aggregate_layer_post_read,
  (VikLayerFuncFree)                    vik_aggregate_layer_free,

  (VikLayerFuncProperties)              NULL,
  (VikLayerFuncDraw)                    vik_aggregate_layer_draw,
  (VikLayerFuncChangeCoordMode)         aggregate_layer_change_coord_mode,
  
  (VikLayerFuncGetTimestamp)            NULL,

  (VikLayerFuncSetMenuItemsSelection)	NULL,
  (VikLayerFuncGetMenuItemsSelection)	NULL,

  (VikLayerFuncAddMenuItems)            aggregate_layer_add_menu_items,
  (VikLayerFuncSublayerAddMenuItems)    NULL,

  (VikLayerFuncSublayerRenameRequest)   NULL,
  (VikLayerFuncSublayerToggleVisible)   NULL,
  (VikLayerFuncSublayerTooltip)         NULL,
  (VikLayerFuncLayerTooltip)            aggregate_layer_tooltip,
  (VikLayerFuncLayerSelected)           NULL,

  (VikLayerFuncMarshall)		aggregate_layer_marshall,
  (VikLayerFuncUnmarshall)		aggregate_layer_unmarshall,

  (VikLayerFuncSetParam)                aggregate_layer_set_param,
  (VikLayerFuncGetParam)                aggregate_layer_get_param,
  (VikLayerFuncChangeParam)             aggregate_layer_change_param,

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

  (VikLayerFuncRefresh)                 NULL,
};

struct _VikAggregateLayer {
  VikLayer vl;
  GList *children;
  // One per layer
  GtkWidget *tracks_analysis_dialog;

  // Tracks Area Coverage
  gboolean calculating;
  guint zoom_level;
  gboolean draw_grid;

  gboolean on[CP_NUM];
  guint8 alpha[CP_NUM];
  GdkColor color[CP_NUM];
  GdkPixbuf *pixbuf[CP_NUM];
  guint num_tiles[CP_NUM];

  guint cont_label;
  guint clust_label;
  guint max_square;
  gint xx,yy; // Location of top left max square tile

  // Maybe a sparse table would be more efficient
  //  but this seems to work OK at least if all tracks are confined within a not too diverse area
  GHashTable *tiles;
  GHashTable *tiles_clust;
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

static gboolean aggregate_layer_set_param ( VikAggregateLayer *val, VikLayerSetParam *vlsp )
{
  switch ( vlsp->id ) {
    case PARAM_DO_TAC: val->on[BASIC] = vlsp->data.b; break;
    case PARAM_ALPHA: if ( vlsp->data.u <= 255 ) val->alpha[BASIC] = vlsp->data.u; break;
    case PARAM_DRAW_GRID: val->draw_grid = vlsp->data.b; break;
    case PARAM_COLOR: val->color[BASIC] = vlsp->data.c; break;
    case PARAM_MAX_SQR_ON: val->on[MAX_SQR] = vlsp->data.b; break;
    case PARAM_MAX_SQR_ALPHA: if ( vlsp->data.u <= 255 ) val->alpha[MAX_SQR] = vlsp->data.u; break;
    case PARAM_MAX_SQR_COLOR: val->color[MAX_SQR] = vlsp->data.c; break;
    case PARAM_CONTIG_ON: val->on[CONTIG] = vlsp->data.b; break;
    case PARAM_CONTIG_ALPHA: if ( vlsp->data.u <= 255 ) val->alpha[CONTIG] = vlsp->data.u; break;
    case PARAM_CONTIG_COLOR: val->color[CONTIG] = vlsp->data.c; break;
    case PARAM_CLUSTER_ON: val->on[CLUSTER] = vlsp->data.b; break;
    case PARAM_CLUSTER_ALPHA: if ( vlsp->data.u <= 255 ) val->alpha[CLUSTER] = vlsp->data.u; break;
    case PARAM_CLUSTER_COLOR: val->color[CLUSTER] = vlsp->data.c; break;
    case PARAM_TILE_AREA_LEVEL:
      if ( vlsp->data.u <= G_N_ELEMENTS(params_tile_area_levels) ) {
        val->zoom_level = pow ( 2, vlsp->data.u + 2);
        // Ensure when 'apply' button is clicked the TAC is recalculated for the new area value
        if ( !vlsp->is_file_operation ) {
          if ( VIK_LAYER(val)->realized ) {
            if ( val->on[BASIC] ) {
              if ( !val->calculating ) {
                tac_calculate ( val );
              }
            }
          }
        }
      }
      break;
    default: break;
  }
  return TRUE;
}

static VikLayerParamData aggregate_layer_get_param ( VikAggregateLayer *val, guint16 id, gboolean is_file_operation )
{
  VikLayerParamData rv;
  switch ( id ) {
    case PARAM_DO_TAC: rv.u = val->on[BASIC]; break;
    case PARAM_ALPHA: rv.u = val->alpha[BASIC]; break;
    case PARAM_DRAW_GRID: rv.b = val->draw_grid; break;
    case PARAM_COLOR: rv.c = val->color[BASIC]; break;
    case PARAM_MAX_SQR_ON: rv.b = val->on[MAX_SQR]; break;
    case PARAM_MAX_SQR_ALPHA: rv.u = val->alpha[MAX_SQR]; break;
    case PARAM_MAX_SQR_COLOR: rv.c = val->color[MAX_SQR]; break;
    case PARAM_CONTIG_ON: rv.b = val->on[CONTIG]; break;
    case PARAM_CONTIG_ALPHA: rv.u = val->alpha[CONTIG]; break;
    case PARAM_CONTIG_COLOR: rv.c = val->color[CONTIG]; break;
    case PARAM_CLUSTER_ON: rv.b = val->on[CLUSTER]; break;
    case PARAM_CLUSTER_ALPHA: rv.u = val->alpha[CLUSTER]; break;
    case PARAM_CLUSTER_COLOR: rv.c = val->color[CLUSTER]; break;
    case PARAM_TILE_AREA_LEVEL: rv.u = map_utils_mpp_to_scale ( val->zoom_level ) - 2; break;
    default: break;
  }
  return rv;
}

static void aggregate_layer_change_param ( GtkWidget *widget, ui_change_values values )
{
  if ( GPOINTER_TO_INT(values[UI_CHG_PARAM_ID]) == PARAM_DO_TAC ) {
    VikLayerParamData vlpd = a_uibuilder_widget_get_value ( widget, values[UI_CHG_PARAM] );
    GtkWidget **ww1 = values[UI_CHG_WIDGETS];
    GtkWidget **ww2 = values[UI_CHG_LABELS];
    // Sensitize all the other coverage widgets setting according the very first 'On' one
    for ( guint xx = PARAM_ALPHA; xx <= PARAM_TILE_AREA_LEVEL; xx++ ) {
      GtkWidget *w1 = ww1[xx];
      GtkWidget *w2 = ww2[xx];
      if ( w1 ) gtk_widget_set_sensitive ( w1, vlpd.b );
      if ( w2 ) gtk_widget_set_sensitive ( w2, vlpd.b );
    }
  }
}

static GdkPixbuf* layer_pixbuf_update ( GdkPixbuf *pixbuf, GdkColor color, guint size_x, guint size_y, guint8 alpha )
{
  if ( pixbuf )
    g_object_unref ( pixbuf );
  pixbuf = gdk_pixbuf_new ( GDK_COLORSPACE_RGB, FALSE, 8, size_x, size_y );
  guint32 pixel = ((color.red & 0xff00) << 16) |
    ((color.green & 0xff00) << 8) |
    (color.blue & 0xff00);

  gdk_pixbuf_fill ( pixbuf, pixel );
  pixbuf = ui_pixbuf_set_alpha ( pixbuf, alpha );
  return pixbuf;
}  

VikAggregateLayer *vik_aggregate_layer_create (VikViewport *vp)
{
  VikAggregateLayer *rv = vik_aggregate_layer_new (vp);
  vik_layer_rename ( VIK_LAYER(rv), vik_aggregate_layer_interface.name );
  vik_layer_set_defaults ( VIK_LAYER(rv), vp );
  return rv;
}

static void aggregate_layer_marshall( VikAggregateLayer *val, guint8 **data, guint *datalen )
{
  GList *child = val->children;
  VikLayer *child_layer;
  guint8 *ld; 
  guint ll;
  GByteArray* b = g_byte_array_new ();
  guint len;

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

static VikAggregateLayer *aggregate_layer_unmarshall( guint8 *data, guint len, VikViewport *vvp )
{
#define alm_size (*(guint *)data)
#define alm_next \
  len -= sizeof(gint) + alm_size; \
  data += sizeof(gint) + alm_size;
  
  VikAggregateLayer *rv = vik_aggregate_layer_new(vvp);
  VikLayer *child_layer;

  vik_layer_unmarshall_params ( VIK_LAYER(rv), data+sizeof(guint), alm_size, vvp );
  alm_next;

  while (len>0) {
    child_layer = vik_layer_unmarshall ( data + sizeof(guint), alm_size, vvp );
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

VikAggregateLayer *vik_aggregate_layer_new (VikViewport *vvp)
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER ( g_object_new ( VIK_AGGREGATE_LAYER_TYPE, NULL ) );
  vik_layer_set_type ( VIK_LAYER(val), VIK_LAYER_AGGREGATE );
  vik_layer_set_defaults ( VIK_LAYER(val), vvp );
  val->children = NULL;
  val->tiles = g_hash_table_new_full ( g_str_hash, g_str_equal, g_free, NULL );
  val->tiles_clust = g_hash_table_new_full ( g_str_hash, g_str_equal, g_free, NULL );

  return val;
}

void vik_aggregate_layer_insert_layer ( VikAggregateLayer *val, VikLayer *l, GtkTreeIter *replace_iter )
{
  GtkTreeIter iter;
  VikLayer *vl = VIK_LAYER(val);

  // By default layers are inserted above the selected layer
  gboolean put_above = TRUE;

  // These types are 'base' types in that you what other information on top
  if ( l->type == VIK_LAYER_MAPS || l->type == VIK_LAYER_DEM || l->type == VIK_LAYER_GEOREF )
    put_above = FALSE;

  if ( vl->realized )
  {
    vik_treeview_insert_layer ( vl->vt, &(vl->iter), &iter, l->name, val, put_above, l, l->type, l->type, replace_iter, vik_layer_get_timestamp(l) );
    if ( ! l->visible )
      vik_treeview_item_set_visible ( vl->vt, &iter, FALSE );
    vik_layer_realize ( l, vl->vt, &iter );

    if ( val->children == NULL )
      vik_treeview_expand ( vl->vt, &(vl->iter) );
  }

  if (replace_iter) {
    GList *theone = g_list_find ( val->children, vik_treeview_item_get_pointer ( vl->vt, replace_iter ) );
    if ( put_above )
      val->children = g_list_insert ( val->children, l, g_list_position(val->children,theone)+1 );
    else
      // Thus insert 'here' (so don't add 1)
      val->children = g_list_insert ( val->children, l, g_list_position(val->children,theone) );
  } else {
    // Effectively insert at 'end' of the list to match how displayed in the treeview
    //  - but since it is drawn from 'bottom first' it is actually the first in the child list
    // This ordering is especially important if it is a map or similar type,
    //  which needs be drawn first for the layering draw method to work properly.
    // ATM this only happens when a layer is drag/dropped to the end of an aggregate layer
    val->children = g_list_prepend ( val->children, l );
  }
  g_signal_connect_swapped ( G_OBJECT(l), "update", G_CALLBACK(vik_layer_emit_update_secondary), val );
}

/**
 * vik_aggregate_layer_add_layer:
 * @allow_reordering: should be set for GUI interactions,
 *                    whereas loading from a file needs strict ordering and so should be FALSE
 */
void vik_aggregate_layer_add_layer ( VikAggregateLayer *val, VikLayer *l, gboolean allow_reordering )
{
  GtkTreeIter iter;
  VikLayer *vl = VIK_LAYER(val);

  // By default layers go to the top
  gboolean put_above = TRUE;

  if ( allow_reordering ) {
    // These types are 'base' types in that you what other information on top
    if ( l->type == VIK_LAYER_MAPS || l->type == VIK_LAYER_DEM || l->type == VIK_LAYER_GEOREF )
      put_above = FALSE;
  }

  if ( vl->realized )
  {
    vik_treeview_add_layer ( vl->vt, &(vl->iter), &iter, l->name, val, put_above, l, l->type, l->type, vik_layer_get_timestamp(l) );
    if ( ! l->visible )
      vik_treeview_item_set_visible ( vl->vt, &iter, FALSE );
    vik_layer_realize ( l, vl->vt, &iter );

    if ( val->children == NULL )
      vik_treeview_expand ( vl->vt, &(vl->iter) );
  }

  if ( put_above )
    val->children = g_list_append ( val->children, l );
  else
    val->children = g_list_prepend ( val->children, l );

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

static gboolean is_tile_occupied ( GHashTable *ght, gint x, gint y )
{
  gchar *key = g_strdup_printf ( "%d:%d", x, y );
#if !GLIB_CHECK_VERSION(2,26,0)
  gpointer gp = g_hash_table_lookup ( ght, key );
  g_free ( key );
  return gp ? TRUE : FALSE;
#else
  gboolean ans = g_hash_table_contains ( ght, key );
  g_free ( key );
  return ans;
#endif
}

static guint tile_label ( GHashTable *ght, gint x, gint y )
{
  gchar *key = g_strdup_printf ( "%d:%d", x, y );
  gpointer gp = g_hash_table_lookup ( ght, key );
  g_free ( key );
  if ( gp )
    return GPOINTER_TO_UINT(gp);
  else
    return 0;
}

/**
 * is_cluster: returns whether a tile is surrounded by occupied tiles
 */
static gboolean is_cluster ( GHashTable *val, gint x, gint y )
{
  //if ( !is_tile_occupied(val, x, y) ) return FALSE;
  if ( !is_tile_occupied(val, x-1, y) ) return FALSE;
  if ( !is_tile_occupied(val, x+1, y) ) return FALSE;

  if ( !is_tile_occupied(val, x-1, y-1) ) return FALSE;
  if ( !is_tile_occupied(val, x+1, y-1) ) return FALSE;
  if ( !is_tile_occupied(val, x, y-1) ) return FALSE;
  
  if ( !is_tile_occupied(val, x-1, y+1) ) return FALSE;
  if ( !is_tile_occupied(val, x+1, y+1) ) return FALSE;
  if ( !is_tile_occupied(val, x, y+1) ) return FALSE;

  return TRUE;
}

/**
 *
 */
static void tac_draw_section ( VikAggregateLayer *val, VikViewport *vvp, VikCoord *ul, VikCoord *br )
{
  MapCoord ulm, brm;
  gdouble zoom = val->zoom_level;
  gdouble shrinkfactor = 1.0;

  //g_printf ( "%s: zoom %d\n", __FUNCTION__, val->zoom_level );
  // Lots of this tile grid processing is based on the vikmaplayer tile drawing code
  if ( map_utils_vikcoord_to_iTMS(ul, zoom, zoom, &ulm) &&
       map_utils_vikcoord_to_iTMS(br, zoom, zoom, &brm) ) {

    gdouble vp_zoom = vik_viewport_get_xmpp ( vvp );
    shrinkfactor = zoom / vp_zoom;
    gint x, y;
    const gint xmin = MIN(ulm.x, brm.x), xmax = MAX(ulm.x, brm.x);
    const gint ymin = MIN(ulm.y, brm.y), ymax = MAX(ulm.y, brm.y);

    VikCoord coord;
    gint xx, yy;
    gint base_yy;
    // Prevent the program grinding to a halt if trying to deal with thousands of tiles
    const gint tiles = (xmax-xmin) * (ymax-ymin);
    if ( tiles > 65356 ) {
      // Maybe put in status bar
      g_warning ( "%s: Giving up trying to draw many tiles (%d)", __FUNCTION__, tiles );
      return;
    }

    const gdouble tilesize = 256 * shrinkfactor;
    gint xx_tmp, yy_tmp;
    map_utils_iTMS_to_center_vikcoord ( &ulm, &coord );
    vik_viewport_coord_to_screen ( vvp, &coord, &xx_tmp, &yy_tmp );
    xx = xx_tmp; yy = yy_tmp;

    // ceiled so tiles will be maximum size in the case of funky shrinkfactor
    gint tilesize_ceil = ceil ( tilesize );
    gint8 xinc = (ulm.x == xmin) ? 1 : -1;
    gint8 yinc = (ulm.y == ymin) ? 1 : -1;
    gint xend, yend;

    xend = (xinc == 1) ? (xmax+1) : (xmin-1);
    yend = (yinc == 1) ? (ymax+1) : (ymin-1);

    /* above trick so xx,yy doubles. this is so shrinkfactors aren't rounded off
     * eg if tile size 128, shrinkfactor 0.333 */
    xx -= (tilesize/2);
    base_yy = yy - (tilesize/2);

    // When a single tile bigger than the whole screen, only need to draw part of the tile.
    // Thus don't draw only the on screen part, to avoid trying to handle a massive pixbuf
    const guint width = vik_viewport_get_width ( vvp );
    const guint height = vik_viewport_get_height ( vvp );
    gboolean is_big = FALSE;
    if ( tilesize > width && tilesize > height ) {
      is_big = TRUE;
    }
    else {
      // Scale up pixbuf size if needed
      val->pixbuf[BASIC] = layer_pixbuf_update ( val->pixbuf[BASIC], val->color[BASIC], tilesize <= 256 ? 256 : tilesize, tilesize <= 256 ? 256 : tilesize, val->alpha[BASIC] );
      val->pixbuf[MAX_SQR] = layer_pixbuf_update ( val->pixbuf[MAX_SQR], val->color[MAX_SQR], tilesize <= 256 ? 256 : tilesize, tilesize <= 256 ? 256 : tilesize, val->alpha[MAX_SQR] );
      val->pixbuf[CONTIG] = layer_pixbuf_update ( val->pixbuf[CONTIG], val->color[CONTIG], tilesize <= 256 ? 256 : tilesize, tilesize <= 256 ? 256 : tilesize, val->alpha[CONTIG] );
      val->pixbuf[CLUSTER] = layer_pixbuf_update ( val->pixbuf[CLUSTER], val->color[CLUSTER], tilesize <= 256 ? 256 : tilesize, tilesize <= 256 ? 256 : tilesize, val->alpha[CLUSTER] );
    }

    guint tile_draw_count = 0;
    for ( x = ((xinc == 1) ? xmin : xmax); x != xend; x+=xinc ) {
      yy = base_yy;
      for ( y = ((yinc == 1) ? ymin : ymax); y != yend; y+=yinc ) {
        ulm.x = x;
        ulm.y = y;

        if ( is_tile_occupied(val->tiles, x, y) ) {
          //g_printf ( "%s: %d, %d, %d, %d\n", __FUNCTION__, xx, yy, tilesize_ceil, tilesize_ceil );
          if ( !is_big ) {

            vik_viewport_draw_pixbuf ( vvp, val->pixbuf[BASIC], 0, 0, xx, yy, tilesize_ceil, tilesize_ceil );

            if ( val->cont_label && (tile_label(val->tiles, x, y) == val->cont_label) ) {
              vik_viewport_draw_pixbuf ( vvp, val->pixbuf[CONTIG], 0, 0, xx, yy, tilesize_ceil, tilesize_ceil );
            }

            // Cluster drawing
            if ( val->on[CLUSTER] )
              if ( val->clust_label && (tile_label(val->tiles_clust, x, y) == val->clust_label) )
                vik_viewport_draw_pixbuf ( vvp, val->pixbuf[CLUSTER], 0, 0, xx, yy, tilesize_ceil, tilesize_ceil );

            // Max Square drawing
            if ( val->on[MAX_SQR] )
              if ( ( x >= val->xx && x < (val->xx + val->max_square) )
                     && ( y >= val->yy && y < (val->yy + val->max_square) ) ) {
                vik_viewport_draw_pixbuf ( vvp, val->pixbuf[MAX_SQR], 0, 0, xx, yy, tilesize_ceil, tilesize_ceil );
              }
          } else {
            gint x2 = xx;
            gint y2 = yy;
            gint w2 = width;
            gint h2 = height;
            if ( x2 < 0 ) {
              w2 = xx + tilesize_ceil;
              x2 = 0;
            }
            if ( y2 < 0 ) {
              y2 = 0;
              h2 = yy + tilesize_ceil;
            }
            //g_printf ( "%s: %d, %d, %d, %d\n", __FUNCTION__, x2, y2, w2, h2);
            val->pixbuf[BASIC] = layer_pixbuf_update ( val->pixbuf[BASIC], val->color[BASIC], w2, h2, val->alpha[BASIC] );
            vik_viewport_draw_pixbuf ( vvp, val->pixbuf[BASIC], 0, 0, x2, y2, w2, h2 );
          }
          tile_draw_count++;
        }
        yy += tilesize;
      }
      xx += tilesize;
    }
    //g_debug ( "%s: Tiles drawn %d", __FUNCTION__, tile_draw_count );

    // Grid lines if wanted and otherwise doesn't dominate the display either...
    // TODO Probably better to determine a value based on the display / HD
    if ( val->draw_grid && tilesize > 4 ) {
      // Grid drawing here so it gets drawn on top of the previous tiles
      // Thus loop around x & y again, but this time separately
      GdkGC *black_gc = GTK_WIDGET(vvp)->style->black_gc;
      // Draw single grid lines across the whole screen
      xx = xx_tmp; yy = yy_tmp;
      gint base_xx = xx - (tilesize/2);
      base_yy = yy - (tilesize/2);
      
      xx = base_xx;
      for ( x = ((xinc == 1) ? xmin : xmax); x != xend; x+=xinc ) {
         vik_viewport_draw_line ( vvp, black_gc, xx, base_yy, xx, height );
         xx += tilesize;
      }

      yy = base_yy;
      for ( y = ((yinc == 1) ? ymin : ymax); y != yend; y+=yinc ) {
        vik_viewport_draw_line ( vvp, black_gc, base_xx, yy, width, yy );
        yy += tilesize;
      }
    }
  }
}

/**
 *
 */
static void tac_draw ( VikAggregateLayer *val, VikViewport *vp )
{
  // Check compatible drawing mode
  if ( vik_viewport_get_drawmode(vp) != VIK_VIEWPORT_DRAWMODE_MERCATOR )
    return;

  if ( vik_viewport_get_xmpp(vp) != vik_viewport_get_ympp(vp) )
    return;

  VikCoord ul, br;
  vik_viewport_screen_to_coord ( vp, 0, 0, &ul );
  vik_viewport_screen_to_coord ( vp, vik_viewport_get_width(vp), vik_viewport_get_height(vp), &br );
  tac_draw_section ( val, vp, &ul, &br );
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
void vik_aggregate_layer_draw ( VikAggregateLayer *val, VikViewport *vp )
{
  GList *iter = val->children;
  VikLayer *vl;
  VikLayer *trigger = VIK_LAYER(vik_viewport_get_trigger( vp ));
  while ( iter ) {
    vl = VIK_LAYER(iter->data);
    if ( vl == trigger ) {
      if ( vik_viewport_get_half_drawn ( vp ) ) {
        vik_viewport_set_half_drawn ( vp, FALSE );
        vik_viewport_snapshot_load( vp );
      } else {
        vik_viewport_snapshot_save( vp );
      }
    }
    if ( vl->type == VIK_LAYER_AGGREGATE || vl->type == VIK_LAYER_GPS || ! vik_viewport_get_half_drawn( vp ) )
      vik_layer_draw ( vl, vp );
    iter = iter->next;
  }

  // Make coverage to be drawn last (i.e. over the top of any maps)
  if ( val->on[BASIC] ) {
    tac_draw ( val, vp );
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

// A slightly better way of defining the menu callback information
// This should be easier to extend/rework compared to previously
typedef enum {
  MA_VAL = 0,
  MA_VLP,
  MA_LAST
} menu_array_index;

typedef gpointer menu_array_values[MA_LAST];

static void aggregate_layer_child_visible_toggle ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER ( values[MA_VAL] );
  VikLayersPanel *vlp = VIK_LAYERS_PANEL ( values[MA_VLP] );
  VikLayer *vl;

  // Loop around all (child) layers applying visibility setting
  // This does not descend the tree if there are aggregates within aggregate - just the first level of layers held
  GList *iter = val->children;
  while ( iter ) {
    vl = VIK_LAYER ( iter->data );
    vl->visible = !vl->visible;
    // Also set checkbox on/off
    vik_treeview_item_toggle_visible ( vik_layers_panel_get_treeview ( vlp ), &(vl->iter) );
    iter = iter->next;
  }
  // Redraw as view may have changed
  vik_layer_emit_update ( VIK_LAYER ( val ) );
}

static void aggregate_layer_child_visible ( menu_array_values values, gboolean on_off)
{
  // Convert data back to correct types
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER ( values[MA_VAL] );
  VikLayersPanel *vlp = VIK_LAYERS_PANEL ( values[MA_VLP] );
  VikLayer *vl;

  // Loop around all (child) layers applying visibility setting
  // This does not descend the tree if there are aggregates within aggregate - just the first level of layers held
  GList *iter = val->children;
  while ( iter ) {
    vl = VIK_LAYER ( iter->data );
    vl->visible = on_off;
    // Also set checkbox on_off
    vik_treeview_item_set_visible ( vik_layers_panel_get_treeview ( vlp ), &(vl->iter), on_off );
    iter = iter->next;
  }
  // Redraw as view may have changed
  vik_layer_emit_update ( VIK_LAYER ( val ) );
}

static void aggregate_layer_child_visible_on ( menu_array_values values )
{
  aggregate_layer_child_visible ( values, TRUE );
}

static void aggregate_layer_child_visible_off ( menu_array_values values )
{
  aggregate_layer_child_visible ( values, FALSE );
}

/**
 * If order is true sort ascending, otherwise a descending sort
 */
static gint sort_layer_compare ( gconstpointer a, gconstpointer b, gpointer order )
{
  VikLayer *sa = (VikLayer *)a;
  VikLayer *sb = (VikLayer *)b;

  // Default ascending order
  gint answer = g_strcmp0 ( sa->name, sb->name );

  if ( GPOINTER_TO_INT(order) ) {
    // Invert sort order for ascending order
    answer = -answer;
  }

  return answer;
}

static void aggregate_layer_sort_a2z ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER ( values[MA_VAL] );
  vik_treeview_sort_children ( VIK_LAYER(val)->vt, &(VIK_LAYER(val)->iter), VL_SO_ALPHABETICAL_ASCENDING );
  val->children = g_list_sort_with_data ( val->children, sort_layer_compare, GINT_TO_POINTER(TRUE) );
}

static void aggregate_layer_sort_z2a ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER ( values[MA_VAL] );
  vik_treeview_sort_children ( VIK_LAYER(val)->vt, &(VIK_LAYER(val)->iter), VL_SO_ALPHABETICAL_DESCENDING );
  val->children = g_list_sort_with_data ( val->children, sort_layer_compare, GINT_TO_POINTER(FALSE) );
}

/**
 * If order is true sort ascending, otherwise a descending sort
 */
static gint sort_layer_compare_timestamp ( gconstpointer a, gconstpointer b, gpointer order )
{
  VikLayer *sa = (VikLayer *)a;
  VikLayer *sb = (VikLayer *)b;

  // Default ascending order
  // NB This might be relatively slow...
  gint answer = ( vik_layer_get_timestamp(sa) > vik_layer_get_timestamp(sb) );

  if ( GPOINTER_TO_INT(order) ) {
    // Invert sort order for ascending order
    answer = !answer;
  }

  return answer;
}

static void aggregate_layer_sort_timestamp_ascend ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER ( values[MA_VAL] );
  vik_treeview_sort_children ( VIK_LAYER(val)->vt, &(VIK_LAYER(val)->iter), VL_SO_DATE_ASCENDING );
  val->children = g_list_sort_with_data ( val->children, sort_layer_compare_timestamp, GINT_TO_POINTER(TRUE) );
}

static void aggregate_layer_sort_timestamp_descend ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER ( values[MA_VAL] );
  vik_treeview_sort_children ( VIK_LAYER(val)->vt, &(VIK_LAYER(val)->iter), VL_SO_DATE_DESCENDING );
  val->children = g_list_sort_with_data ( val->children, sort_layer_compare_timestamp, GINT_TO_POINTER(FALSE) );
}

/**
 * aggregate_layer_waypoint_create_list:
 * @vl:        The layer that should create the waypoint and layers list
 * @user_data: Not used in this function
 *
 * Returns: A list of #vik_trw_waypoint_list_t
 */
static GList* aggregate_layer_waypoint_create_list ( VikLayer *vl, gpointer user_data )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER(vl);

  // Get all TRW layers
  GList *layers = NULL;
  layers = vik_aggregate_layer_get_all_layers_of_type ( val, layers, VIK_LAYER_TRW, TRUE );

  // For each TRW layers keep adding the waypoints to build a list of all of them
  GList *waypoints_and_layers = NULL;
  layers = g_list_first ( layers );
  while ( layers ) {
    GList *waypoints = NULL;
    waypoints = g_list_concat ( waypoints, g_hash_table_get_values ( vik_trw_layer_get_waypoints( VIK_TRW_LAYER(layers->data) ) ) );

    waypoints_and_layers = g_list_concat ( waypoints_and_layers, vik_trw_layer_build_waypoint_list_t ( VIK_TRW_LAYER(layers->data), waypoints ) );

    layers = g_list_next ( layers );
  }
  g_list_free ( layers );

  return waypoints_and_layers;
}

static void aggregate_layer_waypoint_list_dialog ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER ( values[MA_VAL] );
  gchar *title = g_strdup_printf ( _("%s: Waypoint List"), VIK_LAYER(val)->name );
  vik_trw_layer_waypoint_list_show_dialog ( title, VIK_LAYER(val), NULL, aggregate_layer_waypoint_create_list, TRUE );
  g_free ( title );
}

/**
 *
 */
gboolean vik_aggregate_layer_search_date ( VikAggregateLayer *val, gchar *date_str )
{
  gboolean found = FALSE;
  VikViewport *vvp = vik_window_viewport ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(val)) );

  VikCoord position;
  GList *layers = NULL;
  layers = vik_aggregate_layer_get_all_layers_of_type ( val, layers, VIK_LAYER_TRW, TRUE );
  GList *gl = layers;
  // Search tracks first
  while ( gl && !found ) {
    // Make it auto select the item if found
    found = vik_trw_layer_find_date ( VIK_TRW_LAYER(gl->data), date_str, &position, vvp, TRUE, TRUE );
    gl = g_list_next ( gl );
  }
  if ( !found ) {
    // Reset and try on Waypoints
    gl = g_list_first ( layers );
    while ( gl && !found ) {
      // Make it auto select the item if found
      found = vik_trw_layer_find_date ( VIK_TRW_LAYER(gl->data), date_str, &position, vvp, FALSE, TRUE );
      gl = g_list_next ( gl );
    }
  }
  g_list_free ( layers );

  return found;
}
/**
 * Search all TrackWaypoint layers in this aggregate layer for an item on the user specified date
 */
static void aggregate_layer_search_date ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER ( values[MA_VAL] );
  gchar *date_str = a_dialog_get_date ( VIK_GTK_WINDOW_FROM_LAYER(val), _("Search by Date") );
  if ( !date_str )
    return;

  gboolean found = vik_aggregate_layer_search_date ( val, date_str );
  if ( !found )
    a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(val), _("No items found with the requested date.") );
  g_free ( date_str );
}

/**
 * aggregate_layer_track_create_list:
 * @vl:        The layer that should create the track and layers list
 * @user_data: Not used in this function
 *
 * Returns: A list of #vik_trw_and_track_t
 */
static GList* aggregate_layer_track_create_list ( VikLayer *vl, gpointer user_data )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER(vl);

  // Get all TRW layers
  GList *layers = NULL;
  layers = vik_aggregate_layer_get_all_layers_of_type ( val, layers, VIK_LAYER_TRW, TRUE );

  // For each TRW layers keep adding the tracks and routes to build a list of all of them
  GList *tracks_and_layers = NULL;
  layers = g_list_first ( layers );
  while ( layers ) {
    GList *tracks = NULL;
    tracks = g_list_concat ( tracks, g_hash_table_get_values ( vik_trw_layer_get_tracks( VIK_TRW_LAYER(layers->data) ) ) );
    tracks = g_list_concat ( tracks, g_hash_table_get_values ( vik_trw_layer_get_routes( VIK_TRW_LAYER(layers->data) ) ) );

    tracks_and_layers = g_list_concat ( tracks_and_layers, vik_trw_layer_build_track_list_t ( VIK_TRW_LAYER(layers->data), tracks ) );

    layers = g_list_next ( layers );
  }
  g_list_free ( layers );

  return tracks_and_layers;
}

static void aggregate_layer_track_list_dialog ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER ( values[MA_VAL] );
  gchar *title = g_strdup_printf ( _("%s: Track and Route List"), VIK_LAYER(val)->name );
  vik_trw_layer_track_list_show_dialog ( title, VIK_LAYER(val), NULL, aggregate_layer_track_create_list, TRUE );
  g_free ( title );
}

/**
 * aggregate_layer_analyse_close:
 *
 * Stuff to do on dialog closure
 */
static void aggregate_layer_analyse_close ( GtkWidget *dialog, gint resp, VikLayer* vl )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER(vl);
  gtk_widget_destroy ( dialog );
  val->tracks_analysis_dialog = NULL;
}

static void aggregate_layer_analyse ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER ( values[MA_VAL] );

  // There can only be one!
  if ( val->tracks_analysis_dialog )
    return;

  val->tracks_analysis_dialog = vik_trw_layer_analyse_this ( VIK_GTK_WINDOW_FROM_LAYER(VIK_LAYER(val)),
                                                             VIK_LAYER(val)->name,
                                                             VIK_LAYER(val),
                                                             NULL,
                                                             aggregate_layer_track_create_list,
                                                             aggregate_layer_analyse_close );
}

static void aggregate_layer_load_external_layers ( VikAggregateLayer *val )
{
  GList *iter = val->children;
  while ( iter ) {
    VikLayer *vl = VIK_LAYER ( iter->data );
    g_debug ( "child %d",  vl->type );
    switch ( vl->type ) {
      case VIK_LAYER_TRW: trw_ensure_layer_loaded ( VIK_TRW_LAYER ( iter->data ) ); break;
      case VIK_LAYER_AGGREGATE: aggregate_layer_load_external_layers ( VIK_AGGREGATE_LAYER ( iter->data ) ); break;
      default: /* do nothing */ break;
    }
    iter = iter->next;
  }
}

static void aggregate_layer_load_external_layers_click ( menu_array_values values )
{
  aggregate_layer_load_external_layers ( VIK_AGGREGATE_LAYER ( values[MA_VAL] ) );
  vik_layers_panel_calendar_update ( values[MA_VLP] );
}

/**
 * aggregate_layer_file_load:
 *
 * Asks the user to select files and then load them into this aggregate layer
 */
static void aggregate_layer_file_load ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER ( values[MA_VAL] );
  VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(val));
  VikViewport *vvp = vik_window_viewport ( vw );

  gchar *filename = NULL;
  GSList *files = vu_get_ui_selected_gps_files ( vw, TRUE ); // Only GPX types for the filter type ATM

  if ( files ) {
    GSList *cur_file = files;
    while ( cur_file ) {
      filename = cur_file->data;

      VikLoadType_t ans = a_file_load ( val, vvp, NULL, filename, TRUE, FALSE, NULL );
      if ( ans <= LOAD_TYPE_UNSUPPORTED_FAILURE ) {
        a_dialog_error_msg_extra ( GTK_WINDOW(vw), _("Unable to load %s"), filename );
      } else if ( ans <= LOAD_TYPE_VIK_FAILURE_NON_FATAL ) {
        gchar *msg = g_strdup_printf (_("WARNING: issues encountered loading %s"), a_file_basename(filename) );
        vik_window_statusbar_update ( vw, msg, VIK_STATUSBAR_INFO );
        g_free ( msg );
      }
      g_free ( filename );
      cur_file = g_slist_next ( cur_file );
    }
    g_slist_free ( files );
  }

  vik_layer_emit_update ( VIK_LAYER ( val ) );
}

/**
 *
 */
static void add_tile_label ( GHashTable *ght, gint x, gint y, guint id )
{
  gchar *key = g_strdup_printf ( "%d:%d", x, y );
  g_hash_table_insert ( ght, key, GUINT_TO_POINTER(id) );
}

/**
 *
 */
static void add_tile ( GHashTable *ght, gint x, gint y )
{
  add_tile_label ( ght, x, y, 0 );
}

/**
 *
 */
static void check_point ( VikAggregateLayer *val, VikCoord *coord )
{
  MapCoord mc;
  gdouble zoom = val->zoom_level;
  // Give up if can't convert - shouldn't happen
  if ( !map_utils_vikcoord_to_iTMS(coord, zoom, zoom, &mc) ) {
    g_warning ( "%s: %s", __FUNCTION__, "Failed to convert positions" );
    return;
  }

  if ( ! is_tile_occupied ( val->tiles, mc.x, mc.y ) ) {
    add_tile ( val->tiles, mc.x, mc.y );
    val->num_tiles[BASIC]++;
  }
}

/**
 *
 */
static void check_track ( VikAggregateLayer *val, vik_trw_and_track_t *vtlist )
{
  VikTrack *trk = vtlist->trk;
  //g_debug ( "%s: %s", __FUNCTION__, trk->name );
  guint no_times = 0;
  GList *iter = trk->trackpoints;
  while ( iter ) {
    // Only do trackpoints with timestamps
    // - i.e. hopefully to avoid artificial tracks
    if ( !isnan(VIK_TRACKPOINT(iter->data)->timestamp) ) {
      check_point ( val, &VIK_TRACKPOINT(iter->data)->coord );
    }
    else
      no_times++;
    iter = iter->next;
  }
  // Handy to find out if your not expecting any of these
  if ( no_times )
    g_debug ( "%s: %d points encountered with no times", __FUNCTION__, no_times );
}

typedef struct {
  GList *tracks_and_layers;
  VikAggregateLayer *val;
  guint num_of_tracks;
} CalculateThreadT;

static void ct_free ( CalculateThreadT *ct )
{
  ct->val->calculating = FALSE;
  g_list_free ( ct->tracks_and_layers );
  g_free ( ct );
}

static void ct_cancel ( CalculateThreadT *ct )
{
  // Draw as much as we have processed so far
  vik_layer_emit_update ( VIK_LAYER(ct->val) ); // NB update display from background
}

// Since previous square should have been checked, only need to consider the next outer row+column
//  rather than rechecking the contents of the whole square again
//  this is obviously gets more efficient as the square that needs checking gets bigger
static gboolean is_square_next ( VikAggregateLayer *val, gint x, gint y, guint n )
{
  gint tmpx = x;
  gint tmpy = y + n - 1;
  for ( tmpx = x; tmpx < (x + n); tmpx++ ) {
    if ( ! is_tile_occupied(val->tiles, tmpx, tmpy) ) {
      return FALSE;
    }
  }
  tmpx = x + n - 1;
  for ( gint tmpy = y; tmpy < (y + n); tmpy++ ) {
    if ( ! is_tile_occupied(val->tiles, tmpx, tmpy) ) {
      return FALSE;
    }
  }
  // NB tile (tmpx+n-1, tmpy+n-1) gets checked twice, but this isn't too much of a waste
  return TRUE;
}

static gboolean is_square ( VikAggregateLayer *val, gint x, gint y, guint n )
{
  for ( gint tmpx = x; tmpx < (x + n); tmpx++ ) {
     for ( gint tmpy = y; tmpy < (y + n); tmpy++ ) {
       if ( ! is_tile_occupied(val->tiles, tmpx, tmpy) ) {
         return FALSE;
      }
    }
  }
  return TRUE;
}

/*
 * Union Find stuff for labelling
 */

/* The 'labels' array has the meaning that labels[x] is an alias for the label x; by
   following this chain until x == labels[x], you can find the canonical name of an
   equivalence class.  The labels start at one; labels[0] is a special value indicating
   the highest label already used. */

guint *labels;
guint n_labels = 0; /* length of the labels array */

/**
 * uf_find:
 *  returns the canonical label for the equivalence class containing x
 */
static guint uf_find ( guint x ) {
  guint y = x;
  while (labels[y] != y)
    y = labels[y];
  
  while (labels[x] != x) {
    guint z = labels[x];
    labels[x] = y;
    x = z;
  }
  return y;
}

/**
 * uf_union:
 *   joins two equivalence classes and returns the canonical label of the resulting class.
 */
static guint uf_union ( guint x, guint y ) {
  return labels[uf_find(x)] = uf_find(y);
}

/**
 * uf_make_set:
 *  creates a new equivalence class and returns its label
 */
static guint uf_make_set(void) {
  labels[0]++;
  labels[labels[0]] = labels[0];
  return labels[0];
}

/**
 * uf_init:
 *   Allocate array for potential labels
 */
static void uf_init ( guint max_labels ) {
  n_labels = max_labels;
  labels = g_malloc0_n ( sizeof(guint), n_labels );
  labels[0] = 0;
}

/**
 * uf_finish: clean up
 */
static void uf_finish ( void ) {
  n_labels = 0;
  g_free ( labels );
  labels = NULL;
}

// NB ATM This only tracks one such area
//  (there might be multiple such areas)
static void tac_contiguous_calc ( VikAggregateLayer *val )
{
  clock_t begin = clock();

  GHashTableIter iter;
  gpointer key, value;
  gint x,y;
  gint tlx=G_MAXINT,tly=G_MAXINT,brx=-G_MAXINT,bry=-G_MAXINT;

  // Get grid extents
  g_hash_table_iter_init ( &iter, val->tiles );
  while ( g_hash_table_iter_next(&iter, &key, &value) ) {
    (void)sscanf ( key, "%d:%d", &x, &y );
    if ( x < tlx ) tlx = x;
    if ( x > brx ) brx = x;
    if ( y < tly ) tly = y;
    if ( y > bry ) bry = y;
  }
  
  // Process the grid according to 'labelling clusters on a grid'
  // https://en.wikipedia.org/wiki/Hoshen%E2%80%93Kopelman_algorithm
  guint size = (abs(tlx-brx) * abs(tly-bry)) / 2;
  if ( size == 0 )
    return;

  uf_init ( size );

  for ( gint xx = tlx; xx <= brx; xx++ ) {
    for ( gint yy = tly; yy <= bry; yy++ ) {
      if ( is_tile_occupied(val->tiles, xx, yy) ) {
        guint label_up = tile_label (val->tiles, xx-1, yy ); // NB don't have to worry about -1 going out of bounds
        guint label_left = tile_label (val->tiles, xx, yy-1 );
        switch ( !!label_up + !!label_left ) {
        case 0: // New
          add_tile_label ( val->tiles, xx, yy, uf_make_set() );
          break;
        case 1: // Existing
          add_tile_label ( val->tiles, xx, yy, MAX(label_up, label_left) );
          break;
        case 2: // Bind existing
          add_tile_label ( val->tiles, xx, yy, uf_union(label_up, label_left) );
          break;
        default: // Should not happen
          g_critical ("%s: labelling algorithm broken", __FUNCTION__);
          break;
        }
      }
    }
  }

  // Reprocess the grid to compare the size of the labels
  int *new_labels = g_malloc0_n ( sizeof(int), n_labels ); // allocate array, initialized to zero
  int *sizes = g_malloc0_n ( sizeof(int), n_labels ); // allocate array, initialized to zero
  
  for ( gint xx = tlx; xx <= brx; xx++ ) {
    for ( gint yy = tly; yy <= bry; yy++ ) {
      if ( is_tile_occupied(val->tiles, xx, yy) ) {
        gint ll = uf_find(tile_label (val->tiles, xx, yy));
        if (new_labels[ll] == 0) {
          new_labels[0]++;
          new_labels[ll] = new_labels[0];
        }
        sizes[new_labels[ll]]++;
        add_tile_label ( val->tiles, xx, yy, new_labels[ll] );
      }
    }
  }
  gint total_clusters = new_labels[0];

  guint largist = 0;
  for ( guint ss = 1; ss < n_labels; ss++ ) {
    if ( sizes[ss] > largist ) {
      largist = sizes[ss];
      val->cont_label = ss;
      val->num_tiles[CONTIG] = largist;
    }
  }
  g_free ( new_labels );
  uf_finish();
  
  clock_t end = clock();
  double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
  g_debug ( "%s: %f %d %d %d", __FUNCTION__, time_spent, total_clusters, largist, val->cont_label );
}

// NB ATM This only tracks one such area
//  (there might be multiple such areas)
static void tac_cluster_calc ( VikAggregateLayer *val )
{
  clock_t begin = clock();

  GHashTableIter iter;
  gpointer key, value;
  gint x,y;

  g_hash_table_iter_init ( &iter, val->tiles );
  while ( g_hash_table_iter_next(&iter, &key, &value) ) {
    (void)sscanf ( key, "%d:%d", &x, &y );
    if ( is_cluster(val->tiles, x, y) ) {
      // Make new hashtable from just the tiles that are in a cluster
      add_tile ( val->tiles_clust, x, y );
      val->num_tiles[CLUSTER]++;
    }
  }

  gint tlx=G_MAXINT,tly=G_MAXINT,brx=-G_MAXINT,bry=-G_MAXINT;

  // Get grid extents
  g_hash_table_iter_init ( &iter, val->tiles_clust );
  while ( g_hash_table_iter_next(&iter, &key, &value) ) {
    (void)sscanf ( key, "%d:%d", &x, &y );
    if ( x < tlx ) tlx = x;
    if ( x > brx ) brx = x;
    if ( y < tly ) tly = y;
    if ( y > bry ) bry = y;
  }
  
  guint size = (abs(tlx-brx) * abs(tly-bry)) / 2;
  if ( size == 0 )
    return;

  uf_init ( size );

  for ( gint xx = tlx; xx <= brx; xx++ ) {
    for ( gint yy = tly; yy <= bry; yy++ ) {
      if ( is_tile_occupied(val->tiles_clust, xx, yy) ) {
        guint label_up = tile_label (val->tiles_clust, xx-1, yy ); // NB don't have to worry about -1 going out of bounds
        guint label_left = tile_label (val->tiles_clust, xx, yy-1 );
        switch ( !!label_up + !!label_left ) {
        case 0: // New
          add_tile_label ( val->tiles_clust, xx, yy, uf_make_set() );
          break;
        case 1: // Existing
          add_tile_label ( val->tiles_clust, xx, yy, MAX(label_up, label_left) );
          break;
        case 2: // Bind existing
          add_tile_label ( val->tiles_clust, xx, yy, uf_union(label_up, label_left) );
          break;
        default: // Should not happen
          g_critical ("%s: labelling algorithm broken", __FUNCTION__);
          break;
        }
      }
    }
  }

  // Reprocess the grid to compare the size of the labels
  int *new_labels = g_malloc0_n ( sizeof(int), n_labels ); // allocate array, initialized to zero
  int *sizes = g_malloc0_n ( sizeof(int), n_labels ); // allocate array, initialized to zero
  
  for ( gint xx = tlx; xx <= brx; xx++ ) {
    for ( gint yy = tly; yy <= bry; yy++ ) {
      if ( is_tile_occupied(val->tiles_clust, xx, yy) ) {
        gint ll = uf_find(tile_label (val->tiles_clust, xx, yy));
        if (new_labels[ll] == 0) {
          new_labels[0]++;
          new_labels[ll] = new_labels[0];
        }
        sizes[new_labels[ll]]++;
        add_tile_label ( val->tiles_clust, xx, yy, new_labels[ll] );
      }
    }
  }
  gint total_clusters = new_labels[0];

  guint largist = 0;
  for ( guint ss = 1; ss < n_labels; ss++ ) {
    if ( sizes[ss] > largist ) {
      largist = sizes[ss];
      val->clust_label = ss;
      val->num_tiles[CLUSTER] = largist;
    }
  }
  g_free ( new_labels );
  uf_finish();

  clock_t end = clock();
  double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
  g_debug ( "%s: %f %d %d %d", __FUNCTION__, time_spent, total_clusters, largist, val->clust_label );
}


// NB ATM This only tracks one square
//  (there might be multiple such squares)
static void tac_square_calc ( VikAggregateLayer *val )
{
  val->max_square = 1;
  clock_t begin = clock();

  GHashTableIter iter;
  gpointer key, value;
  gint x,y;

  g_hash_table_iter_init ( &iter, val->tiles );
  while ( g_hash_table_iter_next(&iter, &key, &value) ) {
    (void)sscanf ( key, "%d:%d", &x, &y );
    if ( is_square(val, x, y, val->max_square) ) {
      g_debug ( "%s: is_square %d at %d:%d", __FUNCTION__, val->max_square, x, y );
      val->xx = x;
      val->yy = y;
      val->max_square++;
      gboolean do_again = FALSE;
      do {
        do_again = FALSE;
        if ( is_square_next(val, x, y, val->max_square) ) {
          g_debug ( "%s: is_square_next %d at %d:%d", __FUNCTION__, val->max_square, x, y );
          do_again = TRUE;
          val->max_square++;
        }
      } while ( do_again );
    }
  }
  val->max_square--;
  
  clock_t end = clock();
  double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
  g_debug ( "%s: %f\n", __FUNCTION__, time_spent );
}

/**
 *
 */
static gint tac_calculate_thread ( CalculateThreadT *ct, gpointer threaddata )
{
  clock_t begin = clock();
  guint tracks_processed = 0;
  // This is used to prevent the progress going negative or otherwise over 100%
  // It's difficult to get an estimate for the total and track progress of each of these parts
  //  and then combine it in a coherent single thread progress meter.
  // So for simplicity they are considered the same as processing extra set of tracks
  guint extras = (ct->val->on[MAX_SQR] * ct->num_of_tracks) +
    (ct->val->on[CONTIG] * ct->num_of_tracks) +
    (ct->val->on[CLUSTER] * ct->num_of_tracks);
  
  ct->tracks_and_layers = g_list_first ( ct->tracks_and_layers );
  while ( ct->tracks_and_layers ) {
    gdouble percent = (gdouble)tracks_processed/(gdouble)(ct->num_of_tracks+extras);
    gint res = a_background_thread_progress ( threaddata, percent );
    if ( res != 0 ) return -1;

    check_track ( ct->val, ct->tracks_and_layers->data );
    tracks_processed++;
    ct->tracks_and_layers = g_list_next ( ct->tracks_and_layers );
  }

  // Timing for basic tile coverage
  clock_t end = clock();
  double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
  g_debug ( "%s: %f", __FUNCTION__, time_spent );

  if ( ct->val->on[MAX_SQR] ) {
    gdouble percent = (gdouble)tracks_processed/(gdouble)(ct->num_of_tracks+extras);
    gint res = a_background_thread_progress ( threaddata, percent );
    if ( res != 0 ) return -1;

    tac_square_calc ( ct->val );
    tracks_processed = tracks_processed + ct->num_of_tracks;
  }

  if ( ct->val->on[CONTIG] ) {
    gdouble percent = (gdouble)tracks_processed/(gdouble)(ct->num_of_tracks+extras);
    gint res = a_background_thread_progress ( threaddata, percent );
    if ( res != 0 ) return -1;

    tac_contiguous_calc ( ct->val );
    tracks_processed = tracks_processed + ct->num_of_tracks;
  }

  if ( ct->val->on[CLUSTER] ) {
    gdouble percent = (gdouble)tracks_processed/(gdouble)(ct->num_of_tracks+extras);
    gint res = a_background_thread_progress ( threaddata, percent );
    if ( res != 0 ) return -1;

    tac_cluster_calc ( ct->val );
    tracks_processed = tracks_processed + ct->num_of_tracks;
  }
  
  ct->val->calculating = FALSE;
  vik_layer_emit_update ( VIK_LAYER(ct->val) ); // NB update display from background

  return 0;
}

/**
 *
 */
static void tac_clear ( VikAggregateLayer *val )
{
  val->max_square = 0;
  for (gint x = 0; x<CP_NUM; x++ ) {
    val->num_tiles[x] = 0;
  }
  val->cont_label = 0;
  val->clust_label = 0;
  g_hash_table_remove_all ( val->tiles ); // No memory to remove ATM
  g_hash_table_remove_all ( val->tiles_clust ); // No memory to remove ATM
}

/**
 *
 */
static void tac_calculate ( VikAggregateLayer *val )
{
  tac_clear ( val );
  val->calculating = TRUE;

  GList *layers = NULL;
  layers = vik_aggregate_layer_get_all_layers_of_type ( val, layers, VIK_LAYER_TRW, TRUE );

  // For each TRW layers keep adding the tracks to build a list of all of them
  GList *tracks_and_layers = NULL; // A list of #vik_trw_track_list_t
  layers = g_list_first ( layers );
  while ( layers ) {
    GList *tracks = NULL;
    tracks = g_list_concat ( tracks, g_hash_table_get_values ( vik_trw_layer_get_tracks( VIK_TRW_LAYER(layers->data) ) ) );
    tracks_and_layers = g_list_concat ( tracks_and_layers, vik_trw_layer_build_track_list_t ( VIK_TRW_LAYER(layers->data), tracks ) );
    layers = g_list_next ( layers );
  }
  g_list_free ( layers );

  CalculateThreadT *ct = g_malloc ( sizeof(CalculateThreadT) );
  ct->tracks_and_layers = tracks_and_layers;
  ct->val = val;
  ct->num_of_tracks = g_list_length (tracks_and_layers);
  guint extras = ct->val->on[MAX_SQR] + ct->val->on[CONTIG] + ct->val->on[CLUSTER];

  a_background_thread ( BACKGROUND_POOL_LOCAL,
                        VIK_GTK_WINDOW_FROM_LAYER(val),
                        "Track Area Coverage",
                        (vik_thr_func)tac_calculate_thread,
                        ct,
                        (vik_thr_free_func)ct_free,
                        (vik_thr_free_func)ct_cancel,
                        ct->num_of_tracks + extras );
}

/**
 * Ensure TAC values calculated if needed
 */
static void aggregate_layer_post_read ( VikAggregateLayer *val, VikViewport *vvp, gboolean from_file )
{
  if ( val->on[BASIC] )
    if ( !val->calculating )
      tac_calculate ( val );
}

/**
 * Do stuff when a file is loaded into this aggregate
 */
void vik_aggregate_layer_file_load_complete ( VikAggregateLayer *val )
{
  aggregate_layer_post_read ( val, NULL, TRUE );
}

static void tac_clear_cb ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER(values[MA_VAL]);
  tac_clear ( val );
  vik_layer_emit_update ( VIK_LAYER(val) ); // NB update display from background
}

/**
 * View area of all TRW layers within an aggregrate layer
 */
static void aggregate_view_all_trw ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER(values[MA_VAL]);
  VikLayersPanel *vlp = VIK_LAYERS_PANEL ( values[MA_VLP] );
  VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(val));
  VikViewport *vvp = vik_window_viewport ( vw );

  // Get all TRW layers
  GList *layers = NULL;
  layers = vik_aggregate_layer_get_all_layers_of_type ( val, layers, VIK_LAYER_TRW, TRUE );

  struct LatLon maxmin[2] = { {0,0}, {0,0} };

  gboolean have_bbox = FALSE;
  GList *iter = g_list_first ( layers );
  while ( iter ) {
    VikTrwLayer *vtl = VIK_TRW_LAYER(iter->data);
    if ( !vik_trw_layer_is_empty(vtl) ) {
      LatLonBBox bbox = vik_trw_layer_get_bbox ( vtl );
      if ( !have_bbox ) {
	maxmin[0].lat = bbox.north;
	maxmin[1].lat = bbox.south;
	maxmin[0].lon = bbox.east;
	maxmin[1].lon = bbox.west;
	have_bbox = TRUE;
      } else {
        if ( bbox.north > maxmin[0].lat )
          maxmin[0].lat = bbox.north;
        if ( bbox.south < maxmin[1].lat )
          maxmin[1].lat = bbox.south;
        if ( bbox.east > maxmin[0].lon )
          maxmin[0].lon = bbox.east;
        if ( bbox.west < maxmin[1].lon )
          maxmin[1].lon = bbox.west;
      }
    }
    iter = g_list_next ( iter );
  }

  if ( have_bbox ) {
    vu_zoom_to_show_latlons ( vik_viewport_get_coord_mode(vvp), vvp, maxmin );
    vik_layers_panel_emit_update ( vlp );
  }
}


// This shouldn't be called when already running
static void tac_calculate_cb ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER(values[MA_VAL]);
  if ( val->calculating ) {
    return;
  }
  tac_calculate ( val );
}

static void aggregate_layer_add_menu_items ( VikAggregateLayer *val, GtkMenu *menu, gpointer vlp )
{
  // Data to pass on in menu functions
  static menu_array_values values;
  values[MA_VAL] = val;
  values[MA_VLP] = vlp;

  (void)vu_menu_add_item ( menu, NULL, NULL, NULL, NULL ); // Just a separator

  GtkMenu *vis_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *itemv = vu_menu_add_item ( menu, _("_Visibility"), NULL, NULL, NULL );
  gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemv), GTK_WIDGET(vis_submenu) );

  (void)vu_menu_add_item ( vis_submenu, _("_Show All"), GTK_STOCK_APPLY, G_CALLBACK(aggregate_layer_child_visible_on), values );
  (void)vu_menu_add_item ( vis_submenu, _("_Hide All"), GTK_STOCK_CLEAR, G_CALLBACK(aggregate_layer_child_visible_off), values );
  (void)vu_menu_add_item ( vis_submenu, _("_Toggle"), GTK_STOCK_REFRESH, G_CALLBACK(aggregate_layer_child_visible_toggle), values );

  GtkMenu *view_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *itemvw = vu_menu_add_item ( menu, _("V_iew"), NULL, NULL, NULL );
  gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemvw), GTK_WIDGET(view_submenu) );

  GtkWidget *itematl = vu_menu_add_item ( view_submenu, _("_All TrackWaypoint Layers"), GTK_STOCK_ZOOM_FIT, G_CALLBACK(aggregate_view_all_trw), values );
  gtk_widget_set_sensitive ( itematl, val->children ? TRUE : FALSE );

  GtkMenu *submenu_sort = GTK_MENU(gtk_menu_new());
  GtkWidget *items = vu_menu_add_item ( menu, _("_Sort"), GTK_STOCK_REFRESH, NULL, NULL );
  gtk_menu_item_set_submenu ( GTK_MENU_ITEM(items), GTK_WIDGET(submenu_sort) );

  (void)vu_menu_add_item ( submenu_sort, _("Name _Ascending"), GTK_STOCK_SORT_ASCENDING, G_CALLBACK(aggregate_layer_sort_a2z), values );
  (void)vu_menu_add_item ( submenu_sort, _("Name _Descending"), GTK_STOCK_SORT_DESCENDING, G_CALLBACK(aggregate_layer_sort_z2a), values );
  (void)vu_menu_add_item ( submenu_sort, _("Date Ascending"), GTK_STOCK_SORT_ASCENDING, G_CALLBACK(aggregate_layer_sort_timestamp_ascend), values );
  (void)vu_menu_add_item ( submenu_sort, _("Date Descending"), GTK_STOCK_SORT_DESCENDING, G_CALLBACK(aggregate_layer_sort_timestamp_descend), values );

  (void)vu_menu_add_item ( menu, _("_Statistics"), NULL, G_CALLBACK(aggregate_layer_analyse), values );
  (void)vu_menu_add_item ( menu, _("Track _List..."), GTK_STOCK_INDEX, G_CALLBACK(aggregate_layer_track_list_dialog), values );
  (void)vu_menu_add_item ( menu, _("_Waypoint List..."), GTK_STOCK_INDEX, G_CALLBACK(aggregate_layer_waypoint_list_dialog), values );

  GtkMenu *search_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *itemsr = vu_menu_add_item ( menu, _("Searc_h"), GTK_STOCK_JUMP_TO, NULL, NULL );
  gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemsr), GTK_WIDGET(search_submenu) );

  GtkWidget *itemd = vu_menu_add_item ( search_submenu, _("By _Date..."), NULL, G_CALLBACK(aggregate_layer_search_date), values );
  gtk_widget_set_tooltip_text ( itemd, _("Find the first item with a specified date") );

  (void)vu_menu_add_item ( menu, _("Load E_xternal Layers"), NULL, G_CALLBACK(aggregate_layer_load_external_layers_click), values );

  (void)vu_menu_add_item ( menu, _("_Append File..."), GTK_STOCK_ADD, G_CALLBACK(aggregate_layer_file_load), values );

  GtkMenu *tac_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *itemt = vu_menu_add_item ( menu, _("_Tracks Area Coverage"), GTK_STOCK_EXECUTE, NULL, NULL );
  gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemt), GTK_WIDGET(tac_submenu) );

  gboolean available = val->on[BASIC] && !val->calculating;
  GtkWidget *itemtac = vu_menu_add_item ( tac_submenu, _("_Calculate"), GTK_STOCK_REFRESH, G_CALLBACK(tac_calculate_cb), values );
  gtk_widget_set_sensitive ( itemtac, available );

  GtkWidget *itemtclr = vu_menu_add_item ( tac_submenu, _("_Remove"), GTK_STOCK_DELETE, G_CALLBACK(tac_clear_cb), values );
  gtk_widget_set_sensitive ( itemtclr, available );
}

static void disconnect_layer_signal ( VikLayer *vl, VikAggregateLayer *val )
{
  guint number_handlers = g_signal_handlers_disconnect_matched(vl, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, val);
  if ( number_handlers != 1 )
    g_critical ("%s: Unexpected number of disconnect handlers: %d", __FUNCTION__, number_handlers);
}

void vik_aggregate_layer_free ( VikAggregateLayer *val )
{
  g_list_foreach ( val->children, (GFunc)(disconnect_layer_signal), val );
  g_list_foreach ( val->children, (GFunc)(g_object_unref), NULL );
  g_list_free ( val->children );
  if ( val->tracks_analysis_dialog != NULL )
    gtk_widget_destroy ( val->tracks_analysis_dialog );

  g_hash_table_destroy ( val->tiles );
  if ( val->pixbuf[BASIC] )
    g_object_unref ( val->pixbuf[BASIC] );
  if ( val->pixbuf[MAX_SQR] )
    g_object_unref ( val->pixbuf[MAX_SQR] );
  if ( val->pixbuf[CONTIG] )
    g_object_unref ( val->pixbuf[CONTIG] );
  g_hash_table_destroy ( val->tiles_clust );
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

static void aggregate_layer_delete_common ( VikAggregateLayer *val, VikLayer *vl )
{
  val->children = g_list_remove ( val->children, vl );
  disconnect_layer_signal ( vl, val );
  g_object_unref ( vl );
}

gboolean vik_aggregate_layer_delete ( VikAggregateLayer *val, GtkTreeIter *iter )
{
  // Very basic attempt to try preventing the layer being deleted
  if ( val->calculating ) {
    gchar* msg = g_strdup ( _("Can not delete layer as calculation is in progress") );
    vik_window_statusbar_update ( (VikWindow*)VIK_GTK_WINDOW_FROM_LAYER(val), msg, VIK_STATUSBAR_INFO );
    g_free ( msg );
    return TRUE;
  }

  VikLayer *l = VIK_LAYER( vik_treeview_item_get_pointer ( VIK_LAYER(val)->vt, iter ) );
  gboolean was_visible = l->visible;

  vik_treeview_item_delete ( VIK_LAYER(val)->vt, iter );
  aggregate_layer_delete_common ( val, l );

  return was_visible;
}

/**
 * Delete a child layer from the aggregate layer
 */
gboolean vik_aggregate_layer_delete_layer ( VikAggregateLayer *val, VikLayer *vl )
{
  gboolean was_visible = vl->visible;

  if ( vl->realized && &vl->iter )
    vik_treeview_item_delete ( VIK_LAYER(val)->vt, &vl->iter );
  aggregate_layer_delete_common ( val, vl );

  return was_visible;
}

#if 0
/* returns 0 == we're good, 1 == didn't find any layers, 2 == got rejected */
guint vik_aggregate_layer_tool ( VikAggregateLayer *val, VikLayerTypeEnum layer_type, VikToolInterfaceFunc tool_func, GdkEventButton *event, VikViewport *vvp )
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

VikLayer *vik_aggregate_layer_get_top_visible_layer_of_type ( VikAggregateLayer *val, VikLayerTypeEnum type )
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

GList *vik_aggregate_layer_get_all_layers_of_type(VikAggregateLayer *val, GList *layers, VikLayerTypeEnum type, gboolean include_invisible)
{
  GList *l = layers;
  if (type == VIK_LAYER_AGGREGATE && (VIK_LAYER(val)->visible || include_invisible))
    l = g_list_prepend(l, val);

  GList *children = val->children;
  VikLayer *vl;
  if (!children)
    return layers;

  // Where appropriate *don't* include non-visible layers
  while (children) {
    vl = VIK_LAYER(children->data);
    if (vl->type == VIK_LAYER_AGGREGATE ) {
      // Don't even consider invisible aggregates, unless told to
      if (vl->visible || include_invisible) {
        l = vik_aggregate_layer_get_all_layers_of_type(VIK_AGGREGATE_LAYER(children->data), l, type, include_invisible);
      }
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
#ifdef HAVE_LIBGEOCLUE_2
      else if (VIK_LAYER(children->data)->type == VIK_LAYER_GEOCLUE) {
	if (VIK_LAYER(children->data)->visible || include_invisible) {
	  if (!vik_geoclue_layer_is_empty(VIK_GEOCLUE_LAYER(children->data))) {
	    l = g_list_prepend(l, vik_geoclue_layer_get_trw(VIK_GEOCLUE_LAYER(children->data)));
	  }
	}
      }
#endif
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
    vik_treeview_add_layer ( vl->vt, layer_iter, &iter, vli->name, val, TRUE,
                             vli, vli->type, vli->type, vik_layer_get_timestamp(vli) );
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

/**
 * Generate tooltip text for the layer.
 */
static const gchar* aggregate_layer_tooltip ( VikAggregateLayer *val )
{
  static gchar tmp_buf[256];
  tmp_buf[0] = '\0';
  GString *gs = g_string_new ( NULL );
  GList *children = val->children;
  if ( children ) {
    gint nn = g_list_length (children);
    // Could have a more complicated tooltip that numbers each type of layers,
    //  but for now a simple overall count
    g_string_append_printf ( gs, ngettext("One layer", "%d layers", nn), nn );
  }
  else
    g_string_append ( gs, _("Empty") );

  if ( val->on[BASIC] ) {
    if ( val->calculating ) {
      g_string_append ( gs, _("\nTAC: Calculating") );
    } else {
      g_string_append_printf ( gs,
                   _("\nTAC: Area Level %s\nTotal tiles %d\nMax Square %d\nContiguous count %d\nCluster size %d"),
                   params_tile_area_levels[map_utils_mpp_to_scale(val->zoom_level)-2], val->num_tiles[BASIC], val->max_square, val->num_tiles[CONTIG], val->num_tiles[CLUSTER] );
    }
  }
  g_snprintf (tmp_buf, sizeof(tmp_buf), "%s", gs->str );
  g_string_free ( gs, TRUE );
  return tmp_buf;
}

/**
 * Return number of layers held
 */
guint vik_aggregate_layer_count ( VikAggregateLayer *val )
{
  guint nn = 0;
  GList *children = val->children;
  if ( children ) {
    nn = g_list_length (children);
  }
  return nn;
}
