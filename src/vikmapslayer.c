/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2005, Evan Battaglia <viking@greentorch.org>
 * Copyright (C) 2010, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * UTM multi-zone stuff by Kit Transue <notlostyet@didactek.com>
 * Dynamic map type by Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#define MAX_SHRINKFACTOR 8.0000001 /* zoom 1 viewing 8-tiles */
#define MIN_SHRINKFACTOR 0.0312499 /* zoom 32 viewing 1-tiles */

#define REAL_MIN_SHRINKFACTOR 0.0039062499 /* if shrinkfactor is between MAX and REAL_MAX, will only check for existence */

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include "globals.h"
#include "util.h"
#include "coords.h"
#include "vikcoord.h"
#include "viktreeview.h"
#include "vikviewport.h"
#include "viklayer.h"
#include "vikmapslayer.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "mapcache.h"
/* only for dialog.h -- ugh */
#include "vikwaypoint.h"
#include "dialog.h"
#include "preferences.h"

#include "vikstatus.h"
#include "background.h"

#include "vikaggregatelayer.h"
#include "viklayerspanel.h"

#include "mapcoord.h"
#include "terraserver.h"

#include "icons/icons.h"

/****** MAP TYPES ******/

static GList *__map_types = NULL;

#define NUM_MAP_TYPES g_list_length(__map_types)

/* List of label for each map type */
static gchar **params_maptypes = NULL;

/* Corresponding IDS. (Cf. field uniq_id in VikMapsLayer struct) */
static guint *params_maptypes_ids = NULL;

/******** MAPZOOMS *********/

static gchar *params_mapzooms[] = { N_("Use Viking Zoom Level"), "0.25", "0.5", "1", "2", "4", "8", "16", "32", "64", "128", "256", "512", "1024", "USGS 10k", "USGS 24k", "USGS 25k", "USGS 50k", "USGS 100k", "USGS 200k", "USGS 250k", NULL };
static gdouble __mapzooms_x[] = { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0, 512.0, 1024.0, 1.016, 2.4384, 2.54, 5.08, 10.16, 20.32, 25.4 };
static gdouble __mapzooms_y[] = { 0.0, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0, 512.0, 1024.0, 1.016, 2.4384, 2.54, 5.08, 10.16, 20.32, 25.4 };

#define NUM_MAPZOOMS (sizeof(params_mapzooms)/sizeof(params_mapzooms[0]) - 1)

/**************************/


static void maps_layer_post_read (VikLayer *vl, VikViewport *vp, gboolean from_file);
static const gchar* maps_layer_tooltip ( VikMapsLayer *vml );
static void maps_layer_marshall( VikMapsLayer *vml, guint8 **data, gint *len );
static VikMapsLayer *maps_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp );
static gboolean maps_layer_set_param ( VikMapsLayer *vml, guint16 id, VikLayerParamData data, VikViewport *vvp, gboolean is_file_operation );
static VikLayerParamData maps_layer_get_param ( VikMapsLayer *vml, guint16 id, gboolean is_file_operation );
static void maps_layer_draw ( VikMapsLayer *vml, VikViewport *vvp );
static VikMapsLayer *maps_layer_new ( VikViewport *vvp );
static void maps_layer_free ( VikMapsLayer *vml );
static gboolean maps_layer_download_release ( VikMapsLayer *vml, GdkEventButton *event, VikViewport *vvp );
static gboolean maps_layer_download_click ( VikMapsLayer *vml, GdkEventButton *event, VikViewport *vvp );
static gpointer maps_layer_download_create ( VikWindow *vw, VikViewport *vvp );
static void maps_layer_set_cache_dir ( VikMapsLayer *vml, const gchar *dir );
static void start_download_thread ( VikMapsLayer *vml, VikViewport *vvp, const VikCoord *ul, const VikCoord *br, gint redownload );
static void maps_layer_add_menu_items ( VikMapsLayer *vml, GtkMenu *menu, VikLayersPanel *vlp );
static guint map_uniq_id_to_index ( guint uniq_id );


static VikLayerParamScale params_scales[] = {
  /* min, max, step, digits (decimal places) */
 { 0, 255, 3, 0 }, /* alpha */
};

VikLayerParam maps_layer_params[] = {
  { "mode", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Map Type:"), VIK_LAYER_WIDGET_COMBOBOX, NULL, NULL },
  { "directory", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("Maps Directory:"), VIK_LAYER_WIDGET_FOLDERENTRY },
  { "alpha", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Alpha:"), VIK_LAYER_WIDGET_HSCALE, params_scales },
  { "autodownload", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Autodownload maps:"), VIK_LAYER_WIDGET_CHECKBUTTON },
  { "mapzoom", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Zoom Level:"), VIK_LAYER_WIDGET_COMBOBOX, params_mapzooms, NULL },
};

enum { PARAM_MAPTYPE=0, PARAM_CACHE_DIR, PARAM_ALPHA, PARAM_AUTODOWNLOAD, PARAM_MAPZOOM, NUM_PARAMS };

static VikToolInterface maps_tools[] = {
  { N_("Maps Download"), (VikToolConstructorFunc) maps_layer_download_create, NULL, NULL, NULL,  
    (VikToolMouseFunc) maps_layer_download_click, NULL,  (VikToolMouseFunc) maps_layer_download_release,
    (VikToolKeyFunc) NULL, GDK_CURSOR_IS_PIXMAP, &cursor_mapdl_pixbuf },
};

VikLayerInterface vik_maps_layer_interface = {
  N_("Map"),
  &vikmapslayer_pixbuf,

  maps_tools,
  sizeof(maps_tools) / sizeof(maps_tools[0]),

  maps_layer_params,
  NUM_PARAMS,
  NULL,
  0,

  VIK_MENU_ITEM_ALL,

  (VikLayerFuncCreate)                  maps_layer_new,
  (VikLayerFuncRealize)                 NULL,
  (VikLayerFuncPostRead)                maps_layer_post_read,
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
  (VikLayerFuncSublayerTooltip)         NULL,
  (VikLayerFuncLayerTooltip)            maps_layer_tooltip,
  (VikLayerFuncLayerSelected)           NULL,

  (VikLayerFuncMarshall)		maps_layer_marshall,
  (VikLayerFuncUnmarshall)		maps_layer_unmarshall,

  (VikLayerFuncSetParam)                maps_layer_set_param,
  (VikLayerFuncGetParam)                maps_layer_get_param,

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

struct _VikMapsLayer {
  VikLayer vl;
  guint maptype;
  gchar *cache_dir;
  guint8 alpha;
  guint mapzoom_id;
  gdouble xmapzoom, ymapzoom;

  gboolean autodownload;
  VikCoord *last_center;
  gdouble last_xmpp;
  gdouble last_ympp;

  gint dl_tool_x, dl_tool_y;

  GtkMenu *dl_right_click_menu;
  VikCoord redownload_ul, redownload_br; /* right click menu only */
  VikViewport *redownload_vvp;

  gboolean license_notice_shown; // FALSE for new maps only, otherwise
                                 // TRUE for saved maps & other layer changes as we don't need to show it again
};

enum { REDOWNLOAD_NONE = 0,    /* download only missing maps */
       REDOWNLOAD_BAD,         /* download missing and bad maps */
       REDOWNLOAD_NEW,         /* download missing maps that are newer on server only */
       REDOWNLOAD_ALL,         /* download all maps */
       DOWNLOAD_OR_REFRESH };  /* download missing maps and refresh cache */

static VikLayerParam prefs[] = {
  { VIKING_PREFERENCES_NAMESPACE "maplayer_default_dir", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("Default maplayer directory:"), VIK_LAYER_WIDGET_FOLDERENTRY, NULL, NULL },
};

void maps_layer_init ()
{
  VikLayerParamData tmp;
  tmp.s = maps_layer_default_dir();
  a_preferences_register(prefs, tmp, VIKING_PREFERENCES_GROUP_KEY);
}

/****************************************/
/******** MAPS LAYER TYPES **************/
/****************************************/

int _get_index_for_id ( guint id )
{
  int index = 0 ;
  while (params_maptypes_ids[index] != 0)
  {
    if (params_maptypes_ids[index] == id)
      return index;
    index++;
  }
  return -1;
}

void _add_map_source ( guint id, const char *label, VikMapSource *map )
{
  gsize len = 0;
  if (params_maptypes)
    len = g_strv_length (params_maptypes);
  /* Add the label */
  params_maptypes = g_realloc (params_maptypes, (len+2)*sizeof(gchar*));
  params_maptypes[len] = g_strdup (label);
  params_maptypes[len+1] = NULL;

  /* Add the id */
  params_maptypes_ids = g_realloc (params_maptypes_ids, (len+2)*sizeof(guint));
  params_maptypes_ids[len] = id;
  params_maptypes_ids[len+1] = 0;

  /* We have to clone */
  VikMapSource *clone = VIK_MAP_SOURCE(g_object_ref(map));
  /* Register the clone in the list */
  __map_types = g_list_append(__map_types, clone);

  /* Hack
     We have to ensure the mode LayerParam references the up-to-date
     GLists.
  */
  /*
  memcpy(&maps_layer_params[0].widget_data, &params_maptypes, sizeof(gpointer));
  memcpy(&maps_layer_params[0].extra_widget_data, &params_maptypes_ids, sizeof(gpointer));
  */
  maps_layer_params[0].widget_data = params_maptypes;
  maps_layer_params[0].extra_widget_data = params_maptypes_ids;
}

void _update_map_source ( const char *label, VikMapSource *map, int index )
{
  GList *item = g_list_nth (__map_types, index);
  g_object_unref (item->data);
  item->data = g_object_ref (map);
  /* Change previous data */
  g_free (params_maptypes[index]);
  params_maptypes[index] = g_strdup (label);
}

void maps_layer_register_map_source ( VikMapSource *map )
{
  g_assert(map != NULL);
  
  guint id = vik_map_source_get_uniq_id(map);
  const char *label = vik_map_source_get_label(map);
  g_assert(label != NULL);

  int previous = map_uniq_id_to_index (id);
  if (previous != NUM_MAP_TYPES)
  {
    _update_map_source (label, map, previous);
  }
  else
  {
    _add_map_source (id, label, map);
  }
}

#define MAPS_LAYER_NTH_LABEL(n) (params_maptypes[n])
#define MAPS_LAYER_NTH_ID(n) (params_maptypes_ids[n])
#define MAPS_LAYER_NTH_TYPE(n) (VIK_MAP_SOURCE(g_list_nth_data(__map_types, (n))))

gint vik_maps_layer_get_map_type(VikMapsLayer *vml)
{
  return(vml->maptype);
}

gchar *vik_maps_layer_get_map_label(VikMapsLayer *vml)
{
  return(g_strdup(MAPS_LAYER_NTH_LABEL(vml->maptype)));
}

/****************************************/
/******** CACHE DIR STUFF ***************/
/****************************************/

#define DIRSTRUCTURE "%st%ds%dz%d" G_DIR_SEPARATOR_S "%d" G_DIR_SEPARATOR_S "%d"
#define MAPS_CACHE_DIR maps_layer_default_dir()

#ifdef WINDOWS
#include <io.h>
#define GLOBAL_MAPS_DIR "C:\\VIKING-MAPS\\"
#define LOCAL_MAPS_DIR "VIKING-MAPS"
#elif defined __APPLE__
#include <stdlib.h>
#define GLOBAL_MAPS_DIR "/Library/cache/Viking/maps/"
#define LOCAL_MAPS_DIR "/Library/Application Support/Viking/viking-maps"
#else /* POSIX */
#include <stdlib.h>
#define GLOBAL_MAPS_DIR "/var/cache/maps/"
#define LOCAL_MAPS_DIR ".viking-maps"
#endif

gchar *maps_layer_default_dir ()
{
  static gchar *defaultdir = NULL;
  if ( ! defaultdir )
  {
    /* Thanks to Mike Davison for the $VIKING_MAPS usage */
    const gchar *mapdir = g_getenv("VIKING_MAPS");
    if ( mapdir ) {
      defaultdir = g_strdup ( mapdir );
    } else if ( g_access ( GLOBAL_MAPS_DIR, W_OK ) == 0 ) {
      defaultdir = g_strdup ( GLOBAL_MAPS_DIR );
    } else {
      const gchar *home = g_get_home_dir();
      if (!home || g_access(home, W_OK))
        home = g_get_home_dir ();
      if ( home )
        defaultdir = g_build_filename ( home, LOCAL_MAPS_DIR, NULL );
      else
        defaultdir = g_strdup ( LOCAL_MAPS_DIR );
    }
    if (defaultdir && (defaultdir[strlen(defaultdir)-1] != G_DIR_SEPARATOR))
    {
      /* Add the separator at the end */
      gchar *tmp = defaultdir;
      defaultdir = g_strconcat(tmp, G_DIR_SEPARATOR_S, NULL);
      g_free(tmp);
    }
    g_debug("%s: defaultdir=%s", __FUNCTION__, defaultdir);
  }
  return defaultdir;
}

static void maps_layer_mkdir_if_default_dir ( VikMapsLayer *vml )
{
  if ( vml->cache_dir && strcmp ( vml->cache_dir, MAPS_CACHE_DIR ) == 0 && g_file_test ( vml->cache_dir, G_FILE_TEST_EXISTS ) == FALSE )
  {
    g_mkdir ( vml->cache_dir, 0777 );
  }
}

static void maps_layer_set_cache_dir ( VikMapsLayer *vml, const gchar *dir )
{
  guint len;
  g_assert ( vml != NULL);
  g_free ( vml->cache_dir );
  vml->cache_dir = NULL;

  if ( dir == NULL || dir[0] == '\0' )
  {
    if ( a_preferences_get(VIKING_PREFERENCES_NAMESPACE "maplayer_default_dir") )
      vml->cache_dir = g_strdup ( a_preferences_get(VIKING_PREFERENCES_NAMESPACE "maplayer_default_dir")->s );
  }
  else
  {
    len = strlen(dir);
    if ( dir[len-1] != G_DIR_SEPARATOR )
    {
      vml->cache_dir = g_malloc ( len+2 );
      strncpy ( vml->cache_dir, dir, len );
      vml->cache_dir[len] = G_DIR_SEPARATOR;
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
  return vik_map_source_get_uniq_id(MAPS_LAYER_NTH_TYPE(index));
}

static guint map_uniq_id_to_index ( guint uniq_id )
{
  gint i;
  for ( i = 0; i < NUM_MAP_TYPES; i++ )
    if ( vik_map_source_get_uniq_id(MAPS_LAYER_NTH_TYPE(i)) == uniq_id )
      return i;
  return NUM_MAP_TYPES; /* no such thing */
}

static gboolean maps_layer_set_param ( VikMapsLayer *vml, guint16 id, VikLayerParamData data, VikViewport *vvp, gboolean is_file_operation )
{
  // When loading from a file don't need the license reminder
  if ( is_file_operation )
    vml->license_notice_shown = TRUE;

  switch ( id )
  {
    case PARAM_CACHE_DIR: maps_layer_set_cache_dir ( vml, data.s ); break;
    case PARAM_MAPTYPE: {
      gint maptype = map_uniq_id_to_index(data.u);
      if ( maptype == NUM_MAP_TYPES ) g_warning(_("Unknown map type"));
      else vml->maptype = maptype;
      break;
    }
    case PARAM_ALPHA: if ( data.u <= 255 ) vml->alpha = data.u; break;
    case PARAM_AUTODOWNLOAD: vml->autodownload = data.b; break;
    case PARAM_MAPZOOM: if ( data.u < NUM_MAPZOOMS ) {
                          vml->mapzoom_id = data.u;
                          vml->xmapzoom = __mapzooms_x [data.u];
                          vml->ymapzoom = __mapzooms_y [data.u];
                        }else g_warning (_("Unknown Map Zoom")); break;
  }
  return TRUE;
}

static VikLayerParamData maps_layer_get_param ( VikMapsLayer *vml, guint16 id, gboolean is_file_operation )
{
  VikLayerParamData rv;
  switch ( id )
  {
    case PARAM_CACHE_DIR: rv.s = vml->cache_dir ? vml->cache_dir : ""; break;
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
  int idx;
  VikMapsLayer *vml = VIK_MAPS_LAYER ( g_object_new ( VIK_MAPS_LAYER_TYPE, NULL ) );
  vik_layer_init ( VIK_LAYER(vml), VIK_LAYER_MAPS );
  idx = map_uniq_id_to_index(19); /* 19 is id for OSM MapQuest maps */
    vml->maptype = (idx < NUM_MAP_TYPES) ? idx : 0;
  vml->alpha = 255;
  vml->mapzoom_id = 0;
  vml->dl_tool_x = vml->dl_tool_y = -1;
  maps_layer_set_cache_dir ( vml, NULL );
  vml->autodownload = FALSE;
  vml->last_center = NULL;
  vml->last_xmpp = 0.0;
  vml->last_ympp = 0.0;

  vml->dl_right_click_menu = NULL;
  vml->license_notice_shown = FALSE;

  return vml;
}

static void maps_layer_free ( VikMapsLayer *vml )
{
  g_free ( vml->cache_dir );
  vml->cache_dir = NULL;
  if ( vml->dl_right_click_menu )
    g_object_ref_sink ( G_OBJECT(vml->dl_right_click_menu) );
  g_free(vml->last_center);
  vml->last_center = NULL;
}

static void maps_layer_post_read (VikLayer *vl, VikViewport *vp, gboolean from_file)
{
  if (from_file != TRUE)
  {
    /* If this method is not called in file reading context
     * it is called in GUI context.
     * So, we can check if we have to inform the user about inconsistency */
    VikViewportDrawMode vp_drawmode;
    VikMapsLayer *vml = VIK_MAPS_LAYER(vl);
    VikMapSource *map = NULL;
 
    vp_drawmode = vik_viewport_get_drawmode ( vp );
    map = MAPS_LAYER_NTH_TYPE(vml->maptype);
    if (vik_map_source_get_drawmode(map) != vp_drawmode) {
      const gchar *drawmode_name = vik_viewport_get_drawmode_name (vp, vik_map_source_get_drawmode(map));
      gchar *msg = g_strdup_printf(_("New map cannot be displayed in the current drawmode.\nSelect \"%s\" from View menu to view it."), drawmode_name);
      a_dialog_warning_msg ( VIK_GTK_WINDOW_FROM_WIDGET(vp), msg );
      g_free(msg);
    }

    if (vik_map_source_get_license (map) != NULL) {
      if ( ! vml->license_notice_shown ) {
	a_dialog_license (VIK_GTK_WINDOW_FROM_WIDGET(vp), vik_map_source_get_label (map),
			  vik_map_source_get_license (map), vik_map_source_get_license_url (map) );
	vml->license_notice_shown = TRUE;
      }
    }
  }
}

static const gchar* maps_layer_tooltip ( VikMapsLayer *vml )
{
  return vik_maps_layer_get_map_label ( vml );
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
  tmp = gdk_pixbuf_scale_simple(pixbuf, ceil(width * xshrinkfactor), ceil(height * yshrinkfactor), GDK_INTERP_NEAREST);
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
    if ( g_file_test ( filename_buf, G_FILE_TEST_EXISTS ) == TRUE)
    {
      GError *gx = NULL;
      pixbuf = gdk_pixbuf_new_from_file ( filename_buf, &gx );

      /* free the pixbuf on error */
      if (gx)
      {
        if ( gx->domain != GDK_PIXBUF_ERROR || gx->code != GDK_PIXBUF_ERROR_CORRUPT_IMAGE )
          g_warning ( _("Couldn't open image file: %s"), gx->message );

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
              mapcoord->z, vik_map_source_get_uniq_id(MAPS_LAYER_NTH_TYPE(vml->maptype)),
              mapcoord->scale, vml->alpha, xshrinkfactor, yshrinkfactor );
      }
    }
  }
  return pixbuf;
}

gboolean should_start_autodownload(VikMapsLayer *vml, VikViewport *vvp)
{
  const VikCoord *center = vik_viewport_get_center ( vvp );

  if (vik_window_get_pan_move (VIK_WINDOW(VIK_GTK_WINDOW_FROM_WIDGET(GTK_WIDGET(vvp)))))
    /* D'n'D pan in action: do not download */
    return FALSE;

  if (vml->last_center == NULL) {
    VikCoord *new_center = g_malloc(sizeof(VikCoord));
    *new_center = *center;
    vml->last_center = new_center;
    vml->last_xmpp = vik_viewport_get_xmpp(vvp);
    vml->last_ympp = vik_viewport_get_ympp(vvp);
    return TRUE;
  }

  /* TODO: perhaps vik_coord_diff() */
  if (vik_coord_equals(vml->last_center, center)
      && (vml->last_xmpp == vik_viewport_get_xmpp(vvp))
      && (vml->last_ympp == vik_viewport_get_ympp(vvp)))
    return FALSE;

  *(vml->last_center) = *center;
    vml->last_xmpp = vik_viewport_get_xmpp(vvp);
    vml->last_ympp = vik_viewport_get_ympp(vvp);
  return TRUE;
}

static void maps_layer_draw_section ( VikMapsLayer *vml, VikViewport *vvp, VikCoord *ul, VikCoord *br )
{
  MapCoord ulm, brm;
  gdouble xzoom = vik_viewport_get_xmpp ( vvp );
  gdouble yzoom = vik_viewport_get_ympp ( vvp );
  gdouble xshrinkfactor = 1.0, yshrinkfactor = 1.0;
  gdouble existence_only = FALSE;

  if ( vml->xmapzoom && (vml->xmapzoom != xzoom || vml->ymapzoom != yzoom) ) {
    xshrinkfactor = vml->xmapzoom / xzoom;
    yshrinkfactor = vml->ymapzoom / yzoom;
    xzoom = vml->xmapzoom;
    yzoom = vml->xmapzoom;
    if ( ! (xshrinkfactor > MIN_SHRINKFACTOR && xshrinkfactor < MAX_SHRINKFACTOR &&
         yshrinkfactor > MIN_SHRINKFACTOR && yshrinkfactor < MAX_SHRINKFACTOR ) ) {
      if ( xshrinkfactor > REAL_MIN_SHRINKFACTOR && yshrinkfactor > REAL_MIN_SHRINKFACTOR )
        existence_only = TRUE;
      else {
        g_warning ( _("Cowardly refusing to draw tiles or existence of tiles beyond %d zoom out factor"), (int)( 1.0/REAL_MIN_SHRINKFACTOR));
        return;
      }
    }
  }

  /* coord -> ID */
  VikMapSource *map = MAPS_LAYER_NTH_TYPE(vml->maptype);
  if ( vik_map_source_coord_to_mapcoord ( map, ul, xzoom, yzoom, &ulm ) &&
       vik_map_source_coord_to_mapcoord ( map, br, xzoom, yzoom, &brm ) ) {

    /* loop & draw */
    gint x, y;
    gint xmin = MIN(ulm.x, brm.x), xmax = MAX(ulm.x, brm.x);
    gint ymin = MIN(ulm.y, brm.y), ymax = MAX(ulm.y, brm.y);
    gint mode = vik_map_source_get_uniq_id(map);

    VikCoord coord;
    gint xx, yy, width, height;
    GdkPixbuf *pixbuf;

    guint max_path_len = strlen(vml->cache_dir) + 40;
    gchar *path_buf = g_malloc ( max_path_len * sizeof(char) );

    if ( (!existence_only) && vml->autodownload  && should_start_autodownload(vml, vvp)) {
      g_debug("%s: Starting autodownload", __FUNCTION__);
      if ( vik_map_source_supports_download_only_new (map) )
        // Try to download newer tiles
        start_download_thread ( vml, vvp, ul, br, REDOWNLOAD_NEW );
      else
        // Download only missing tiles
        start_download_thread ( vml, vvp, ul, br, REDOWNLOAD_NONE );
    }

    if ( vik_map_source_get_tilesize_x(map) == 0 && !existence_only ) {
      for ( x = xmin; x <= xmax; x++ ) {
        for ( y = ymin; y <= ymax; y++ ) {
          ulm.x = x;
          ulm.y = y;
          pixbuf = get_pixbuf ( vml, mode, &ulm, path_buf, max_path_len, xshrinkfactor, yshrinkfactor );
          if ( pixbuf ) {
            width = gdk_pixbuf_get_width ( pixbuf );
            height = gdk_pixbuf_get_height ( pixbuf );

            vik_map_source_mapcoord_to_center_coord ( map, &ulm, &coord );
            vik_viewport_coord_to_screen ( vvp, &coord, &xx, &yy );
            xx -= (width/2);
            yy -= (height/2);

            vik_viewport_draw_pixbuf ( vvp, pixbuf, 0, 0, xx, yy, width, height );
          }
        }
      }
    } else { /* tilesize is known, don't have to keep converting coords */
      gdouble tilesize_x = vik_map_source_get_tilesize_x(map) * xshrinkfactor;
      gdouble tilesize_y = vik_map_source_get_tilesize_y(map) * yshrinkfactor;
      /* ceiled so tiles will be maximum size in the case of funky shrinkfactor */
      gint tilesize_x_ceil = ceil ( tilesize_x );
      gint tilesize_y_ceil = ceil ( tilesize_y );
      gint8 xinc = (ulm.x == xmin) ? 1 : -1;
      gint8 yinc = (ulm.y == ymin) ? 1 : -1;
      gdouble xx, yy; gint xx_tmp, yy_tmp;
      gint base_yy, xend, yend;

      xend = (xinc == 1) ? (xmax+1) : (xmin-1);
      yend = (yinc == 1) ? (ymax+1) : (ymin-1);

      vik_map_source_mapcoord_to_center_coord ( map, &ulm, &coord );
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

          if ( existence_only ) {
            g_snprintf ( path_buf, max_path_len, DIRSTRUCTURE,
                     vml->cache_dir, mode,
                     ulm.scale, ulm.z, ulm.x, ulm.y );
            if ( g_file_test ( path_buf, G_FILE_TEST_EXISTS ) == TRUE ) {
	      GdkGC *black_gc = GTK_WIDGET(vvp)->style->black_gc;
              vik_viewport_draw_line ( vvp, black_gc, xx+tilesize_x_ceil, yy, xx, yy+tilesize_y_ceil );
            }
          } else {
            int scale_inc;
            for (scale_inc = 0; scale_inc < 4; scale_inc ++) {
              /* try with correct then smaller zooms */
              int scale_factor = 1 << scale_inc;  /*  2^scale_inc */
              MapCoord ulm2 = ulm;
              ulm2.x = ulm.x / scale_factor;
              ulm2.y = ulm.y / scale_factor;
              ulm2.scale = ulm.scale + scale_inc;
              pixbuf = get_pixbuf ( vml, mode, &ulm2, path_buf, max_path_len, xshrinkfactor * scale_factor, yshrinkfactor * scale_factor );
              if ( pixbuf ) {
                gint src_x = (ulm.x % scale_factor) * tilesize_x_ceil;
                gint src_y = (ulm.y % scale_factor) * tilesize_y_ceil;
#ifdef DEBUG
                printf("maps_layer_draw_section - x=%d, y=%d, z=%d, src_x=%d, src_y=%d, xx=%d, yy=%d - %x\n", ulm.x, ulm.y, ulm.scale, src_x, src_y, (int)xx, (int)yy, vvp);
#endif
                vik_viewport_draw_pixbuf ( vvp, pixbuf, src_x, src_y, xx, yy, tilesize_x_ceil, tilesize_y_ceil );
                break;
              }
            }
            if ( !pixbuf ) {
              /* retry with bigger zooms */
              int scale_dec;
              for (scale_dec = 1; scale_dec < 2; scale_dec ++) {
                int pict_x, pict_y;
                int scale_factor = 1 << scale_dec;  /*  2^scale_dec */
                MapCoord ulm2 = ulm;
                ulm2.x = ulm.x * scale_factor;
                ulm2.y = ulm.y * scale_factor;
                ulm2.scale = ulm.scale - scale_dec;
                for (pict_x = 0; pict_x < scale_factor; pict_x ++) {
                  for (pict_y = 0; pict_y < scale_factor; pict_y ++) {
                    MapCoord ulm3 = ulm2;
                    ulm3.x += pict_x;
                    ulm3.y += pict_y;
                    pixbuf = get_pixbuf ( vml, mode, &ulm3, path_buf, max_path_len, xshrinkfactor / scale_factor, yshrinkfactor / scale_factor );
                    if ( pixbuf ) {
                      gint src_x = 0;
                      gint src_y = 0;
                      gint dest_x = xx + pict_x * (tilesize_x_ceil / scale_factor);
                      gint dest_y = yy + pict_y * (tilesize_y_ceil / scale_factor);
                      vik_viewport_draw_pixbuf ( vvp, pixbuf, src_x, src_y, dest_x, dest_y, tilesize_x_ceil / scale_factor, tilesize_y_ceil / scale_factor );
                    }
                  }
                }
              }
            }
          }

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
  if ( vik_map_source_get_drawmode(MAPS_LAYER_NTH_TYPE(vml->maptype)) == vik_viewport_get_drawmode ( vvp ) )
  {
    VikCoord ul, br;

    /* Copyright */
    gdouble level = vik_viewport_get_zoom ( vvp );
    LatLonBBox bbox;
    vik_viewport_get_min_max_lat_lon ( vvp, &bbox.south, &bbox.north, &bbox.west, &bbox.east );
    vik_map_source_get_copyright ( MAPS_LAYER_NTH_TYPE(vml->maptype), bbox, level, vik_viewport_add_copyright, vvp );

    /* Logo */
    const GdkPixbuf *logo = vik_map_source_get_logo ( MAPS_LAYER_NTH_TYPE(vml->maptype) );
    vik_viewport_add_logo ( vvp, logo );

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
  gboolean refresh_display;
  VikMapsLayer *vml;
  VikViewport *vvp;
  gboolean map_layer_alive;
  GMutex *mutex;
} MapDownloadInfo;

static void mdi_free ( MapDownloadInfo *mdi )
{
  g_mutex_free(mdi->mutex);
  g_free ( mdi->cache_dir );
  mdi->cache_dir = NULL;
  g_free ( mdi->filename_buf );
  mdi->filename_buf = NULL;
  g_free ( mdi );
}

static void weak_ref_cb(gpointer ptr, GObject * dead_vml)
{
  MapDownloadInfo *mdi = ptr;
  g_mutex_lock(mdi->mutex);
  mdi->map_layer_alive = FALSE;
  g_mutex_unlock(mdi->mutex);
}

static int map_download_thread ( MapDownloadInfo *mdi, gpointer threaddata )
{
  void *handle = vik_map_source_download_handle_init(MAPS_LAYER_NTH_TYPE(mdi->maptype));
  guint donemaps = 0;
  gint x, y;
  for ( x = mdi->x0; x <= mdi->xf; x++ )
  {
    for ( y = mdi->y0; y <= mdi->yf; y++ )
    {
      gboolean remove_mem_cache = FALSE;
      gboolean need_download = FALSE;
      g_snprintf ( mdi->filename_buf, mdi->maxlen, DIRSTRUCTURE,
                     mdi->cache_dir, vik_map_source_get_uniq_id(MAPS_LAYER_NTH_TYPE(mdi->maptype)),
                     mdi->mapcoord.scale, mdi->mapcoord.z, x, y );

      donemaps++;
      int res = a_background_thread_progress ( threaddata, ((gdouble)donemaps) / mdi->mapstoget ); /* this also calls testcancel */
      if (res != 0) {
        vik_map_source_download_handle_cleanup(MAPS_LAYER_NTH_TYPE(mdi->maptype), handle);
        return -1;
      }

      if ( g_file_test ( mdi->filename_buf, G_FILE_TEST_EXISTS ) == FALSE ) {
        need_download = TRUE;
        remove_mem_cache = TRUE;

      } else {  /* in case map file already exists */
        switch (mdi->redownload) {
          case REDOWNLOAD_NONE:
            continue;

          case REDOWNLOAD_BAD:
          {
            /* see if this one is bad or what */
            GError *gx = NULL;
            GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file ( mdi->filename_buf, &gx );
            if (gx || (!pixbuf)) {
              g_remove ( mdi->filename_buf );
              need_download = TRUE;
              remove_mem_cache = TRUE;
              g_error_free ( gx );

            } else {
              g_object_unref ( pixbuf );
            }
            break;
          }

          case REDOWNLOAD_NEW:
            need_download = TRUE;
            remove_mem_cache = TRUE;
            break;

          case REDOWNLOAD_ALL:
            /* FIXME: need a better way than to erase file in case of server/network problem */
            g_remove ( mdi->filename_buf );
            need_download = TRUE;
            remove_mem_cache = TRUE;
            break;

          case DOWNLOAD_OR_REFRESH:
            remove_mem_cache = TRUE;
            break;

          default:
            g_warning ( "redownload state %d unknown\n", mdi->redownload);
        }
      }

      mdi->mapcoord.x = x; mdi->mapcoord.y = y;

      if (need_download) {
        if ( vik_map_source_download( MAPS_LAYER_NTH_TYPE(mdi->maptype), &(mdi->mapcoord), mdi->filename_buf, handle))
          continue;
      }

      g_mutex_lock(mdi->mutex);
      if (remove_mem_cache)
          a_mapcache_remove_all_shrinkfactors ( x, y, mdi->mapcoord.z, vik_map_source_get_uniq_id(MAPS_LAYER_NTH_TYPE(mdi->maptype)), mdi->mapcoord.scale );
      if (mdi->refresh_display && mdi->map_layer_alive) {
        /* TODO: check if it's on visible area */
        vik_layer_emit_update ( VIK_LAYER(mdi->vml), TRUE ); // Yes update display from background
      }
      g_mutex_unlock(mdi->mutex);
      mdi->mapcoord.x = mdi->mapcoord.y = 0; /* we're temporarily between downloads */

    }
  }
  vik_map_source_download_handle_cleanup(MAPS_LAYER_NTH_TYPE(mdi->maptype), handle);
  g_mutex_lock(mdi->mutex);
  if (mdi->map_layer_alive)
    g_object_weak_unref(G_OBJECT(mdi->vml), weak_ref_cb, mdi);
  g_mutex_unlock(mdi->mutex); 
  return 0;
}

static void mdi_cancel_cleanup ( MapDownloadInfo *mdi )
{
  if ( mdi->mapcoord.x || mdi->mapcoord.y )
  {
    g_snprintf ( mdi->filename_buf, mdi->maxlen, DIRSTRUCTURE,
                     mdi->cache_dir, vik_map_source_get_uniq_id(MAPS_LAYER_NTH_TYPE(mdi->maptype)),
                     mdi->mapcoord.scale, mdi->mapcoord.z, mdi->mapcoord.x, mdi->mapcoord.y );
    if ( g_file_test ( mdi->filename_buf, G_FILE_TEST_EXISTS ) == TRUE)
    {
      g_remove ( mdi->filename_buf );
    }
  }
}

static void start_download_thread ( VikMapsLayer *vml, VikViewport *vvp, const VikCoord *ul, const VikCoord *br, gint redownload )
{
  gdouble xzoom = vml->xmapzoom ? vml->xmapzoom : vik_viewport_get_xmpp ( vvp );
  gdouble yzoom = vml->ymapzoom ? vml->ymapzoom : vik_viewport_get_ympp ( vvp );
  MapCoord ulm, brm;
  VikMapSource *map = MAPS_LAYER_NTH_TYPE(vml->maptype);
  if ( vik_map_source_coord_to_mapcoord ( map, ul, xzoom, yzoom, &ulm ) 
    && vik_map_source_coord_to_mapcoord ( map, br, xzoom, yzoom, &brm ) )
  {
    MapDownloadInfo *mdi = g_malloc ( sizeof(MapDownloadInfo) );
    gint a, b;

    mdi->vml = vml;
    mdi->vvp = vvp;
    mdi->map_layer_alive = TRUE;
    mdi->mutex = g_mutex_new();
    mdi->refresh_display = TRUE;

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
                       vml->cache_dir, vik_map_source_get_uniq_id(map), ulm.scale,
                       ulm.z, a, b );
          if ( g_file_test ( mdi->filename_buf, G_FILE_TEST_EXISTS ) == FALSE )
            mdi->mapstoget++;
        }
      }
    }

    mdi->mapcoord.x = mdi->mapcoord.y = 0; /* for cleanup -- no current map */

    if ( mdi->mapstoget )
    {
      const gchar *tmp_str;
      gchar *tmp;

      if (redownload) 
      {
        if (redownload == REDOWNLOAD_BAD)
          tmp_str = ngettext("Redownloading up to %d %s map...", "Redownloading up to %d %s maps...", mdi->mapstoget);
        else
          tmp_str = ngettext("Redownloading %d %s map...", "Redownloading %d %s maps...", mdi->mapstoget);
      } 
      else 
      {
        tmp_str = ngettext("Downloading %d %s map...", "Downloading %d %s maps...", mdi->mapstoget);
      }
      tmp = g_strdup_printf ( tmp_str, mdi->mapstoget, MAPS_LAYER_NTH_LABEL(vml->maptype));
 
      g_object_weak_ref(G_OBJECT(mdi->vml), weak_ref_cb, mdi);
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

void maps_layer_download_section_without_redraw( VikMapsLayer *vml, VikViewport *vvp, VikCoord *ul, VikCoord *br, gdouble zoom)
{
  MapCoord ulm, brm;
  VikMapSource *map = MAPS_LAYER_NTH_TYPE(vml->maptype);

  if (!vik_map_source_coord_to_mapcoord(map, ul, zoom, zoom, &ulm) 
    || !vik_map_source_coord_to_mapcoord(map, br, zoom, zoom, &brm)) {
    g_warning("%s() coord_to_mapcoord() failed", __PRETTY_FUNCTION__);
    return;
  }

  MapDownloadInfo *mdi = g_malloc(sizeof(MapDownloadInfo));
  gint i, j;

  mdi->vml = vml;
  mdi->vvp = vvp;
  mdi->map_layer_alive = TRUE;
  mdi->mutex = g_mutex_new();
  mdi->refresh_display = FALSE;

  mdi->cache_dir = g_strdup ( vml->cache_dir );
  mdi->maxlen = strlen ( vml->cache_dir ) + 40;
  mdi->filename_buf = g_malloc ( mdi->maxlen * sizeof(gchar) );
  mdi->maptype = vml->maptype;

  mdi->mapcoord = ulm;

  mdi->redownload = REDOWNLOAD_NONE;

  mdi->x0 = MIN(ulm.x, brm.x);
  mdi->xf = MAX(ulm.x, brm.x);
  mdi->y0 = MIN(ulm.y, brm.y);
  mdi->yf = MAX(ulm.y, brm.y);

  mdi->mapstoget = 0;

  for (i = mdi->x0; i <= mdi->xf; i++) {
    for (j = mdi->y0; j <= mdi->yf; j++) {
      g_snprintf ( mdi->filename_buf, mdi->maxlen, DIRSTRUCTURE,
                   vml->cache_dir, vik_map_source_get_uniq_id(map), ulm.scale,
                   ulm.z, i, j );
      if ( g_file_test ( mdi->filename_buf, G_FILE_TEST_EXISTS ) == FALSE )
            mdi->mapstoget++;
    }
  }

  mdi->mapcoord.x = mdi->mapcoord.y = 0; /* for cleanup -- no current map */

  if (mdi->mapstoget) {
    gchar *tmp;
    const gchar *fmt;
    fmt = ngettext("Downloading %d %s map...",
                   "Downloading %d %s maps...",
		   mdi->mapstoget);
    tmp = g_strdup_printf ( fmt, mdi->mapstoget, MAPS_LAYER_NTH_LABEL(vml->maptype) );

    g_object_weak_ref(G_OBJECT(mdi->vml), weak_ref_cb, mdi);
      /* launch the thread */
    a_background_thread ( VIK_GTK_WINDOW_FROM_LAYER(vml), /* parent window */
      tmp,                                /* description string */
      (vik_thr_func) map_download_thread, /* function to call within thread */
      mdi,                                /* pass along data */
      (vik_thr_free_func) mdi_free,       /* function to free pass along data */
      (vik_thr_free_func) mdi_cancel_cleanup,
      mdi->mapstoget );
    g_free ( tmp );
  }
  else
    mdi_free ( mdi );
}

static void maps_layer_redownload_bad ( VikMapsLayer *vml )
{
  start_download_thread ( vml, vml->redownload_vvp, &(vml->redownload_ul), &(vml->redownload_br), REDOWNLOAD_BAD );
}

static void maps_layer_redownload_all ( VikMapsLayer *vml )
{
  start_download_thread ( vml, vml->redownload_vvp, &(vml->redownload_ul), &(vml->redownload_br), REDOWNLOAD_ALL );
}

static void maps_layer_redownload_new ( VikMapsLayer *vml )
{
  start_download_thread ( vml, vml->redownload_vvp, &(vml->redownload_ul), &(vml->redownload_br), REDOWNLOAD_NEW );
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
      start_download_thread ( vml, vvp, &ul, &br, DOWNLOAD_OR_REFRESH );
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

        item = gtk_menu_item_new_with_mnemonic ( _("Redownload _Bad Map(s)") );
        g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(maps_layer_redownload_bad), vml );
        gtk_menu_shell_append ( GTK_MENU_SHELL(vml->dl_right_click_menu), item );

        item = gtk_menu_item_new_with_mnemonic ( _("Redownload _New Map(s)") );
        g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(maps_layer_redownload_new), vml );
        gtk_menu_shell_append ( GTK_MENU_SHELL(vml->dl_right_click_menu), item );

        item = gtk_menu_item_new_with_mnemonic ( _("Redownload _All Map(s)") );
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
  VikMapSource *map = MAPS_LAYER_NTH_TYPE(vml->maptype);
  if ( vik_map_source_get_drawmode(map) == vik_viewport_get_drawmode ( vvp ) &&
       vik_map_source_coord_to_mapcoord ( map, vik_viewport_get_center ( vvp ),
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

static void download_onscreen_maps ( gpointer vml_vvp[2], gint redownload )
{
  VikMapsLayer *vml = vml_vvp[0];
  VikViewport *vvp = vml_vvp[1];
  VikViewportDrawMode vp_drawmode = vik_viewport_get_drawmode ( vvp );

  gdouble xzoom = vml->xmapzoom ? vml->xmapzoom : vik_viewport_get_xmpp ( vvp );
  gdouble yzoom = vml->ymapzoom ? vml->ymapzoom : vik_viewport_get_ympp ( vvp );

  VikCoord ul, br;
  MapCoord ulm, brm;

  vik_viewport_screen_to_coord ( vvp, 0, 0, &ul );
  vik_viewport_screen_to_coord ( vvp, vik_viewport_get_width(vvp), vik_viewport_get_height(vvp), &br );

  VikMapSource *map = MAPS_LAYER_NTH_TYPE(vml->maptype);
  if ( vik_map_source_get_drawmode(map) == vp_drawmode &&
       vik_map_source_coord_to_mapcoord ( map, &ul, xzoom, yzoom, &ulm ) &&
       vik_map_source_coord_to_mapcoord ( map, &br, xzoom, yzoom, &brm ) )
    start_download_thread ( vml, vvp, &ul, &br, redownload );
  else if (vik_map_source_get_drawmode(map) != vp_drawmode) {
    const gchar *drawmode_name = vik_viewport_get_drawmode_name (vvp, vik_map_source_get_drawmode(map));
    gchar *err = g_strdup_printf(_("Wrong drawmode for this map.\nSelect \"%s\" from View menu and try again."), _(drawmode_name));
    a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_LAYER(vml), err );
    g_free(err);
  }
  else
    a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_LAYER(vml), _("Wrong zoom level for this map.") );

}

static void maps_layer_download_missing_onscreen_maps ( gpointer vml_vvp[2] )
{
  download_onscreen_maps( vml_vvp, REDOWNLOAD_NONE);
}

static void maps_layer_download_new_onscreen_maps ( gpointer vml_vvp[2] )
{
  download_onscreen_maps( vml_vvp, REDOWNLOAD_NEW);
}

static void maps_layer_redownload_all_onscreen_maps ( gpointer vml_vvp[2] )
{
  download_onscreen_maps( vml_vvp, REDOWNLOAD_ALL);
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

  /* Now with icons */
  item = gtk_image_menu_item_new_with_mnemonic ( _("Download _Missing Onscreen Maps") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(maps_layer_download_missing_onscreen_maps), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );

  if ( vik_map_source_supports_download_only_new (MAPS_LAYER_NTH_TYPE(vml->maptype)) ) {
    item = gtk_image_menu_item_new_with_mnemonic ( _("Download _New Onscreen Maps") );
    gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REDO, GTK_ICON_SIZE_MENU) );
    g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(maps_layer_download_new_onscreen_maps), pass_along );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show ( item );
  }

  item = gtk_image_menu_item_new_with_mnemonic ( _("Reload _All Onscreen Maps") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REFRESH, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(maps_layer_redownload_all_onscreen_maps), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );
}
