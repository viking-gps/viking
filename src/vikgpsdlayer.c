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
#include <gps.h>

#include "globals.h"
#include "dialog.h"
#include "vikgpsdlayer.h"
#include "viklayer.h"
#include "vikgpsdlayer_pixmap.h"

static void gpsd_layer_marshall( VikGpsdLayer *vgl, guint8 **data, gint *len );
static VikGpsdLayer *gpsd_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp );
static gboolean gpsd_layer_set_param ( VikGpsdLayer *vgl, guint16 id, VikLayerParamData data, VikViewport *vp );
static VikLayerParamData gpsd_layer_get_param ( VikGpsdLayer *vgl, guint16 id );
static VikGpsdLayer *vik_gpsd_layer_new ( VikViewport *vp );
static void vik_gpsd_layer_free ( VikGpsdLayer *vgl );
static void vik_gpsd_layer_draw ( VikGpsdLayer *vgl, gpointer data );


static VikLayerParam gpsd_layer_params[] = {
};


enum { NUM_PARAMS=0 };

VikLayerInterface vik_gpsd_layer_interface = {
  "Gpsd",
  &gpsdlayer_pixbuf,

  NULL,
  0,

//  gpsd_layer_params,
  NULL,
  NUM_PARAMS,
  NULL,
  0,

  VIK_MENU_ITEM_ALL,

  (VikLayerFuncCreate)                  vik_gpsd_layer_new,
  (VikLayerFuncRealize)                 NULL,
                                        NULL,
  (VikLayerFuncFree)                    vik_gpsd_layer_free,

  (VikLayerFuncProperties)              NULL,
  (VikLayerFuncDraw)                    vik_gpsd_layer_draw,
  (VikLayerFuncChangeCoordMode)         NULL,

  (VikLayerFuncSetMenuItemsSelection)   NULL,
  (VikLayerFuncGetMenuItemsSelection)   NULL,

  (VikLayerFuncAddMenuItems)            NULL,
  (VikLayerFuncSublayerAddMenuItems)    NULL,

  (VikLayerFuncSublayerRenameRequest)   NULL,
  (VikLayerFuncSublayerToggleVisible)   NULL,

  (VikLayerFuncMarshall)		gpsd_layer_marshall,
  (VikLayerFuncUnmarshall)		gpsd_layer_unmarshall,

  (VikLayerFuncSetParam)                gpsd_layer_set_param,
  (VikLayerFuncGetParam)                gpsd_layer_get_param,

  (VikLayerFuncReadFileData)            NULL,
  (VikLayerFuncWriteFileData)           NULL,

  (VikLayerFuncDeleteItem)              NULL,
  (VikLayerFuncCopyItem)                NULL,
  (VikLayerFuncPasteItem)               NULL,
  (VikLayerFuncFreeCopiedItem)          NULL,
  (VikLayerFuncDragDropRequest)		NULL,
};

typedef struct {
  struct gps_data_t data;
  VikGpsdLayer *vgl;
} FakeGpsData;

struct _VikGpsdLayer {
  VikLayer vl;
  GdkGC *gc;
  struct LatLon ll;
  gdouble course;
  FakeGpsData *fgd;
  gint timeout;
};

GType vik_gpsd_layer_get_type ()
{
  static GType vgl_type = 0;

  if (!vgl_type)
  {
    static const GTypeInfo vgl_info =
    {
      sizeof (VikGpsdLayerClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      NULL, /* class init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikGpsdLayer),
      0,
      NULL /* instance init */
    };
    vgl_type = g_type_register_static ( VIK_LAYER_TYPE, "VikGpsdLayer", &vgl_info, 0 );
  }

  return vgl_type;
}

static void gpsd_layer_marshall( VikGpsdLayer *vgl, guint8 **data, gint *len )
{
  vik_layer_marshall_params ( VIK_LAYER(vgl), data, len );
}

static VikGpsdLayer *gpsd_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp )
{
  VikGpsdLayer *rv = vik_gpsd_layer_new ( vvp );
  vik_layer_unmarshall_params ( VIK_LAYER(rv), data, len, vvp );
  return rv;
}

gboolean gpsd_layer_set_param ( VikGpsdLayer *vgl, guint16 id, VikLayerParamData data, VikViewport *vp )
{
  switch ( id )
  {

  }
  return TRUE;
}

static VikLayerParamData gpsd_layer_get_param ( VikGpsdLayer *vgl, guint16 id )
{
  VikLayerParamData rv;
  switch ( id )
  {
//    case PARAM_COLOR: rv.s = vgl->color ? vgl->color : ""; break;
  }
  return rv;
}

static void vik_gpsd_layer_draw ( VikGpsdLayer *vgl, gpointer data )
{
  VikViewport *vp = (VikViewport *) data;
  VikCoord nw, se;
  struct LatLon lnw, lse;
  vik_viewport_screen_to_coord ( vp, -20, -20, &nw );
  vik_viewport_screen_to_coord ( vp, vik_viewport_get_width(vp)+20, vik_viewport_get_width(vp)+20, &se );
  vik_coord_to_latlon ( &nw, &lnw );
  vik_coord_to_latlon ( &se, &lse );
  if ( vgl->ll.lat > lse.lat &&
       vgl->ll.lat < lnw.lat &&
       vgl->ll.lon > lnw.lon &&
       vgl->ll.lon < lse.lon ) {
    VikCoord gps;
    gint x, y;
    gint pt_x, pt_y;

    vik_coord_load_from_latlon ( &gps, vik_viewport_get_coord_mode(vp), &(vgl->ll) );
    vik_viewport_coord_to_screen ( vp, &gps, &x, &y );
    vik_viewport_draw_rectangle ( vp, vgl->gc, TRUE, x-2, y-2, 4, 4 );

    pt_y = y-20*cos(M_PI/180*vgl->course);
    pt_x = x+20*sin(M_PI/180*vgl->course);

    g_print("%d %d %d %d\n", x, y, pt_x, pt_y);
    vik_viewport_draw_line ( vp, vgl->gc, x, y, pt_x, pt_y );
  }
}

static void vik_gpsd_layer_free ( VikGpsdLayer *vgl )
{
  /* we already free'd the original gps_data,
   * and the current one lives in the VikGpsdLayer and would be freed twice.
   * so we malloc one, copy the data, and close/free it.
   */
  if ( vgl->fgd ) {
    gps_close ( (struct gps_data_t *) vgl->fgd );
    gtk_timeout_remove ( vgl->timeout );
  }

  if ( vgl->gc != NULL )
    g_object_unref ( G_OBJECT(vgl->gc) );
}

void gpsd_hook(FakeGpsData *fgd, gchar *data)
{
  gdouble lat, lon, alt, herror, verror, course, speed;
  /* skip thru three spaces */
  while (*data && *data != ' ') data++; if (*data) data++;
  while (*data && *data != ' ') data++; if (*data) data++;
  while (*data && *data != ' ') data++; if (*data) data++;
  if ( sscanf(data, "%lf %lf %lf %lf %lf %lf %lf", &lat, &lon,
	&alt, &herror, &verror, &course, &speed) ) {
    VikGpsdLayer *vgl = fgd->vgl;
    vgl->ll.lat = lat;
    vgl->ll.lon = lon;
    vgl->course = course;
    /* could/should emit update here. */
  }
}

static gboolean gpsd_timeout(VikGpsdLayer *vgl)
{
  if ( vgl->fgd ) {
    gps_query( (struct gps_data_t *) vgl->fgd, "o");
    vik_layer_emit_update ( VIK_LAYER(vgl) );
  }
  return TRUE;
}

static VikGpsdLayer *vik_gpsd_layer_new ( VikViewport *vp )
{
  VikGpsdLayer *vgl = VIK_GPSD_LAYER ( g_object_new ( VIK_GPSD_LAYER_TYPE, NULL ) );

  vik_layer_init ( VIK_LAYER(vgl), VIK_LAYER_GPSD );

  struct gps_data_t *orig_data = gps_open ("localhost", DEFAULT_GPSD_PORT);
  if ( orig_data ) {
    vgl->fgd = g_realloc ( orig_data, sizeof(FakeGpsData) );
    vgl->fgd->vgl = vgl;

    gps_set_raw_hook( (struct gps_data_t *) vgl->fgd, gpsd_hook ); /* pass along vgl in fgd */

    vgl->timeout = gtk_timeout_add ( 1000, (GtkFunction)gpsd_timeout, vgl);
  } else {
    a_dialog_warning_msg(VIK_GTK_WINDOW_FROM_WIDGET(vp), "No Gpsd found! Right-click layer and click 'Enable GPSD' (not yet implemented) once daemon is started.");
  }

  vgl->gc = vik_viewport_new_gc ( vp, "red", 2 );
  vgl->ll.lat = vgl->ll.lon = vgl->course = 0;

  return vgl;
}

