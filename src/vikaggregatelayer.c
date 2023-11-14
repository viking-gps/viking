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
#include "viktrwlayer_export.h"
#include "maputils.h"
#include "background.h"
#include "gpx.h"
#include "dir.h"
#ifdef HAVE_SQLITE3_H
#include "sqlite3.h"
#endif
#include "misc/heatmap.h"

#define AGGREGATE_FIXED_NAME "Aggregate"

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
static gboolean aggregate_layer_selected_viewport_menu ( VikAggregateLayer *val, GdkEventButton *event, VikViewport *vvp );

static void tac_calculate ( VikAggregateLayer *val );
static void hm_calculate ( VikAggregateLayer *val );

static gchar *params_tile_area_levels[] = { "17", "16", "15", "14", "13", "12", "11", "10", "9", "8", "7", "6", "5", "4", NULL };
static gchar *params_tac_time_ranges[] = { N_("All Time"), "1", "2", "3", "5", "7", "10", "15", "20", "25", NULL };

static VikLayerParamData tac_time_to_internal ( VikLayerParamData value )
{
  // From array index into the year value
  if ( value.u && value.u < G_N_ELEMENTS(params_tac_time_ranges) )
    return VIK_LPD_UINT(atoi(params_tac_time_ranges[value.u]));
  return VIK_LPD_UINT(0);
}

static VikLayerParamData tac_time_to_display ( VikLayerParamData value )
{
  VikLayerParamData ans = VIK_LPD_UINT(0);
  // From the year value into array index
  switch ( value.u ) {
  case 1:  ans = VIK_LPD_UINT(1); break;
  case 2:  ans = VIK_LPD_UINT(2); break;
  case 3:  ans = VIK_LPD_UINT(3); break;
  case 5:  ans = VIK_LPD_UINT(4); break;
  case 7:  ans = VIK_LPD_UINT(5); break;
  case 10: ans = VIK_LPD_UINT(6); break;
  case 15: ans = VIK_LPD_UINT(7); break;
  case 20: ans = VIK_LPD_UINT(8); break;
  case 25: ans = VIK_LPD_UINT(9); break;
  default: break;
  }
  return ans;
}

static VikLayerParamScale params_scales[] = {
 // min, max, step, digits (decimal places)
 { 0, 255, 3, 0 }, // alpha
 { 0, 100, 2, 0 }, // stamp width factor
};

static VikLayerParamData width_default ( void ) { return VIK_LPD_UINT ( 10 ); }
static VikLayerParamData combo_1st_default ( void ) { return VIK_LPD_UINT ( 0 ); }

static VikLayerParamData color_default ( void ) {
  VikLayerParamData data; gdk_color_parse ( "orange", &data.c ); return data;
}
static VikLayerParamData alpha_default ( void ) { return VIK_LPD_UINT ( 100 ); }
static VikLayerParamData tile_area_level_default ( void ) { return VIK_LPD_UINT ( 3 ); }

static VikLayerParamData color_default_max_sqr ( void ) {
  VikLayerParamData data; gdk_color_parse ( "purple", &data.c ); return data;
}
static VikLayerParamData color_default_contig ( void ) {
  VikLayerParamData data; gdk_color_parse ( "lightgreen", &data.c ); return data;
}
static VikLayerParamData color_default_cluster ( void ) {
  VikLayerParamData data; gdk_color_parse ( "darkgreen", &data.c ); return data;
}
static VikLayerParamData color_default_new ( void ) {
  VikLayerParamData data; gdk_color_parse ( "#4365B8", &data.c ); return data; // A kind of blue
}
static VikLayerParamData color_default_lines ( void ) {
  VikLayerParamData data; gdk_color_parse ( "#241f31", &data.c ); return data; // A dark greyish
}

static VikLayerParamData hm_alpha_default ( void ) { return VIK_LPD_UINT ( 127 ); }

static gchar * params_styles[] =
  { N_("Spectral"),
    N_("Blue/Purple"),
    N_("Red/Green"),
    N_("Yellow/Orange/Red"),
    NULL
  };

static gchar *params_groups[] = { N_("Tracks Area Coverage"), N_("TAC Advanced"), N_("Tracks Heatmap") };
enum { GROUP_TAC, GROUP_TAC_ADV, GROUP_THM };

static void aggregate_reset_cb ( GtkWidget *widget, gpointer ptr )
{
  a_layer_defaults_reset_show ( AGGREGATE_FIXED_NAME, ptr, GROUP_TAC );
  a_layer_defaults_reset_show ( AGGREGATE_FIXED_NAME, ptr, GROUP_TAC_ADV );
  a_layer_defaults_reset_show ( AGGREGATE_FIXED_NAME, ptr, GROUP_THM );
}

static VikLayerParamData reset_default ( void ) { return VIK_LPD_PTR(aggregate_reset_cb); }

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
  { VIK_LAYER_AGGREGATE, "new_on", VIK_LAYER_PARAM_BOOLEAN, GROUP_TAC_ADV, N_("New On:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "new_alpha", VIK_LAYER_PARAM_UINT, GROUP_TAC_ADV, N_("New Alpha:"), VIK_LAYER_WIDGET_HSCALE, params_scales, NULL,
    N_("Control the Alpha value for transparency effects"), alpha_default, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "new_color", VIK_LAYER_PARAM_COLOR, GROUP_TAC_ADV, N_("New Color:"), VIK_LAYER_WIDGET_COLOR, NULL, NULL, NULL, color_default_new, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "lines_on", VIK_LAYER_PARAM_BOOLEAN, GROUP_TAC_ADV, N_("Consecutive Lines On:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    "Consecutive line of tiles in North/South and East/West directions", vik_lpd_false_default, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "lines_alpha", VIK_LAYER_PARAM_UINT, GROUP_TAC_ADV, N_("Consecutive Lines Alpha:"), VIK_LAYER_WIDGET_HSCALE, params_scales, NULL,
    N_("Control the Alpha value for transparency effects"), alpha_default, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "lines_color", VIK_LAYER_PARAM_COLOR, GROUP_TAC_ADV, N_("Consecutive Lines Color:"), VIK_LAYER_WIDGET_COLOR, NULL, NULL, NULL, color_default_lines, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "drawgrid", VIK_LAYER_PARAM_BOOLEAN, GROUP_TAC, N_("Draw Grid:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "tilearealevel", VIK_LAYER_PARAM_UINT, GROUP_TAC, N_("Tile Area Level:"), VIK_LAYER_WIDGET_COMBOBOX, params_tile_area_levels, NULL,
    N_("Area size. A higher level means a smaller grid."), tile_area_level_default, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "tileareatimerange", VIK_LAYER_PARAM_UINT, GROUP_TAC, N_("Within Years:"), VIK_LAYER_WIDGET_COMBOBOX, params_tac_time_ranges, NULL,
    N_("Only include tracks that are within this time range."), combo_1st_default, tac_time_to_display, tac_time_to_internal },
  { VIK_LAYER_AGGREGATE, "hm_alpha", VIK_LAYER_PARAM_UINT, GROUP_THM, N_("Alpha:"), VIK_LAYER_WIDGET_HSCALE, params_scales, NULL,
    N_("Control the Alpha value for transparency effects"), hm_alpha_default, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "hm_factor", VIK_LAYER_PARAM_UINT, GROUP_THM, N_("Width Factor:"), VIK_LAYER_WIDGET_HSCALE, &params_scales[1], NULL,
    N_("Note higher values means the heatmap takes longer to generate"), width_default, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "hm_style", VIK_LAYER_PARAM_UINT, GROUP_THM, N_("Color Style:"), VIK_LAYER_WIDGET_COMBOBOX, params_styles, NULL, NULL, combo_1st_default, NULL, NULL },
  { VIK_LAYER_AGGREGATE, "reset", VIK_LAYER_PARAM_PTR_DEFAULT, VIK_LAYER_GROUP_NONE, NULL,
    VIK_LAYER_WIDGET_BUTTON, N_("Reset All to Defaults"), NULL, NULL, reset_default, NULL, NULL },
};

typedef enum { BASIC, CONTIG, CLUSTER, MAX_SQR, TNEW, LINES, CP_NUM } common_property_types;

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
      PARAM_NEW_ON,
      PARAM_NEW_ALPHA,
      PARAM_NEW_COLOR,
      PARAM_LINES_ON,
      PARAM_LINES_ALPHA,
      PARAM_LINES_COLOR,
      PARAM_DRAW_GRID,
      PARAM_TILE_AREA_LEVEL,
      PARAM_TAC_TIME_RANGE,
      PARAM_HM_ALPHA,
      PARAM_HM_STAMP_FACTOR,
      PARAM_HM_STYLE,
      PARAM_RESET,
      NUM_PARAMS
};

VikLayerInterface vik_aggregate_layer_interface = {
  AGGREGATE_FIXED_NAME,
  N_("Aggregate"),
  "<control><shift>A",
  "vikaggregatelayer",

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
  (VikLayerFuncConfigure)               vik_aggregate_layer_configure,
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
  (VikLayerFuncLayerToggleVisible)      NULL,

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
  (VikLayerFuncSelectedViewportMenu)    aggregate_layer_selected_viewport_menu,

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
  guint zoom_level_prev;

  gboolean on[CP_NUM];
  guint8 alpha[CP_NUM];
  GdkColor color[CP_NUM];
  GdkPixbuf *pixbuf[CP_NUM];      // Individual tile
  GdkPixbuf *full_pixbuf[CP_NUM]; // Whole screen
  GdkPixbuf *unreachable_pixbuf; // Whole screen
  guint num_tiles[CP_NUM]; // NB ATM Not used for lines coverage type
  guint num_prev[CP_NUM]; // Counts to determine change after a new calculation
  guint num_calcs;

  guint cont_label;
  guint clust_label;
  guint max_square;
  guint max_square_prev;
  gint xx,yy; // Location of top left max square tile

  // For consecutive lines of coverage North/South (ns) and East/West (ew)
  // Store where the lines are (tile at the end of the line) and the size
  gint ns_x;
  gint ns_y;
  guint ns_size;
  guint ns_size_prev;
  gint ew_x;
  gint ew_y;
  guint ew_size;
  guint ew_size_prev;

  guint8 tac_time_range; // Years
  // Maybe a sparse table would be more efficient
  //  but this seems to work OK at least if all tracks are confined within a not too diverse area
  GHashTable *tiles;
  GHashTable *tiles_clust;

  // Enable to determine changed tiles (mainly for those added rather than removed)
  GHashTable *prev;
  GHashTable *tiles_new;

  // Heatmap
  gboolean hm_calculating;
  // Values as at original request
  guint hm_scale;
  gint hm_zoom;
  gint hm_width;
  gint hm_height;
  LatLonBBox hm_bbox;
  const VikCoord *hm_center;
  VikCoord hm_tl;
  // Drawing values (zoom level may have changed)
  gint hm_scaled_zoom;
  gint hm_zoom_max;
  guint8 hm_alpha;
  GdkPixbuf *hm_pixbuf;
  GdkPixbuf *hm_pbf_scaled;
  gboolean hm_scaled;
  guint8 hm_stamp_factor;
  guint8 hm_style;
  GdkColor hm_color;

  MapCoord rc_menu_mc; // Position of Right Click menu
};

// Single global
GHashTable *tiles_unreachable = NULL;

static GdkColor black_color;

static void aggregate_layer_class_init ( VikAggregateLayerClass *klass )
{
  gchar *fn = g_build_filename ( a_get_viking_dir(), "unreachable_tiles.txt", NULL );
  FILE *ff = g_fopen ( fn, "r" );

  // Load
  if ( ff ) {
    tiles_unreachable = g_hash_table_new_full ( g_str_hash, g_str_equal, g_free, NULL );

    gchar buf[4096];
    guint line = 0;
    gint xx, yy, zz;
    while ( !feof(ff) ) {
      if ( fgets(buf, sizeof(buf), ff) == NULL )
        break;
      line++;
      // Skip comment style lines
      if ( buf[0] == '\0' || buf[0] == '!' || buf[0] == ';' || buf[0] == '#' )
        continue;
      if ( sscanf(buf, "%d %d %d", &zz, &xx, &yy) == 3 ) {
        gchar *key = g_strdup_printf ( "%d %d %d", zz, xx, yy );
        (void)g_hash_table_insert ( tiles_unreachable, key, GUINT_TO_POINTER(0) );
      }
      else
        g_warning ( "%s: %s line %d does not contain 3 numbers", __FUNCTION__, fn, line );
    }
    fclose ( ff );

    guint sz = g_hash_table_size ( tiles_unreachable );
    if ( sz )
      g_debug ( "%s: using %d unreachable tiles", __FUNCTION__, sz );
  }

  g_free ( fn );

  gdk_color_parse ( "#000000", &black_color );
}

void vik_aggregate_layer_uninit ()
{
  if ( tiles_unreachable )
    g_hash_table_destroy ( tiles_unreachable );
}

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
      (GClassInitFunc)aggregate_layer_class_init, /* class init */
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

static const unsigned char dd0[] = {
    0, 0, 0, 0,
    247, 252, 253, 255,
    224, 236, 244, 255,
    191, 211, 230, 255,
    158, 188, 218, 255,
    140, 150, 198, 255,
    140, 107, 177, 255,
    136, 65, 157, 255,
    129, 15, 124, 255,
    77, 0, 75, 255,
};
static const heatmap_colorscheme_t d0 = { dd0, sizeof(dd0)/sizeof(dd0[0]/4) };

static const unsigned char dd1[] = {
    0, 0, 0, 0,
    26, 26, 26, 255,
    77, 77, 77, 255,
    135, 135, 135, 255,
    186, 186, 186, 255,
    224, 224, 224, 255,
    255, 255, 255, 255,
    253, 219, 199, 255,
    244, 165, 130, 255,
    214, 96, 77, 255,
    178, 24, 43, 255,
    103, 0, 31, 255,
};
static const heatmap_colorscheme_t d1 = { dd1, sizeof(dd1)/sizeof(dd1[0]/4) };

static const unsigned char dd2[] = {
    0, 0, 0, 0,
    255, 255, 204, 255,
    255, 237, 160, 255,
    254, 217, 118, 255,
    254, 178, 76, 255,
    253, 141, 60, 255,
    252, 78, 42, 255,
    227, 26, 28, 255,
    189, 0, 38, 255,
    128, 0, 38, 255,
};
static const heatmap_colorscheme_t d2 = { dd2, sizeof(dd2)/sizeof(dd2[0]/4) };

const heatmap_colorscheme_t* hm_colorschemes[] =
  { &d0, // Blue/Purple
    &d1, // Rd/Gr
    &d2, // Yellow/Orange/Red
  };

// Ensure when 'apply' button heatmap regenerated to use new values
static void hm_apply ( VikAggregateLayer *val )
{
  if ( VIK_LAYER(val)->realized )
    if ( !val->hm_calculating && val->hm_pixbuf )
      hm_calculate ( val );
}

static void tac_apply ( VikAggregateLayer *val, VikLayerSetParam *vlsp )
{
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

static gboolean aggregate_layer_set_param ( VikAggregateLayer *val, VikLayerSetParam *vlsp )
{
  gboolean changed = FALSE;
  switch ( vlsp->id ) {
    case PARAM_DO_TAC:
      changed = vik_layer_param_change_boolean ( vlsp->data, &val->on[BASIC] );
      break;
    case PARAM_ALPHA:
      if ( vlsp->data.u <= 255 )
        changed = vik_layer_param_change_uint8 ( vlsp->data, &val->alpha[BASIC] );
      break;
    case PARAM_DRAW_GRID:
      changed = vik_layer_param_change_boolean ( vlsp->data, &val->draw_grid );
      break;
    case PARAM_COLOR:
      changed = vik_layer_param_change_color ( vlsp->data, &val->color[BASIC] );
      break;
    case PARAM_MAX_SQR_ON:
      changed = vik_layer_param_change_boolean ( vlsp->data, &val->on[MAX_SQR] );
      break;
    case PARAM_MAX_SQR_ALPHA:
      if ( vlsp->data.u <= 255 )
        changed = vik_layer_param_change_uint8 ( vlsp->data, &val->alpha[MAX_SQR] );
      break;
    case PARAM_MAX_SQR_COLOR:
      changed = vik_layer_param_change_color ( vlsp->data, &val->color[MAX_SQR] );
      break;
    case PARAM_CONTIG_ON:
      changed = vik_layer_param_change_boolean ( vlsp->data, &val->on[CONTIG] );
      break;
    case PARAM_CONTIG_ALPHA:
      if ( vlsp->data.u <= 255 )
        changed = vik_layer_param_change_uint8 ( vlsp->data, &val->alpha[CONTIG] );
      break;
    case PARAM_CONTIG_COLOR:
      changed = vik_layer_param_change_color ( vlsp->data, &val->color[CONTIG] );
      break;
    case PARAM_CLUSTER_ON:
      changed = vik_layer_param_change_boolean ( vlsp->data, &val->on[CLUSTER] );
      break;
    case PARAM_CLUSTER_ALPHA:
      if ( vlsp->data.u <= 255 )
        changed = vik_layer_param_change_uint8 ( vlsp->data, &val->alpha[CLUSTER] );
      break;
    case PARAM_CLUSTER_COLOR:
      changed = vik_layer_param_change_color ( vlsp->data, &val->color[CLUSTER] );
      break;
    case PARAM_NEW_ON:
      changed = vik_layer_param_change_boolean ( vlsp->data, &val->on[TNEW] );
      break;
    case PARAM_NEW_ALPHA:
      if ( vlsp->data.u <= 255 )
        changed = vik_layer_param_change_uint8 ( vlsp->data, &val->alpha[TNEW] );
      break;
    case PARAM_NEW_COLOR:
      changed = vik_layer_param_change_color ( vlsp->data, &val->color[TNEW] );
      break;
    case PARAM_LINES_ON:
      changed = vik_layer_param_change_boolean ( vlsp->data, &val->on[LINES] );
      break;
    case PARAM_LINES_ALPHA:
      if ( vlsp->data.u <= 255 )
        changed = vik_layer_param_change_uint8 ( vlsp->data, &val->alpha[LINES] );
      break;
    case PARAM_LINES_COLOR:
      changed = vik_layer_param_change_color ( vlsp->data, &val->color[LINES] );
      break;
    case PARAM_TILE_AREA_LEVEL:
      if ( vlsp->data.u <= G_N_ELEMENTS(params_tile_area_levels) ) {
        guint old = val->zoom_level;
        val->zoom_level = pow ( 2, vlsp->data.u );
        // Ensure when 'apply' button is clicked the TAC is recalculated for the new area value
        if ( val->zoom_level != old ) {
          tac_apply ( val, vlsp );
          changed = TRUE;
        }
      }
      break;
    case PARAM_TAC_TIME_RANGE:
      changed = vik_layer_param_change_uint8 ( vlsp->data, &val->tac_time_range );
      if ( changed )
        tac_apply ( val, vlsp );
      break;
    case PARAM_HM_ALPHA:
      if ( vlsp->data.u <= 255 ) {
        changed = vik_layer_param_change_uint8 ( vlsp->data, &val->hm_alpha );
	if ( changed )
	  if ( !vlsp->is_file_operation )
	    hm_apply ( val );
      }
      break;
    case PARAM_HM_STAMP_FACTOR:
      if ( vlsp->data.u <= 255 ) {
        changed = vik_layer_param_change_uint8 ( vlsp->data, &val->hm_stamp_factor );
        if ( changed ) {
          if ( !vlsp->is_file_operation )
            hm_apply ( val );
        }
      }
      break;
    case PARAM_HM_STYLE:
      changed = vik_layer_param_change_uint8 ( vlsp->data, &val->hm_style );
      if ( changed )
        if ( !vlsp->is_file_operation )
          hm_apply ( val );
      break;
    default: break;
  }
  if ( vik_debug && changed )
    g_debug ( "%s: Detected change on param %d", __FUNCTION__, vlsp->id );
  return changed;
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
    case PARAM_NEW_ON: rv.b = val->on[TNEW]; break;
    case PARAM_NEW_ALPHA: rv.u = val->alpha[TNEW]; break;
    case PARAM_NEW_COLOR: rv.c = val->color[TNEW]; break;
    case PARAM_LINES_ON: rv.b = val->on[LINES]; break;
    case PARAM_LINES_ALPHA: rv.u = val->alpha[LINES]; break;
    case PARAM_LINES_COLOR: rv.c = val->color[LINES]; break;
    case PARAM_TILE_AREA_LEVEL: rv.u = map_utils_mpp_to_scale ( val->zoom_level ); break;
    case PARAM_TAC_TIME_RANGE: rv.u = val->tac_time_range; break;
    case PARAM_HM_ALPHA: rv.u = val->hm_alpha; break;
    case PARAM_HM_STAMP_FACTOR: rv.u = val->hm_stamp_factor; break;
    case PARAM_HM_STYLE: rv.u = val->hm_style; break;
    case PARAM_RESET: rv.ptr = aggregate_reset_cb; break;
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
    for ( guint xx = PARAM_ALPHA; xx <= PARAM_TAC_TIME_RANGE; xx++ ) {
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
  pixbuf = ui_pixbuf_new ( &color, size_x, size_y );
  pixbuf = ui_pixbuf_set_alpha ( pixbuf, alpha );
  return pixbuf;
}  

VikAggregateLayer *vik_aggregate_layer_create (VikViewport *vp)
{
  VikAggregateLayer *rv = vik_aggregate_layer_new (vp);
  vik_layer_rename ( VIK_LAYER(rv), vik_aggregate_layer_interface.name );
  vik_layer_set_defaults ( VIK_LAYER(rv), vp );

  for (gint x = 0; x<CP_NUM; x++ )
    rv->num_prev[x] = 0;
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
  val->tiles_new = g_hash_table_new_full ( g_str_hash, g_str_equal, g_free, NULL );
  val->prev = g_hash_table_new_full ( g_str_hash, g_str_equal, g_free, NULL );

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
  gboolean ans = g_hash_table_contains ( ght, key );
  g_free ( key );
  return ans;
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

static GdkPixbuf *setup_pixbuf ( GdkPixbuf *pixbuf, guint width, guint height )
{
  if ( pixbuf )
    g_object_unref ( pixbuf );
  pixbuf = gdk_pixbuf_new ( GDK_COLORSPACE_RGB, TRUE, 8, width, height );
  gdk_pixbuf_fill ( pixbuf, 0x00000000 );
  return pixbuf;
}

static void get_pixel_limits ( guint *sizex, guint *sizey, guint *destx, guint *desty, gint xx, gint yy, guint width, guint height, gint tilesize_ceil )
{
  *sizex = ( xx + tilesize_ceil > width ) ? width - xx : tilesize_ceil;
  *sizey = ( yy + tilesize_ceil > height ) ? height - yy : tilesize_ceil;

  *destx = 0;
  if ( xx < 0 )
    *sizex += xx;
  else
    *destx = xx;

  *desty = 0;
  if ( yy < 0 )
    *sizey += yy;
  else
    *desty = yy;
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
    // Prevent the program grinding to a halt if trying to deal with thousands of tiles
    const gint tiles = (xmax-xmin) * (ymax-ymin);
    if ( tiles > 524288 ) {
      // Maybe put in status bar
      g_warning ( "%s: Giving up trying to draw many tiles (%d)", __FUNCTION__, tiles );
      return;
    }

    const gdouble tilesize = 256 * shrinkfactor * vik_viewport_get_scale ( vvp );
    gint xx_tmp, yy_tmp;
    map_utils_iTMS_to_center_vikcoord ( &ulm, &coord );
    vik_viewport_coord_to_screen ( vvp, &coord, &xx_tmp, &yy_tmp );

    // ceiled so tiles will be maximum size in the case of funky shrinkfactor
    const gint tilesize_ceil = ceil ( tilesize );
    const gint8 xinc = (ulm.x == xmin) ? 1 : -1;
    const gint8 yinc = (ulm.y == ymin) ? 1 : -1;
    const gint xend = (xinc == 1) ? (xmax+1) : (xmin-1);
    const gint yend = (yinc == 1) ? (ymax+1) : (ymin-1);

    /* above trick so xx,yy doubles. this is so shrinkfactors aren't rounded off
     * eg if tile size 128, shrinkfactor 0.333 */
    const gint base_xx = xx_tmp - (tilesize/2);
    const gint base_yy = yy_tmp - (tilesize/2);
    xx = base_xx; yy = base_yy;

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
      for ( guint ii=0; ii<CP_NUM; ii++ )
        val->pixbuf[ii] = layer_pixbuf_update ( val->pixbuf[ii], val->color[ii], tilesize <= 256 ? 256 : tilesize, tilesize <= 256 ? 256 : tilesize, val->alpha[ii] );
    }

    // Create pixbufs the size of the screen
    for ( guint ii=0; ii<CP_NUM; ii++ ) {
      if ( val->on[ii] ) {
        val->full_pixbuf[ii] = setup_pixbuf ( val->full_pixbuf[ii], width, height );
      }
    }
    if ( tiles_unreachable )
      val->unreachable_pixbuf = setup_pixbuf ( val->unreachable_pixbuf, width, height );

    guint tile_draw_count = 0;
    for ( x = ((xinc == 1) ? xmin : xmax); x != xend; x+=xinc ) {
      yy = base_yy;
      for ( y = ((yinc == 1) ? ymin : ymax); y != yend; y+=yinc ) {
        ulm.x = x;
        ulm.y = y;

        if ( is_tile_occupied(val->tiles, x, y) ) {
          //g_printf ( "%s1: %d, %d, %d, %d, %d, %d %0.2f\n", __FUNCTION__, xx, yy, tilesize_ceil, tilesize_ceil, width, height, shrinkfactor );
          if ( !is_big ) {

            // Limit size request of pixbufs at the edge, as to not request a size overflowing the destination pixbuf
            // otherwise gdk_pixbuf_copy_area() will complain
            guint sizex, sizey, destx, desty;
            get_pixel_limits ( &sizex, &sizey, &destx, &desty, xx, yy, width, height, tilesize_ceil );

            // Note ATM each type is simply drawn on top of each other,
            //  hence colours and alpha values will get blended into the final output

            gdk_pixbuf_copy_area ( val->pixbuf[BASIC], 0, 0, sizex, sizey, val->full_pixbuf[BASIC], destx, desty );

            if ( val->cont_label && (tile_label(val->tiles, x, y) == val->cont_label) )
              gdk_pixbuf_copy_area ( val->pixbuf[CONTIG], 0, 0, sizex, sizey, val->full_pixbuf[CONTIG], destx, desty );

            // Cluster drawing
            if ( val->on[CLUSTER] )
              if ( val->clust_label && (tile_label(val->tiles_clust, x, y) == val->clust_label) )
                gdk_pixbuf_copy_area ( val->pixbuf[CLUSTER], 0, 0, sizex, sizey, val->full_pixbuf[CLUSTER], destx, desty );

            // Max Square drawing
            if ( val->on[MAX_SQR] )
              if ( ( x >= val->xx && x < (val->xx + val->max_square) )
                     && ( y >= val->yy && y < (val->yy + val->max_square) ) ) {
                gdk_pixbuf_copy_area ( val->pixbuf[MAX_SQR], 0, 0, sizex, sizey, val->full_pixbuf[MAX_SQR], destx, desty );
              }

            // Line of tiles drawing
            if ( val->on[LINES] && val->ns_size && val->ew_size ) {
              if ( x == val->ns_x ) {
                if ( y <= val->ns_y && y >= val->ns_y-val->ns_size )
                  gdk_pixbuf_copy_area ( val->pixbuf[LINES], 0, 0, sizex, sizey, val->full_pixbuf[LINES], destx, desty );
              }
              if ( y == val->ew_y ) {
                if ( x <= val->ew_x && x >= val->ew_x-val->ew_size )
                  gdk_pixbuf_copy_area ( val->pixbuf[LINES], 0, 0, sizex, sizey, val->full_pixbuf[LINES], destx, desty );
              }
            }

            if ( val->on[TNEW] )
              if ( is_tile_occupied(val->tiles_new, x, y) ) {
                gdk_pixbuf_copy_area ( val->pixbuf[TNEW], 0, 0, sizex, sizey, val->full_pixbuf[TNEW], destx, desty );
              }
          } else {
            gint x2 = xx;
            gint y2 = yy;
            gint w2 = width;
            gint h2 = height;
            if ( x2 < 0 ) {
              w2 = xx + tilesize_ceil;
              if ( w2 > width )
                w2 = width;
              x2 = 0;
            }
            if ( y2 < 0 ) {
              y2 = 0;
              h2 = yy + tilesize_ceil;
              if ( h2 > height )
                h2 = height;
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

    // Draw any unreachable tiles if they are in the display area
    if ( tiles_unreachable && !is_big ) {

      GHashTableIter iter;
      gpointer key, value;
      gint uz, ux, uy;
      const guint zl = (guint)map_utils_mpp_to_zoom_level ( zoom );

      // Always red - not bothered to allow config of this ATM
      GdkColor gdc;
      gdk_color_parse ( "red", &gdc );
      GdkPixbuf *pixbuf = NULL;
      pixbuf = layer_pixbuf_update ( pixbuf, gdc, tilesize <= 256 ? 256 : tilesize, tilesize <= 256 ? 256 : tilesize, val->alpha[BASIC] );

      MapCoord mc;
      mc.scale = ulm.scale; // Current display zoom level
      guint sizex, sizey, destx, desty;

      g_hash_table_iter_init ( &iter, tiles_unreachable );
      while ( g_hash_table_iter_next(&iter, &key, &value) ) {
        (void)sscanf ( key, "%d %d %d", &uz, &ux, &uy );
        if ( uz == zl && (ux >= xmin) && (ux <= xmax) && (uy >= ymin) && (uy <=ymax) ) {
          mc.x = ux;
          mc.y = uy;
          map_utils_iTMS_to_vikcoord ( &mc, &coord );
          vik_viewport_coord_to_screen ( vvp, &coord, &xx_tmp, &yy_tmp );
          get_pixel_limits ( &sizex, &sizey, &destx, &desty, xx_tmp, yy_tmp, width, height, tilesize_ceil );
          gdk_pixbuf_copy_area ( pixbuf, 0, 0, sizex, sizey, val->unreachable_pixbuf, destx, desty );
        }
      }
      g_object_unref ( pixbuf );
    }

    if ( !is_big ) {
      for ( guint ii=0; ii<CP_NUM; ii++ )
        if ( val->on[ii] && val->full_pixbuf[ii] )
          vik_viewport_draw_pixbuf ( vvp, val->full_pixbuf[ii], 0, 0, 0, 0, width, height );
      if ( tiles_unreachable )
        vik_viewport_draw_pixbuf ( vvp, val->unreachable_pixbuf, 0, 0, 0, 0, width, height );
    }

    //g_debug ( "%s: Tiles drawn %d", __FUNCTION__, tile_draw_count );

    // Grid lines if wanted and otherwise doesn't dominate the display either...
    // TODO Probably better to determine a value based on the display / HD
    if ( val->draw_grid && tilesize > 4 ) {
      guint scale = vik_viewport_get_scale ( vvp );
      // Grid drawing here so it gets drawn on top of the previous tiles
      // Thus loop around x & y again, but this time separately
      GdkGC *black_gc = vik_viewport_get_black_gc ( vvp );
      // Draw single grid lines across the whole screen
      xx = base_xx;
      for ( x = ((xinc == 1) ? xmin : xmax); x != xend; x+=xinc ) {
         vik_viewport_draw_line ( vvp, black_gc, xx, base_yy, xx, height, &black_color, scale );
         xx += tilesize;
      }

      yy = base_yy;
      for ( y = ((yinc == 1) ? ymin : ymax); y != yend; y+=yinc ) {
        vik_viewport_draw_line ( vvp, black_gc, base_xx, yy, width, yy, &black_color, scale );
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

/**
 * c.f. vik_viewport_coord_to_screen() but for a separately configurable zoom level & Mercator only
 */
static void coord_to_screen ( gint width, gint height, gdouble mf, struct LatLon *center, VikCoord *coord, gint *xx, gint *yy )
{
  struct LatLon *ll = (struct LatLon *)coord;
  *xx = (int)(width/2 + ( mf * (ll->lon - center->lon) ));
  *yy = (int)(height/2 + ( mf * ( MERCLAT(center->lat) - MERCLAT(ll->lat) ) ));
}

/**
 *
 */
static void hm_clear ( VikAggregateLayer *val )
{
  if ( val->hm_pixbuf )
    g_object_unref ( val->hm_pixbuf );
  if ( val->hm_pbf_scaled )
    g_object_unref ( val->hm_pbf_scaled );
  val->hm_pixbuf = NULL;
  val->hm_pbf_scaled = NULL;
}

/**
 * Draw heatmap
 */
static void hm_draw ( VikAggregateLayer *val, VikViewport *vp )
{
  LatLonBBox bbox = vik_viewport_get_bbox ( vp );

  if ( BBOX_INTERSECT ( bbox, val->hm_bbox ) ) {

    clock_t begin, end;

    gint zz = (gint)vik_viewport_get_zoom ( vp );
    // Avoid excessive image scaling as it will be too slow
    //  (and memory intensive) so simply avoid trying.
    if ( zz < val->hm_zoom_max ) {
      return;
    }

    if ( val->hm_width != vik_viewport_get_width(vp) ||
         val->hm_height != vik_viewport_get_height(vp) ) {
      hm_clear ( val );
      return;
    }

    // Calculate width & height (even if no scaling as not excessive computation)
    gint ww = round (val->hm_width * (gdouble)val->hm_zoom/(gdouble)zz );
    gint hh = round (val->hm_height * (gdouble)val->hm_zoom/(gdouble)zz );
    // Scale only once as necessary when zoom level has changed
    if ( val->hm_scaled_zoom != zz ) {
      val->hm_scaled_zoom = zz;
      // Different zoom level so generate new image from the original image
      if ( val->hm_scaled_zoom != val->hm_zoom ) {
        if ( val->hm_pbf_scaled )
          g_object_unref ( val->hm_pbf_scaled );

        val->hm_scaled = TRUE;
        GdkInterpType interp_type = GDK_INTERP_BILINEAR;
        // When scaling up: use the fastest method (as scaling up is much slower than scaling down)
        //  especially since this is being performed in the main thread
        if ( val->hm_scaled_zoom < val->hm_zoom )
          interp_type = GDK_INTERP_NEAREST;
        begin = clock();
        val->hm_pbf_scaled = gdk_pixbuf_scale_simple ( val->hm_pixbuf, ww, hh, interp_type );
        end = clock();
        double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
        // Last usable zoom-level as the next one is going to be an order of magnitude worse or more
        if ( interp_type == GDK_INTERP_NEAREST && time_spent > 0.05 )
          val->hm_zoom_max = zz;
        g_debug ( "%s: time %f scaling to %d, %d", __FUNCTION__, time_spent, ww, hh );
      } else {
        // Use original image
        val->hm_scaled = FALSE;
      }
    }
    gint xx, yy;
    gdouble mf = mercator_factor ( val->hm_scaled_zoom, val->hm_scale );
    coord_to_screen ( val->hm_width, val->hm_height, mf, (struct LatLon*)val->hm_center, &val->hm_tl, &xx, &yy);
    vik_viewport_draw_pixbuf ( vp, val->hm_scaled ? val->hm_pbf_scaled : val->hm_pixbuf, 0, 0, xx, yy, ww, hh );
  }
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
#if GTK_CHECK_VERSION (3,0,0)
  // GTK3 Version does not use pixmaps, so no point in trigger layers ATM
  while ( iter ) {
    vik_layer_draw ( VIK_LAYER(iter->data), vp );
    iter = iter->next;
  }
#else
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
#endif
  // Make coverage to be drawn last (i.e. over the top of any maps)
  if ( val->on[BASIC] ) {
    tac_draw ( val, vp );
  }

  if ( !val->hm_calculating && val->hm_pixbuf ) {
    hm_draw ( val, vp );
  }
}

void vik_aggregate_layer_configure ( VikAggregateLayer *val, VikViewport *vp )
{
  GList *iter = val->children;
  while ( iter ) {
    vik_layer_configure ( VIK_LAYER(iter->data), vp );
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

// A slightly better way of defining the menu callback information
// This should be easier to extend/rework compared to previously
typedef enum {
  MA_VAL = 0,
  MA_VLP,
  MA_LAST
} menu_array_index;

typedef gpointer menu_array_values[MA_LAST];

/**
 * May not want to monitor visibility changes
 */
static void vis_change_update ( VikAggregateLayer *val )
{
  gboolean ignore_toggle = FALSE;
  (void)a_settings_get_boolean ( VIK_SETTINGS_IGNORE_VIS_MOD, &ignore_toggle );
  // Redraw as view may have changed
  vik_layer_emit_update ( VIK_LAYER(val), !ignore_toggle );
}

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
  vis_change_update ( val );
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
  vis_change_update ( val );
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
 * @user_data: If not NULL then invisible layers are excluded
 *
 * Returns: A list of #vik_trw_waypoint_list_t
 */
static GList* aggregate_layer_waypoint_create_list ( VikLayer *vl, gpointer user_data )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER(vl);

  // Get all TRW layers
  GList *layers = NULL;
  layers = vik_aggregate_layer_get_all_layers_of_type ( val, layers, VIK_LAYER_TRW, user_data ? FALSE : TRUE );

  // For each TRW layers keep adding the waypoints to build a list of all of them
  GList *waypoints_and_layers = NULL;
  for ( GList *layer = layers; layer != NULL; layer = layer->next ) {
    GList *waypoints = g_hash_table_get_values ( vik_trw_layer_get_waypoints( VIK_TRW_LAYER(layer->data) ) );
    waypoints_and_layers = g_list_concat ( waypoints_and_layers, vik_trw_layer_build_waypoint_list_t ( VIK_TRW_LAYER(layer->data), waypoints ) );
    g_list_free ( waypoints );
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
 * @user_data: If not NULL then invisible layers are excluded
 *
 * Returns: A list of #vik_trw_and_track_t
 */
static GList* aggregate_layer_track_create_list ( VikLayer *vl, gpointer user_data )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER(vl);

  // Get all TRW layers
  GList *layers = NULL;
  layers = vik_aggregate_layer_get_all_layers_of_type ( val, layers, VIK_LAYER_TRW, user_data ? FALSE : TRUE );

  // For each TRW layers keep adding the tracks and routes to build a list of all of them
  GList *tracks_and_layers = NULL;
  for ( GList *layer = layers; layer != NULL; layer = layer->next ) {
    GList *tracks = g_hash_table_get_values ( vik_trw_layer_get_tracks( VIK_TRW_LAYER(layer->data) ) );
    tracks = g_list_concat ( tracks, g_hash_table_get_values ( vik_trw_layer_get_routes( VIK_TRW_LAYER(layer->data) ) ) );
    tracks_and_layers = g_list_concat ( tracks_and_layers, vik_trw_layer_build_track_list_t ( VIK_TRW_LAYER(layer->data), tracks ) );
    g_list_free ( tracks );
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
 * Load selected files as external layers into this Aggregate Layer
 *
 */
static void aggregate_layer_load_external_file_cb ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER ( values[MA_VAL] );
  VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(val));
  VikViewport *vvp = vik_window_viewport ( vw );

  if ( !vu_check_confirm_external_use(GTK_WINDOW(vw)) )
    return;

  GSList *files = vu_get_ui_selected_gps_files ( vw, TRUE );
  GSList *cur_file = files;
  while ( cur_file ) {
    gchar *filename = cur_file->data;
    VikLoadType_t ans = a_file_load ( val, vvp, NULL, filename, FALSE, TRUE, NULL );
    if ( ans < LOAD_TYPE_OTHER_FAILURE_NON_FATAL )
      a_dialog_error_msg_extra ( GTK_WINDOW(vw), _("Failed to open %s"), filename );
    else if ( ans == LOAD_TYPE_OTHER_FAILURE_NON_FATAL ) {
      gchar *msg = g_strdup_printf ( _("WARNING: issues encountered loading %s"), a_file_basename (filename) );
      vik_statusbar_set_message ( vik_window_get_statusbar(VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(val))), VIK_STATUSBAR_INFO, msg );
      g_free ( msg );
    }
    g_free ( filename );
    cur_file = g_slist_next ( cur_file );
  }
  g_slist_free (files);

  vik_layer_emit_update ( VIK_LAYER(val), TRUE );
}

static void aggregate_layer_save_layer_as_cb ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER ( values[MA_VAL] );
  VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(val));
  (void)vik_window_save_file_as ( vw, val );
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

  vik_layer_emit_update ( VIK_LAYER(val), TRUE );
}

/**
 *
 */
static void aggregate_layer_export_gpx ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER ( values[MA_VAL] );
  vik_aggregate_layer_export_gpx_setup ( val );
}

/**
 *
 */
static void add_tile_label ( GHashTable *ght, gint x, gint y, guint id )
{
  gchar *key = g_strdup_printf ( "%d:%d", x, y );
  (void)g_hash_table_insert ( ght, key, GUINT_TO_POINTER(id) );
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
  g_list_free_full ( ct->tracks_and_layers, g_free );
  g_free ( ct );
}

static void ct_cancel ( CalculateThreadT *ct )
{
  // Draw as much as we have processed so far
  vik_layer_emit_update ( VIK_LAYER(ct->val), FALSE ); // NB update display from background
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
  g_free ( sizes );
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
  g_free ( sizes );
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
  g_debug ( "%s: %f", __FUNCTION__, time_spent );
}

/**
 * Finds biggest line of tiles, both vertically and horizontally
 */
static void tac_lines_calc ( VikAggregateLayer *val )
{
  clock_t begin = clock();

  GHashTableIter iter;
  gpointer key, value;
  gint x,y;
  gint tlx=G_MAXINT,tly=G_MAXINT,brx=-G_MAXINT,bry=-G_MAXINT;

  // Get extents of tile coverage
  g_hash_table_iter_init ( &iter, val->tiles );
  while ( g_hash_table_iter_next(&iter, &key, &value) ) {
    (void)sscanf ( key, "%d:%d", &x, &y );
    if ( x < tlx ) tlx = x;
    if ( x > brx ) brx = x;
    if ( y < tly ) tly = y;
    if ( y > bry ) bry = y;
  }

  // Simple brute force method
  // Detects the first instance of the biggest consective run of tiles
  //  in both vertical and horizontal directions
  guint crt_sz = 0;

  // North/South passage...
  for ( gint xx = tlx; xx <= brx; xx++ ) {
    gint yy;
    for ( yy = tly ; yy <= bry; yy++ ) {
      if ( is_tile_occupied(val->tiles, xx, yy) )
        crt_sz++;
      else {
        if ( crt_sz > val->ns_size ) {
          val->ns_size = crt_sz;
          val->ns_x = xx;
          val->ns_y = yy-1;
        }
        crt_sz = 0;
      }
    }
    // Detect finish at edge of extents
    if ( crt_sz > val->ns_size ) {
      val->ns_size = crt_sz;
      val->ns_x = xx;
      val->ns_y = yy;
    }
    // Reset for next line
    crt_sz = 0;
  }

  crt_sz = 0;
  // East/West passage...
  for ( gint yy = tly; yy <= bry; yy++ ) {
    gint xx;
    for ( xx = tlx; xx <= brx; xx++ ) {
      if ( is_tile_occupied(val->tiles, xx, yy) )
        crt_sz++;
      else {
        if ( crt_sz > val->ew_size ) {
          val->ew_size = crt_sz;
          val->ew_x = xx-1;
          val->ew_y = yy;
        }
        crt_sz = 0;
      }
    }
    // Detect finish at edge of extents
    if ( crt_sz > val->ew_size ) {
      val->ew_size = crt_sz;
      val->ew_x = xx;
      val->ew_y = yy;
    }
    // Reset for next line
    crt_sz = 0;
  }

  g_debug ( "%s: ns_x %d, ns_y %d, ns_size %d | ew_x %d, ew_y %d, ew_size %d:",
            __FUNCTION__, val->ns_x, val->ns_y, val->ns_size, val->ew_x, val->ew_y, val->ew_size );

  clock_t end = clock();
  double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
  g_debug ( "%s: %f", __FUNCTION__, time_spent );
}

/**
 * Insert unreachable tiles to pretend they have been visited
 *  thus contributing to max squares, clusters and contiguous calculations
 * NB: ATM this doesn't effect the numbers reported too much as it uses the
 *  separate count 'num_tiles' rather than the number in the hash table
 */
static void tac_unreachable ( VikAggregateLayer *val )
{
  if ( !tiles_unreachable ) return;

  GHashTableIter iter;
  gpointer key, value;
  gint z,x,y;

  guint zoom = (guint)map_utils_mpp_to_zoom_level(val->zoom_level);

  g_hash_table_iter_init ( &iter, tiles_unreachable );
  while ( g_hash_table_iter_next(&iter, &key, &value) ) {
    (void)sscanf ( key, "%d %d %d", &z, &x, &y );
    if ( z == zoom )
      add_tile ( val->tiles, x, y );
  }
}

// Fwd declaration
static void tac_clear ( VikAggregateLayer *val );

/**
 *
 */
static gint tac_calculate_thread ( CalculateThreadT *ct, gpointer threaddata )
{
  clock_t begin = clock();

  g_hash_table_remove_all ( ct->val->prev );
  g_hash_table_remove_all ( ct->val->tiles_new );
  ct->val->num_tiles[TNEW] = 0;

  // Only if there's something before then 'turn on' detection of new tiles...
  //  (too otherwise avoid marking everything new on first time calculation
  //   on particularly initial file loads)
  // Also don't try to find new ones when the zoom level has changed
  //  and discount the number of unreachable tiles
  gboolean zoom_level_chgd = (ct->val->zoom_level_prev != ct->val->zoom_level);
  if ( zoom_level_chgd )
    ct->val->zoom_level_prev = ct->val->zoom_level;
  guint sz = 0;
  if ( tiles_unreachable )
    sz = g_hash_table_size ( tiles_unreachable );
  if ( (ct->val->num_tiles[BASIC] > sz) && ct->val->on[TNEW]) {
    // Copy current tiles into prev
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init ( &iter, ct->val->tiles );
    while ( g_hash_table_iter_next(&iter, &key, &value) )
      (void)g_hash_table_insert ( ct->val->prev, g_strdup(key), GUINT_TO_POINTER(0) );

    for (gint x = 0; x<CP_NUM; x++ )
      ct->val->num_prev[x] = ct->val->num_tiles[x];
  }
  ct->val->max_square_prev = ct->val->max_square;
  ct->val->ns_size_prev = ct->val->ns_size;
  ct->val->ew_size_prev = ct->val->ew_size;

  tac_clear ( ct->val );

  tac_unreachable ( ct->val );

  guint tracks_processed = 0;
  // This is used to prevent the progress going negative or otherwise over 100%
  // It's difficult to get an estimate for the total and track progress of each of these parts
  //  and then combine it in a coherent single thread progress meter.
  // So for simplicity they are considered the same as processing extra set of tracks
  guint extras = (ct->val->on[MAX_SQR] * ct->num_of_tracks) +
    (ct->val->on[CONTIG] * ct->num_of_tracks) +
    (ct->val->on[CLUSTER] * ct->num_of_tracks) +
    (ct->val->on[LINES] * ct->num_of_tracks);

  for ( GList *tl = ct->tracks_and_layers; tl != NULL; tl = tl->next ) {
    gdouble percent = (gdouble)tracks_processed/(gdouble)(ct->num_of_tracks+extras);
    gint res = a_background_thread_progress ( threaddata, percent );
    if ( res != 0 ) return -1;

    check_track ( ct->val, tl->data );
    tracks_processed++;
  }

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

  if ( ct->val->on[LINES] ) {
    gdouble percent = (gdouble)tracks_processed/(gdouble)(ct->num_of_tracks+extras);
    gint res = a_background_thread_progress ( threaddata, percent );
    if ( res != 0 ) return -1;

    tac_lines_calc ( ct->val );
    tracks_processed = tracks_processed + ct->num_of_tracks;
  }

  if ( (ct->val->num_prev[BASIC] > sz) && ct->val->on[TNEW] && !zoom_level_chgd ) {
    // Determine difference in latest tiles vs prev
    // Could be slow, but seems not too bad
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init ( &iter, ct->val->tiles );
    while ( g_hash_table_iter_next(&iter, &key, &value) ) {
      gboolean contained = g_hash_table_contains ( ct->val->prev, key );
      if ( !contained ) {
        (void)g_hash_table_insert ( ct->val->tiles_new, g_strdup(key), GUINT_TO_POINTER(0) );
        ct->val->num_tiles[TNEW]++;
      }
    }
    // Also doing it here means the detection is only done for the 'first' calculation update
    // (this calculation alsootherwise gets done for any config change - even if just colour changed).
    //  so ATM the new tiles get reset for such subsequent recalculations
    g_hash_table_remove_all ( ct->val->prev );
  }

  // Timing for all tile calcs
  clock_t end = clock();
  double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
  g_debug ( "%s: %f", __FUNCTION__, time_spent );

  ct->val->calculating = FALSE;
  vik_layer_emit_update ( VIK_LAYER(ct->val), FALSE ); // NB update display from background

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
  g_hash_table_remove_all ( val->tiles_new ); // No memory to remove ATM
  // NB val->prev is not cleared at this point as needed for the later comparison
  val->ns_size = 0;
  val->ew_size = 0;
}

/**
 *
 */
static void tac_calculate ( VikAggregateLayer *val )
{
  val->calculating = TRUE;
  val->num_calcs++;

  GDate *now = g_date_new ();
  g_date_set_time_t ( now, time(NULL) );

  GList *layers = NULL;
  layers = vik_aggregate_layer_get_all_layers_of_type ( val, layers, VIK_LAYER_TRW, TRUE );

  // For each TRW layers keep adding the tracks to build a list of all of them
  GList *tracks_and_layers = NULL; // A list of #vik_trw_track_list_t
  for ( GList *layer = layers; layer != NULL; layer = layer->next ) {
    GList *tracks = g_hash_table_get_values ( vik_trw_layer_get_tracks( VIK_TRW_LAYER(layer->data) ) );
    if ( !val->tac_time_range )
      // All
      tracks_and_layers = g_list_concat ( tracks_and_layers, vik_trw_layer_build_track_list_t ( VIK_TRW_LAYER(layer->data), tracks ) );
    else {
      // Only those within specified time period
      for ( GList *track = tracks; track != NULL; track = track->next ) {
        VikTrack *trk = VIK_TRACK(track->data);
        gdouble ts = VIK_TRACKPOINT(trk->trackpoints->data)->timestamp;
        if ( trk->trackpoints && !isnan(ts) ) {
          GDate* gdate = g_date_new ();
          g_date_set_time_t ( gdate, (time_t)ts );
          gint diff = g_date_days_between ( gdate, now );
          g_date_free ( gdate );
          // NB this doesn't get the year date range exact
          //  however this generally should be good/close enough for practical purposes
          if ( diff > 0 && diff < (365.25*val->tac_time_range) ) {
            vik_trw_and_track_t *vtdl = g_malloc (sizeof(vik_trw_and_track_t));
            vtdl->trk = trk;
            vtdl->vtl = VIK_TRW_LAYER(layer->data);
            tracks_and_layers = g_list_prepend ( tracks_and_layers, vtdl );
          }
        }
      }
    }
    g_list_free ( tracks );
  }
  g_list_free ( layers );
  g_date_free ( now );

  CalculateThreadT *ct = g_malloc ( sizeof(CalculateThreadT) );
  ct->tracks_and_layers = tracks_and_layers;
  ct->val = val;
  ct->num_of_tracks = g_list_length (tracks_and_layers);
  guint extras = ct->val->on[MAX_SQR] + ct->val->on[CONTIG] + ct->val->on[CLUSTER];

  a_background_thread ( BACKGROUND_POOL_LOCAL,
                        VIK_GTK_WINDOW_FROM_LAYER(val),
                        _("Track Area Coverage"),
                        (vik_thr_func)tac_calculate_thread,
                        ct,
                        (vik_thr_free_func)ct_free,
                        (vik_thr_free_func)ct_cancel,
                        ct->num_of_tracks + extras );
}

static void rhomboidal (float *values, unsigned d, unsigned r)
{
  for (guint y = 0 ; y < d ; ++y) {
    for (guint x = 0 ; x < d ; ++x) {
      values[y*d+x] = 1.0 - fmin(1.0, (float)(labs(x-(long)r)+labs(y-(long)r))/(r+1));
    }
  }
}

/**
 *
 */
static void hm_track ( VikAggregateLayer *val, vik_trw_and_track_t *vtlist, gdouble mf, heatmap_t* hm, heatmap_stamp_t *stamp )
{
  int xx, yy;
  VikTrack *trk = vtlist->trk;
  GList *iter = trk->trackpoints;
  while ( iter ) {
    // Only do trackpoints with timestamps
    // - i.e. hopefully to avoid artificial tracks
    if ( !isnan(VIK_TRACKPOINT(iter->data)->timestamp) ) {
      coord_to_screen (val->hm_width, val->hm_height, mf, (struct LatLon*)val->hm_center, &VIK_TRACKPOINT(iter->data)->coord, &xx, &yy);
      /*
      struct LatLon *ll = (struct LatLon *)&VIK_TRACKPOINT(iter->data)->coord;
      xx = (int)(val->hm_width/2 + ( mf * (ll->lon - center->lon) ));
      yy = (int)(val->hm_height/2 + ( mf * ( MERCLAT(center->lat) - MERCLAT(ll->lat) ) ));
      */
      //heatmap_add_point ( hm, xx, yy );
      heatmap_add_point_with_stamp ( hm, xx, yy, stamp );
      //hm_point ( val, &VIK_TRACKPOINT(iter->data)->coord, hm );
    }
    iter = iter->next;
  }
}

static void hm_img_free ( guchar *pixels, gpointer data )
{
  g_free ( pixels );
}

/**
 *
 */
static gint hm_calculate_thread ( CalculateThreadT *ct, gpointer threaddata )
{
  VikAggregateLayer *val = ct->val;

  clock_t begin = clock();

  // Generate a stamp with a size relative to the zoom level
  unsigned radius = map_utils_mpp_to_zoom_level ( val->hm_zoom ) *
    (gdouble)val->hm_stamp_factor/(gdouble)width_default().u;
  unsigned d = 2*radius + 1;
  float pts[d * d];
  rhomboidal ( pts, d, radius );
  heatmap_stamp_t *stamp = heatmap_stamp_load ( d, d, pts );
  heatmap_t* hm = heatmap_new ( val->hm_width, val->hm_height );

  int ww = val->hm_width;
  int hh = val->hm_height;

  // Only needs calculating once
  gdouble mf = mercator_factor ( val->hm_zoom, val->hm_scale );

  guint tracks_processed = 0;
  for ( GList *tl = ct->tracks_and_layers; tl != NULL; tl = tl->next ) {
    gdouble percent = (gdouble)tracks_processed/(gdouble)(ct->num_of_tracks);
    gint res = a_background_thread_progress ( threaddata, percent );
    if ( res != 0 ) return -1;

    vik_trw_and_track_t *vtlist = tl->data;
    if ( BBOX_INTERSECT ( vtlist->trk->bbox, val->hm_bbox ) )
      hm_track ( ct->val, vtlist, mf, hm, stamp );

    tracks_processed++;
  }

  // Would be better if testing for any tracks actually used
  if ( tracks_processed > 0 ) {
    unsigned char *image = g_malloc ( ww*hh*4 );

    if ( val->hm_style > 0 && val->hm_style < 4 )
      heatmap_render_to ( hm, hm_colorschemes[val->hm_style-1], image );
    else
      heatmap_render_default_to ( hm, image );

    val->hm_pixbuf = gdk_pixbuf_new_from_data ( image, GDK_COLORSPACE_RGB, TRUE, 8, ww, hh, 4*ww, hm_img_free, NULL );
    val->hm_pixbuf = ui_pixbuf_set_alpha ( val->hm_pixbuf, val->hm_alpha );
  }

  heatmap_free ( hm );
  heatmap_stamp_free ( stamp );

  // Timing
  clock_t end = clock();
  double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
  g_debug ( "%s: %f", __FUNCTION__, time_spent );

  ct->val->hm_calculating = FALSE;
  vik_layer_emit_update ( VIK_LAYER(ct->val), FALSE ); // NB update display from background

  return 0;
}

/**
 *
 */
static void hm_calculate ( VikAggregateLayer *val )
{
  VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(val));
  VikViewport *vvp = vik_window_viewport ( vw );

  val->hm_width = vik_viewport_get_width ( vvp );
  val->hm_height = vik_viewport_get_height ( vvp );
  val->hm_bbox = vik_viewport_get_bbox ( vvp );
  val->hm_zoom = (gint)vik_viewport_get_zoom ( vvp );
  val->hm_scaled_zoom = val->hm_zoom;
  val->hm_center = vik_viewport_get_center ( vvp );
  vik_viewport_screen_to_coord ( vvp, 0, 0, &val->hm_tl );
  val->hm_scale = vik_viewport_get_scale ( vvp );
  val->hm_scaled = FALSE;
  val->hm_zoom_max = 0;
  if ( val->hm_zoom == 0 ) {
    g_warning ( "%s: Zoom invalid", __FUNCTION__ );
    return;
  }

  hm_clear ( val );
  val->hm_calculating = TRUE;

  GList *layers = NULL;
  layers = vik_aggregate_layer_get_all_layers_of_type ( val, layers, VIK_LAYER_TRW, TRUE );

  // For each TRW layers keep adding the tracks to build a list of all of them
  GList *tracks_and_layers = NULL; // A list of #vik_trw_track_list_t
  for ( GList *layer = layers; layer != NULL; layer = layer->next ) {
    GList *tracks = g_hash_table_get_values ( vik_trw_layer_get_tracks( VIK_TRW_LAYER(layer->data) ) );
    tracks_and_layers = g_list_concat ( tracks_and_layers, vik_trw_layer_build_track_list_t ( VIK_TRW_LAYER(layer->data), tracks ) );
    g_list_free ( tracks );
  }
  g_list_free ( layers );

  CalculateThreadT *ct = g_malloc ( sizeof(CalculateThreadT) );
  ct->tracks_and_layers = tracks_and_layers;
  ct->val = val;
  ct->num_of_tracks = g_list_length ( tracks_and_layers );

  a_background_thread ( BACKGROUND_POOL_LOCAL,
                        VIK_GTK_WINDOW_FROM_LAYER(val),
                        _("Heatmap generation"),
                        (vik_thr_func)hm_calculate_thread,
                        ct,
                        (vik_thr_free_func)ct_free,
                        (vik_thr_free_func)ct_cancel,
                        ct->num_of_tracks );
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
  vik_layer_emit_update ( VIK_LAYER(val), FALSE ); // NB update display from background
}

/**
 * Generate MBTiles file of TAC pixbufs
 */
#ifdef HAVE_SQLITE3_H

typedef struct {
  VikAggregateLayer *val;
  gchar *fn;
} MBT_T;

static void mbt_free ( MBT_T *mbt )
{
  g_free ( mbt->fn );
  g_free ( mbt );
}

static void tac_mbtiles_insert_metadata_pair ( sqlite3 *sql, const gchar *name, const gchar *value )
{
  gchar *ins = g_strdup_printf ( "INSERT INTO metadata (name, value) VALUES ('%s', \"%s\");", name, value );

  sqlite3_stmt *sql_stmt;
  int ans = sqlite3_prepare_v2 ( sql, ins, -1, &sql_stmt, NULL );
  g_free ( ins );
  if ( ans != SQLITE_OK ) {
    g_warning ( "%s: %s", __FUNCTION__, sqlite3_errmsg(sql) );
  } else {
    int step = sqlite3_step ( sql_stmt );
    if ( step != SQLITE_DONE ) {
      g_warning ( "%s: sqlite3_step result was %d", __FUNCTION__, step );
    }
  }
  (void)sqlite3_finalize ( sql_stmt );
}

static gint tac_mbtiles_thread ( MBT_T *mbt, gpointer threaddata  )
{
  VikAggregateLayer *val = mbt->val;
  clock_t begin = clock();
  guint num_tiles = 0;
  gint result = 0;

  gchar *msg = NULL;
  sqlite3 *mbtiles;
  int ans = sqlite3_open ( mbt->fn, &mbtiles );
  if ( ans != SQLITE_OK ) {
    msg = g_strdup ( sqlite3_errmsg(mbtiles) );
    goto cleanup;
  }

  char *err_msg = 0;
  // Recreate tables and use fast writing options, since the data is not critical and the file can be easily regenerated
  char *cmd =
    "DROP TABLE IF EXISTS tiles;"
    "DROP TABLE IF EXISTS metadata;"
    "CREATE TABLE tiles (zoom_level integer, tile_column integer, tile_row integer, tile_data blob);"
    "CREATE TABLE metadata (name text, value text);"
    "CREATE unique index name on metadata (name);"
    "CREATE unique index tile_index on tiles (zoom_level, tile_column, tile_row);"
    "PRAGMA synchronous=0;"
    "PRAGMA locking_mode=EXCLUSIVE;"
    "PRAGMA journal_mode=OFF;";

  ans = sqlite3_exec ( mbtiles, cmd, 0, 0, &err_msg );
  if ( ans != SQLITE_OK ) {
    msg = g_strdup ( err_msg );
    sqlite3_free ( err_msg );
    goto cleanup;
  }

  // v1.1 Metadata
  tac_mbtiles_insert_metadata_pair ( mbtiles, "name", vik_layer_get_name(VIK_LAYER(val)) );
  tac_mbtiles_insert_metadata_pair ( mbtiles, "type", "overlay" );
  tac_mbtiles_insert_metadata_pair ( mbtiles, "version", "1" );
  tac_mbtiles_insert_metadata_pair ( mbtiles, "description", "Created by Viking - " PACKAGE_URL );
  tac_mbtiles_insert_metadata_pair ( mbtiles, "format", "png" );

  guint zoom = (guint)map_utils_mpp_to_zoom_level(val->zoom_level);

  GHashTableIter iter;
  gpointer key, value;
  gint x,y;
  GdkPixbuf *pixbuf = NULL;
  guint sz = g_hash_table_size ( val->tiles );

  g_hash_table_iter_init ( &iter, val->tiles );
  while ( g_hash_table_iter_next(&iter, &key, &value) ) {

    num_tiles++;
    gdouble percent = (gdouble)num_tiles/(gdouble)sz;
    gint res = a_background_thread_progress ( threaddata, percent );
    if ( res != 0 ) {
      result = -1;
      goto cleanup;
    }

    (void)sscanf ( key, "%d:%d", &x, &y );

    pixbuf = layer_pixbuf_update ( pixbuf, val->color[BASIC], 256, 256, val->alpha[BASIC] );

    gint flip_y = (gint) pow(2, zoom)-1 - y;

    gchar *ins = g_strdup_printf
      ("INSERT INTO tiles VALUES (%d, %d, %d, ?);", zoom, x, flip_y);

    sqlite3_stmt *sql_stmt;
    ans = sqlite3_prepare_v2 ( mbtiles, ins, -1, &sql_stmt, NULL );
    g_free ( ins );
    if ( ans != SQLITE_OK ) {
      msg = g_strdup ( sqlite3_errmsg(mbtiles) );
      goto cleanup;
    }

    gchar *buffer;
    gsize size;
    GError *error = NULL;
    ans = gdk_pixbuf_save_to_buffer ( pixbuf, &buffer, &size, "png", &error, NULL );
    if ( error ) {
      msg = g_strdup ( error->message );
      g_error_free ( error );
      goto cleanup;
    }

    ans = sqlite3_bind_blob ( sql_stmt, 1, buffer, size, g_free );
    if ( ans != SQLITE_OK ) {
      msg = g_strdup ( err_msg );
      sqlite3_free ( err_msg );
      goto cleanup;
    }

    int step = sqlite3_step ( sql_stmt );
    // This should always complete
    if ( step != SQLITE_DONE ) {
      msg = g_strdup_printf ( "sqlite3_step result was %d", step );
      goto cleanup;
    }

    (void)sqlite3_finalize ( sql_stmt );

    // Minimize filesize
    (void)sqlite3_exec ( mbtiles, "ANALYZE; VACUUM;", 0, 0, NULL );
  }

 cleanup:
  (void)sqlite3_close ( mbtiles );
  clock_t end = clock();
  double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
  g_message ( "%s: %f %d", __FUNCTION__, time_spent, num_tiles );

  if ( msg ) {
    gchar *fullmsg = g_strdup_printf ( _("MBTiles file write problem: %s"), msg );
    vik_window_statusbar_update ( (VikWindow*)VIK_GTK_WINDOW_FROM_LAYER(val), fullmsg, VIK_STATUSBAR_INFO );
    g_free ( fullmsg );
    g_free ( msg );
  }

  return result;
}

static void tac_generate_mbtiles_cb ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER(values[MA_VAL]);

  gchar *fn = NULL;
  GtkWidget *dialog = gtk_file_chooser_dialog_new ( _("Export"),
						    NULL,
						    GTK_FILE_CHOOSER_ACTION_SAVE,
						    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						    GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
						    NULL );
  gchar *name = g_strdup_printf ( "%s.mbtiles", vik_layer_get_name(VIK_LAYER(val)) );
  gtk_file_chooser_set_current_name ( GTK_FILE_CHOOSER(dialog), name );
  g_free ( name );

  while ( gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT ) {
    fn = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER(dialog) );
    if ( g_file_test(fn, G_FILE_TEST_EXISTS) == FALSE || a_dialog_yes_or_no ( GTK_WINDOW(dialog), _("The file \"%s\" exists, do you wish to overwrite it?"), a_file_basename ( fn ) ) )
      break;
    g_free ( fn );
    fn = NULL;
  }
  gtk_widget_destroy ( dialog );

  if ( !fn )
    return;

  MBT_T *mbt = g_malloc ( sizeof(MBT_T) );
  mbt->val = val;
  mbt->fn = fn;
  a_background_thread ( BACKGROUND_POOL_LOCAL,
                        VIK_GTK_WINDOW_FROM_LAYER(val),
                        _("Creating MBTiles File"),
                        (vik_thr_func)tac_mbtiles_thread,
                        mbt,
                        (vik_thr_free_func)mbt_free,
                        NULL, // cancel() nothing to do, could delete file but ATM leave as progressed
                        g_hash_table_size(val->tiles) );
}
#endif

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
  for ( GList *layer = layers; layer != NULL; layer = layer->next ) {
    VikTrwLayer *vtl = VIK_TRW_LAYER(layer->data);
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
  }
  g_list_free ( layers );

  if ( have_bbox ) {
    vu_zoom_to_show_latlons ( vik_viewport_get_coord_mode(vvp), vvp, maxmin );
    vik_layers_panel_emit_update ( vlp, FALSE );
  }
}

static void tac_on_off_cb ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER(values[MA_VAL]);
  //tac_on_off ( val );
    val->on[BASIC] = !val->on[BASIC];
  if ( val->on[BASIC] ) {
    if ( !val->calculating )
      tac_calculate ( val );
  }
  else
    // Redraw to clear previous display
    vik_layer_emit_update ( VIK_LAYER(val), FALSE );

  vik_window_set_modified ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(val)) );
}

static void tac_increase_cb ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER(values[MA_VAL]);
  if ( val->zoom_level < 4097 )
    val->zoom_level = val->zoom_level * 2;
  if ( val->on[BASIC] )
    if ( !val->calculating )
      tac_calculate ( val );

  vik_window_set_modified ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(val)) );
}

static void tac_decrease_cb ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER(values[MA_VAL]);
  if ( val->zoom_level > 1.1 )
    val->zoom_level = val->zoom_level / 2;
  if ( val->on[BASIC] )
    if ( !val->calculating )
      tac_calculate ( val );

  vik_window_set_modified ( VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(val)) );
}

static void tac_goto_square_cb ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER(values[MA_VAL]);
  VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(val));
  VikViewport *vvp = vik_window_viewport ( vw );
  // NB Simply moving to position, keeping current zoom level
  // (i.e. not trying to determine best zoom level to view extents)
  MapCoord mc;
  mc.x = val->xx + val->max_square/2;
  mc.y = val->yy + val->max_square/2;
  mc.scale = map_utils_mpp_to_scale ( val->zoom_level );
  VikCoord vc;
  map_utils_iTMS_to_center_vikcoord ( &mc, &vc );
  vik_viewport_set_center_coord ( vvp, &vc, TRUE );
  vik_layer_emit_update ( VIK_LAYER(val), FALSE );
}

static void tac_goto_east_west_cb ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER(values[MA_VAL]);
  VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(val));
  VikViewport *vvp = vik_window_viewport ( vw );
  // NB Simply moving to position, keeping current zoom level
  // (i.e. not trying to determine best zoom level to view extents)
  MapCoord mc;
  mc.x = val->ew_x - val->ew_size/2;
  mc.y = val->ew_y;
  mc.scale = map_utils_mpp_to_scale ( val->zoom_level );
  VikCoord vc;
  map_utils_iTMS_to_center_vikcoord ( &mc, &vc );
  vik_viewport_set_center_coord ( vvp, &vc, TRUE );
  vik_layer_emit_update ( VIK_LAYER(val), FALSE );
}

static void tac_goto_north_south_cb ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER(values[MA_VAL]);
  VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(val));
  VikViewport *vvp = vik_window_viewport ( vw );
  // NB Simply moving to position, keeping current zoom level
  // (i.e. not trying to determine best zoom level to view extents)
  MapCoord mc;
  mc.x = val->ns_x;
  mc.y = val->ns_y - val->ns_size/2;;
  mc.scale = map_utils_mpp_to_scale ( val->zoom_level );
  VikCoord vc;
  map_utils_iTMS_to_center_vikcoord ( &mc, &vc );
  vik_viewport_set_center_coord ( vvp, &vc, TRUE );
  vik_layer_emit_update ( VIK_LAYER(val), FALSE );
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

// This shouldn't be called when already running
static void hm_calculate_cb ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER(values[MA_VAL]);
  if ( val->hm_calculating ) {
    return;
  }
  hm_calculate ( val );
}

static void hm_clear_cb ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER(values[MA_VAL]);
  hm_clear ( val );
  vik_layer_emit_update ( VIK_LAYER(val), FALSE );
}

/**
 * Returns the submenu, so the caller can append menuitems if desired
 */
static GtkMenu* aggregate_build_submenu_tac ( VikAggregateLayer *val, GtkMenu *menu, menu_array_values values )
{
  gboolean available = val->on[BASIC] && !val->calculating;
  GtkMenu *tac_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *itemt = vu_menu_add_item ( menu, _("_Tracks Area Coverage"), GTK_STOCK_EXECUTE, NULL, NULL );
  gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemt), GTK_WIDGET(tac_submenu) );
  GtkWidget *itemtoo = vu_menu_add_item ( tac_submenu, val->on[BASIC] ? _("_Off") : _("_On"), GTK_STOCK_EXECUTE, G_CALLBACK(tac_on_off_cb), values );
  gtk_widget_set_sensitive ( itemtoo, !val->calculating );

  GtkWidget *itemtac = vu_menu_add_item ( tac_submenu, _("_Calculate"), GTK_STOCK_REFRESH, G_CALLBACK(tac_calculate_cb), values );
  gtk_widget_set_sensitive ( itemtac, available );

  GtkWidget *itemti = vu_menu_add_item ( tac_submenu, _("_Increase Tile Area"), GTK_STOCK_GO_UP, G_CALLBACK(tac_increase_cb), values );
  gtk_widget_set_sensitive ( itemti, available );

  GtkWidget *itemtd = vu_menu_add_item ( tac_submenu, _("_Decrease Tile Area"), GTK_STOCK_GO_DOWN, G_CALLBACK(tac_decrease_cb), values );
  gtk_widget_set_sensitive ( itemtd, available );

  GtkMenu *goto_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *itemg = vu_menu_add_item ( tac_submenu, _("_Goto"), GTK_STOCK_JUMP_TO, NULL, NULL );
  gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemg), GTK_WIDGET(goto_submenu) );

  GtkWidget *itemgs = vu_menu_add_item ( goto_submenu, _("_Max Square"), GTK_STOCK_JUMP_TO, G_CALLBACK(tac_goto_square_cb), values );
  gtk_widget_set_sensitive ( itemgs, available && val->on[MAX_SQR] );

  GtkWidget *itemgwe = vu_menu_add_item ( goto_submenu, _("_East/West"), GTK_STOCK_JUMP_TO, G_CALLBACK(tac_goto_east_west_cb), values );
  gtk_widget_set_sensitive ( itemgwe, available && val->on[LINES] );

  GtkWidget *itemgns = vu_menu_add_item ( goto_submenu, _("_North/South"), GTK_STOCK_JUMP_TO, G_CALLBACK(tac_goto_north_south_cb), values );
  gtk_widget_set_sensitive ( itemgns, available && val->on[LINES] );

#ifdef HAVE_SQLITE3_H
  if ( val->on[BASIC] )
    if ( !val->calculating )
      (void)vu_menu_add_item ( tac_submenu, _("_Export as MBTiles"), GTK_STOCK_CONVERT, G_CALLBACK(tac_generate_mbtiles_cb), values );
#endif

  (void)vu_menu_add_item ( tac_submenu, NULL, NULL, NULL, NULL );

  GtkWidget *itemtclr = vu_menu_add_item ( tac_submenu, _("_Remove"), GTK_STOCK_DELETE, G_CALLBACK(tac_clear_cb), values );
  gtk_widget_set_sensitive ( itemtclr, available );

  return tac_submenu;
}

static void aggregate_build_submenu_hm ( VikAggregateLayer *val, GtkMenu *menu, menu_array_values values, VikViewport *vvp )
{
  // ATM heatmap only in this mode
  if ( vik_viewport_get_drawmode(vvp) == VIK_VIEWPORT_DRAWMODE_MERCATOR ) {
    gboolean hm_available = !val->hm_calculating;
    GtkMenu *hm_submenu = GTK_MENU(gtk_menu_new());
    GtkWidget *itemhm = vu_menu_add_item ( menu, _("Tracks Heat_map"), GTK_STOCK_EXECUTE, NULL, NULL );
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemhm), GTK_WIDGET(hm_submenu) );

    GtkWidget *itemhmc = vu_menu_add_item ( hm_submenu, _("_Calculate"), GTK_STOCK_REFRESH, G_CALLBACK(hm_calculate_cb), values );
    gtk_widget_set_sensitive ( itemhmc, hm_available );

    GtkWidget *itemhmlr = vu_menu_add_item ( hm_submenu, _("_Remove"), GTK_STOCK_DELETE, G_CALLBACK(hm_clear_cb), values );
    gtk_widget_set_sensitive ( itemhmlr, (val->hm_pixbuf != NULL) );
  }
}

/**
 * Find tracks from this list of tracks that go through this MapCoord tile
 */
GList *build_tac_track_list ( VikTrwLayer *vtl, GList *tracks, MapCoord *mc )
{
  GList *tracks_and_layers = NULL;
  // build tracks_and_layers list
  while ( tracks ) {
    VikTrack *trk = VIK_TRACK(tracks->data);

    VikCoord tl, br;
    // Get the tile bounds
    map_utils_iTMS_to_vikcoords ( mc, &tl, &br );
    LatLonBBox bbox;
    bbox.north = tl.north_south;
    bbox.east  = br.east_west;
    bbox.south = br.north_south;
    bbox.west  = tl.east_west;

    // First a quick check to see if the track bounds covers this tile
    if ( BBOX_INTERSECT ( bbox, trk->bbox ) ) {
      // Now check each point to see if actually in the tile bounds
      GList *iter = trk->trackpoints;
      while ( iter ) {
        if ( !isnan(VIK_TRACKPOINT(iter->data)->timestamp) ) {
          if ( vik_coord_inside ( &(VIK_TRACKPOINT(iter->data)->coord), &tl, &br ) ) {
            vik_trw_and_track_t *vtdl = g_malloc(sizeof(vik_trw_and_track_t));
            vtdl->trk = trk;
            vtdl->vtl = vtl;
            tracks_and_layers = g_list_prepend ( tracks_and_layers, vtdl );
            break;
          }
        }
        iter = iter->next;
      }
    }
    tracks = g_list_next ( tracks );
  }
  return tracks_and_layers;
}

/**
 * aggregate_layer_tac_tracks_list:
 * @vl:        The layer that should create the track and layers list
 * @user_data: The tile in #MapCoord
 *
 * Returns: A list of #vik_trw_and_track_t
 */
static GList* aggregate_layer_tac_tracks_list ( VikLayer *vl, gpointer user_data )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER(vl);
  MapCoord *mc = (MapCoord*)user_data;

  // Get all TRW layers
  GList *layers = NULL;
  layers = vik_aggregate_layer_get_all_layers_of_type ( val, layers, VIK_LAYER_TRW, TRUE );

  // For each TRW layers keep adding the tracks the match our criteria
  GList *tracks_and_layers = NULL;
  for ( GList *layer = layers; layer != NULL; layer = layer->next ) {
    GList *tracks = g_hash_table_get_values ( vik_trw_layer_get_tracks( VIK_TRW_LAYER(layer->data) ) );
    tracks_and_layers = g_list_concat ( tracks_and_layers, build_tac_track_list(VIK_TRW_LAYER(layer->data), tracks, mc) );
    g_list_free ( tracks );
  }
  g_list_free ( layers );

  return tracks_and_layers;
}

static void tac_track_list_cb ( menu_array_values values )
{
  VikAggregateLayer *val = VIK_AGGREGATE_LAYER ( values[MA_VAL] );
  gchar *title = g_strdup_printf ( _("%s: Tracks in tile %d %d, %d"), VIK_LAYER(val)->name, (17-val->rc_menu_mc.scale), val->rc_menu_mc.x, val->rc_menu_mc.y );
  vik_trw_layer_track_list_show_dialog ( title, VIK_LAYER(val), &val->rc_menu_mc, aggregate_layer_tac_tracks_list, TRUE );
  g_free ( title );
}

static gboolean aggregate_layer_selected_viewport_menu ( VikAggregateLayer *val, GdkEventButton *event, VikViewport *vvp )
{
  static menu_array_values values;
  values[MA_VAL] = val;
  values[MA_VLP] = NULL;

  if ( event && event->button == 3 ) {
    VikCoord coord;
    vik_viewport_screen_to_coord ( vvp, MAX(0, event->x), MAX(0, event->y), &coord );

    GtkMenu *menu = GTK_MENU(gtk_menu_new ());
    GtkWidget *name = vu_menu_add_item ( menu, VIK_LAYER(val)->name, NULL, NULL, NULL ); // Say which layer this is
    gtk_widget_set_sensitive ( name, FALSE ); // Prevent useless clicking on the name
    (void)vu_menu_add_item ( menu, NULL, NULL, NULL, NULL ); // Just a separator

    gboolean available = val->on[BASIC] && !val->calculating;
    GtkMenu *sm = aggregate_build_submenu_tac ( val, menu, values );

    if ( map_utils_vikcoord_to_iTMS(&coord, val->zoom_level, val->zoom_level, &val->rc_menu_mc) ) {
      GtkWidget *itemtt = vu_menu_add_item ( sm, _("_Tracks in this Tile"), GTK_STOCK_INFO, G_CALLBACK(tac_track_list_cb), values );
      available = available && is_tile_occupied ( val->tiles, val->rc_menu_mc.x, val->rc_menu_mc.y );
      gtk_widget_set_sensitive ( itemtt, available );
    }

    aggregate_build_submenu_hm ( val, menu, values, vvp );

    gtk_widget_show_all ( GTK_WIDGET(menu) );
    // Unclear why using '0' is more reliable for activating submenu items than using 'event->button'!
    // Possibly https://bugzilla.gnome.org/show_bug.cgi?id=695488
    gtk_menu_popup ( menu, NULL, NULL, NULL, NULL, 0, event->time );
  }

  return FALSE;
}

static void aggregate_layer_add_menu_items ( VikAggregateLayer *val, GtkMenu *menu, gpointer vlp )
{
  // Data to pass on in menu functions
  static menu_array_values values;
  values[MA_VAL] = val;
  values[MA_VLP] = vlp;

  (void)vu_menu_add_item ( menu, NULL, NULL, NULL, NULL ); // Just a separator

  GtkMenu *vis_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *itemv = vu_menu_add_item ( menu, _("_Visibility"), VIK_ICON_CHECKBOX, NULL, NULL );
  gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemv), GTK_WIDGET(vis_submenu) );

  (void)vu_menu_add_item ( vis_submenu, _("_Show All"), GTK_STOCK_APPLY, G_CALLBACK(aggregate_layer_child_visible_on), values );
  (void)vu_menu_add_item ( vis_submenu, _("_Hide All"), GTK_STOCK_CLEAR, G_CALLBACK(aggregate_layer_child_visible_off), values );
  (void)vu_menu_add_item ( vis_submenu, _("_Toggle"), GTK_STOCK_REFRESH, G_CALLBACK(aggregate_layer_child_visible_toggle), values );

  GtkMenu *view_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *itemvw = vu_menu_add_item ( menu, _("V_iew"), GTK_STOCK_JUMP_TO, NULL, NULL );
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

  (void)vu_menu_add_item ( menu, _("_Statistics"), GTK_STOCK_INFO, G_CALLBACK(aggregate_layer_analyse), values );
  (void)vu_menu_add_item ( menu, _("Track _List..."), GTK_STOCK_INDEX, G_CALLBACK(aggregate_layer_track_list_dialog), values );
  (void)vu_menu_add_item ( menu, _("_Waypoint List..."), GTK_STOCK_INDEX, G_CALLBACK(aggregate_layer_waypoint_list_dialog), values );

  GtkMenu *search_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *itemsr = vu_menu_add_item ( menu, _("Searc_h"), GTK_STOCK_FIND, NULL, NULL );
  gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemsr), GTK_WIDGET(search_submenu) );

  GtkWidget *itemd = vu_menu_add_item ( search_submenu, _("By _Date..."), NULL, G_CALLBACK(aggregate_layer_search_date), values );
  gtk_widget_set_tooltip_text ( itemd, _("Find the first item with a specified date") );

  GtkMenu *file_submenu = GTK_MENU(gtk_menu_new());
  GtkWidget *itemsf = vu_menu_add_item ( menu, _("_File"), GTK_STOCK_FILE, NULL, NULL );
  gtk_menu_item_set_submenu ( GTK_MENU_ITEM(itemsf), GTK_WIDGET(file_submenu) );
  (void)vu_menu_add_item ( file_submenu, _("Load E_xternal Layers"), GTK_STOCK_EXECUTE, G_CALLBACK(aggregate_layer_load_external_layers_click), values );
  (void)vu_menu_add_item ( file_submenu, _("_Open GPX as External Layer..."), GTK_STOCK_OPEN, G_CALLBACK(aggregate_layer_load_external_file_cb), values );

  (void)vu_menu_add_item ( file_submenu, _("Save _Layer As..."), GTK_STOCK_SAVE, G_CALLBACK(aggregate_layer_save_layer_as_cb), values );
  (void)vu_menu_add_item ( file_submenu, _("_Append File..."), GTK_STOCK_ADD, G_CALLBACK(aggregate_layer_file_load), values );
  (void)vu_menu_add_item ( file_submenu, _("_Export as GPX..."), GTK_STOCK_CONVERT, G_CALLBACK(aggregate_layer_export_gpx), values );

  (void)aggregate_build_submenu_tac ( val, menu, values );

  aggregate_build_submenu_hm ( val, menu, values, vik_window_viewport(VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(val))) );
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
  for ( guint ii=0; ii<CP_NUM; ii++ ) {
    if ( val->pixbuf[ii] )
      g_object_unref ( val->pixbuf[ii] );
    if ( val->full_pixbuf[ii] )
      g_object_unref ( val->full_pixbuf[ii] );
  }
  if ( val->unreachable_pixbuf )
    g_object_unref ( val->unreachable_pixbuf );
  g_hash_table_destroy ( val->tiles_clust );
  g_hash_table_destroy ( val->tiles_new );
  g_hash_table_destroy ( val->prev );

  if ( val->hm_pixbuf )
    g_object_unref ( val->hm_pixbuf );
  if ( val->hm_pbf_scaled )
    g_object_unref ( val->hm_pbf_scaled );
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

  if ( vl->realized )
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
      g_string_append_printf ( gs, _("\nTAC: Area Level %s\nTotal tiles %d"),
                               params_tile_area_levels[map_utils_mpp_to_scale(val->zoom_level)], val->num_tiles[BASIC] );
      // Changes only shown after initial calculation
      //  mainly for the idea to prevent showing that everything has initially changed (e.g. on file load)
      if ( (val->num_calcs > 1) && val->num_tiles[TNEW] )
        g_string_append_printf ( gs, _(" (%+d)"), val->num_tiles[TNEW] );

      g_string_append_printf ( gs, _("\nMax Square %d"), val->max_square );
      if ( (val->num_calcs > 1) && (val->max_square != val->max_square_prev) )
        g_string_append_printf ( gs, " (%+d)", (val->max_square-val->max_square_prev) );

      g_string_append_printf ( gs, _("\nContiguous count %d"), val->num_tiles[CONTIG] );
      if ( (val->num_calcs > 1) && (val->num_tiles[CONTIG] != val->num_prev[CONTIG]) )
        g_string_append_printf ( gs, " (%+d)", (val->num_tiles[CONTIG]-val->num_prev[CONTIG]) );

      g_string_append_printf ( gs, _("\nCluster size %d"), val->num_tiles[CLUSTER] );
      if ( (val->num_calcs > 1) && (val->num_tiles[CLUSTER] != val->num_prev[CLUSTER]) )
        g_string_append_printf ( gs, " (%+d)", (val->num_tiles[CLUSTER]-val->num_prev[CLUSTER]) );

      g_string_append_printf ( gs, _("\nConsecutive north/south %d"), val->ns_size );
      if ( (val->num_calcs > 1) && (val->ns_size != val->ns_size_prev) )
        g_string_append_printf ( gs, " (%+d)", (val->ns_size-val->ns_size_prev) );

      g_string_append_printf ( gs, _("\nConsecutive east/west %d"), val->ew_size );
      if ( (val->num_calcs > 1) && (val->ew_size != val->ew_size_prev) )
        g_string_append_printf ( gs, " (%+d)", (val->ew_size-val->ew_size_prev) );
    }
  }
  g_snprintf ( tmp_buf, sizeof(tmp_buf), "%s", gs->str );
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

/**
 * vik_aggregate_layer_export_gpx_setup:
 *
 * Export all visible VikTrwLayers in this aggregate into a GPX file
 * This checks there is something to save before calling other functions to do the work
 */
void vik_aggregate_layer_export_gpx_setup ( VikAggregateLayer *val )
{
  VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(val));

  GList *layers = NULL;
  layers = vik_aggregate_layer_get_all_layers_of_type ( val, layers, VIK_LAYER_TRW, FALSE );
  if ( !layers ) {
    a_dialog_info_msg ( GTK_WINDOW(vw), _("Nothing to Export!") );
    return;
  }
  g_list_free ( layers );

  // This export function mostly gets the file to save
  //  and will call the vik_aggregate_layer_export_gpx_main() to do the actual conversion work
  gchar *auto_save_name = append_file_ext ( VIK_LAYER(val)->name, FILE_TYPE_GPX );
  vik_trw_layer_export ( VIK_LAYER(val), _("Export to GPX"), auto_save_name, NULL, FILE_TYPE_GPX );
  g_free ( auto_save_name );
}

/**
 * vik_aggregate_layer_export_gpx_main:
 *
 * Exports all visible VikTrwLayers in this aggregate into a GPX file
 */
gboolean vik_aggregate_layer_export_gpx_main ( VikAggregateLayer *val, const gchar *filename )
{
  gboolean ans = TRUE;

  FILE *ff = g_fopen ( filename, "w" );
  if ( ff ) {
    GList *vtt = aggregate_layer_track_create_list ( VIK_LAYER(val), GINT_TO_POINTER(1) );
    GList *vtwl = aggregate_layer_waypoint_create_list ( VIK_LAYER(val), GINT_TO_POINTER(1) );

    // Scan list(s) of vtls to find latest version
    gpx_version_t vers = GPX_V1_0; // default
    for ( GList *iter = vtt; iter != NULL; iter = iter->next ) {
      VikTrwLayer *vtl = ((vik_trw_and_track_t*)iter->data)->vtl;
      if ( vik_trw_layer_get_gpx_version(vtl) == GPX_V1_1 ) {
        vers = GPX_V1_1;
        break;
      }
    }
    if ( vers == GPX_V1_0 ) {
      for ( GList *iter = vtwl; iter != NULL; iter = iter->next ) {
        VikTrwLayer *vtl = ((vik_trw_waypoint_list_t*)iter->data)->vtl;
        if ( vik_trw_layer_get_gpx_version(vtl) == GPX_V1_1 ) {
          vers = GPX_V1_1;
          break;
        }
      }
    }

    GpxWritingOptions options = { FALSE, FALSE, FALSE, FALSE, vers };
    a_gpx_write_combined_file ( VIK_LAYER(val)->name, vtt, vtwl, ff, &options, NULL );
    fclose ( ff );
    g_list_free ( vtt );
    g_list_free ( vtwl );
  }
  else
    ans = FALSE;

  return ans;
}
