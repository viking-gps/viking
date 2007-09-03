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
#include <math.h>

#include "globals.h"
#include "coords.h"
#include "vikcoord.h"
#include "download.h"
#include "vikwaypoint.h"
#include "viktrack.h"
#include "vikviewport.h"
#include "viktreeview.h"
#include "viklayer.h"
#include "vikaggregatelayer.h"
#include "viklayerspanel.h"
#include "vikdemlayer.h"
#include "vikdemlayer_pixmap.h"

#include "dem.h"
#include "dems.h"

static VikDEMLayer *dem_layer_copy ( VikDEMLayer *vdl, gpointer vp );
static void dem_layer_marshall( VikDEMLayer *vdl, guint8 **data, gint *len );
static VikDEMLayer *dem_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp );
static gboolean dem_layer_set_param ( VikDEMLayer *vdl, guint16 id, VikLayerParamData data, VikViewport *vp );
static VikLayerParamData dem_layer_get_param ( VikDEMLayer *vdl, guint16 id );
static void dem_layer_update_gc ( VikDEMLayer *vdl, VikViewport *vp, const gchar *color );
static void dem_layer_post_read ( VikLayer *vl, VikViewport *vp );

static VikLayerParamScale param_scales[] = {
  { 1, 10000, 10, 1 },
  { 1, 10, 1, 0 },
};

static VikLayerParam dem_layer_params[] = {
  { "files", VIK_LAYER_PARAM_STRING_LIST, VIK_LAYER_GROUP_NONE, "DEM Files:", VIK_LAYER_WIDGET_FILELIST },
  { "color", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, "Color:", VIK_LAYER_WIDGET_ENTRY },
  { "max_elev", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_GROUP_NONE, "Max Elev:", VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 0 },
  { "line_thickness", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, "Line Thickness:", VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 1 },
};

/*
*/


static gchar *dem_colors[] = {
"#0000FF",
"#9b793c",
"#9c7d40",
"#9d8144",
"#9e8549",
"#9f894d",
"#a08d51",
"#a29156",
"#a3955a",
"#a4995e",
"#a69d63",
"#a89f65",
"#aaa267",
"#ada569",
"#afa76b",
"#b1aa6d",
"#b4ad6f",
"#b6b071",
"#b9b373",
"#bcb676",
"#beb978",
"#c0bc7a",
"#c2c07d",
"#c4c37f",
"#c6c681",
"#c8ca84",
"#cacd86",
"#ccd188",
"#cfd58b",
"#c2ce84",
"#b5c87e",
"#a9c278",
"#9cbb71",
"#8fb56b",
"#83af65",
"#76a95e",
"#6aa358",
"#5e9d52",
"#63a055",
"#69a458",
"#6fa85c",
"#74ac5f",
"#7ab063",
"#80b467",
"#86b86a",
"#8cbc6e",
"#92c072",
"#94c175",
"#97c278",
"#9ac47c",
"#9cc57f",
"#9fc682",
"#a2c886",
"#a4c989",
"#a7cb8d",
"#aacd91",
"#afce99",
"#b5d0a1",
"#bbd2aa",
"#c0d3b2",
"#c6d5ba",
"#ccd7c3",
"#d1d9cb",
"#d7dbd4",
"#DDDDDD",
"#e0e0e0",
"#e4e4e4",
"#e8e8e8",
"#ebebeb",
"#efefef",
"#f3f3f3",
"#f7f7f7",
"#fbfbfb",
"#ffffff"
};

/*
"#9b793c",
"#9e8549",
"#a29156",
"#a69d63",
"#ada569",
"#b4ad6f",
"#bcb676",
"#c2c07d",
"#c8ca84",
"#cfd58b",
"#a9c278",
"#83af65",
"#5e9d52",
"#6fa85c",
"#80b467",
"#92c072",
"#9ac47c",
"#a2c886",
"#aacd91",
"#bbd2aa",
"#ccd7c3",
"#DDDDDD",
"#e8e8e8",
"#f3f3f3",
"#FFFFFF"
};
*/

static const guint dem_n_colors = sizeof(dem_colors)/sizeof(dem_colors[0]);

enum { PARAM_FILES=0, PARAM_COLOR, PARAM_MAX_ELEV, PARAM_LINE_THICKNESS, NUM_PARAMS };

VikLayerInterface vik_dem_layer_interface = {
  "DEM",
  &demlayer_pixbuf,

  NULL,
  0,

  dem_layer_params,
  NUM_PARAMS,
  NULL,
  0,

  VIK_MENU_ITEM_ALL,

  (VikLayerFuncCreate)                  vik_dem_layer_create,
  (VikLayerFuncRealize)                 NULL,
                                        dem_layer_post_read,
  (VikLayerFuncFree)                    vik_dem_layer_free,

  (VikLayerFuncProperties)              NULL,
  (VikLayerFuncDraw)                    vik_dem_layer_draw,
  (VikLayerFuncChangeCoordMode)         NULL,

  (VikLayerFuncSetMenuItemsSelection)   NULL,
  (VikLayerFuncGetMenuItemsSelection)   NULL,

  (VikLayerFuncAddMenuItems)            NULL,
  (VikLayerFuncSublayerAddMenuItems)    NULL,

  (VikLayerFuncSublayerRenameRequest)   NULL,
  (VikLayerFuncSublayerToggleVisible)   NULL,

  (VikLayerFuncCopy)                    dem_layer_copy,
  (VikLayerFuncMarshall)		dem_layer_marshall,
  (VikLayerFuncUnmarshall)		dem_layer_unmarshall,

  (VikLayerFuncSetParam)                dem_layer_set_param,
  (VikLayerFuncGetParam)                dem_layer_get_param,

  (VikLayerFuncReadFileData)            NULL,
  (VikLayerFuncWriteFileData)           NULL,

  (VikLayerFuncDeleteItem)              NULL,
  (VikLayerFuncCopyItem)                NULL,
  (VikLayerFuncPasteItem)               NULL,
  (VikLayerFuncFreeCopiedItem)          NULL,
  (VikLayerFuncDragDropRequest)		NULL,
};

struct _VikDEMLayer {
  VikLayer vl;
  GdkGC *gc;
  GdkGC **gcs;
  GList *files;
  gdouble max_elev;
  guint8 line_thickness;
  gchar *color;
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

static VikDEMLayer *dem_layer_copy ( VikDEMLayer *vdl, gpointer vp )
{
  VikDEMLayer *rv = vik_dem_layer_new ( );


  /* TODO -- FIX for files */

  rv->color = g_strdup ( vdl->color );
  rv->max_elev = vdl->max_elev;
  rv->line_thickness = vdl->line_thickness;
  rv->gc = vdl->gc;
  g_object_ref ( rv->gc );
  return rv;
}

static void dem_layer_marshall( VikDEMLayer *vdl, guint8 **data, gint *len )
{
  vik_layer_marshall_params ( VIK_LAYER(vdl), data, len );
}

static VikDEMLayer *dem_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp )
{
  VikDEMLayer *rv = vik_dem_layer_new ( vvp );
  vik_layer_unmarshall_params ( VIK_LAYER(rv), data, len, vvp );
  return rv;
}

gboolean dem_layer_set_param ( VikDEMLayer *vdl, guint16 id, VikLayerParamData data, VikViewport *vp )
{
  switch ( id )
  {
    case PARAM_COLOR: if ( vdl->color ) g_free ( vdl->color ); vdl->color = g_strdup ( data.s ); break;
    case PARAM_MAX_ELEV: vdl->max_elev = data.d; break;
    case PARAM_LINE_THICKNESS: if ( data.u >= 1 && data.u <= 15 ) vdl->line_thickness = data.u; break;
    case PARAM_FILES: a_dems_load_list ( &(data.sl) ); a_dems_list_free ( vdl->files ); vdl->files = data.sl; break;
  }
  return TRUE;
}

static VikLayerParamData dem_layer_get_param ( VikDEMLayer *vdl, guint16 id )
{
  VikLayerParamData rv;
  switch ( id )
  {
    case PARAM_FILES: rv.sl = vdl->files; break;
    case PARAM_COLOR: rv.s = vdl->color ? vdl->color : ""; break;
    case PARAM_MAX_ELEV: rv.d = vdl->max_elev; break;
    case PARAM_LINE_THICKNESS: rv.i = vdl->line_thickness; break;
  }
  return rv;
}

static void dem_layer_post_read ( VikLayer *vl, VikViewport *vp )
{
  VikDEMLayer *vdl = VIK_DEM_LAYER(vl);
  if ( vdl->gc )
    g_object_unref ( G_OBJECT(vdl->gc) );

  vdl->gc = vik_viewport_new_gc ( vp, vdl->color, vdl->line_thickness );
}

VikDEMLayer *vik_dem_layer_new ( )
{
  VikDEMLayer *vdl = VIK_DEM_LAYER ( g_object_new ( VIK_DEM_LAYER_TYPE, NULL ) );
  vik_layer_init ( VIK_LAYER(vdl), VIK_LAYER_DEM );

  vdl->files = NULL;


  vdl->gc = NULL;

  vdl->gcs = g_malloc(sizeof(GdkGC *)*dem_n_colors);

  vdl->max_elev = 1000.0;
  vdl->line_thickness = 3;
  vdl->color = NULL;
  return vdl;
}


static void vik_dem_layer_draw_dem ( VikDEMLayer *vdl, VikViewport *vp, VikDEM *dem )
{
  VikCoord tleft, tright, bleft, bright;
  VikDEMColumn *column;

  struct LatLon dem_northeast, dem_southwest;
  gdouble max_lat, max_lon, min_lat, min_lon;  


  /**** Check if viewport and DEM data overlap ****/

  /* get min, max lat/lon of viewport */
  vik_viewport_screen_to_coord ( vp, 0, 0, &tleft );
  vik_viewport_screen_to_coord ( vp, vik_viewport_get_width(vp), 0, &tright );
  vik_viewport_screen_to_coord ( vp, 0, vik_viewport_get_height(vp), &bleft );
  vik_viewport_screen_to_coord ( vp, vik_viewport_get_width(vp), vik_viewport_get_height(vp), &bright );

  vik_coord_convert(&tleft, VIK_COORD_LATLON);
  vik_coord_convert(&tright, VIK_COORD_LATLON);
  vik_coord_convert(&bleft, VIK_COORD_LATLON);
  vik_coord_convert(&bright, VIK_COORD_LATLON);

  max_lat = MAX(tleft.north_south, tright.north_south);
  min_lat = MIN(bleft.north_south, bright.north_south);
  max_lon = MAX(tright.east_west, bright.east_west);
  min_lon = MIN(tleft.east_west, bleft.east_west);

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

  if ( dem->horiz_units == VIK_DEM_HORIZ_LL_ARCSECONDS ) {
    VikCoord tmp; /* TODO: don't use coord_load_from_latlon, especially if in latlon drawing mode */

    gdouble max_lat_as, max_lon_as, min_lat_as, min_lon_as;  
    gdouble start_lat_as, end_lat_as, start_lon_as, end_lon_as;

    gdouble start_lat, end_lat, start_lon, end_lon;

    struct LatLon counter;

    guint x, y, start_x, start_y;

    gint16 elev;

    guint skip_factor = ceil ( vik_viewport_get_xmpp(vp) / 40 ); /* todo: smarter calculation. */

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

    for ( x=start_x, counter.lon = start_lon; counter.lon <= end_lon; counter.lon += escale_deg * skip_factor, x += skip_factor ) {
      if ( x > 0 && x < dem->n_columns ) {
        column = g_ptr_array_index ( dem->columns, x );
        for ( y=start_y, counter.lat = start_lat; counter.lat <= end_lat; counter.lat += nscale_deg * skip_factor, y += skip_factor ) {
          if ( y > column->n_points )
            break;
          elev = column->points[y];
          if ( elev > vdl->max_elev ) elev=vdl->max_elev;
          {
            gint a, b;

            vik_coord_load_from_latlon(&tmp, vik_viewport_get_coord_mode(vp), &counter);
            vik_viewport_coord_to_screen(vp, &tmp, &a, &b);
            if ( elev == VIK_DEM_INVALID_ELEVATION )
              ; /* don't draw it */
            else if ( elev <= 0 )
              vik_viewport_draw_rectangle(vp, vdl->gcs[0], TRUE, a-2, b-2, 4, 4 );
            else
              vik_viewport_draw_rectangle(vp, vdl->gcs[(gint)floor(elev/vdl->max_elev*(dem_n_colors-2))+1], TRUE, a-2, b-2, 4, 4 );
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
          if ( elev > vdl->max_elev ) elev=vdl->max_elev;
          {
            gint a, b;
            vik_coord_load_from_utm(&tmp, vik_viewport_get_coord_mode(vp), &counter);
	            vik_viewport_coord_to_screen(vp, &tmp, &a, &b);
            if ( elev == VIK_DEM_INVALID_ELEVATION )
              ; /* don't draw it */
            else if ( elev <= 0 )
              vik_viewport_draw_rectangle(vp, vdl->gcs[0], TRUE, a-2, b-2, 4, 4 );
            else
              vik_viewport_draw_rectangle(vp, vdl->gcs[(gint)floor(elev/vdl->max_elev*(dem_n_colors-2))+1], TRUE, a-2, b-2, 4, 4 );
          }
        } /* for y= */
      }
    } /* for x= */
  }
}

void vik_dem_layer_draw ( VikDEMLayer *vdl, gpointer data )
{
  VikViewport *vp = (VikViewport *) data;
  GList *dems_iter = vdl->files;
  VikDEM *dem;
  while ( dems_iter ) {
    dem = a_dems_get ( (const char *) (dems_iter->data) );
    if ( dem )
      vik_dem_layer_draw_dem ( vdl, vp, dem );
    dems_iter = dems_iter->next;
  }
}

void vik_dem_layer_free ( VikDEMLayer *vdl )
{
  if ( vdl->gc != NULL )
    g_object_unref ( G_OBJECT(vdl->gc) );

  if ( vdl->color != NULL )
    g_free ( vdl->color );

  a_dems_list_free ( vdl->files );
}

static void dem_layer_update_gc ( VikDEMLayer *vdl, VikViewport *vp, const gchar *color )
{
  guint i;

  if ( vdl->color )
    g_free ( vdl->color );

  vdl->color = g_strdup ( color );

  if ( vdl->gc )
    g_object_unref ( G_OBJECT(vdl->gc) );

  vdl->gc = vik_viewport_new_gc ( vp, vdl->color, vdl->line_thickness );

  for ( i = 0 ; i < dem_n_colors; i++ )
    vdl->gcs[i] = vik_viewport_new_gc ( vp, dem_colors[i], vdl->line_thickness );

}

VikDEMLayer *vik_dem_layer_create ( VikViewport *vp )
{
  VikDEMLayer *vdl = vik_dem_layer_new ();
  dem_layer_update_gc ( vdl, vp, "red" );
  return vdl;
}

