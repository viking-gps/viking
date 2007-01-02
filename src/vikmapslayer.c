/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2005, Evan Battaglia <viking@greentorch.org>
 * UTM multi-zone stuff by Kit Transue <notlostyet@didactek.com>
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

#define MAX_SHRINKFACTOR 8.0000001 /* zoom 1 viewing 8-tiles */
#define MIN_SHRINKFACTOR 0.0312499 /* zoom 32 viewing 1-tiles */

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "globals.h"
#include "coords.h"
#include "vikcoord.h"
#include "viktreeview.h"
#include "vikviewport.h"
#include "viklayer.h"
#include "vikmapslayer.h"
#include "vikmapslayer_pixmap.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "mapcache.h"
/* only for dialog.h -- ugh */
#include "vikwaypoint.h"
#include "dialog.h"

#include "vikstatus.h"
#include "background.h"

#include "vikaggregatelayer.h"
#include "viklayerspanel.h"

#include "mapcoord.h"
#include "terraserver.h"
#include "googlemaps.h"
#include "google.h"
#include "khmaps.h"
#include "expedia.h"

typedef struct {
  guint8 uniq_id;
  guint16 tilesize_x;
  guint16 tilesize_y;
  guint drawmode;
  gboolean (*coord_to_mapcoord) ( const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest );
  void (*mapcoord_to_center_coord) ( MapCoord *src, VikCoord *dest );
  void (*download) ( MapCoord *src, const gchar *dest_fn );
  /* TODO: constant size (yay!) */
} VikMapsLayer_MapType;


/****** MAP TYPES ******/

static const VikMapsLayer_MapType __map_types[] = {

{ 2, 200, 200, VIK_VIEWPORT_DRAWMODE_UTM, terraserver_topo_coord_to_mapcoord, terraserver_mapcoord_to_center_coord, terraserver_topo_download },
{ 1, 200, 200, VIK_VIEWPORT_DRAWMODE_UTM, terraserver_aerial_coord_to_mapcoord, terraserver_mapcoord_to_center_coord, terraserver_aerial_download },
{ 4, 200, 200, VIK_VIEWPORT_DRAWMODE_UTM, terraserver_urban_coord_to_mapcoord, terraserver_mapcoord_to_center_coord, terraserver_urban_download },
{ 5, 0, 0, VIK_VIEWPORT_DRAWMODE_EXPEDIA, expedia_coord_to_mapcoord, expedia_mapcoord_to_center_coord, expedia_download },
{ 9, 128, 128, VIK_VIEWPORT_DRAWMODE_GOOGLE, googlemaps_coord_to_mapcoord, googlemaps_mapcoord_to_center_coord, googlemaps_download },
{ 8, 256, 256, VIK_VIEWPORT_DRAWMODE_KH, khmaps_coord_to_mapcoord, khmaps_mapcoord_to_center_coord, khmaps_download },
{ 7, 256, 256, VIK_VIEWPORT_DRAWMODE_MERCATOR, google_coord_to_mapcoord, google_mapcoord_to_center_coord, google_download },
{ 10, 256, 256, VIK_VIEWPORT_DRAWMODE_MERCATOR, google_coord_to_mapcoord, google_mapcoord_to_center_coord, google_trans_download },
{ 11, 256, 256, VIK_VIEWPORT_DRAWMODE_MERCATOR, google_coord_to_mapcoord, google_mapcoord_to_center_coord, google_kh_download },
};

#define NUM_MAP_TYPES (sizeof(__map_types)/sizeof(__map_types[0]))

/******** MAPZOOMS *********/

static gchar *params_mapzooms[] = { "Use Viking Zoom Level", "0.25", "1", "2", "4", "8", "16", "32", "64", "128", "256", "512", "1024", "USGS 10k", "USGS 24k", "USGS 25k", "USGS 50k", "USGS 100k", "USGS 200k", "USGS 250k", NULL };
static gdouble __mapzooms_x[] = { 0.0, 0.25, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0, 512.0, 1024.0, 1.016, 2.4384, 2.54, 5.08, 10.16, 20.32, 25.4 };
static gdouble __mapzooms_y[] = { 0.0, 0.25, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0, 512.0, 1024.0, 1.016, 2.4384, 2.54, 5.08, 10.16, 20.32, 25.4 };

static gchar *params_maptypes[] = { "Terraserver Topos", "Terraserver Aerials", "Terraserver Urban Areas", "Expedia Street Maps", "Old Google Maps", "Old KH Satellite Images", "Google Maps", "Transparent Google Maps", "Google Satellite Images" };
static guint params_maptypes_ids[] = { 2, 1, 4, 5, 9, 8, 7, 10, 11 };
#define NUM_MAPZOOMS (sizeof(params_mapzooms)/sizeof(params_mapzooms[0]) - 1)

/**************************/


static VikMapsLayer *maps_layer_copy ( VikMapsLayer *vml, VikViewport *vvp );
static void maps_layer_marshall( VikMapsLayer *vml, guint8 **data, gint *len );
static VikMapsLayer *maps_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp );
static gboolean maps_layer_set_param ( VikMapsLayer *vml, guint16 id, VikLayerParamData data, VikViewport *vvp );
static VikLayerParamData maps_layer_get_param ( VikMapsLayer *vml, guint16 id );
static void maps_layer_draw ( VikMapsLayer *vml, VikViewport *vvp );
static VikMapsLayer *maps_layer_new ( VikViewport *vvp );
static void maps_layer_free ( VikMapsLayer *vml );
static gboolean maps_layer_download_release ( VikMapsLayer *vml, GdkEventButton *event, VikViewport *vvp );
static gboolean maps_layer_download_click ( VikMapsLayer *vml, GdkEventButton *event, VikViewport *vvp );
static gpointer maps_layer_download_create ( VikWindow *vw, VikViewport *vvp );
static void maps_layer_set_cache_dir ( VikMapsLayer *vml, const gchar *dir );
static void start_download_thread ( VikMapsLayer *vml, VikViewport *vvp, const VikCoord *ul, const VikCoord *br, gint redownload );
static void maps_layer_add_menu_items ( VikMapsLayer *vml, GtkMenu *menu, VikLayersPanel *vlp );


static VikLayerParamScale params_scales[] = {
  /* min, max, step, digits (decimal places) */
 { 0, 255, 3, 0 }, /* alpha */
};

VikLayerParam maps_layer_params[] = {
  { "mode", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, "Map Type:", VIK_LAYER_WIDGET_RADIOGROUP, params_maptypes, params_maptypes_ids },
  { "directory", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, "Maps Directory (Optional):", VIK_LAYER_WIDGET_FILEENTRY },
  { "alpha", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, "Alpha:", VIK_LAYER_WIDGET_HSCALE, params_scales },
  { "autodownload", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, "Autodownload maps:", VIK_LAYER_WIDGET_CHECKBUTTON },
  { "mapzoom", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, "Zoom Level:", VIK_LAYER_WIDGET_COMBOBOX, params_mapzooms },
};

enum { PARAM_MAPTYPE=0, PARAM_CACHE_DIR, PARAM_ALPHA, PARAM_AUTODOWNLOAD, PARAM_MAPZOOM, NUM_PARAMS };

static VikToolInterface maps_tools[] = {
  { "Maps Download", (VikToolConstructorFunc) maps_layer_download_create, NULL, NULL, NULL,  
    (VikToolMouseFunc) maps_layer_download_click, NULL,  (VikToolMouseFunc) maps_layer_download_release },
};

VikLayerInterface vik_maps_layer_interface = {
  "Map",
  &mapslayer_pixbuf,

  maps_tools,
  sizeof(maps_tools) / sizeof(maps_tools[0]),

  maps_layer_params,
  NUM_PARAMS,
  NULL,
  0,

  VIK_MENU_ITEM_ALL,

  (VikLayerFuncCreate)                  maps_layer_new,
  (VikLayerFuncRealize)                 NULL,
  (VikLayerFuncPostRead)                NULL,
  (VikLayerFuncFree)                    maps_layer_free,

  (VikLayerFuncProperties)              NULL,
  (VikLayerFuncDraw)                    maps_layer_draw,
  (VikLayerFuncChangeCoordMode)         NULL,

  (VikLayerFuncSetMenuItemsSelection)   NULL,
  (VikLayerFuncGetMenuItemsSelection)   NULL,

  (VikLayerFuncAddMenuItems)            maps_layer_add_menu_items,
  (VikLayerFuncSublayerAddMenuItems)    NULL,

  (VikLayerFuncSublayerRenameRequest)   NULL,
  (VikLayerFuncSublayerToggleVisible)   NULL,

  (VikLayerFuncCopy)                    maps_layer_copy,
  (VikLayerFuncMarshall)		maps_layer_marshall,
  (VikLayerFuncUnmarshall)		maps_layer_unmarshall,

  (VikLayerFuncSetParam)                maps_layer_set_param,
  (VikLayerFuncGetParam)                maps_layer_get_param,

  (VikLayerFuncReadFileData)            NULL,
  (VikLayerFuncWriteFileData)           NULL,

  (VikLayerFuncDeleteItem)              NULL,
  (VikLayerFuncCopyItem)                NULL,
  (VikLayerFuncPasteItem)               NULL,
  (VikLayerFuncFreeCopiedItem)          NULL,
  (VikLayerFuncDragDropRequest)		NULL,
};

struct _VikMapsLayer {
  VikLayer vl;
  guint maptype;
  gchar *cache_dir;
  guint8 alpha;
  guint mapzoom_id;
  gdouble xmapzoom, ymapzoom;

  gboolean autodownload;

  gint dl_tool_x, dl_tool_y;

  GtkMenu *dl_right_click_menu;
  VikCoord redownload_ul, redownload_br; /* right click menu only */
  VikViewport *redownload_vvp;
};

enum { REDOWNLOAD_NONE = 0, REDOWNLOAD_BAD, REDOWNLOAD_ALL };


/****************************************/
/******** CACHE DIR STUFF ***************/
/****************************************/

#ifdef WINDOWS
#define MAPS_CACHE_DIR "C:\\VIKING-MAPS\\"
#define DIRSTRUCTURE "%st%ds%dz%d\\%d\\%d"
#else /* POSIX */

#include <stdlib.h>

#define MAPS_CACHE_DIR maps_layer_default_dir()
#define GLOBAL_MAPS_DIR "/var/cache/maps/"
#define DIRSTRUCTURE "%st%ds%dz%d/%d/%d"

static gchar *maps_layer_default_dir ()
{
  static gchar defaultdir[512];
  static gboolean already_run = 0;
  if ( ! already_run )
  {
    /* Thanks to Mike Davison for the $VIKING_MAPS usage */
    gchar *mapdir = getenv("VIKING_MAPS");
    if ( mapdir && strlen(mapdir) < 497 ) {
      strcpy ( defaultdir, mapdir );
    } else if ( access ( GLOBAL_MAPS_DIR, W_OK ) == 0 ) {
      strcpy ( defaultdir, GLOBAL_MAPS_DIR );
    } else {
      gchar *home = getenv("HOME");
      if ( home && strlen(home) < 497 )
      {
        strcpy ( defaultdir, home );
        strcat ( defaultdir, "/.viking-maps/" );
      }
      else
      {
        strcpy ( defaultdir, ".viking-maps/" );
      }
    }
    already_run = 1;
  }
  return defaultdir;
}

#endif

static void maps_layer_mkdir_if_default_dir ( VikMapsLayer *vml )
{
  if ( vml->cache_dir && strcmp ( vml->cache_dir, MAPS_CACHE_DIR ) == 0 && access ( vml->cache_dir, F_OK ) != 0 )
  {
#ifdef WINDOWS
    mkdir ( vml->cache_dir );
#else
    mkdir ( vml->cache_dir, 0777 );
#endif
  }
}

static void maps_layer_set_cache_dir ( VikMapsLayer *vml, const gchar *dir )
{
  guint len;
  g_assert ( vml != NULL);
  if ( vml->cache_dir )
    g_free ( vml->cache_dir );

  if ( dir == NULL || dir[0] == '\0' )
    vml->cache_dir = g_strdup ( MAPS_CACHE_DIR );
  else
  {
    len = strlen(dir);
    if ( dir[len-1] != VIKING_FILE_SEP )
    {
      vml->cache_dir = g_malloc ( len+2 );
      strncpy ( vml->cache_dir, dir, len );
      vml->cache_dir[len] = VIKING_FILE_SEP;
      vml->cache_dir[len+1] = '\0';
    }
    else
      vml->cache_dir = g_strdup ( dir );
  }
  maps_layer_mkdir_if_default_dir ( vml );
}

/****************************************/
/******** GOBJECT STUFF *****************/
/****************************************/

GType vik_maps_layer_get_type ()
{
  static GType vml_type = 0;

  if (!vml_type)
  {
    static const GTypeInfo vml_info =
    {
      sizeof (VikMapsLayerClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      NULL, /* class init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikMapsLayer),
      0,
      NULL /* instance init */
    };
    vml_type = g_type_register_static ( VIK_LAYER_TYPE, "VikMapsLayer", &vml_info, 0 );
  }

  return vml_type;
}

/****************************************/
/************** PARAMETERS **************/
/****************************************/

static guint map_index_to_uniq_id (guint8 index)
{
  g_assert ( index < NUM_MAP_TYPES );
  return __map_types[index].uniq_id;
}

static guint map_uniq_id_to_index ( guint uniq_id )
{
  gint i;
  for ( i = 0; i < NUM_MAP_TYPES; i++ )
    if ( __map_types[i].uniq_id == uniq_id )
      return i;
  return NUM_MAP_TYPES; /* no such thing */
}

static gboolean maps_layer_set_param ( VikMapsLayer *vml, guint16 id, VikLayerParamData data, VikViewport *vvp )
{
  switch ( id )
  {
    case PARAM_CACHE_DIR: maps_layer_set_cache_dir ( vml, data.s ); break;
    case PARAM_MAPTYPE: {
      gint maptype = map_uniq_id_to_index(data.u);
      if ( maptype == NUM_MAP_TYPES ) g_warning("Unknown map type");
      else vml->maptype = maptype;
      break;
    }
    case PARAM_ALPHA: if ( data.u <= 255 ) vml->alpha = data.u; break;
    case PARAM_AUTODOWNLOAD: vml->autodownload = data.b; break;
    case PARAM_MAPZOOM: if ( data.u < NUM_MAPZOOMS ) {
                          vml->mapzoom_id = data.u;
                          vml->xmapzoom = __mapzooms_x [data.u];
                          vml->ymapzoom = __mapzooms_y [data.u];
                        }else g_warning ("Unknown Map Zoom"); break;
  }
  return TRUE;
}

static VikLayerParamData maps_layer_get_param ( VikMapsLayer *vml, guint16 id )
{
  VikLayerParamData rv;
  switch ( id )
  {
    case PARAM_CACHE_DIR: rv.s = (vml->cache_dir && strcmp(vml->cache_dir, MAPS_CACHE_DIR) != 0) ? vml->cache_dir : ""; break;
    case PARAM_MAPTYPE: rv.u = map_index_to_uniq_id ( vml->maptype ); break;
    case PARAM_ALPHA: rv.u = vml->alpha; break;
    case PARAM_AUTODOWNLOAD: rv.u = vml->autodownload; break;
    case PARAM_MAPZOOM: rv.u = vml->mapzoom_id; break;
  }
  return rv;
}

/****************************************/
/****** CREATING, COPYING, FREEING ******/
/****************************************/

static VikMapsLayer *maps_layer_new ( VikViewport *vvp )
{
  VikMapsLayer *vml = VIK_MAPS_LAYER ( g_object_new ( VIK_MAPS_LAYER_TYPE, NULL ) );
  vik_layer_init ( VIK_LAYER(vml), VIK_LAYER_MAPS );
  vml->maptype = 0;
  vml->alpha = 255;
  vml->mapzoom_id = 0;
  vml->dl_tool_x = vml->dl_tool_y = -1;
  maps_layer_set_cache_dir ( vml, NULL );
  vml->autodownload = FALSE;

  vml->dl_right_click_menu = NULL;

  return vml;
}

static void maps_layer_free ( VikMapsLayer *vml )
{
  if ( vml->cache_dir )
    g_free ( vml->cache_dir );
  if ( vml->dl_right_click_menu )
    gtk_object_sink ( GTK_OBJECT(vml->dl_right_click_menu) );
}

static VikMapsLayer *maps_layer_copy ( VikMapsLayer *vml, VikViewport *vvp )
{
  VikMapsLayer *rv = maps_layer_new ( vvp );
  *rv = *vml;
  rv->cache_dir = g_strdup(rv->cache_dir);
  VIK_LAYER(rv)->name = NULL;
  return rv;
}

static void maps_layer_marshall( VikMapsLayer *vml, guint8 **data, gint *len )
{
  vik_layer_marshall_params ( VIK_LAYER(vml), data, len );
}

static VikMapsLayer *maps_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp )
{
  VikMapsLayer *rv = maps_layer_new ( vvp );
  vik_layer_unmarshall_params ( VIK_LAYER(rv), data, len, vvp );
  return rv;
}

/*********************/
/****** DRAWING ******/
/*********************/

static GdkPixbuf *pixbuf_set_alpha ( GdkPixbuf *pixbuf, guint8 alpha )
{
  guchar *pixels;
  gint width, height, iii, jjj;

  if ( ! gdk_pixbuf_get_has_alpha ( pixbuf ) )
  {
    GdkPixbuf *tmp = gdk_pixbuf_add_alpha(pixbuf,FALSE,0,0,0);
    g_object_unref(G_OBJECT(pixbuf));
    pixbuf = tmp;
  }

  pixels = gdk_pixbuf_get_pixels(pixbuf);
  width = gdk_pixbuf_get_width(pixbuf);
  height = gdk_pixbuf_get_height(pixbuf);

  /* r,g,b,a,r,g,b,a.... */
  for (iii = 0; iii < width; iii++) for (jjj = 0; jjj < height; jjj++)
  {
    pixels += 3;
    *pixels++ = alpha;
  }
  return pixbuf;
}

static GdkPixbuf *pixbuf_shrink ( GdkPixbuf *pixbuf, gdouble xshrinkfactor, gdouble yshrinkfactor )
{
  GdkPixbuf *tmp;
  guint16 width = gdk_pixbuf_get_width(pixbuf), height = gdk_pixbuf_get_height(pixbuf);
  tmp = gdk_pixbuf_scale_simple(pixbuf, ceil(width * xshrinkfactor), ceil(height * yshrinkfactor), GDK_INTERP_BILINEAR);
  g_object_unref ( G_OBJECT(pixbuf) );
  return tmp;
}

static GdkPixbuf *get_pixbuf( VikMapsLayer *vml, gint mode, MapCoord *mapcoord, gchar *filename_buf, gint buf_len, gdouble xshrinkfactor, gdouble yshrinkfactor )
{
  GdkPixbuf *pixbuf;

  /* get the thing */
  pixbuf = a_mapcache_get ( mapcoord->x, mapcoord->y, mapcoord->z,
                            mode, mapcoord->scale, vml->alpha, xshrinkfactor, yshrinkfactor );

  if ( ! pixbuf ) {
    g_snprintf ( filename_buf, buf_len, DIRSTRUCTURE,
                     vml->cache_dir, mode,
                     mapcoord->scale, mapcoord->z, mapcoord->x, mapcoord->y );
    if ( access ( filename_buf, R_OK ) == 0) {
    {
      GError *gx = NULL;
      pixbuf = gdk_pixbuf_new_from_file ( filename_buf, &gx );

      if (gx)
      {
        if ( gx->domain != GDK_PIXBUF_ERROR || gx->code != GDK_PIXBUF_ERROR_CORRUPT_IMAGE )
          g_warning ( "Couldn't open image file: %s", gx->message );

          g_error_free ( gx );
          if ( pixbuf )
            g_object_unref ( G_OBJECT(pixbuf) );
          pixbuf = NULL;
        } else {
          if ( vml->alpha < 255 )
            pixbuf = pixbuf_set_alpha ( pixbuf, vml->alpha );
          if ( xshrinkfactor != 1.0 || yshrinkfactor != 1.0 )
            pixbuf = pixbuf_shrink ( pixbuf, xshrinkfactor, yshrinkfactor );

          a_mapcache_add ( pixbuf, mapcoord->x, mapcoord->y, 
              mapcoord->z, __map_types[vml->maptype].uniq_id, 
              mapcoord->scale, vml->alpha, xshrinkfactor, yshrinkfactor );
        }
      }
    }
  }
  return pixbuf;
}

static void maps_layer_draw_section ( VikMapsLayer *vml, VikViewport *vvp, VikCoord *ul, VikCoord *br )
{
  MapCoord ulm, brm;
  gdouble xzoom = vik_viewport_get_xmpp ( vvp );
  gdouble yzoom = vik_viewport_get_ympp ( vvp );
  gdouble xshrinkfactor = 1.0, yshrinkfactor = 1.0;

  if ( vml->xmapzoom && (vml->xmapzoom != xzoom || vml->ymapzoom != yzoom) ) {
    xshrinkfactor = vml->xmapzoom / xzoom;
    yshrinkfactor = vml->ymapzoom / yzoom;
    if ( xshrinkfactor > MIN_SHRINKFACTOR && xshrinkfactor < MAX_SHRINKFACTOR &&
         yshrinkfactor > MIN_SHRINKFACTOR && yshrinkfactor < MAX_SHRINKFACTOR ) {
      xzoom = vml->xmapzoom;
      yzoom = vml->xmapzoom;
    } else {
      g_warning ( "Cowardly refusing to draw tiles at a shrinkfactor more than %.3f (zoomed out) or less than %.3f (zoomed in).", 1/MIN_SHRINKFACTOR, 1/MAX_SHRINKFACTOR );
    }
  }

  /* coord -> ID */
  if ( __map_types[vml->maptype].coord_to_mapcoord ( ul, xzoom, yzoom, &ulm ) &&
       __map_types[vml->maptype].coord_to_mapcoord ( br, xzoom, yzoom, &brm ) ) {

    /* loop & draw */
    gint x, y;
    gint xmin = MIN(ulm.x, brm.x), xmax = MAX(ulm.x, brm.x);
    gint ymin = MIN(ulm.y, brm.y), ymax = MAX(ulm.y, brm.y);
    gint mode = __map_types[vml->maptype].uniq_id;

    VikCoord coord;
    gint xx, yy, width, height;
    GdkPixbuf *pixbuf;

    guint max_path_len = strlen(vml->cache_dir) + 40;
    gchar *path_buf = g_malloc ( max_path_len * sizeof(char) );

    if ( vml->autodownload )
      start_download_thread ( vml, vvp, ul, br, REDOWNLOAD_NONE );

    if ( __map_types[vml->maptype].tilesize_x == 0 ) {
      for ( x = xmin; x <= xmax; x++ ) {
        for ( y = ymin; y <= ymax; y++ ) {
          ulm.x = x;
          ulm.y = y;
          pixbuf = get_pixbuf ( vml, mode, &ulm, path_buf, max_path_len, xshrinkfactor, yshrinkfactor );
          if ( pixbuf ) {
            width = gdk_pixbuf_get_width ( pixbuf );
            height = gdk_pixbuf_get_height ( pixbuf );

            __map_types[vml->maptype].mapcoord_to_center_coord ( &ulm, &coord );
            vik_viewport_coord_to_screen ( vvp, &coord, &xx, &yy );
            xx -= (width/2);
            yy -= (height/2);

            vik_viewport_draw_pixbuf ( vvp, pixbuf, 0, 0, xx, yy, width, height );
          }
        }
      }
    } else { /* tilesize is known, don't have to keep converting coords */
      gdouble tilesize_x = __map_types[vml->maptype].tilesize_x * xshrinkfactor;
      gdouble tilesize_y = __map_types[vml->maptype].tilesize_y * yshrinkfactor;
      /* ceiled so tiles will be maximum size in the case of funky shrinkfactor */
      gint tilesize_x_ceil = ceil ( tilesize_x );
      gint tilesize_y_ceil = ceil ( tilesize_y );
      gint8 xinc = (ulm.x == xmin) ? 1 : -1;
      gint8 yinc = (ulm.y == ymin) ? 1 : -1;
      gdouble xx, yy; gint xx_tmp, yy_tmp;
      gint base_yy, xend, yend;
      xend = (xinc == 1) ? (xmax+1) : (xmin-1);
      yend = (yinc == 1) ? (ymax+1) : (ymin-1);

      __map_types[vml->maptype].mapcoord_to_center_coord ( &ulm, &coord );
      vik_viewport_coord_to_screen ( vvp, &coord, &xx_tmp, &yy_tmp );
      xx = xx_tmp; yy = yy_tmp;
      /* above trick so xx,yy doubles. this is so shrinkfactors aren't rounded off
       * eg if tile size 128, shrinkfactor 0.333 */
      xx -= (tilesize_x/2);
      base_yy = yy - (tilesize_y/2);

      for ( x = ((xinc == 1) ? xmin : xmax); x != xend; x+=xinc ) {
        yy = base_yy;
        for ( y = ((yinc == 1) ? ymin : ymax); y != yend; y+=yinc ) {
          ulm.x = x;
          ulm.y = y;
          pixbuf = get_pixbuf ( vml, mode, &ulm, path_buf, max_path_len, xshrinkfactor, yshrinkfactor );
          if ( pixbuf )
            vik_viewport_draw_pixbuf ( vvp, pixbuf, 0, 0, xx, yy, tilesize_x_ceil, tilesize_y_ceil );

          yy += tilesize_y;
        }
        xx += tilesize_x;
      }
    }

    g_free ( path_buf );
  }
}

static void maps_layer_draw ( VikMapsLayer *vml, VikViewport *vvp )
{
  if ( __map_types[vml->maptype].drawmode == vik_viewport_get_drawmode ( vvp ) )
  {
    VikCoord ul, br;

    /* get corner coords */
    if ( vik_viewport_get_coord_mode ( vvp ) == VIK_COORD_UTM && ! vik_viewport_is_one_zone ( vvp ) ) {
      /* UTM multi-zone stuff by Kit Transue */
      gchar leftmost_zone, rightmost_zone, i;
      leftmost_zone = vik_viewport_leftmost_zone( vvp );
      rightmost_zone = vik_viewport_rightmost_zone( vvp );
      for ( i = leftmost_zone; i <= rightmost_zone; ++i ) {
        vik_viewport_corners_for_zonen ( vvp, i, &ul, &br );
        maps_layer_draw_section ( vml, vvp, &ul, &br );
      }
    }
    else {
      vik_viewport_screen_to_coord ( vvp, 0, 0, &ul );
      vik_viewport_screen_to_coord ( vvp, vik_viewport_get_width(vvp), vik_viewport_get_height(vvp), &br );

      maps_layer_draw_section ( vml, vvp, &ul, &br );
    }
  }
}

/*************************/
/****** DOWNLOADING ******/
/*************************/

/* pass along data to thread, exists even if layer is deleted. */
typedef struct {
  gchar *cache_dir;
  gchar *filename_buf;
  gint x0, y0, xf, yf;
  MapCoord mapcoord;
  gint maptype;
  gint maxlen;
  gint mapstoget;
  gint redownload;
} MapDownloadInfo;

static void mdi_free ( MapDownloadInfo *mdi )
{
  g_free ( mdi->cache_dir );
  g_free ( mdi->filename_buf );
  g_free ( mdi );
}

static void map_download_thread ( MapDownloadInfo *mdi, gpointer threaddata )
{
  guint donemaps = 0;
  gint x, y;
  for ( x = mdi->x0; x <= mdi->xf; x++ )
  {
    for ( y = mdi->y0; y <= mdi->yf; y++ )
    {
      g_snprintf ( mdi->filename_buf, mdi->maxlen, DIRSTRUCTURE,
                     mdi->cache_dir, __map_types[mdi->maptype].uniq_id,
                     mdi->mapcoord.scale, mdi->mapcoord.z, x, y );

      if ( mdi->redownload == REDOWNLOAD_ALL)
        remove ( mdi->filename_buf );
      else if ( mdi->redownload == REDOWNLOAD_BAD && access ( mdi->filename_buf, F_OK ) == 0 )
      {
        /* see if this one is bad or what */
        GError *gx = NULL;
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file ( mdi->filename_buf, &gx );
        if (gx || (!pixbuf))
          remove ( mdi->filename_buf );
        if ( pixbuf )
          g_object_unref ( pixbuf );
        if ( gx )
          g_error_free ( gx );
      }

      if ( access ( mdi->filename_buf, F_OK ) != 0 )
      {
        mdi->mapcoord.x = x; mdi->mapcoord.y = y;
        __map_types[mdi->maptype].download ( &(mdi->mapcoord), mdi->filename_buf );
        mdi->mapcoord.x = mdi->mapcoord.y = 0; /* we're temporarily between downloads */

       if ( mdi->redownload !=- REDOWNLOAD_NONE )
         a_mapcache_remove_all_shrinkfactors ( x, y, mdi->mapcoord.z, mdi->maptype, mdi->mapcoord.scale );


        donemaps++;
        a_background_thread_progress ( threaddata, ((gdouble)donemaps) / mdi->mapstoget ); /* this also calls testcancel */
      }
    }
  }
}

static void mdi_cancel_cleanup ( MapDownloadInfo *mdi )
{
  if ( mdi->mapcoord.x || mdi->mapcoord.y )
  {
    g_snprintf ( mdi->filename_buf, mdi->maxlen, DIRSTRUCTURE,
                     mdi->cache_dir, __map_types[mdi->maptype].uniq_id,
                     mdi->mapcoord.scale, mdi->mapcoord.z, mdi->mapcoord.x, mdi->mapcoord.y );
    if ( access ( mdi->filename_buf, F_OK ) == 0)
    {
      remove ( mdi->filename_buf );
    }
  }
}

static void start_download_thread ( VikMapsLayer *vml, VikViewport *vvp, const VikCoord *ul, const VikCoord *br, gint redownload )
{
  gdouble xzoom = vml->xmapzoom ? vml->xmapzoom : vik_viewport_get_xmpp ( vvp );
  gdouble yzoom = vml->ymapzoom ? vml->ymapzoom : vik_viewport_get_ympp ( vvp );
  MapCoord ulm, brm;
  if ( __map_types[vml->maptype].coord_to_mapcoord ( ul, xzoom, yzoom, &ulm ) 
    && __map_types[vml->maptype].coord_to_mapcoord ( br, xzoom, yzoom, &brm ) )
  {
    MapDownloadInfo *mdi = g_malloc ( sizeof(MapDownloadInfo) );
    gint a, b;

    /* cache_dir and buffer for dest filename */
    mdi->cache_dir = g_strdup ( vml->cache_dir );
    mdi->maxlen = strlen ( vml->cache_dir ) + 40;
    mdi->filename_buf = g_malloc ( mdi->maxlen * sizeof(gchar) );
    mdi->maptype = vml->maptype;

    mdi->mapcoord = ulm;

    mdi->redownload = redownload;

    mdi->x0 = MIN(ulm.x, brm.x);
    mdi->xf = MAX(ulm.x, brm.x);
    mdi->y0 = MIN(ulm.y, brm.y);
    mdi->yf = MAX(ulm.y, brm.y);

    mdi->mapstoget = 0;

    if ( mdi->redownload ) {
      mdi->mapstoget = (mdi->xf - mdi->x0 + 1) * (mdi->yf - mdi->y0 + 1);
    } else {
      /* calculate how many we need */
      for ( a = mdi->x0; a <= mdi->xf; a++ )
      {
        for ( b = mdi->y0; b <= mdi->yf; b++ )
        {
          g_snprintf ( mdi->filename_buf, mdi->maxlen, DIRSTRUCTURE,
                       vml->cache_dir, __map_types[vml->maptype].uniq_id, ulm.scale,
                       ulm.z, a, b );
          if ( access ( mdi->filename_buf, F_OK ) != 0)
            mdi->mapstoget++;
        }
      }
    }

    mdi->mapcoord.x = mdi->mapcoord.y = 0; /* for cleanup -- no current map */

    if ( mdi->mapstoget )
    {
      gchar *tmp = g_strdup_printf ( "%s %s%d %s %s...", redownload ? "Redownloading" : "Downloading", redownload == REDOWNLOAD_BAD ? "up to " : "", mdi->mapstoget, params_maptypes[vml->maptype], (mdi->mapstoget == 1) ? "map" : "maps" );

      /* launch the thread */
      a_background_thread ( VIK_GTK_WINDOW_FROM_LAYER(vml), /* parent window */
                            tmp,                                              /* description string */
                            (vik_thr_func) map_download_thread,               /* function to call within thread */
                            mdi,                                              /* pass along data */
                            (vik_thr_free_func) mdi_free,                     /* function to free pass along data */
                            (vik_thr_free_func) mdi_cancel_cleanup,
                            mdi->mapstoget );
      g_free ( tmp );
    }
    else
      mdi_free ( mdi );
  }
}

static void maps_layer_redownload_bad ( VikMapsLayer *vml )
{
  start_download_thread ( vml, vml->redownload_vvp, &(vml->redownload_ul), &(vml->redownload_br), REDOWNLOAD_BAD );
}
static void maps_layer_redownload_all ( VikMapsLayer *vml )
{
  start_download_thread ( vml, vml->redownload_vvp, &(vml->redownload_ul), &(vml->redownload_br), REDOWNLOAD_ALL );
}

static gboolean maps_layer_download_release ( VikMapsLayer *vml, GdkEventButton *event, VikViewport *vvp )
{
  if (!vml || vml->vl.type != VIK_LAYER_MAPS)
    return FALSE;
  if ( vml->dl_tool_x != -1 && vml->dl_tool_y != -1 )
  {
    if ( event->button == 1 )
    {
      VikCoord ul, br;
      vik_viewport_screen_to_coord ( vvp, MAX(0, MIN(event->x, vml->dl_tool_x)), MAX(0, MIN(event->y, vml->dl_tool_y)), &ul );
      vik_viewport_screen_to_coord ( vvp, MIN(vik_viewport_get_width(vvp), MAX(event->x, vml->dl_tool_x)), MIN(vik_viewport_get_height(vvp), MAX ( event->y, vml->dl_tool_y ) ), &br );
      start_download_thread ( vml, vvp, &ul, &br, REDOWNLOAD_NONE );
      vml->dl_tool_x = vml->dl_tool_y = -1;
      return TRUE;
    }
    else
    {
      vik_viewport_screen_to_coord ( vvp, MAX(0, MIN(event->x, vml->dl_tool_x)), MAX(0, MIN(event->y, vml->dl_tool_y)), &(vml->redownload_ul) );
      vik_viewport_screen_to_coord ( vvp, MIN(vik_viewport_get_width(vvp), MAX(event->x, vml->dl_tool_x)), MIN(vik_viewport_get_height(vvp), MAX ( event->y, vml->dl_tool_y ) ), &(vml->redownload_br) );

      vml->redownload_vvp = vvp;

      vml->dl_tool_x = vml->dl_tool_y = -1;

      if ( ! vml->dl_right_click_menu ) {
        GtkWidget *item;
        vml->dl_right_click_menu = GTK_MENU ( gtk_menu_new () );

        item = gtk_menu_item_new_with_label ( "Redownload bad map(s)" );
        g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(maps_layer_redownload_bad), vml );
        gtk_menu_shell_append ( GTK_MENU_SHELL(vml->dl_right_click_menu), item );

        item = gtk_menu_item_new_with_label ( "Redownload all map(s)" );
        g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(maps_layer_redownload_all), vml );
        gtk_menu_shell_append ( GTK_MENU_SHELL(vml->dl_right_click_menu), item );
      }

      gtk_menu_popup ( vml->dl_right_click_menu, NULL, NULL, NULL, NULL, event->button, event->time );
      gtk_widget_show_all ( GTK_WIDGET(vml->dl_right_click_menu) );
    }
  }
  return FALSE;
}

static gpointer maps_layer_download_create ( VikWindow *vw, VikViewport *vvp)
{
  return vvp;
}

static gboolean maps_layer_download_click ( VikMapsLayer *vml, GdkEventButton *event, VikViewport *vvp )
{
  MapCoord tmp;
  if (!vml || vml->vl.type != VIK_LAYER_MAPS)
    return FALSE;
  if ( __map_types[vml->maptype].drawmode == vik_viewport_get_drawmode ( vvp ) &&
    __map_types[vml->maptype].coord_to_mapcoord ( vik_viewport_get_center ( vvp ),
           vml->xmapzoom ? vml->xmapzoom : vik_viewport_get_xmpp ( vvp ),
           vml->ymapzoom ? vml->ymapzoom : vik_viewport_get_ympp ( vvp ),
           &tmp ) ) {
    vml->dl_tool_x = event->x, vml->dl_tool_y = event->y;
    return TRUE;
  }
  return FALSE;

 
#if 0
  if ( __map_types[vml->maptype].drawmode == vik_viewport_get_drawmode ( vvp ) )
  {
    VikCoord coord;
    MapCoord mapcoord;
    vik_viewport_screen_to_coord ( vvp, event->x, event->y, &coord );
    if ( __map_types[vml->maptype].coord_to_mapcoord ( &coord,
                vml->xmapzoom ? vml->xmapzoom : vik_viewport_get_xmpp ( vvp ),
                vml->ymapzoom ? vml->ymapzoom : vik_viewport_get_ympp ( vvp ),
                &mapcoord ) ) {
      gchar *filename_buf = g_strdup_printf ( DIRSTRUCTURE,
                     vml->cache_dir, __map_types[vml->maptype].uniq_id,
                     mapcoord.scale, mapcoord.z, mapcoord.x, mapcoord.y );

      __map_types[vml->maptype].download ( &mapcoord, filename_buf );
      g_free ( filename_buf );
      vik_layer_emit_update ( VIK_LAYER(vml) );
      return TRUE;
    }
  }
  return FALSE;
#endif
}

static void maps_layer_download_onscreen_maps ( gpointer vml_vvp[2] )
{
  VikMapsLayer *vml = vml_vvp[0];
  VikViewport *vvp = vml_vvp[1];

  gdouble xzoom = vml->xmapzoom ? vml->xmapzoom : vik_viewport_get_xmpp ( vvp );
  gdouble yzoom = vml->ymapzoom ? vml->ymapzoom : vik_viewport_get_ympp ( vvp );

  VikCoord ul, br;
  MapCoord ulm, brm;

  vik_viewport_screen_to_coord ( vvp, 0, 0, &ul );
  vik_viewport_screen_to_coord ( vvp, vik_viewport_get_width(vvp), vik_viewport_get_height(vvp), &br );

  if ( __map_types[vml->maptype].drawmode == vik_viewport_get_drawmode ( vvp ) &&
       __map_types[vml->maptype].coord_to_mapcoord ( &ul, xzoom, yzoom, &ulm ) &&
       __map_types[vml->maptype].coord_to_mapcoord ( &br, xzoom, yzoom, &brm ) )
    start_download_thread ( vml, vvp, &ul, &br, REDOWNLOAD_NONE );
  else
    a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_LAYER(vml), "Wrong drawmode / zoom level for this map." );

}

static void maps_layer_add_menu_items ( VikMapsLayer *vml, GtkMenu *menu, VikLayersPanel *vlp )
{
  static gpointer pass_along[2];
  GtkWidget *item;
  pass_along[0] = vml;
  pass_along[1] = vik_layers_panel_get_viewport( VIK_LAYERS_PANEL(vlp) );

  item = gtk_menu_item_new();
  gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
  gtk_widget_show ( item );

  item = gtk_menu_item_new_with_label ( "Download Onscreen Maps" );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(maps_layer_download_onscreen_maps), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );
}
