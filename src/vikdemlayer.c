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

#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <stdlib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "background.h"
#include "viking.h"
#include "vikmapslayer.h"
#include "vikdemlayer.h"
#include "dem.h"
#include "dems.h"
#include "icons/icons.h"

#define MAPS_CACHE_DIR maps_layer_default_dir()

#define SRTM_CACHE_TEMPLATE "%ssrtm3-%s%s%c%02d%c%03d.hgt.zip"
#define SRTM_HTTP_SITE "dds.cr.usgs.gov"
#define SRTM_HTTP_URI  "/srtm/version2_1/SRTM3/"

#ifdef VIK_CONFIG_DEM24K
#define DEM24K_DOWNLOAD_SCRIPT "dem24k.pl"
#endif

#define UNUSED_LINE_THICKNESS 3

static VikDEMLayer *dem_layer_new ( VikViewport *vvp );
static void dem_layer_draw ( VikDEMLayer *vdl, VikViewport *vp );
static void dem_layer_free ( VikDEMLayer *vdl );
static VikDEMLayer *dem_layer_create ( VikViewport *vp );
static const gchar* dem_layer_tooltip( VikDEMLayer *vdl );
static void dem_layer_marshall( VikDEMLayer *vdl, guint8 **data, gint *len );
static VikDEMLayer *dem_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp );
static gboolean dem_layer_set_param ( VikDEMLayer *vdl, guint16 id, VikLayerParamData data, VikViewport *vp, gboolean is_file_operation );
static VikLayerParamData dem_layer_get_param ( VikDEMLayer *vdl, guint16 id, gboolean is_file_operation );
static void dem_layer_post_read ( VikLayer *vl, VikViewport *vp, gboolean from_file );
static void srtm_draw_existence ( VikViewport *vp );

#ifdef VIK_CONFIG_DEM24K
static void dem24k_draw_existence ( VikViewport *vp );
#endif

/* Upped upper limit incase units are feet */
static VikLayerParamScale param_scales[] = {
  { 0, 30000, 10, 1 },
  { 1, 30000, 10, 1 },
};

static gchar *params_source[] = {
	"SRTM Global 90m (3 arcsec)",
#ifdef VIK_CONFIG_DEM24K
	"USA 10m (USGS 24k)",
#endif
	NULL
	};

static gchar *params_type[] = {
	N_("Absolute height"),
	N_("Height gradient"),
	NULL
};

enum { DEM_SOURCE_SRTM,
#ifdef VIK_CONFIG_DEM24K
       DEM_SOURCE_DEM24K,
#endif
     };

enum { DEM_TYPE_HEIGHT = 0,
       DEM_TYPE_GRADIENT,
       DEM_TYPE_NONE,
};

static VikLayerParamData color_default ( void ) {
  VikLayerParamData data; gdk_color_parse ( "blue", &data.c ); return data;
}

static VikLayerParamData source_default ( void ) { return VIK_LPD_UINT ( DEM_SOURCE_SRTM ); }
static VikLayerParamData type_default ( void ) { return VIK_LPD_UINT ( DEM_TYPE_HEIGHT ); }
static VikLayerParamData min_elev_default ( void ) { return VIK_LPD_DOUBLE ( 0.0 ); }
static VikLayerParamData max_elev_default ( void ) { return VIK_LPD_DOUBLE ( 1000.0 ); }

static VikLayerParam dem_layer_params[] = {
  { VIK_LAYER_DEM, "files", VIK_LAYER_PARAM_STRING_LIST, VIK_LAYER_GROUP_NONE, N_("DEM Files:"), VIK_LAYER_WIDGET_FILELIST, NULL, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_DEM, "source", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Download Source:"), VIK_LAYER_WIDGET_RADIOGROUP_STATIC, params_source, NULL, NULL, source_default, NULL, NULL },
  { VIK_LAYER_DEM, "color", VIK_LAYER_PARAM_COLOR, VIK_LAYER_GROUP_NONE, N_("Min Elev Color:"), VIK_LAYER_WIDGET_COLOR, NULL, NULL, NULL, color_default, NULL, NULL },
  { VIK_LAYER_DEM, "type", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Type:"), VIK_LAYER_WIDGET_RADIOGROUP_STATIC, params_type, NULL, NULL, type_default, NULL, NULL },
  { VIK_LAYER_DEM, "min_elev", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_GROUP_NONE, N_("Min Elev:"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 0, NULL, NULL, min_elev_default, NULL, NULL },
  { VIK_LAYER_DEM, "max_elev", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_GROUP_NONE, N_("Max Elev:"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 0, NULL, NULL, max_elev_default, NULL, NULL },
};


enum { PARAM_FILES=0, PARAM_SOURCE, PARAM_COLOR, PARAM_TYPE, PARAM_MIN_ELEV, PARAM_MAX_ELEV, NUM_PARAMS };

static gpointer dem_layer_download_create ( VikWindow *vw, VikViewport *vvp);
static gboolean dem_layer_download_release ( VikDEMLayer *vdl, GdkEventButton *event, VikViewport *vvp );
static gboolean dem_layer_download_click ( VikDEMLayer *vdl, GdkEventButton *event, VikViewport *vvp );

static VikToolInterface dem_tools[] = {
  { { "DEMDownload", "vik-icon-DEM Download", N_("_DEM Download"), NULL, N_("DEM Download"), 0 },
    (VikToolConstructorFunc) dem_layer_download_create, NULL, NULL, NULL,
    (VikToolMouseFunc) dem_layer_download_click, NULL,  (VikToolMouseFunc) dem_layer_download_release,
    (VikToolKeyFunc) NULL,
    FALSE,
    GDK_CURSOR_IS_PIXMAP, &cursor_demdl_pixbuf },
};


/* HEIGHT COLORS
   The first entry is blue for a default 'sea' colour,
   however the value used by the corresponding gc can be configured as part of the DEM layer properties.
   The other colours, shaded from brown to white are used to give an indication of height.
*/
static gchar *dem_height_colors[] = {
"#0000FF",
"#9b793c", "#9c7d40", "#9d8144", "#9e8549", "#9f894d", "#a08d51", "#a29156", "#a3955a", "#a4995e", "#a69d63",
"#a89f65", "#aaa267", "#ada569", "#afa76b", "#b1aa6d", "#b4ad6f", "#b6b071", "#b9b373", "#bcb676", "#beb978",
"#c0bc7a", "#c2c07d", "#c4c37f", "#c6c681", "#c8ca84", "#cacd86", "#ccd188", "#cfd58b", "#c2ce84", "#b5c87e",
"#a9c278", "#9cbb71", "#8fb56b", "#83af65", "#76a95e", "#6aa358", "#5e9d52", "#63a055", "#69a458", "#6fa85c",
"#74ac5f", "#7ab063", "#80b467", "#86b86a", "#8cbc6e", "#92c072", "#94c175", "#97c278", "#9ac47c", "#9cc57f",
"#9fc682", "#a2c886", "#a4c989", "#a7cb8d", "#aacd91", "#afce99", "#b5d0a1", "#bbd2aa", "#c0d3b2", "#c6d5ba",
"#ccd7c3", "#d1d9cb", "#d7dbd4", "#DDDDDD", "#e0e0e0", "#e4e4e4", "#e8e8e8", "#ebebeb", "#efefef", "#f3f3f3",
"#f7f7f7", "#fbfbfb", "#ffffff"
};

static const guint DEM_N_HEIGHT_COLORS = sizeof(dem_height_colors)/sizeof(dem_height_colors[0]);

/*
"#9b793c", "#9e8549", "#a29156", "#a69d63", "#ada569", "#b4ad6f", "#bcb676", "#c2c07d", "#c8ca84", "#cfd58b",
"#a9c278", "#83af65", "#5e9d52", "#6fa85c", "#80b467", "#92c072", "#9ac47c", "#a2c886", "#aacd91", "#bbd2aa",
"#ccd7c3", "#DDDDDD", "#e8e8e8", "#f3f3f3", "#FFFFFF"
};
*/

static gchar *dem_gradient_colors[] = {
"#AAAAAA",
"#000000", "#000011", "#000022", "#000033", "#000044", "#00004c", "#000055", "#00005d", "#000066", "#00006e",
"#000077", "#00007f", "#000088", "#000090", "#000099", "#0000a1", "#0000aa", "#0000b2", "#0000bb", "#0000c3",
"#0000cc", "#0000d4", "#0000dd", "#0000e5", "#0000ee", "#0000f6", "#0000ff", "#0008f7", "#0011ee", "#0019e6",
"#0022dd", "#002ad5", "#0033cc", "#003bc4", "#0044bb", "#004cb3", "#0055aa", "#005da2", "#006699", "#006e91",
"#007788", "#007f80", "#008877", "#00906f", "#009966", "#00a15e", "#00aa55", "#00b24d", "#00bb44", "#00c33c",
"#00cc33", "#00d42b", "#00dd22", "#00e51a", "#00ee11", "#00f609", "#00ff00", "#08f700", "#11ee00", "#19e600",
"#22dd00", "#2ad500", "#33cc00", "#3bc400", "#44bb00", "#4cb300", "#55aa00", "#5da200", "#669900", "#6e9100",
"#778800", "#7f8000", "#887700", "#906f00", "#996600", "#a15e00", "#aa5500", "#b24d00", "#bb4400", "#c33c00",
"#cc3300", "#d42b00", "#dd2200", "#e51a00", "#ee1100", "#f60900", "#ff0000",
"#FFFFFF"
};

static const guint DEM_N_GRADIENT_COLORS = sizeof(dem_gradient_colors)/sizeof(dem_gradient_colors[0]);


VikLayerInterface vik_dem_layer_interface = {
  "DEM",
  N_("DEM"),
  "<control><shift>D",
  &vikdemlayer_pixbuf,

  dem_tools,
  sizeof(dem_tools) / sizeof(dem_tools[0]),

  dem_layer_params,
  NUM_PARAMS,
  NULL,
  0,

  VIK_MENU_ITEM_ALL,

  (VikLayerFuncCreate)                  dem_layer_create,
  (VikLayerFuncRealize)                 NULL,
  (VikLayerFuncPostRead)                dem_layer_post_read,
  (VikLayerFuncFree)                    dem_layer_free,

  (VikLayerFuncProperties)              NULL,
  (VikLayerFuncDraw)                    dem_layer_draw,
  (VikLayerFuncChangeCoordMode)         NULL,

  (VikLayerFuncSetMenuItemsSelection)   NULL,
  (VikLayerFuncGetMenuItemsSelection)   NULL,

  (VikLayerFuncAddMenuItems)            NULL,
  (VikLayerFuncSublayerAddMenuItems)    NULL,

  (VikLayerFuncSublayerRenameRequest)   NULL,
  (VikLayerFuncSublayerToggleVisible)   NULL,
  (VikLayerFuncSublayerTooltip)         NULL,
  (VikLayerFuncLayerTooltip)            dem_layer_tooltip,
  (VikLayerFuncLayerSelected)           NULL,

  (VikLayerFuncMarshall)		dem_layer_marshall,
  (VikLayerFuncUnmarshall)		dem_layer_unmarshall,

  (VikLayerFuncSetParam)                dem_layer_set_param,
  (VikLayerFuncGetParam)                dem_layer_get_param,

  (VikLayerFuncReadFileData)            NULL,
  (VikLayerFuncWriteFileData)           NULL,

  (VikLayerFuncDeleteItem)              NULL,
  (VikLayerFuncCutItem)                 NULL,
  (VikLayerFuncCopyItem)                NULL,
  (VikLayerFuncPasteItem)               NULL,
  (VikLayerFuncFreeCopiedItem)          NULL,
  (VikLayerFuncDragDropRequest)		NULL,

  (VikLayerFuncSelectClick)             NULL,
  (VikLayerFuncSelectMove)              NULL,
  (VikLayerFuncSelectRelease)           NULL,
  (VikLayerFuncSelectedViewportMenu)    NULL,
};

struct _VikDEMLayer {
  VikLayer vl;
  GdkGC **gcs;
  GdkGC **gcsgradient;
  GList *files;
  gdouble min_elev;
  gdouble max_elev;
  GdkColor color;
  guint source;
  guint type;

  // right click menu only stuff - similar to mapslayer
  GtkMenu *right_click_menu;
};

GType vik_dem_layer_get_type ()
{
  static GType vdl_type = 0;

  if (!vdl_type)
  {
    static const GTypeInfo vdl_info =
    {
      sizeof (VikDEMLayerClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      NULL, /* class init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikDEMLayer),
      0,
      NULL /* instance init */
    };
    vdl_type = g_type_register_static ( VIK_LAYER_TYPE, "VikDEMLayer", &vdl_info, 0 );
  }

  return vdl_type;
}

static const gchar* dem_layer_tooltip( VikDEMLayer *vdl )
{
  static gchar tmp_buf[100];
  g_snprintf (tmp_buf, sizeof(tmp_buf), _("Number of files: %d"), g_list_length (vdl->files));
  return tmp_buf;
}

static void dem_layer_marshall( VikDEMLayer *vdl, guint8 **data, gint *len )
{
  vik_layer_marshall_params ( VIK_LAYER(vdl), data, len );
}

static VikDEMLayer *dem_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp )
{
  VikDEMLayer *rv = dem_layer_new ( vvp );
  gint i;

  /* TODO: share GCS between layers */
  for ( i = 0; i < DEM_N_HEIGHT_COLORS; i++ ) {
    if ( i == 0 )
      rv->gcs[i] = vik_viewport_new_gc_from_color ( vvp, &(rv->color), UNUSED_LINE_THICKNESS );
    else
      rv->gcs[i] = vik_viewport_new_gc ( vvp, dem_height_colors[i], UNUSED_LINE_THICKNESS );
  }
  for ( i = 0; i < DEM_N_GRADIENT_COLORS; i++ )
    rv->gcsgradient[i] = vik_viewport_new_gc ( vvp, dem_gradient_colors[i], UNUSED_LINE_THICKNESS );

  vik_layer_unmarshall_params ( VIK_LAYER(rv), data, len, vvp );
  return rv;
}

/* Structure for DEM data used in background thread */
typedef struct {
  VikDEMLayer *vdl;
} dem_load_thread_data;

/*
 * Function for starting the DEM file loading as a background thread
 */
static int dem_layer_load_list_thread ( dem_load_thread_data *dltd, gpointer threaddata )
{
  int result = 0; // Default to good
  // Actual Load
  if ( a_dems_load_list ( &(dltd->vdl->files), threaddata ) ) {
    // Thread cancelled
    result = -1;
  }

  // ATM as each file is processed the screen is not updated (no mechanism exposed to a_dems_load_list)
  // Thus force draw only at the end, as loading is complete/aborted
  //gdk_threads_enter();
  // Test is helpful to prevent Gtk-CRITICAL warnings if the program is exitted whilst loading
  if ( IS_VIK_LAYER(dltd->vdl) )
    vik_layer_emit_update ( VIK_LAYER(dltd->vdl) ); // NB update from background thread
  //gdk_threads_leave();

  return result;
}

static void dem_layer_thread_data_free ( dem_load_thread_data *data )
{
  // Simple release
  g_free ( data );
}

static void dem_layer_thread_cancel ( dem_load_thread_data *data )
{
  // Abort loading
  // Instead of freeing the list, leave it as partially processed
  // Thus we can see/use what was done
}

/**
 * Process the list of DEM files and convert each one to a relative path
 */
static GList *dem_layer_convert_to_relative_filenaming ( GList *files )
{
  gchar *cwd = g_get_current_dir();
  if ( !cwd )
    return files;

  GList *relfiles = NULL;

  while ( files ) {
    gchar *file = g_strdup ( file_GetRelativeFilename ( cwd, files->data ) );
    relfiles = g_list_prepend ( relfiles, file );
    files = files->next;
  }

  g_free ( cwd );

  if ( relfiles ) {
    // Replacing current list, so delete old values first.
    GList *iter = files;
    while ( iter ) {
      g_free ( iter->data );
      iter = iter->next;
    }
    g_list_free ( files );

    return relfiles;
  }

  return files;
}

gboolean dem_layer_set_param ( VikDEMLayer *vdl, guint16 id, VikLayerParamData data, VikViewport *vp, gboolean is_file_operation )
{
  switch ( id )
  {
    case PARAM_COLOR: vdl->color = data.c; gdk_gc_set_rgb_fg_color ( vdl->gcs[0], &(vdl->color) ); break;
    case PARAM_SOURCE: vdl->source = data.u; break;
    case PARAM_TYPE: vdl->type = data.u; break;
    case PARAM_MIN_ELEV:
      /* Convert to store internally
         NB file operation always in internal units (metres) */
      if (!is_file_operation && a_vik_get_units_height () == VIK_UNITS_HEIGHT_FEET )
        vdl->min_elev = VIK_FEET_TO_METERS(data.d);
      else
        vdl->min_elev = data.d;
      break;
    case PARAM_MAX_ELEV:
      /* Convert to store internally
         NB file operation always in internal units (metres) */
      if (!is_file_operation && a_vik_get_units_height () == VIK_UNITS_HEIGHT_FEET )
        vdl->max_elev = VIK_FEET_TO_METERS(data.d);
      else
        vdl->max_elev = data.d;
      break;
    case PARAM_FILES:
    {
      // Clear out old settings - if any commonalities with new settings they will have to be read again
      a_dems_list_free ( vdl->files );
      // Set file list so any other intermediate screen drawing updates will show currently loaded DEMs by the working thread
      vdl->files = data.sl;
      // No need for thread if no files
      if ( vdl->files ) {
        // Thread Load
        dem_load_thread_data *dltd = g_malloc ( sizeof(dem_load_thread_data) );
        dltd->vdl = vdl;
        dltd->vdl->files = data.sl;

        a_background_thread ( VIK_GTK_WINDOW_FROM_WIDGET(vp),
                              _("DEM Loading"),
                              (vik_thr_func) dem_layer_load_list_thread,
                              dltd,
                              (vik_thr_free_func) dem_layer_thread_data_free,
                              (vik_thr_free_func) dem_layer_thread_cancel,
                              g_list_length ( data.sl ) ); // Number of DEM files
      }
      break;
    }
  }
  return TRUE;
}

static VikLayerParamData dem_layer_get_param ( VikDEMLayer *vdl, guint16 id, gboolean is_file_operation )
{
  VikLayerParamData rv;
  switch ( id )
  {
    case PARAM_FILES:
      rv.sl = vdl->files;
      if ( is_file_operation )
        // Save in relative format if necessary
        if ( a_vik_get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE )
          rv.sl = dem_layer_convert_to_relative_filenaming ( rv.sl );
      break;
    case PARAM_SOURCE: rv.u = vdl->source; break;
    case PARAM_TYPE: rv.u = vdl->type; break;
    case PARAM_COLOR: rv.c = vdl->color; break;
    case PARAM_MIN_ELEV:
      /* Convert for display in desired units
         NB file operation always in internal units (metres) */
      if (!is_file_operation && a_vik_get_units_height () == VIK_UNITS_HEIGHT_FEET )
        rv.d = VIK_METERS_TO_FEET(vdl->min_elev);
      else
        rv.d = vdl->min_elev;
      break;
    case PARAM_MAX_ELEV:
      /* Convert for display in desired units
         NB file operation always in internal units (metres) */
      if (!is_file_operation && a_vik_get_units_height () == VIK_UNITS_HEIGHT_FEET )
        rv.d = VIK_METERS_TO_FEET(vdl->max_elev);
      else
        rv.d = vdl->max_elev;
      break;
  }
  return rv;
}

static void dem_layer_post_read ( VikLayer *vl, VikViewport *vp, gboolean from_file )
{
  /* nothing ATM, but keep in case it's needed the future */
}

static VikDEMLayer *dem_layer_new ( VikViewport *vvp )
{
  VikDEMLayer *vdl = VIK_DEM_LAYER ( g_object_new ( VIK_DEM_LAYER_TYPE, NULL ) );

  vik_layer_set_type ( VIK_LAYER(vdl), VIK_LAYER_DEM );

  vdl->files = NULL;

  vdl->gcs = g_malloc(sizeof(GdkGC *)*DEM_N_HEIGHT_COLORS);
  vdl->gcsgradient = g_malloc(sizeof(GdkGC *)*DEM_N_GRADIENT_COLORS);
  /* make new gcs only if we need it (copy layer -> use old) */

  // Ensure the base GC is available so the default colour can be applied
  if ( vvp ) vdl->gcs[0] = vik_viewport_new_gc ( vvp, "#0000FF", 1 );

  vik_layer_set_defaults ( VIK_LAYER(vdl), vvp );

  return vdl;
}


static inline guint16 get_height_difference(gint16 elev, gint16 new_elev)
{
  if(new_elev == VIK_DEM_INVALID_ELEVATION)
    return 0;
  else
    return abs(new_elev - elev);
}


static void vik_dem_layer_draw_dem ( VikDEMLayer *vdl, VikViewport *vp, VikDEM *dem )
{
  VikDEMColumn *column, *prevcolumn, *nextcolumn;

  struct LatLon dem_northeast, dem_southwest;
  gdouble max_lat, max_lon, min_lat, min_lon;

  /**** Check if viewport and DEM data overlap ****/

  /* get min, max lat/lon of viewport */
  vik_viewport_get_min_max_lat_lon ( vp, &min_lat, &max_lat, &min_lon, &max_lon );

  /* get min, max lat/lon of DEM data */
  if ( dem->horiz_units == VIK_DEM_HORIZ_LL_ARCSECONDS ) {
    dem_northeast.lat = dem->max_north / 3600.0;
    dem_northeast.lon = dem->max_east / 3600.0;
    dem_southwest.lat = dem->min_north / 3600.0;
    dem_southwest.lon = dem->min_east / 3600.0;
  } else if ( dem->horiz_units == VIK_DEM_HORIZ_UTM_METERS ) {
    struct UTM dem_northeast_utm, dem_southwest_utm;
    dem_northeast_utm.northing = dem->max_north;
    dem_northeast_utm.easting = dem->max_east;
    dem_southwest_utm.northing = dem->min_north;
    dem_southwest_utm.easting = dem->min_east;
    dem_northeast_utm.zone = dem_southwest_utm.zone = dem->utm_zone;
    dem_northeast_utm.letter = dem_southwest_utm.letter = dem->utm_letter;

    a_coords_utm_to_latlon(&dem_northeast_utm, &dem_northeast);
    a_coords_utm_to_latlon(&dem_southwest_utm, &dem_southwest);
  }

  if ( (max_lat > dem_northeast.lat && min_lat > dem_northeast.lat) ||
       (max_lat < dem_southwest.lat && min_lat < dem_southwest.lat) )
    return;
  else if ( (max_lon > dem_northeast.lon && min_lon > dem_northeast.lon) ||
            (max_lon < dem_southwest.lon && min_lon < dem_southwest.lon) )
    return;
  /* else they overlap */

  /**** End Overlap Check ****/
  /* boxes to show where we have DEM instead of actually drawing the DEM.
   * useful if we want to see what areas we have coverage for (if we want
   * to get elevation data for a track) but don't want to cover the map.
   */

  #if 0
  /* draw a box if a DEM is loaded. in future I'd like to add an option for this
   * this is useful if we want to see what areas we have dem for but don't want to
   * cover the map (or maybe we just need translucent DEM?) */
  {
    VikCoord demne, demsw;
    gint x1, y1, x2, y2;
    vik_coord_load_from_latlon(&demne, vik_viewport_get_coord_mode(vp), &dem_northeast);
    vik_coord_load_from_latlon(&demsw, vik_viewport_get_coord_mode(vp), &dem_southwest);

    vik_viewport_coord_to_screen ( vp, &demne, &x1, &y1 );
    vik_viewport_coord_to_screen ( vp, &demsw, &x2, &y2 );

    if ( x1 > vik_viewport_get_width(vp) ) x1=vik_viewport_get_width(vp);
    if ( y2 > vik_viewport_get_height(vp) ) y2=vik_viewport_get_height(vp);
    if ( x2 < 0 ) x2 = 0;
    if ( y1 < 0 ) y1 = 0;
    vik_viewport_draw_rectangle ( vp, gtk_widget_get_style(GTK_WIDGET(vp))->black_gc,
	FALSE, x2, y1, x1-x2, y2-y1 );
    return;
  }
  #endif

  if ( dem->horiz_units == VIK_DEM_HORIZ_LL_ARCSECONDS ) {
    VikCoord tmp; /* TODO: don't use coord_load_from_latlon, especially if in latlon drawing mode */

    gdouble max_lat_as, max_lon_as, min_lat_as, min_lon_as;  
    gdouble start_lat_as, end_lat_as, start_lon_as, end_lon_as;

    gdouble start_lat, end_lat, start_lon, end_lon;

    struct LatLon counter;

    guint x, y, start_x, start_y;

    gint16 elev;

    guint skip_factor = ceil ( vik_viewport_get_xmpp(vp) / 80 ); /* todo: smarter calculation. */

    gdouble nscale_deg = dem->north_scale / ((gdouble) 3600);
    gdouble escale_deg = dem->east_scale / ((gdouble) 3600);

    max_lat_as = max_lat * 3600;
    min_lat_as = min_lat * 3600;
    max_lon_as = max_lon * 3600;
    min_lon_as = min_lon * 3600;

    start_lat_as = MAX(min_lat_as, dem->min_north);
    end_lat_as   = MIN(max_lat_as, dem->max_north);
    start_lon_as = MAX(min_lon_as, dem->min_east);
    end_lon_as   = MIN(max_lon_as, dem->max_east);

    start_lat = floor(start_lat_as / dem->north_scale) * nscale_deg;
    end_lat   = ceil (end_lat_as / dem->north_scale) * nscale_deg;
    start_lon = floor(start_lon_as / dem->east_scale) * escale_deg;
    end_lon   = ceil (end_lon_as / dem->east_scale) * escale_deg;

    vik_dem_east_north_to_xy ( dem, start_lon_as, start_lat_as, &start_x, &start_y );
    guint gradient_skip_factor = 1;
    if(vdl->type == DEM_TYPE_GRADIENT)
	    gradient_skip_factor = skip_factor;

    /* verify sane elev interval */
    if ( vdl->max_elev <= vdl->min_elev )
      vdl->max_elev = vdl->min_elev + 1;

    for ( x=start_x, counter.lon = start_lon; counter.lon <= end_lon+escale_deg*skip_factor; counter.lon += escale_deg * skip_factor, x += skip_factor ) {
      // NOTE: ( counter.lon <= end_lon + ESCALE_DEG*SKIP_FACTOR ) is neccessary so in high zoom modes,
      // the leftmost column does also get drawn, if the center point is out of viewport.
      if ( x < dem->n_columns ) {
        column = g_ptr_array_index ( dem->columns, x );
        // get previous and next column. catch out-of-bound.
	gint32 new_x = x;
	new_x -= gradient_skip_factor;
        if(new_x < 1)
          prevcolumn = g_ptr_array_index ( dem->columns, x+1);
        else
          prevcolumn = g_ptr_array_index ( dem->columns, new_x);
	new_x = x;
	new_x += gradient_skip_factor;
        if(new_x >= dem->n_columns)
          nextcolumn = g_ptr_array_index ( dem->columns, x-1);
        else
          nextcolumn = g_ptr_array_index ( dem->columns, new_x);

        for ( y=start_y, counter.lat = start_lat; counter.lat <= end_lat; counter.lat += nscale_deg * skip_factor, y += skip_factor ) {
          if ( y > column->n_points )
            break;

          elev = column->points[y];

	  // calculate bounding box for drawing
	  gint box_x, box_y, box_width, box_height;
	  struct LatLon box_c;
	  box_c = counter;
	  box_c.lat += (nscale_deg * skip_factor)/2;
          box_c.lon -= (escale_deg * skip_factor)/2;
	  vik_coord_load_from_latlon(&tmp, vik_viewport_get_coord_mode(vp), &box_c);
	  vik_viewport_coord_to_screen(vp, &tmp, &box_x, &box_y);
	  // catch box at borders
	  if(box_x < 0)
	          box_x = 0;
	  if(box_y < 0)
	          box_y = 0;
          box_c.lat -= nscale_deg * skip_factor;
	  box_c.lon += escale_deg * skip_factor;
	  vik_coord_load_from_latlon(&tmp, vik_viewport_get_coord_mode(vp), &box_c);
	  vik_viewport_coord_to_screen(vp, &tmp, &box_width, &box_height);
	  box_width -= box_x;
	  box_height -= box_y;
          // catch box at borders
	  if(box_width < 0 || box_height < 0)
		  continue; // skip this. this is out of our viewport anyway. FIXME: why?

	  gboolean below_minimum = FALSE;
          if(vdl->type == DEM_TYPE_HEIGHT) {
            if ( elev != VIK_DEM_INVALID_ELEVATION && elev < vdl->min_elev ) {
              // Prevent 'elev - vdl->min_elev' from being negative so can safely use as array index
              elev = ceil ( vdl->min_elev );
	      below_minimum = TRUE;
	    }
            if ( elev != VIK_DEM_INVALID_ELEVATION && elev > vdl->max_elev )
              elev = vdl->max_elev;
          }

          {
	    if(box_width < 0 || box_height < 0) // FIXME: why does this happen?
              continue;

            if(vdl->type == DEM_TYPE_GRADIENT) {
              if( elev == VIK_DEM_INVALID_ELEVATION ) {
                /* don't draw it */
              } else {
                // calculate and sum gradient in all directions
                gint16 change = 0;
		gint32 new_y;

		// calculate gradient from height points all around the current one
		new_y = y - gradient_skip_factor;
		if(new_y < 0)
			new_y = y;
		change += get_height_difference(elev, prevcolumn->points[new_y]);
		change += get_height_difference(elev, column->points[new_y]);
		change += get_height_difference(elev, nextcolumn->points[new_y]);

		change += get_height_difference(elev, prevcolumn->points[y]);
		change += get_height_difference(elev, nextcolumn->points[y]);

		new_y = y + gradient_skip_factor;
		if(new_y >= column->n_points)
			new_y = y;
		change += get_height_difference(elev, prevcolumn->points[new_y]);
		change += get_height_difference(elev, column->points[new_y]);
		change += get_height_difference(elev, nextcolumn->points[new_y]);

		change = change / ((skip_factor > 1) ? log(skip_factor) : 0.55); // FIXME: better calc.

                if(change < vdl->min_elev)
                  // Prevent 'change - vdl->min_elev' from being negative so can safely use as array index
                  change = ceil ( vdl->min_elev );

                if(change > vdl->max_elev)
                  change = vdl->max_elev;

                // void vik_viewport_draw_rectangle ( VikViewport *vvp, GdkGC *gc, gboolean filled, gint x1, gint y1, gint x2, gint y2 );
                vik_viewport_draw_rectangle(vp, vdl->gcsgradient[(gint)floor(((change - vdl->min_elev)/(vdl->max_elev - vdl->min_elev))*(DEM_N_GRADIENT_COLORS-2))+1], TRUE, box_x, box_y, box_width, box_height);
              }
            } else {
              if(vdl->type == DEM_TYPE_HEIGHT) {
                if ( elev == VIK_DEM_INVALID_ELEVATION )
                  ; /* don't draw it */
                else if ( elev <= 0 || below_minimum )
		  /* If 'sea' colour or below the defined mininum draw in the configurable colour */
                  vik_viewport_draw_rectangle(vp, vdl->gcs[0], TRUE, box_x, box_y, box_width, box_height);
                else
                  vik_viewport_draw_rectangle(vp, vdl->gcs[(gint)floor(((elev - vdl->min_elev)/(vdl->max_elev - vdl->min_elev))*(DEM_N_HEIGHT_COLORS-2))+1], TRUE, box_x, box_y, box_width, box_height);
              }
            }
          }
        } /* for y= */
      }
    } /* for x= */
  } else if ( dem->horiz_units == VIK_DEM_HORIZ_UTM_METERS ) {
    gdouble max_nor, max_eas, min_nor, min_eas;
    gdouble start_nor, start_eas, end_nor, end_eas;

    gint16 elev;

    guint x, y, start_x, start_y;

    VikCoord tmp; /* TODO: don't use coord_load_from_latlon, especially if in latlon drawing mode */
    struct UTM counter;

    guint skip_factor = ceil ( vik_viewport_get_xmpp(vp) / 10 ); /* todo: smarter calculation. */

    VikCoord tleft, tright, bleft, bright;

    vik_viewport_screen_to_coord ( vp, 0, 0, &tleft );
    vik_viewport_screen_to_coord ( vp, vik_viewport_get_width(vp), 0, &tright );
    vik_viewport_screen_to_coord ( vp, 0, vik_viewport_get_height(vp), &bleft );
    vik_viewport_screen_to_coord ( vp, vik_viewport_get_width(vp), vik_viewport_get_height(vp), &bright );


    vik_coord_convert(&tleft, VIK_COORD_UTM);
    vik_coord_convert(&tright, VIK_COORD_UTM);
    vik_coord_convert(&bleft, VIK_COORD_UTM);
    vik_coord_convert(&bright, VIK_COORD_UTM);

    max_nor = MAX(tleft.north_south, tright.north_south);
    min_nor = MIN(bleft.north_south, bright.north_south);
    max_eas = MAX(bright.east_west, tright.east_west);
    min_eas = MIN(bleft.east_west, tleft.east_west);

    start_nor = MAX(min_nor, dem->min_north);
    end_nor   = MIN(max_nor, dem->max_north);
    if ( tleft.utm_zone == dem->utm_zone && bleft.utm_zone == dem->utm_zone
         && (tleft.utm_letter >= 'N') == (dem->utm_letter >= 'N')
         && (bleft.utm_letter >= 'N') == (dem->utm_letter >= 'N') ) /* if the utm zones/hemispheres are different, min_eas will be bogus */
      start_eas = MAX(min_eas, dem->min_east);
    else
      start_eas = dem->min_east;
    if ( tright.utm_zone == dem->utm_zone && bright.utm_zone == dem->utm_zone
         && (tright.utm_letter >= 'N') == (dem->utm_letter >= 'N')
         && (bright.utm_letter >= 'N') == (dem->utm_letter >= 'N') ) /* if the utm zones/hemispheres are different, min_eas will be bogus */
      end_eas = MIN(max_eas, dem->max_east);
    else
      end_eas = dem->max_east;

    start_nor = floor(start_nor / dem->north_scale) * dem->north_scale;
    end_nor   = ceil (end_nor / dem->north_scale) * dem->north_scale;
    start_eas = floor(start_eas / dem->east_scale) * dem->east_scale;
    end_eas   = ceil (end_eas / dem->east_scale) * dem->east_scale;

    vik_dem_east_north_to_xy ( dem, start_eas, start_nor, &start_x, &start_y );

    /* TODO: why start_x and start_y are -1 -- rounding error from above? */

    counter.zone = dem->utm_zone;
    counter.letter = dem->utm_letter;

    for ( x=start_x, counter.easting = start_eas; counter.easting <= end_eas; counter.easting += dem->east_scale * skip_factor, x += skip_factor ) {
      if ( x > 0 && x < dem->n_columns ) {
        column = g_ptr_array_index ( dem->columns, x );
        for ( y=start_y, counter.northing = start_nor; counter.northing <= end_nor; counter.northing += dem->north_scale * skip_factor, y += skip_factor ) {
          if ( y > column->n_points )
            continue;
          elev = column->points[y];
          if ( elev != VIK_DEM_INVALID_ELEVATION && elev < vdl->min_elev )
            elev=vdl->min_elev;
          if ( elev != VIK_DEM_INVALID_ELEVATION && elev > vdl->max_elev )
            elev=vdl->max_elev;

          {
            gint a, b;
            vik_coord_load_from_utm(&tmp, vik_viewport_get_coord_mode(vp), &counter);
	            vik_viewport_coord_to_screen(vp, &tmp, &a, &b);
            if ( elev == VIK_DEM_INVALID_ELEVATION )
              ; /* don't draw it */
            else if ( elev <= 0 )
              vik_viewport_draw_rectangle(vp, vdl->gcs[0], TRUE, a-1, b-1, 2, 2 );
            else
              vik_viewport_draw_rectangle(vp, vdl->gcs[(gint)floor((elev - vdl->min_elev)/(vdl->max_elev - vdl->min_elev)*(DEM_N_HEIGHT_COLORS-2))+1], TRUE, a-1, b-1, 2, 2 );
          }
        } /* for y= */
      }
    } /* for x= */
  }
}

/* return the continent for the specified lat, lon */
/* TODO */
static const gchar *srtm_continent_dir ( gint lat, gint lon )
{
  extern const char *_srtm_continent_data[];
  static GHashTable *srtm_continent = NULL;
  const gchar *continent;
  gchar name[16];

  if (!srtm_continent) {
    const gchar **s;

    srtm_continent = g_hash_table_new(g_str_hash, g_str_equal);
    s = _srtm_continent_data;
    while (*s != (gchar *)-1) {
      continent = *s++;
      while (*s) {
        g_hash_table_insert(srtm_continent, (gpointer) *s, (gpointer) continent);
        s++;
      }
      s++;
    }
  }
  g_snprintf(name, sizeof(name), "%c%02d%c%03d",
                  (lat >= 0) ? 'N' : 'S', ABS(lat),
		  (lon >= 0) ? 'E' : 'W', ABS(lon));

  return(g_hash_table_lookup(srtm_continent, name));
}

static void dem_layer_draw ( VikDEMLayer *vdl, VikViewport *vp )
{
  GList *dems_iter = vdl->files;
  VikDEM *dem;


  /* search for SRTM3 90m */

  if ( vdl->source == DEM_SOURCE_SRTM )
    srtm_draw_existence ( vp );
#ifdef VIK_CONFIG_DEM24K
  else if ( vdl->source == DEM_SOURCE_DEM24K )
    dem24k_draw_existence ( vp );
#endif

  while ( dems_iter ) {
    dem = a_dems_get ( (const char *) (dems_iter->data) );
    if ( dem )
      vik_dem_layer_draw_dem ( vdl, vp, dem );
    dems_iter = dems_iter->next;
  }
}

static void dem_layer_free ( VikDEMLayer *vdl )
{
  gint i;
  if ( vdl->gcs )
    for ( i = 0; i < DEM_N_HEIGHT_COLORS; i++ )
      g_object_unref ( vdl->gcs[i] );
  g_free ( vdl->gcs );

  if ( vdl->gcsgradient )
    for ( i = 0; i < DEM_N_GRADIENT_COLORS; i++ )
      g_object_unref ( vdl->gcsgradient[i] );
  g_free ( vdl->gcsgradient );

  a_dems_list_free ( vdl->files );
}

VikDEMLayer *dem_layer_create ( VikViewport *vp )
{
  VikDEMLayer *vdl = dem_layer_new ( vp );
  gint i;
  if ( vp ) {
    /* TODO: share GCS between layers */
    for ( i = 0; i < DEM_N_HEIGHT_COLORS; i++ ) {
      if ( i > 0 )
        vdl->gcs[i] = vik_viewport_new_gc ( vp, dem_height_colors[i], UNUSED_LINE_THICKNESS );
    }
    for ( i = 0; i < DEM_N_GRADIENT_COLORS; i++ )
      vdl->gcsgradient[i] = vik_viewport_new_gc ( vp, dem_gradient_colors[i], UNUSED_LINE_THICKNESS );
  }
  return vdl;
}
/**************************************************************
 **** SOURCES & DOWNLOADING
 **************************************************************/
typedef struct {
  gchar *dest;
  gdouble lat, lon;

  GMutex *mutex;
  VikDEMLayer *vdl; /* NULL if not alive */

  guint source;
} DEMDownloadParams;


/**************************************************
 *  SOURCE: SRTM                                  *
 **************************************************/

static void srtm_dem_download_thread ( DEMDownloadParams *p, gpointer threaddata )
{
  gint intlat, intlon;
  const gchar *continent_dir;

  intlat = (int)floor(p->lat);
  intlon = (int)floor(p->lon);
  continent_dir = srtm_continent_dir(intlat, intlon);

  if (!continent_dir) {
    if ( p->vdl ) {
      gchar *msg = g_strdup_printf ( _("No SRTM data available for %f, %f"), p->lat, p->lon );
      vik_window_statusbar_update ( (VikWindow*)VIK_GTK_WINDOW_FROM_LAYER(p->vdl), msg, VIK_STATUSBAR_INFO );
      g_free ( msg );
    }
    return;
  }

  gchar *src_fn = g_strdup_printf("%s%s/%c%02d%c%03d.hgt.zip",
                SRTM_HTTP_URI,
                continent_dir,
		(intlat >= 0) ? 'N' : 'S',
		ABS(intlat),
		(intlon >= 0) ? 'E' : 'W',
		ABS(intlon) );

  static DownloadMapOptions options = { FALSE, FALSE, NULL, 0, a_check_map_file, NULL };
  a_http_download_get_url ( SRTM_HTTP_SITE, src_fn, p->dest, &options, NULL );
  g_free ( src_fn );
}

static gchar *srtm_lat_lon_to_dest_fn ( gdouble lat, gdouble lon )
{
  gint intlat, intlon;
  const gchar *continent_dir;

  intlat = (int)floor(lat);
  intlon = (int)floor(lon);
  continent_dir = srtm_continent_dir(intlat, intlon);

  if (!continent_dir)
    continent_dir = "nowhere";

  return g_strdup_printf("srtm3-%s%s%c%02d%c%03d.hgt.zip",
                continent_dir,
                G_DIR_SEPARATOR_S,
		(intlat >= 0) ? 'N' : 'S',
		ABS(intlat),
		(intlon >= 0) ? 'E' : 'W',
		ABS(intlon) );

}

/* TODO: generalize */
static void srtm_draw_existence ( VikViewport *vp )
{
  gdouble max_lat, max_lon, min_lat, min_lon;  
  gchar buf[strlen(MAPS_CACHE_DIR)+strlen(SRTM_CACHE_TEMPLATE)+30];
  gint i, j;

  vik_viewport_get_min_max_lat_lon ( vp, &min_lat, &max_lat, &min_lon, &max_lon );

  for (i = floor(min_lat); i <= floor(max_lat); i++) {
    for (j = floor(min_lon); j <= floor(max_lon); j++) {
      const gchar *continent_dir;
      if ((continent_dir = srtm_continent_dir(i, j)) == NULL)
        continue;
      g_snprintf(buf, sizeof(buf), SRTM_CACHE_TEMPLATE,
                MAPS_CACHE_DIR,
                continent_dir,
                G_DIR_SEPARATOR_S,
		(i >= 0) ? 'N' : 'S',
		ABS(i),
		(j >= 0) ? 'E' : 'W',
		ABS(j) );
      if ( g_file_test(buf, G_FILE_TEST_EXISTS ) == TRUE ) {
        VikCoord ne, sw;
        gint x1, y1, x2, y2;
        sw.north_south = i;
        sw.east_west = j;
        sw.mode = VIK_COORD_LATLON;
        ne.north_south = i+1;
        ne.east_west = j+1;
        ne.mode = VIK_COORD_LATLON;
        vik_viewport_coord_to_screen ( vp, &sw, &x1, &y1 );
        vik_viewport_coord_to_screen ( vp, &ne, &x2, &y2 );
        if ( x1 < 0 ) x1 = 0;
        if ( y2 < 0 ) y2 = 0;
        vik_viewport_draw_rectangle ( vp, gtk_widget_get_style(GTK_WIDGET(vp))->black_gc,
		FALSE, x1, y2, x2-x1, y1-y2 );
      }
    }
  }
}


/**************************************************
 *  SOURCE: USGS 24K                              *
 **************************************************/

#ifdef VIK_CONFIG_DEM24K

static void dem24k_dem_download_thread ( DEMDownloadParams *p, gpointer threaddata )
{
  /* TODO: dest dir */
  gchar *cmdline = g_strdup_printf("%s %.03f %.03f",
	DEM24K_DOWNLOAD_SCRIPT,
	floor(p->lat*8)/8,
	ceil(p->lon*8)/8 );
  /* FIX: don't use system, use execv or something. check for existence */
  system(cmdline);
  g_free ( cmdline );
}

static gchar *dem24k_lat_lon_to_dest_fn ( gdouble lat, gdouble lon )
{
  return g_strdup_printf("dem24k/%d/%d/%.03f,%.03f.dem",
	(gint) lat,
	(gint) lon,
	floor(lat*8)/8,
	ceil(lon*8)/8);
}

/* TODO: generalize */
static void dem24k_draw_existence ( VikViewport *vp )
{
  gdouble max_lat, max_lon, min_lat, min_lon;  
  gchar buf[strlen(MAPS_CACHE_DIR)+40];
  gdouble i, j;

  vik_viewport_get_min_max_lat_lon ( vp, &min_lat, &max_lat, &min_lon, &max_lon );

  for (i = floor(min_lat*8)/8; i <= floor(max_lat*8)/8; i+=0.125) {
    /* check lat dir first -- faster */
    g_snprintf(buf, sizeof(buf), "%sdem24k/%d/",
        MAPS_CACHE_DIR,
	(gint) i );
    if ( g_file_test(buf, G_FILE_TEST_EXISTS) == FALSE )
      continue;
    for (j = floor(min_lon*8)/8; j <= floor(max_lon*8)/8; j+=0.125) {
      /* check lon dir first -- faster */
      g_snprintf(buf, sizeof(buf), "%sdem24k/%d/%d/",
        MAPS_CACHE_DIR,
	(gint) i,
        (gint) j );
      if ( g_file_test(buf, G_FILE_TEST_EXISTS) == FALSE )
        continue;
      g_snprintf(buf, sizeof(buf), "%sdem24k/%d/%d/%.03f,%.03f.dem",
	        MAPS_CACHE_DIR,
		(gint) i,
		(gint) j,
		floor(i*8)/8,
		floor(j*8)/8 );
      if ( g_file_test(buf, G_FILE_TEST_EXISTS ) == TRUE ) {
        VikCoord ne, sw;
        gint x1, y1, x2, y2;
        sw.north_south = i;
        sw.east_west = j-0.125;
        sw.mode = VIK_COORD_LATLON;
        ne.north_south = i+0.125;
        ne.east_west = j;
        ne.mode = VIK_COORD_LATLON;
        vik_viewport_coord_to_screen ( vp, &sw, &x1, &y1 );
        vik_viewport_coord_to_screen ( vp, &ne, &x2, &y2 );
        if ( x1 < 0 ) x1 = 0;
        if ( y2 < 0 ) y2 = 0;
        vik_viewport_draw_rectangle ( vp, gtk_widget_get_style(GTK_WIDGET(vp))->black_gc,
		FALSE, x1, y2, x2-x1, y1-y2 );
      }
    }
  }
}
#endif

/**************************************************
 *   SOURCES -- DOWNLOADING & IMPORTING TOOL      *
 **************************************************
 */

static void weak_ref_cb ( gpointer ptr, GObject * dead_vdl )
{
  DEMDownloadParams *p = ptr;
  g_mutex_lock ( p->mutex );
  p->vdl = NULL;
  g_mutex_unlock ( p->mutex );
}

/* Try to add file full_path.
 * filename will be copied.
 * returns FALSE if file does not exists, TRUE otherwise.
 */
static gboolean dem_layer_add_file ( VikDEMLayer *vdl, const gchar *filename )
{
  if ( g_file_test(filename, G_FILE_TEST_EXISTS) == TRUE ) {
    /* only load if file size is not 0 (not in progress) */
    struct stat sb;
    stat ( filename, &sb );
    if ( sb.st_size ) {
      gchar *duped_path = g_strdup(filename);
      vdl->files = g_list_prepend ( vdl->files, duped_path );
      a_dems_load ( duped_path );
      g_debug("%s: %s", __FUNCTION__, duped_path);
    }
    return TRUE;
  } else
    return FALSE;
}

static void dem_download_thread ( DEMDownloadParams *p, gpointer threaddata )
{
  if ( p->source == DEM_SOURCE_SRTM )
    srtm_dem_download_thread ( p, threaddata );
#ifdef VIK_CONFIG_DEM24K
  else if ( p->source == DEM_SOURCE_DEM24K )
    dem24k_dem_download_thread ( p, threaddata );
#endif
  else
    return;

  //gdk_threads_enter();
  g_mutex_lock ( p->mutex );
  if ( p->vdl ) {
    g_object_weak_unref ( G_OBJECT(p->vdl), weak_ref_cb, p );

    if ( dem_layer_add_file ( p->vdl, p->dest ) )
      vik_layer_emit_update ( VIK_LAYER(p->vdl) ); // NB update from background thread
  }
  g_mutex_unlock ( p->mutex );
  //gdk_threads_leave();
}


static void free_dem_download_params ( DEMDownloadParams *p )
{
  g_mutex_free ( p->mutex );
  g_free ( p->dest );
  g_free ( p );
}

static gpointer dem_layer_download_create ( VikWindow *vw, VikViewport *vvp)
{
  return vvp;
}

/**
 * Display a simple dialog with information about the DEM file at this location
 */
static void dem_layer_file_info ( GtkWidget *widget, struct LatLon *ll )
{
  gint intlat, intlon;
  const gchar *continent_dir;

  intlat = (int)floor(ll->lat);
  intlon = (int)floor(ll->lon);
  continent_dir = srtm_continent_dir(intlat, intlon);

  gchar *source = NULL;
  if ( continent_dir )
    source = g_strdup_printf ( "http://%s%s%s/%c%02d%c%03d.hgt.zip",
                               SRTM_HTTP_SITE,
                               SRTM_HTTP_URI,
                               continent_dir,
                               (intlat >= 0) ? 'N' : 'S',
                               ABS(intlat),
                               (intlon >= 0) ? 'E' : 'W',
                               ABS(intlon) );
  else
    // Probably not over any land...
    source = g_strdup ( _("No DEM File Available") );

  gchar *filename = NULL;
  gchar *dem_file = NULL;
#ifdef VIK_CONFIG_DEM24K
  dem_file = dem24k_lat_lon_to_dest_fn ( ll->lat, ll->lon );
#else
  dem_file = srtm_lat_lon_to_dest_fn ( ll->lat, ll->lon );
#endif
  gchar *message = NULL;

  filename = g_strdup_printf ( "%s%s", MAPS_CACHE_DIR, dem_file );

  if ( g_file_test ( filename, G_FILE_TEST_EXISTS ) ) {
    // Get some timestamp information of the file
    struct stat stat_buf;
    if ( g_stat ( filename, &stat_buf ) == 0 ) {
      gchar time_buf[64];
      strftime ( time_buf, sizeof(time_buf), "%c", gmtime((const time_t *)&stat_buf.st_mtime) );
      message = g_strdup_printf ( _("\nSource: %s\n\nDEM File: %s\nDEM File Timestamp: %s"), source, filename, time_buf );
    }
  }
  else
    message = g_strdup_printf ( _("Source: %s\n\nNo DEM File!"), source );

  // Show the info
  a_dialog_info_msg ( GTK_WINDOW(gtk_widget_get_toplevel(widget)), message );

  g_free ( message );
  g_free ( source );
  g_free ( dem_file );
  g_free ( filename );
}

static gboolean dem_layer_download_release ( VikDEMLayer *vdl, GdkEventButton *event, VikViewport *vvp )
{
  VikCoord coord;
  static struct LatLon ll;

  gchar *full_path;
  gchar *dem_file = NULL;

  vik_viewport_screen_to_coord ( vvp, event->x, event->y, &coord );
  vik_coord_to_latlon ( &coord, &ll );

  
  if ( vdl->source == DEM_SOURCE_SRTM )
    dem_file = srtm_lat_lon_to_dest_fn ( ll.lat, ll.lon );
#ifdef VIK_CONFIG_DEM24K
  else if ( vdl->source == DEM_SOURCE_DEM24K )
    dem_file = dem24k_lat_lon_to_dest_fn ( ll.lat, ll.lon );
#endif

  if ( ! dem_file )
    return TRUE;

  full_path = g_strdup_printf("%s%s", MAPS_CACHE_DIR, dem_file );

  g_debug("%s: %s", __FUNCTION__, full_path);

  if ( event->button == 1 ) {
    // TODO: check if already in filelist
    if ( ! dem_layer_add_file(vdl, full_path) ) {
      gchar *tmp = g_strdup_printf ( _("Downloading DEM %s"), dem_file );
      DEMDownloadParams *p = g_malloc(sizeof(DEMDownloadParams));
      p->dest = g_strdup(full_path);
      p->lat = ll.lat;
      p->lon = ll.lon;
      p->vdl = vdl;
      p->mutex = g_mutex_new();
      p->source = vdl->source;
      g_object_weak_ref(G_OBJECT(p->vdl), weak_ref_cb, p );

      a_background_thread ( VIK_GTK_WINDOW_FROM_LAYER(vdl), tmp,
                            (vik_thr_func) dem_download_thread, p,
                            (vik_thr_free_func) free_dem_download_params, NULL, 1 );

      g_free ( tmp );
    }
    else
      vik_layer_emit_update ( VIK_LAYER(vdl) );
  }
  else {
    if ( !vdl->right_click_menu ) {
      GtkWidget *item;
      vdl->right_click_menu = GTK_MENU ( gtk_menu_new () );

      item = gtk_image_menu_item_new_with_mnemonic ( _("_Show DEM File Information") );
      gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_INFO, GTK_ICON_SIZE_MENU) );
      g_signal_connect ( G_OBJECT(item), "activate", G_CALLBACK(dem_layer_file_info), &ll );
      gtk_menu_shell_append (GTK_MENU_SHELL(vdl->right_click_menu), item);
    }

    gtk_menu_popup ( vdl->right_click_menu, NULL, NULL, NULL, NULL, event->button, event->time );
    gtk_widget_show_all ( GTK_WIDGET(vdl->right_click_menu) );
  }

  g_free ( dem_file );
  g_free ( full_path );

  return TRUE;
}

static gboolean dem_layer_download_click ( VikDEMLayer *vdl, GdkEventButton *event, VikViewport *vvp )
{
/* choose & keep track of cache dir
 * download in background thread
 * download over area */
  return TRUE;
}


