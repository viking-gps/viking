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
#include <glib/gi18n.h>

#include "viking.h"
#include "icons/icons.h"

static void coord_layer_marshall( VikCoordLayer *vcl, guint8 **data, gint *len );
static VikCoordLayer *coord_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp );
static gboolean coord_layer_set_param ( VikCoordLayer *vcl, guint16 id, VikLayerParamData data, VikViewport *vp, gboolean is_file_operation );
static VikLayerParamData coord_layer_get_param ( VikCoordLayer *vcl, guint16 id, gboolean is_file_operation );
static void coord_layer_update_gc ( VikCoordLayer *vcl, VikViewport *vp, const gchar *color );
static void coord_layer_post_read ( VikLayer *vl, VikViewport *vp, gboolean from_file );

static VikLayerParamScale param_scales[] = {
  { 0.05, 60.0, 0.25, 10 },
  { 1, 10, 1, 0 },
};

static VikLayerParam coord_layer_params[] = {
  { "color", VIK_LAYER_PARAM_COLOR, VIK_LAYER_GROUP_NONE, N_("Color:"), VIK_LAYER_WIDGET_COLOR, 0 },
  { "min_inc", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_GROUP_NONE, N_("Minutes Width:"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 0 },
  { "line_thickness", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Line Thickness:"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 1 },
};


enum { PARAM_COLOR = 0, PARAM_MIN_INC, PARAM_LINE_THICKNESS, NUM_PARAMS };

VikLayerInterface vik_coord_layer_interface = {
  "Coord",
  &vikcoordlayer_pixbuf,

  NULL,
  0,

  coord_layer_params,
  NUM_PARAMS,
  NULL,
  0,

  VIK_MENU_ITEM_ALL,

  (VikLayerFuncCreate)                  vik_coord_layer_create,
  (VikLayerFuncRealize)                 NULL,
  (VikLayerFuncPostRead)                coord_layer_post_read,
  (VikLayerFuncFree)                    vik_coord_layer_free,

  (VikLayerFuncProperties)              NULL,
  (VikLayerFuncDraw)                    vik_coord_layer_draw,
  (VikLayerFuncChangeCoordMode)         NULL,

  (VikLayerFuncSetMenuItemsSelection)   NULL,
  (VikLayerFuncGetMenuItemsSelection)   NULL,

  (VikLayerFuncAddMenuItems)            NULL,
  (VikLayerFuncSublayerAddMenuItems)    NULL,

  (VikLayerFuncSublayerRenameRequest)   NULL,
  (VikLayerFuncSublayerToggleVisible)   NULL,
  (VikLayerFuncSublayerTooltip)         NULL,
  (VikLayerFuncLayerTooltip)            NULL,
  (VikLayerFuncLayerSelected)           NULL,

  (VikLayerFuncMarshall)		coord_layer_marshall,
  (VikLayerFuncUnmarshall)		coord_layer_unmarshall,

  (VikLayerFuncSetParam)                coord_layer_set_param,
  (VikLayerFuncGetParam)                coord_layer_get_param,

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

struct _VikCoordLayer {
  VikLayer vl;
  GdkGC *gc;
  gdouble deg_inc;
  guint8 line_thickness;
  GdkColor *color;
};

GType vik_coord_layer_get_type ()
{
  static GType vcl_type = 0;

  if (!vcl_type)
  {
    static const GTypeInfo vcl_info =
    {
      sizeof (VikCoordLayerClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      NULL, /* class init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikCoordLayer),
      0,
      NULL /* instance init */
    };
    vcl_type = g_type_register_static ( VIK_LAYER_TYPE, "VikCoordLayer", &vcl_info, 0 );
  }

  return vcl_type;
}

static void coord_layer_marshall( VikCoordLayer *vcl, guint8 **data, gint *len )
{
  vik_layer_marshall_params ( VIK_LAYER(vcl), data, len );
}

static VikCoordLayer *coord_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp )
{
  VikCoordLayer *rv = vik_coord_layer_new ();
  vik_layer_unmarshall_params ( VIK_LAYER(rv), data, len, vvp );
  return rv;
}

gboolean coord_layer_set_param ( VikCoordLayer *vcl, guint16 id, VikLayerParamData data, VikViewport *vp, gboolean is_file_operation )
{
  switch ( id )
  {
    case PARAM_COLOR: if ( vcl->color ) gdk_color_free ( vcl->color ); vcl->color = gdk_color_copy( &(data.c)); break;
    case PARAM_MIN_INC: vcl->deg_inc = data.d / 60.0; break;
    case PARAM_LINE_THICKNESS: if ( data.u >= 1 && data.u <= 15 ) vcl->line_thickness = data.u; break;
  }
  return TRUE;
}

static VikLayerParamData coord_layer_get_param ( VikCoordLayer *vcl, guint16 id, gboolean is_file_operation )
{
  VikLayerParamData rv;
  switch ( id )
  {
    case PARAM_COLOR:
      if (vcl->color)
      {
        rv.c.pixel = vcl->color->pixel;
        rv.c.red = vcl->color->red;
        rv.c.green = vcl->color->green;
        rv.c.blue = vcl->color->blue;
      }
      break;
    case PARAM_MIN_INC: rv.d = vcl->deg_inc * 60.0; break;
    case PARAM_LINE_THICKNESS: rv.i = vcl->line_thickness; break;
  }
  return rv;
}

static void coord_layer_post_read ( VikLayer *vl, VikViewport *vp, gboolean from_file )
{
  VikCoordLayer *vcl = VIK_COORD_LAYER(vl);
  if ( vcl->gc )
    g_object_unref ( G_OBJECT(vcl->gc) );

  vcl->gc = vik_viewport_new_gc_from_color ( vp, vcl->color, vcl->line_thickness );

}

VikCoordLayer *vik_coord_layer_new ( )
{
  GdkColor InitColor;
  
  VikCoordLayer *vcl = VIK_COORD_LAYER ( g_object_new ( VIK_COORD_LAYER_TYPE, NULL ) );
  vik_layer_init ( VIK_LAYER(vcl), VIK_LAYER_COORD );

  InitColor.pixel = 0;
  InitColor.red = 65535;
  InitColor.green = 65535;
  InitColor.blue = 65535;

  vcl->gc = NULL;
  vcl->deg_inc = 1.0/60.0;
  vcl->line_thickness = 3;
  vcl->color = gdk_color_copy (&InitColor);
  return vcl;
}

void vik_coord_layer_draw ( VikCoordLayer *vcl, gpointer data )
{
  VikViewport *vp = (VikViewport *) data;

  if ( !vcl->gc ) {
    return;
  }

  if ( vik_viewport_get_coord_mode(vp) != VIK_COORD_UTM ) 
  {
    VikCoord left, right, left2, right2;
    gdouble l, r, i, j;
    gint x1, y1, x2, y2, smod = 1, mmod = 1;
    gboolean mins = FALSE, secs = FALSE;
    GdkGC *dgc = vik_viewport_new_gc_from_color(vp, vcl->color, vcl->line_thickness);
    GdkGC *mgc = vik_viewport_new_gc_from_color(vp, vcl->color, MAX(vcl->line_thickness/2, 1));
    GdkGC *sgc = vik_viewport_new_gc_from_color(vp, vcl->color, MAX(vcl->line_thickness/5, 1));

    vik_viewport_screen_to_coord ( vp, 0, 0, &left );
    vik_viewport_screen_to_coord ( vp, vik_viewport_get_width(vp), 0, &right );
    vik_viewport_screen_to_coord ( vp, 0, vik_viewport_get_height(vp), &left2 );
    vik_viewport_screen_to_coord ( vp, vik_viewport_get_width(vp), vik_viewport_get_height(vp), &right2 );

#define CLINE(gc, c1, c2) { \
	  vik_viewport_coord_to_screen(vp, (c1), &x1, &y1);  \
	  vik_viewport_coord_to_screen(vp, (c2), &x2, &y2);  \
	  vik_viewport_draw_line (vp, (gc), x1, y1, x2, y2); \
	}

    l = left.east_west;
    r = right.east_west;
    if (60*fabs(l-r) < 4) {
      secs = TRUE;
      smod = MIN(6, (int)ceil(3600*fabs(l-r)/30.0));
    }
    if (fabs(l-r) < 4) {
      mins = TRUE;
      mmod = MIN(6, (int)ceil(60*fabs(l-r)/30.0));
    }
    for (i=floor(l*60); i<ceil(r*60); i+=1.0) {
      if (secs) {
	for (j=i*60+1; j<(i+1)*60; j+=1.0) {
	  left.east_west = j/3600.0;
	  left2.east_west = j/3600.0;
	  if ((int)j % smod == 0) CLINE(sgc, &left, &left2);
	}
      }
      if (mins) {
	left.east_west = i/60.0;
	left2.east_west = i/60.0;
	if ((int)i % mmod == 0) CLINE(mgc, &left, &left2);
      }
      if ((int)i % 60 == 0) {
	left.east_west = i/60.0;
	left2.east_west = i/60.0;
	CLINE(dgc, &left, &left2);
      }
    }

    vik_viewport_screen_to_coord ( vp, 0, 0, &left );
    l = left2.north_south;
    r = left.north_south;
    for (i=floor(l*60); i<ceil(r*60); i+=1.0) {
      if (secs) {
	for (j=i*60+1; j<(i+1)*60; j+=1.0) {
	  left.north_south = j/3600.0;
	  right.north_south = j/3600.0;
	  if ((int)j % smod == 0) CLINE(sgc, &left, &right);
	}
      }
      if (mins) {
	left.north_south = i/60.0;
	right.north_south = i/60.0;
	if ((int)i % mmod == 0) CLINE(mgc, &left, &right);
      }
      if ((int)i % 60 == 0) {
	left.north_south = i/60.0;
	right.north_south = i/60.0;
	CLINE(dgc, &left, &right);
      }
    }
#undef CLINE
    g_object_unref(dgc);
    g_object_unref(sgc);
    g_object_unref(mgc);
    return;
  }

  if ( vik_viewport_get_coord_mode(vp) == VIK_COORD_UTM ) 
  {
    const struct UTM *center = (const struct UTM *)vik_viewport_get_center ( vp );
    gdouble xmpp = vik_viewport_get_xmpp ( vp ), ympp = vik_viewport_get_ympp ( vp );
    guint16 width = vik_viewport_get_width ( vp ), height = vik_viewport_get_height ( vp );
    struct LatLon ll, ll2, min, max;
    double lon;
    int x1, x2;
    struct UTM utm;

    utm = *center;
    utm.northing = center->northing - ( ympp * height / 2 );

    a_coords_utm_to_latlon ( &utm, &ll );

    utm.northing = center->northing + ( ympp * height / 2 );

    a_coords_utm_to_latlon ( &utm, &ll2 );

    {
      /* find corner coords in lat/lon.
        start at whichever is less: top or bottom left lon. goto whichever more: top or bottom right lon
      */
      struct LatLon topleft, topright, bottomleft, bottomright;
      struct UTM temp_utm;
      temp_utm = *center;
      temp_utm.easting -= (width/2)*xmpp;
      temp_utm.northing += (height/2)*ympp;
      a_coords_utm_to_latlon ( &temp_utm, &topleft );
      temp_utm.easting += (width*xmpp);
      a_coords_utm_to_latlon ( &temp_utm, &topright );
      temp_utm.northing -= (height*ympp);
      a_coords_utm_to_latlon ( &temp_utm, &bottomright );
      temp_utm.easting -= (width*xmpp);
      a_coords_utm_to_latlon ( &temp_utm, &bottomleft );
      min.lon = (topleft.lon < bottomleft.lon) ? topleft.lon : bottomleft.lon;
      max.lon = (topright.lon > bottomright.lon) ? topright.lon : bottomright.lon;
      min.lat = (bottomleft.lat < bottomright.lat) ? bottomleft.lat : bottomright.lat;
      max.lat = (topleft.lat > topright.lat) ? topleft.lat : topright.lat;
    }

    /* Can zoom out more than whole world and so the above can give invalid positions */
    /* Restrict values properly so drawing doesn't go into a near 'infinite' loop */
    if ( min.lon < -180.0 )
      min.lon = -180.0;
    if ( max.lon > 180.0 )
      max.lon = 180.0;
    if ( min.lat < -90.0 )
      min.lat = -90.0;
    if ( max.lat > 90.0 )
      max.lat = 90.0;

    lon = ((double) ((long) ((min.lon)/ vcl->deg_inc))) * vcl->deg_inc;
    ll.lon = ll2.lon = lon;

    for (; ll.lon <= max.lon; ll.lon+=vcl->deg_inc, ll2.lon+=vcl->deg_inc )
    {
      a_coords_latlon_to_utm ( &ll, &utm );
      x1 = ( (utm.easting - center->easting) / xmpp ) + (width / 2);
      a_coords_latlon_to_utm ( &ll2, &utm );
      x2 = ( (utm.easting - center->easting) / xmpp ) + (width / 2);
      vik_viewport_draw_line (vp, vcl->gc, x1, height, x2, 0);
    }

    utm = *center;
    utm.easting = center->easting - ( xmpp * width / 2 );

    a_coords_utm_to_latlon ( &utm, &ll );

    utm.easting = center->easting + ( xmpp * width / 2 );

    a_coords_utm_to_latlon ( &utm, &ll2 );

    /* really lat, just reusing a variable */
    lon = ((double) ((long) ((min.lat)/ vcl->deg_inc))) * vcl->deg_inc;
    ll.lat = ll2.lat = lon;

    for (; ll.lat <= max.lat ; ll.lat+=vcl->deg_inc, ll2.lat+=vcl->deg_inc )
    {
      a_coords_latlon_to_utm ( &ll, &utm );
      x1 = (height / 2) - ( (utm.northing - center->northing) / ympp );
      a_coords_latlon_to_utm ( &ll2, &utm );
      x2 = (height / 2) - ( (utm.northing - center->northing) / ympp );
      vik_viewport_draw_line (vp, vcl->gc, width, x2, 0, x1);
    }
  }
}

void vik_coord_layer_free ( VikCoordLayer *vcl )
{
  if ( vcl->gc != NULL )
    g_object_unref ( G_OBJECT(vcl->gc) );

  if ( vcl->color != NULL )
    gdk_color_free ( vcl->color );
}

static void coord_layer_update_gc ( VikCoordLayer *vcl, VikViewport *vp, const gchar *color )
{
  GdkColor InitColor;
  
  if ( vcl->color )
    gdk_color_free ( vcl->color );

  gdk_color_parse( color, &InitColor);
  vcl->color = gdk_color_copy( &InitColor );

  if ( vcl->gc )
    g_object_unref ( G_OBJECT(vcl->gc) );

  vcl->gc = vik_viewport_new_gc_from_color ( vp, vcl->color, vcl->line_thickness );
}

VikCoordLayer *vik_coord_layer_create ( VikViewport *vp )
{
  VikCoordLayer *vcl = vik_coord_layer_new ();
  coord_layer_update_gc ( vcl, vp, "red" );
  return vcl;
}

