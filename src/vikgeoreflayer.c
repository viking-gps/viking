/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2014, Rob Norris <rw_norris@hotmail.com>
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

#include "viking.h"
#include "vikutils.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>

#include "ui_util.h"
#include "preferences.h"
#include "icons/icons.h"
/*
static VikLayerParamData image_default ( void )
{
  VikLayerParamData data;
  data.s = g_strdup ("");
  return data;
}
*/

VikLayerParam georef_layer_params[] = {
  { VIK_LAYER_GEOREF, "image", VIK_LAYER_PARAM_STRING, VIK_LAYER_NOT_IN_PROPERTIES, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_GEOREF, "corner_easting", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_NOT_IN_PROPERTIES, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_GEOREF, "corner_northing", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_NOT_IN_PROPERTIES, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_GEOREF, "mpp_easting", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_NOT_IN_PROPERTIES, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_GEOREF, "mpp_northing", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_NOT_IN_PROPERTIES, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_GEOREF, "corner_zone", VIK_LAYER_PARAM_UINT, VIK_LAYER_NOT_IN_PROPERTIES, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_GEOREF, "corner_letter_as_int", VIK_LAYER_PARAM_UINT, VIK_LAYER_NOT_IN_PROPERTIES, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL },
  { VIK_LAYER_GEOREF, "alpha", VIK_LAYER_PARAM_UINT, VIK_LAYER_NOT_IN_PROPERTIES, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL },
};

enum {
  PARAM_IMAGE = 0,
  PARAM_CE,
  PARAM_CN,
  PARAM_ME,
  PARAM_MN,
  PARAM_CZ,
  PARAM_CL,
  PARAM_AA,
  NUM_PARAMS };

static const gchar* georef_layer_tooltip ( VikGeorefLayer *vgl );
static void georef_layer_marshall( VikGeorefLayer *vgl, guint8 **data, gint *len );
static VikGeorefLayer *georef_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp );
static gboolean georef_layer_set_param ( VikGeorefLayer *vgl, guint16 id, VikLayerParamData data, VikViewport *vp, gboolean is_file_operation );
static VikLayerParamData georef_layer_get_param ( VikGeorefLayer *vgl, guint16 id, gboolean is_file_operation );
static VikGeorefLayer *georef_layer_new ( VikViewport *vvp );
static VikGeorefLayer *georef_layer_create ( VikViewport *vp );
static void georef_layer_free ( VikGeorefLayer *vgl );
static gboolean georef_layer_properties ( VikGeorefLayer *vgl, gpointer vp );
static void georef_layer_draw ( VikGeorefLayer *vgl, VikViewport *vp );
static void georef_layer_add_menu_items ( VikGeorefLayer *vgl, GtkMenu *menu, gpointer vlp );
static void georef_layer_set_image ( VikGeorefLayer *vgl, const gchar *image );
static gboolean georef_layer_dialog ( VikGeorefLayer *vgl, gpointer vp, GtkWindow *w );
static void georef_layer_load_image ( VikGeorefLayer *vgl, VikViewport *vp, gboolean from_file );

/* tools */
static gpointer georef_layer_move_create ( VikWindow *vw, VikViewport *vvp);
static gboolean georef_layer_move_release ( VikGeorefLayer *vgl, GdkEventButton *event, VikViewport *vvp );
static gboolean georef_layer_move_press ( VikGeorefLayer *vgl, GdkEventButton *event, VikViewport *vvp );
static gpointer georef_layer_zoom_create ( VikWindow *vw, VikViewport *vvp);
static gboolean georef_layer_zoom_press ( VikGeorefLayer *vgl, GdkEventButton *event, VikViewport *vvp );

// See comment in viktrwlayer.c for advice on values used
static VikToolInterface georef_tools[] = {
  { { "GeorefMoveMap", "vik-icon-Georef Move Map",  N_("_Georef Move Map"), NULL,  N_("Georef Move Map"), 0 },
    (VikToolConstructorFunc) georef_layer_move_create, NULL, NULL, NULL,
    (VikToolMouseFunc) georef_layer_move_press, NULL, (VikToolMouseFunc) georef_layer_move_release,
    (VikToolKeyFunc) NULL,
    FALSE,
    GDK_CURSOR_IS_PIXMAP, &cursor_geomove_pixbuf, NULL },

  { { "GeorefZoomTool", "vik-icon-Georef Zoom Tool",  N_("Georef Z_oom Tool"), NULL,  N_("Georef Zoom Tool"), 0 },
    (VikToolConstructorFunc) georef_layer_zoom_create, NULL, NULL, NULL,
    (VikToolMouseFunc) georef_layer_zoom_press, NULL, NULL,
    (VikToolKeyFunc) NULL,
    FALSE,
    GDK_CURSOR_IS_PIXMAP, &cursor_geozoom_pixbuf, NULL },
};

VikLayerInterface vik_georef_layer_interface = {
  "GeoRef Map",
  N_("GeoRef Map"),
  NULL,
  &vikgeoreflayer_pixbuf, /*icon */

  georef_tools,
  sizeof(georef_tools) / sizeof(VikToolInterface),

  georef_layer_params,
  NUM_PARAMS,
  NULL,
  0,

  VIK_MENU_ITEM_ALL,

  (VikLayerFuncCreate)                  georef_layer_create,
  (VikLayerFuncRealize)                 NULL,
  (VikLayerFuncPostRead)                georef_layer_load_image,
  (VikLayerFuncFree)                    georef_layer_free,

  (VikLayerFuncProperties)              georef_layer_properties,
  (VikLayerFuncDraw)                    georef_layer_draw,
  (VikLayerFuncChangeCoordMode)         NULL,

  (VikLayerFuncGetTimestamp)            NULL,

  (VikLayerFuncSetMenuItemsSelection)   NULL,
  (VikLayerFuncGetMenuItemsSelection)   NULL,

  (VikLayerFuncAddMenuItems)            georef_layer_add_menu_items,
  (VikLayerFuncSublayerAddMenuItems)    NULL,

  (VikLayerFuncSublayerRenameRequest)   NULL,
  (VikLayerFuncSublayerToggleVisible)   NULL,
  (VikLayerFuncSublayerTooltip)         NULL,
  (VikLayerFuncLayerTooltip)            georef_layer_tooltip,
  (VikLayerFuncLayerSelected)           NULL,

  (VikLayerFuncMarshall)		georef_layer_marshall,
  (VikLayerFuncUnmarshall)		georef_layer_unmarshall,

  (VikLayerFuncSetParam)                georef_layer_set_param,
  (VikLayerFuncGetParam)                georef_layer_get_param,
  (VikLayerFuncChangeParam)             NULL,

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

typedef struct {
  GtkWidget *x_spin;
  GtkWidget *y_spin;
  // UTM widgets
  GtkWidget *ce_spin; // top left
  GtkWidget *cn_spin; //    "
  GtkWidget *utm_zone_spin;
  GtkWidget *utm_letter_entry;

  GtkWidget *lat_tl_spin;
  GtkWidget *lon_tl_spin;
  GtkWidget *lat_br_spin;
  GtkWidget *lon_br_spin;
  //
  GtkWidget *tabs;
  GtkWidget *imageentry;
} changeable_widgets;

struct _VikGeorefLayer {
  VikLayer vl;
  gchar *image;
  GdkPixbuf *pixbuf;
  guint8 alpha;

  struct UTM corner; // Top Left
  gdouble mpp_easting, mpp_northing;
  struct LatLon ll_br; // Bottom Right
  guint width, height;

  GdkPixbuf *scaled;
  guint32 scaled_width, scaled_height;

  gint click_x, click_y;
  changeable_widgets cw;
};

static VikLayerParam io_prefs[] = {
  { VIK_LAYER_NUM_TYPES, VIKING_PREFERENCES_IO_NAMESPACE "georef_auto_read_world_file", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Auto Read World Files:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL,
    N_("Automatically attempt to read associated world file of a new image for a GeoRef layer"), NULL, NULL, NULL}
};

void vik_georef_layer_init (void)
{
  VikLayerParamData tmp;
  tmp.b = TRUE;
  a_preferences_register(&io_prefs[0], tmp, VIKING_PREFERENCES_IO_GROUP_KEY);
}

GType vik_georef_layer_get_type ()
{
  static GType vgl_type = 0;

  if (!vgl_type)
  {
    static const GTypeInfo vgl_info =
    {
      sizeof (VikGeorefLayerClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      NULL, /* class init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikGeorefLayer),
      0,
      NULL /* instance init */
    };
    vgl_type = g_type_register_static ( VIK_LAYER_TYPE, "VikGeorefLayer", &vgl_info, 0 );
  }

  return vgl_type;
}

static const gchar* georef_layer_tooltip ( VikGeorefLayer *vgl )
{
  return vgl->image;
}

static void georef_layer_marshall( VikGeorefLayer *vgl, guint8 **data, gint *len )
{
  vik_layer_marshall_params ( VIK_LAYER(vgl), data, len );
}

static VikGeorefLayer *georef_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp )
{
  VikGeorefLayer *rv = georef_layer_new ( vvp );
  vik_layer_unmarshall_params ( VIK_LAYER(rv), data, len, vvp );
  if (rv->image) {
    georef_layer_load_image ( rv, vvp, TRUE );
  }
  return rv;
}

static gboolean georef_layer_set_param ( VikGeorefLayer *vgl, guint16 id, VikLayerParamData data, VikViewport *vp, gboolean is_file_operation )
{
  switch ( id )
  {
    case PARAM_IMAGE: georef_layer_set_image ( vgl, data.s ); break;
    case PARAM_CN: vgl->corner.northing = data.d; break;
    case PARAM_CE: vgl->corner.easting = data.d; break;
    case PARAM_MN: vgl->mpp_northing = data.d; break;
    case PARAM_ME: vgl->mpp_easting = data.d; break;
    case PARAM_CZ: if ( data.u <= 60 ) vgl->corner.zone = data.u; break;
    case PARAM_CL: if ( data.u >= 65 || data.u <= 90 ) vgl->corner.letter = data.u; break;
    case PARAM_AA: if ( data.u <= 255 ) vgl->alpha = data.u; break;
    default: break;
  }
  return TRUE;
}

static VikLayerParamData georef_layer_get_param ( VikGeorefLayer *vgl, guint16 id, gboolean is_file_operation )
{
  VikLayerParamData rv;
  switch ( id )
  {
    case PARAM_IMAGE: {
      gboolean set = FALSE;
      if ( is_file_operation ) {
        if ( a_vik_get_file_ref_format() == VIK_FILE_REF_FORMAT_RELATIVE ) {
          gchar *cwd = g_get_current_dir();
          if ( cwd ) {
            rv.s = file_GetRelativeFilename ( cwd, vgl->image );
	    if ( !rv.s ) rv.s = "";
            set = TRUE;
	  }
	}
      }
      if ( !set )
        rv.s = vgl->image ? vgl->image : "";
      break;
    }
    case PARAM_CN: rv.d = vgl->corner.northing; break;
    case PARAM_CE: rv.d = vgl->corner.easting; break;
    case PARAM_MN: rv.d = vgl->mpp_northing; break;
    case PARAM_ME: rv.d = vgl->mpp_easting; break;
    case PARAM_CZ: rv.u = vgl->corner.zone; break;
    case PARAM_CL: rv.u = vgl->corner.letter; break;
    case PARAM_AA: rv.u = vgl->alpha; break;
    default: break;
  }
  return rv;
}

static VikGeorefLayer *georef_layer_new ( VikViewport *vvp )
{
  VikGeorefLayer *vgl = VIK_GEOREF_LAYER ( g_object_new ( VIK_GEOREF_LAYER_TYPE, NULL ) );
  vik_layer_set_type ( VIK_LAYER(vgl), VIK_LAYER_GEOREF );

  // Since GeoRef layer doesn't use uibuilder
  //  initializing this way won't do anything yet..
  vik_layer_set_defaults ( VIK_LAYER(vgl), vvp );

  // Make these defaults based on the current view
  vgl->mpp_northing = vik_viewport_get_ympp ( vvp );
  vgl->mpp_easting = vik_viewport_get_xmpp ( vvp );
  vik_coord_to_utm ( vik_viewport_get_center ( vvp ), &(vgl->corner) );

  vgl->image = NULL;
  vgl->pixbuf = NULL;
  vgl->click_x = -1;
  vgl->click_y = -1;
  vgl->scaled = NULL;
  vgl->scaled_width = 0;
  vgl->scaled_height = 0;
  vgl->ll_br.lat = 0.0;
  vgl->ll_br.lon = 0.0;
  vgl->alpha = 255;
  return vgl;
}

static void georef_layer_draw ( VikGeorefLayer *vgl, VikViewport *vp )
{
  if ( vgl->pixbuf )
  {
    gdouble xmpp = vik_viewport_get_xmpp(vp), ympp = vik_viewport_get_ympp(vp);
    GdkPixbuf *pixbuf = vgl->pixbuf;
    guint layer_width = vgl->width;
    guint layer_height = vgl->height;

    guint width = vik_viewport_get_width(vp), height = vik_viewport_get_height(vp);
    gint32 x, y;
    VikCoord corner_coord;
    vik_coord_load_from_utm ( &corner_coord, vik_viewport_get_coord_mode(vp), &(vgl->corner) );
    vik_viewport_coord_to_screen ( vp, &corner_coord, &x, &y );

    /* mark to scale the pixbuf if it doesn't match our dimensions */
    gboolean scale = FALSE;
    if ( xmpp != vgl->mpp_easting || ympp != vgl->mpp_northing )
    {
      scale = TRUE;
      layer_width = round(vgl->width * vgl->mpp_easting / xmpp);
      layer_height = round(vgl->height * vgl->mpp_northing / ympp);
    }

    // If image not in viewport bounds - no need to draw it (or bother with any scaling)
    if ( (x < 0 || x < width) && (y < 0 || y < height) && x+layer_width > 0 && y+layer_height > 0 ) {

      if ( scale )
      {
        /* rescale if necessary */
        if (layer_width == vgl->scaled_width && layer_height == vgl->scaled_height && vgl->scaled != NULL)
          pixbuf = vgl->scaled;
        else
        {
          pixbuf = gdk_pixbuf_scale_simple(
            vgl->pixbuf,
            layer_width,
            layer_height,
            GDK_INTERP_BILINEAR
          );

          if (vgl->scaled != NULL)
            g_object_unref(vgl->scaled);

          vgl->scaled = pixbuf;
          vgl->scaled_width = layer_width;
          vgl->scaled_height = layer_height;
        }
      }
      vik_viewport_draw_pixbuf ( vp, pixbuf, 0, 0, x, y, layer_width, layer_height ); /* todo: draw only what we need to. */
    }
  }
}

static void georef_layer_free ( VikGeorefLayer *vgl )
{
  if ( vgl->image != NULL )
    g_free ( vgl->image );
  if ( vgl->scaled != NULL )
    g_object_unref ( vgl->scaled );
}

static VikGeorefLayer *georef_layer_create ( VikViewport *vp )
{
  return georef_layer_new ( vp );
}

static gboolean georef_layer_properties ( VikGeorefLayer *vgl, gpointer vp )
{
  return georef_layer_dialog ( vgl, vp, VIK_GTK_WINDOW_FROM_WIDGET(vp) );
}

static void georef_layer_load_image ( VikGeorefLayer *vgl, VikViewport *vp, gboolean from_file )
{
  GError *gx = NULL;
  if ( vgl->image == NULL )
    return;

  if ( vgl->pixbuf )
    g_object_unref ( G_OBJECT(vgl->pixbuf) );
  if ( vgl->scaled )
  {
    g_object_unref ( G_OBJECT(vgl->scaled) );
    vgl->scaled = NULL;
  }

  vgl->pixbuf = gdk_pixbuf_new_from_file ( vgl->image, &gx );

  if (gx)
  {
    if ( !from_file )
      a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_WIDGET(vp), _("Couldn't open image file: %s"), gx->message );
    g_error_free ( gx );
  }
  else
  {
    vgl->width = gdk_pixbuf_get_width ( vgl->pixbuf );
    vgl->height = gdk_pixbuf_get_height ( vgl->pixbuf );

    if ( vgl->pixbuf && vgl->alpha < 255 )
      vgl->pixbuf = ui_pixbuf_set_alpha ( vgl->pixbuf, vgl->alpha );
  }
  /* should find length and width here too */
}

static void georef_layer_set_image ( VikGeorefLayer *vgl, const gchar *image )
{
  if ( vgl->image )
    g_free ( vgl->image );
  if ( vgl->scaled )
  {
    g_object_unref ( vgl->scaled );
    vgl->scaled = NULL;
  }
  if ( image == NULL )
    vgl->image = NULL;

  if ( g_strcmp0 (image, "") != 0 )
    vgl->image = vu_get_canonical_filename ( VIK_LAYER(vgl), image );
  else
    vgl->image = g_strdup (image);
}

// Only positive values allowed here
static void gdouble2spinwidget ( GtkWidget *widget, gdouble val )
{
  gtk_spin_button_set_value ( GTK_SPIN_BUTTON(widget), val > 0 ? val : -val );
}

static void set_widget_values ( changeable_widgets *cw, gdouble values[4] )
{
  gdouble2spinwidget ( cw->x_spin, values[0] );
  gdouble2spinwidget ( cw->y_spin, values[1] );
  gdouble2spinwidget ( cw->ce_spin, values[2] );
  gdouble2spinwidget ( cw->cn_spin, values[3] );
}

static gboolean world_file_read_line ( FILE *ff, gdouble *value, gboolean use_value )
{
  gboolean answer = TRUE; // Success
  gchar buffer[1024];
  if ( !fgets ( buffer, 1024, ff ) ) {
    answer = FALSE;
  }
  if ( answer && use_value )
      *value = g_strtod ( buffer, NULL );

  return answer;
}

/**
 * http://en.wikipedia.org/wiki/World_file
 *
 * Note world files do not define the units and nor are the units standardized :(
 * Currently Viking only supports:
 *  x&y scale as meters per pixel
 *  x&y coords as UTM eastings and northings respectively
 */
static gint world_file_read_file ( const gchar* filename, gdouble values[4] )
{
  g_debug ("%s - trying world file %s", __FUNCTION__, filename);

  FILE *f = g_fopen ( filename, "r" );
  if ( !f )
    return 1;
  else {
    gint answer = 2; // Not enough info read yet
    // **We do not handle 'skew' values ATM - normally they are a value of 0 anyway to align with the UTM grid
    if ( world_file_read_line ( f, &values[0], TRUE ) // x scale
      && world_file_read_line ( f, NULL, FALSE ) // Ignore value in y-skew line**
      && world_file_read_line ( f, NULL, FALSE ) // Ignore value in x-skew line**
      && world_file_read_line ( f, &values[1], TRUE ) // y scale
      && world_file_read_line ( f, &values[2], TRUE ) // x-coordinate of the upper left pixel
      && world_file_read_line ( f, &values[3], TRUE ) // y-coordinate of the upper left pixel
       )
    {
       // Success
       g_debug ("%s - %s - world file read success", __FUNCTION__, filename);
       answer = 0;
    }
    fclose ( f );
    return answer;
  }
}

static void georef_layer_dialog_load ( changeable_widgets *cw )
{
  GtkWidget *file_selector = gtk_file_chooser_dialog_new (_("Choose World file"),
				      NULL,
				      GTK_FILE_CHOOSER_ACTION_OPEN,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      NULL);

  if ( gtk_dialog_run ( GTK_DIALOG ( file_selector ) ) == GTK_RESPONSE_ACCEPT )
  {
     gdouble values[4];
     gint answer = world_file_read_file ( gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_selector)), values );
     if ( answer == 1 )
       a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_WIDGET(cw->x_spin), _("The World file you requested could not be opened for reading.") );
     else if ( answer == 2 )
       a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_WIDGET(cw->x_spin), _("Unexpected end of file reading World file.") );
     else
       // NB answer should == 0 for success
       set_widget_values ( cw, values );
  }

  gtk_widget_destroy ( file_selector );
}

static void georef_layer_export_params ( gpointer *pass_along[2] )
{
  VikGeorefLayer *vgl = VIK_GEOREF_LAYER(pass_along[0]);
  GtkWidget *file_selector = gtk_file_chooser_dialog_new (_("Choose World file"),
				      NULL,
				      GTK_FILE_CHOOSER_ACTION_SAVE,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      NULL);
  if ( gtk_dialog_run ( GTK_DIALOG ( file_selector ) ) == GTK_RESPONSE_ACCEPT )
  {
    FILE *f = g_fopen ( gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER(file_selector) ), "w" );
    
    gtk_widget_destroy ( file_selector ); 
    if ( !f )
    {
      a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_WIDGET(pass_along[0]), _("The file you requested could not be opened for writing.") );
      return;
    }
    else
    {
      fprintf ( f, "%f\n%f\n%f\n%f\n%f\n%f\n", vgl->mpp_easting, vgl->mpp_northing, 0.0, 0.0, vgl->corner.easting, vgl->corner.northing );
      fclose ( f );
      f = NULL;
    }
  }
  else
   gtk_widget_destroy ( file_selector ); 
}

/**
 * Auto attempt to read the world file associated with the image used for the georef
 *  Based on simple file name conventions
 * Only attempted if the preference is on.
 */
static void maybe_read_world_file ( VikFileEntry *vfe, gpointer user_data )
{
  if ( a_preferences_get (VIKING_PREFERENCES_IO_NAMESPACE "georef_auto_read_world_file")->b ) {
    const gchar* filename = vik_file_entry_get_filename(VIK_FILE_ENTRY(vfe));
    gdouble values[4];
    if ( filename && user_data ) {

      changeable_widgets *cw = user_data;

      gboolean upper = g_ascii_isupper (filename[strlen(filename)-1]);
      gchar* filew = g_strconcat ( filename, (upper ? "W" : "w") , NULL );

      if ( world_file_read_file ( filew, values ) == 0 ) {
        set_widget_values ( cw, values );
      }
      else {
        if ( strlen(filename) > 3 ) {
          gchar* file0 = g_strndup ( filename, strlen(filename)-2 );
          gchar* file1 = g_strdup_printf ( "%s%c%c", file0, filename[strlen(filename)-1], (upper ? 'W' : 'w')  );
          if ( world_file_read_file ( file1, values ) == 0 ) {
            set_widget_values ( cw, values );
          }
          g_free ( file1 );
          g_free ( file0 );
        }
      }
      g_free ( filew );
    }
  }
}

static struct LatLon get_ll_tl (VikGeorefLayer *vgl)
{
  struct LatLon ll_tl;
  ll_tl.lat = gtk_spin_button_get_value ( GTK_SPIN_BUTTON(vgl->cw.lat_tl_spin) );
  ll_tl.lon = gtk_spin_button_get_value ( GTK_SPIN_BUTTON(vgl->cw.lon_tl_spin) );
  return ll_tl;
}

static struct LatLon get_ll_br (VikGeorefLayer *vgl)
{
  struct LatLon ll_br;
  ll_br.lat = gtk_spin_button_get_value ( GTK_SPIN_BUTTON(vgl->cw.lat_br_spin) );
  ll_br.lon = gtk_spin_button_get_value ( GTK_SPIN_BUTTON(vgl->cw.lon_br_spin) );
  return ll_br;
}

// Align displayed UTM values with displayed Lat/Lon values
static void align_utm2ll (VikGeorefLayer *vgl)
{
  struct LatLon ll_tl = get_ll_tl (vgl);

  struct UTM utm;
  a_coords_latlon_to_utm ( &ll_tl, &utm );
  gtk_spin_button_set_value ( GTK_SPIN_BUTTON(vgl->cw.ce_spin), utm.easting );
  gtk_spin_button_set_value ( GTK_SPIN_BUTTON(vgl->cw.cn_spin), utm.northing );

  gchar tmp_letter[2];
  tmp_letter[0] = utm.letter;
  tmp_letter[1] = '\0';
  gtk_entry_set_text ( GTK_ENTRY(vgl->cw.utm_letter_entry), tmp_letter );

  gtk_spin_button_set_value ( GTK_SPIN_BUTTON(vgl->cw.utm_zone_spin), utm.zone );
}

// Align displayed Lat/Lon values with displayed UTM values
static void align_ll2utm (VikGeorefLayer *vgl)
{
  struct UTM corner;
  const gchar *letter = gtk_entry_get_text ( GTK_ENTRY(vgl->cw.utm_letter_entry) );
  if (*letter)
    corner.letter = toupper(*letter);
  corner.zone = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(vgl->cw.utm_zone_spin) );
  corner.easting = gtk_spin_button_get_value ( GTK_SPIN_BUTTON(vgl->cw.ce_spin) );
  corner.northing = gtk_spin_button_get_value ( GTK_SPIN_BUTTON(vgl->cw.cn_spin) );

  struct LatLon ll;
  a_coords_utm_to_latlon ( &corner, &ll );
  gtk_spin_button_set_value ( GTK_SPIN_BUTTON(vgl->cw.lat_tl_spin), ll.lat );
  gtk_spin_button_set_value ( GTK_SPIN_BUTTON(vgl->cw.lon_tl_spin), ll.lon );
}

/**
 * Align coordinates between tabs as the user may have changed the values
 *   Use this before acting on the user input
 * This is easier then trying to use the 'value-changed' signal for each individual coordinate
 *  especiallly since it tends to end up in an infinite loop continually updating each other.
 */
static void align_coords ( VikGeorefLayer *vgl )
{
  if (gtk_notebook_get_current_page(GTK_NOTEBOOK(vgl->cw.tabs)) == 0)
    align_ll2utm ( vgl );
  else
    align_utm2ll ( vgl );
}

static void switch_tab (GtkNotebook *notebook, gpointer tab, guint tab_num, gpointer user_data)
{
  VikGeorefLayer *vgl = user_data;
  if ( tab_num == 0 )
    align_utm2ll (vgl);
  else
    align_ll2utm (vgl);
}

/**
 *
 */
static void check_br_is_good_or_msg_user ( VikGeorefLayer *vgl )
{
  // if a 'blank' ll value that's alright
  if ( vgl->ll_br.lat == 0.0 && vgl->ll_br.lon == 0.0 )
    return;

  struct LatLon ll_tl = get_ll_tl (vgl);
  if ( ll_tl.lat < vgl->ll_br.lat || ll_tl.lon > vgl->ll_br.lon )
    a_dialog_warning_msg ( VIK_GTK_WINDOW_FROM_LAYER(vgl), _("Lower right corner values may not be consistent with upper right values") );
}

/**
 *
 */
static void calculate_mpp_from_coords ( GtkWidget *ww, VikGeorefLayer *vgl )
{
  const gchar* filename = vik_file_entry_get_filename(VIK_FILE_ENTRY(vgl->cw.imageentry));
  if ( !filename ) {
    return;
  }
  GError *gx = NULL;
  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file ( filename, &gx );
  if ( gx ) {
    a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_WIDGET(ww), _("Couldn't open image file: %s"), gx->message );
    g_error_free ( gx );
    return;
  }

  guint width = gdk_pixbuf_get_width ( pixbuf );
  guint height = gdk_pixbuf_get_height ( pixbuf );

  if ( width == 0 || height == 0 ) {
    a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_WIDGET(ww), _("Invalid image size: %s"), filename);
  }
  else {
    align_coords ( vgl );

    struct LatLon ll_tl = get_ll_tl (vgl);
    struct LatLon ll_br = get_ll_br (vgl);

    struct LatLon ll_tr;
    ll_tr.lat = ll_tl.lat;
    ll_tr.lon = ll_br.lon;

    struct LatLon ll_bl;
    ll_bl.lat = ll_br.lat;
    ll_bl.lon = ll_tl.lon;

    gdouble diffx = a_coords_latlon_diff ( &ll_tl, &ll_tr );
    gdouble xmpp = diffx / width;

    gdouble diffy = a_coords_latlon_diff ( &ll_tl, &ll_bl );
    gdouble ympp = diffy / height;

    gtk_spin_button_set_value ( GTK_SPIN_BUTTON(vgl->cw.x_spin), xmpp );
    gtk_spin_button_set_value ( GTK_SPIN_BUTTON(vgl->cw.y_spin), ympp );

    check_br_is_good_or_msg_user ( vgl );
  }

  g_object_unref ( G_OBJECT(pixbuf) );
}

#define VIK_SETTINGS_GEOREF_TAB "georef_coordinate_tab"

/* returns TRUE if OK was pressed. */
static gboolean georef_layer_dialog ( VikGeorefLayer *vgl, gpointer vp, GtkWindow *w )
{
  GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Layer Properties"),
                                                  w,
                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_REJECT,
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_ACCEPT,
                                                  NULL );
  /* Default to reject as user really needs to specify map file first */
  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_REJECT );
  GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
  response_w = gtk_dialog_get_widget_for_response ( GTK_DIALOG(dialog), GTK_RESPONSE_REJECT );
#endif
  GtkWidget *table, *wfp_hbox, *wfp_label, *wfp_button, *ce_label, *cn_label, *xlabel, *ylabel, *imagelabel;
  changeable_widgets cw;

  GtkBox *dgbox = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
  table = gtk_table_new ( 4, 2, FALSE );
  gtk_box_pack_start ( dgbox, table, TRUE, TRUE, 0 );

  wfp_hbox = gtk_hbox_new ( FALSE, 0 );
  wfp_label = gtk_label_new ( _("World File Parameters:") );
  wfp_button = gtk_button_new_with_label ( _("Load From File...") );

  gtk_box_pack_start ( GTK_BOX(wfp_hbox), wfp_label, TRUE, TRUE, 0 );
  gtk_box_pack_start ( GTK_BOX(wfp_hbox), wfp_button, FALSE, FALSE, 3 );

  ce_label = gtk_label_new ( _("Corner pixel easting:") );
  cw.ce_spin = gtk_spin_button_new ( (GtkAdjustment *) gtk_adjustment_new ( 4, 0.0, 1500000.0, 1, 5, 0 ), 1, 4 );
  gtk_widget_set_tooltip_text ( GTK_WIDGET(cw.ce_spin), _("the UTM \"easting\" value of the upper-left corner pixel of the map") );

  cn_label = gtk_label_new ( _("Corner pixel northing:") );
  cw.cn_spin = gtk_spin_button_new ( (GtkAdjustment *) gtk_adjustment_new ( 4, 0.0, 9000000.0, 1, 5, 0 ), 1, 4 );
  gtk_widget_set_tooltip_text ( GTK_WIDGET(cw.cn_spin), _("the UTM \"northing\" value of the upper-left corner pixel of the map") );

  xlabel = gtk_label_new ( _("X (easting) scale (mpp): "));
  ylabel = gtk_label_new ( _("Y (northing) scale (mpp): "));

  cw.x_spin = gtk_spin_button_new ( (GtkAdjustment *) gtk_adjustment_new ( 4, VIK_VIEWPORT_MIN_ZOOM, VIK_VIEWPORT_MAX_ZOOM, 1, 5, 0 ), 1, 8 );
  gtk_widget_set_tooltip_text ( GTK_WIDGET(cw.x_spin), _("the scale of the map in the X direction (meters per pixel)") );
  cw.y_spin = gtk_spin_button_new ( (GtkAdjustment *) gtk_adjustment_new ( 4, VIK_VIEWPORT_MIN_ZOOM, VIK_VIEWPORT_MAX_ZOOM, 1, 5, 0 ), 1, 8 );
  gtk_widget_set_tooltip_text ( GTK_WIDGET(cw.y_spin), _("the scale of the map in the Y direction (meters per pixel)") );

  imagelabel = gtk_label_new ( _("Map Image:") );
  cw.imageentry = vik_file_entry_new (GTK_FILE_CHOOSER_ACTION_OPEN, VF_FILTER_IMAGE, maybe_read_world_file, &cw);

  gtk_spin_button_set_value ( GTK_SPIN_BUTTON(cw.ce_spin), vgl->corner.easting );
  gtk_spin_button_set_value ( GTK_SPIN_BUTTON(cw.cn_spin), vgl->corner.northing );
  gtk_spin_button_set_value ( GTK_SPIN_BUTTON(cw.x_spin), vgl->mpp_easting );
  gtk_spin_button_set_value ( GTK_SPIN_BUTTON(cw.y_spin), vgl->mpp_northing );
  if ( vgl->image )
    vik_file_entry_set_filename ( VIK_FILE_ENTRY(cw.imageentry), vgl->image );

  gtk_table_attach_defaults ( GTK_TABLE(table), imagelabel, 0, 1, 0, 1 );
  gtk_table_attach_defaults ( GTK_TABLE(table), cw.imageentry, 1, 2, 0, 1 );
  gtk_table_attach_defaults ( GTK_TABLE(table), wfp_hbox, 0, 2, 1, 2 );
  gtk_table_attach_defaults ( GTK_TABLE(table), xlabel, 0, 1, 2, 3 );
  gtk_table_attach_defaults ( GTK_TABLE(table), cw.x_spin, 1, 2, 2, 3 );
  gtk_table_attach_defaults ( GTK_TABLE(table), ylabel, 0, 1, 3, 4 );
  gtk_table_attach_defaults ( GTK_TABLE(table), cw.y_spin, 1, 2, 3, 4 );

  cw.tabs = gtk_notebook_new();
  GtkWidget *table_utm = gtk_table_new ( 3, 2, FALSE );

  gtk_table_attach_defaults ( GTK_TABLE(table_utm), ce_label, 0, 1, 0, 1 );
  gtk_table_attach_defaults ( GTK_TABLE(table_utm), cw.ce_spin, 1, 2, 0, 1 );
  gtk_table_attach_defaults ( GTK_TABLE(table_utm), cn_label, 0, 1, 1, 2 );
  gtk_table_attach_defaults ( GTK_TABLE(table_utm), cw.cn_spin, 1, 2, 1, 2 );

  GtkWidget *utm_hbox = gtk_hbox_new ( FALSE, 0 );
  cw.utm_zone_spin = gtk_spin_button_new ((GtkAdjustment*)gtk_adjustment_new( vgl->corner.zone, 1, 60, 1, 5, 0 ), 1, 0 );
  gtk_box_pack_start ( GTK_BOX(utm_hbox), gtk_label_new(_("Zone:")), TRUE, TRUE, 0 );
  gtk_box_pack_start ( GTK_BOX(utm_hbox), cw.utm_zone_spin, TRUE, TRUE, 0 );
  gtk_box_pack_start ( GTK_BOX(utm_hbox), gtk_label_new(_("Letter:")), TRUE, TRUE, 0 );
  cw.utm_letter_entry = gtk_entry_new ();
  gtk_entry_set_max_length ( GTK_ENTRY(cw.utm_letter_entry), 1 );
  gtk_entry_set_width_chars ( GTK_ENTRY(cw.utm_letter_entry), 2 );
  gchar tmp_letter[2];
  tmp_letter[0] = vgl->corner.letter;
  tmp_letter[1] = '\0';
  gtk_entry_set_text ( GTK_ENTRY(cw.utm_letter_entry), tmp_letter );
  gtk_box_pack_start ( GTK_BOX(utm_hbox), cw.utm_letter_entry, TRUE, TRUE, 0 );

  gtk_table_attach_defaults ( GTK_TABLE(table_utm), utm_hbox, 0, 2, 2, 3 );

  // Lat/Lon
  GtkWidget *table_ll = gtk_table_new ( 5, 2, FALSE );

  GtkWidget *lat_tl_label = gtk_label_new ( _("Upper left latitude:") );
  cw.lat_tl_spin = gtk_spin_button_new ( (GtkAdjustment *) gtk_adjustment_new (0.0,-90,90.0,0.05,0.1,0), 0.1, 6 );
  GtkWidget *lon_tl_label = gtk_label_new ( _("Upper left longitude:") );
  cw.lon_tl_spin = gtk_spin_button_new ( (GtkAdjustment *) gtk_adjustment_new (0.0,-180,180.0,0.05,0.1,0), 0.1, 6 );
  GtkWidget *lat_br_label = gtk_label_new ( _("Lower right latitude:") );
  cw.lat_br_spin = gtk_spin_button_new ( (GtkAdjustment *) gtk_adjustment_new (0.0,-90,90.0,0.05,0.1,0), 0.1, 6 );
  GtkWidget *lon_br_label = gtk_label_new ( _("Lower right longitude:") );
  cw.lon_br_spin = gtk_spin_button_new ( (GtkAdjustment *) gtk_adjustment_new (0.0,-180.0,180.0,0.05,0.1,0), 0.1, 6 );

  gtk_table_attach_defaults ( GTK_TABLE(table_ll), lat_tl_label, 0, 1, 0, 1 );
  gtk_table_attach_defaults ( GTK_TABLE(table_ll), cw.lat_tl_spin, 1, 2, 0, 1 );
  gtk_table_attach_defaults ( GTK_TABLE(table_ll), lon_tl_label, 0, 1, 1, 2 );
  gtk_table_attach_defaults ( GTK_TABLE(table_ll), cw.lon_tl_spin, 1, 2, 1, 2 );
  gtk_table_attach_defaults ( GTK_TABLE(table_ll), lat_br_label, 0, 1, 2, 3 );
  gtk_table_attach_defaults ( GTK_TABLE(table_ll), cw.lat_br_spin, 1, 2, 2, 3 );
  gtk_table_attach_defaults ( GTK_TABLE(table_ll), lon_br_label, 0, 1, 3, 4 );
  gtk_table_attach_defaults ( GTK_TABLE(table_ll), cw.lon_br_spin, 1, 2, 3, 4 );

  GtkWidget *calc_mpp_button = gtk_button_new_with_label ( _("Calculate MPP values from coordinates") );
  gtk_widget_set_tooltip_text ( calc_mpp_button, _("Enter all corner coordinates before calculating the MPP values from the image size") );
  gtk_table_attach_defaults ( GTK_TABLE(table_ll), calc_mpp_button, 0, 2, 4, 5 );

  VikCoord vc;
  vik_coord_load_from_utm (&vc, VIK_COORD_LATLON, &(vgl->corner));
  gtk_spin_button_set_value ( GTK_SPIN_BUTTON(cw.lat_tl_spin), vc.north_south );
  gtk_spin_button_set_value ( GTK_SPIN_BUTTON(cw.lon_tl_spin), vc.east_west );
  gtk_spin_button_set_value ( GTK_SPIN_BUTTON(cw.lat_br_spin), vgl->ll_br.lat );
  gtk_spin_button_set_value ( GTK_SPIN_BUTTON(cw.lon_br_spin), vgl->ll_br.lon );

  gtk_notebook_append_page(GTK_NOTEBOOK(cw.tabs), GTK_WIDGET(table_utm), gtk_label_new(_("UTM")));
  gtk_notebook_append_page(GTK_NOTEBOOK(cw.tabs), GTK_WIDGET(table_ll), gtk_label_new(_("Latitude/Longitude")));
  gtk_box_pack_start ( dgbox, cw.tabs, TRUE, TRUE, 0 );

  GtkWidget *alpha_hbox = gtk_hbox_new ( FALSE, 0 );
  // GTK3 => GtkWidget *alpha_scale = gtk_scale_new_with_range ( GTK_ORIENTATION_HORIZONTAL, 0, 255, 1 );
  GtkWidget *alpha_scale = gtk_hscale_new_with_range ( 0, 255, 1 );
  gtk_scale_set_digits ( GTK_SCALE(alpha_scale), 0 );
  gtk_range_set_value ( GTK_RANGE(alpha_scale), vgl->alpha );
  gtk_box_pack_start ( GTK_BOX(alpha_hbox), gtk_label_new(_("Alpha:")), TRUE, TRUE, 0 );
  gtk_box_pack_start ( GTK_BOX(alpha_hbox), alpha_scale, TRUE, TRUE, 0 );
  gtk_box_pack_start ( dgbox, alpha_hbox, TRUE, TRUE, 0 );

  vgl->cw = cw;

  g_signal_connect ( G_OBJECT(vgl->cw.tabs), "switch-page", G_CALLBACK(switch_tab), vgl );
  g_signal_connect ( G_OBJECT(calc_mpp_button), "clicked", G_CALLBACK(calculate_mpp_from_coords), vgl );

  g_signal_connect_swapped ( G_OBJECT(wfp_button), "clicked", G_CALLBACK(georef_layer_dialog_load), &cw );

  if ( response_w )
    gtk_widget_grab_focus ( response_w );

  gtk_widget_show_all ( dialog );

  // Remember setting the notebook page must be done after the widget is visible.
  gint page_num = 0;
  if ( a_settings_get_integer ( VIK_SETTINGS_GEOREF_TAB, &page_num ) )
    if ( page_num < 0 || page_num > 1 )
      page_num = 0;
  gtk_notebook_set_current_page ( GTK_NOTEBOOK(cw.tabs), page_num );

  if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT )
  {
    align_coords ( vgl );

    vgl->corner.easting = gtk_spin_button_get_value ( GTK_SPIN_BUTTON(cw.ce_spin) );
    vgl->corner.northing = gtk_spin_button_get_value ( GTK_SPIN_BUTTON(cw.cn_spin) );
    vgl->corner.zone = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(cw.utm_zone_spin) );
    const gchar *letter = gtk_entry_get_text ( GTK_ENTRY(cw.utm_letter_entry) );
    if (*letter)
       vgl->corner.letter = toupper(*letter);
    vgl->mpp_easting = gtk_spin_button_get_value ( GTK_SPIN_BUTTON(cw.x_spin) );
    vgl->mpp_northing = gtk_spin_button_get_value ( GTK_SPIN_BUTTON(cw.y_spin) );
    vgl->ll_br = get_ll_br (vgl);
    check_br_is_good_or_msg_user ( vgl );
    if ( g_strcmp0 (vgl->image, vik_file_entry_get_filename(VIK_FILE_ENTRY(cw.imageentry)) ) != 0 )
    {
      georef_layer_set_image ( vgl, vik_file_entry_get_filename(VIK_FILE_ENTRY(cw.imageentry)) );
      georef_layer_load_image ( vgl, VIK_VIEWPORT(vp), FALSE );
    }

    vgl->alpha = (guint8) gtk_range_get_value ( GTK_RANGE(alpha_scale) );
    if ( vgl->pixbuf && vgl->alpha < 255 )
      vgl->pixbuf = ui_pixbuf_set_alpha ( vgl->pixbuf, vgl->alpha );
    if ( vgl->scaled && vgl->alpha < 255 )
      vgl->scaled = ui_pixbuf_set_alpha ( vgl->scaled, vgl->alpha );

    a_settings_set_integer ( VIK_SETTINGS_GEOREF_TAB, gtk_notebook_get_current_page(GTK_NOTEBOOK(cw.tabs)) );

    gtk_widget_destroy ( GTK_WIDGET(dialog) );
    return TRUE;
  }
  gtk_widget_destroy ( GTK_WIDGET(dialog) );
  return FALSE;
}

static void georef_layer_zoom_to_fit ( gpointer vgl_vlp[2] )
{
  vik_viewport_set_xmpp ( vik_layers_panel_get_viewport(VIK_LAYERS_PANEL(vgl_vlp[1])), VIK_GEOREF_LAYER(vgl_vlp[0])->mpp_easting );
  vik_viewport_set_ympp ( vik_layers_panel_get_viewport(VIK_LAYERS_PANEL(vgl_vlp[1])), VIK_GEOREF_LAYER(vgl_vlp[0])->mpp_northing );
  vik_layers_panel_emit_update ( VIK_LAYERS_PANEL(vgl_vlp[1]) );
}

static void georef_layer_goto_center ( gpointer vgl_vlp[2] )
{
  VikGeorefLayer *vgl = VIK_GEOREF_LAYER ( vgl_vlp[0] );
  VikViewport *vp = vik_layers_panel_get_viewport(VIK_LAYERS_PANEL(vgl_vlp[1]));
  struct UTM utm;
  VikCoord coord;

  vik_coord_to_utm ( vik_viewport_get_center ( vp ), &utm );

  utm.easting = vgl->corner.easting + (vgl->width * vgl->mpp_easting / 2); /* only an approximation */
  utm.northing = vgl->corner.northing - (vgl->height * vgl->mpp_northing / 2);

  vik_coord_load_from_utm ( &coord, vik_viewport_get_coord_mode ( vp ), &utm );
  vik_viewport_set_center_coord ( vp, &coord, TRUE );

  vik_layers_panel_emit_update ( VIK_LAYERS_PANEL(vgl_vlp[1]) );
}

static void georef_layer_add_menu_items ( VikGeorefLayer *vgl, GtkMenu *menu, gpointer vlp )
{
  static gpointer pass_along[2];
  GtkWidget *item;
  pass_along[0] = vgl;
  pass_along[1] = vlp;

  item = gtk_menu_item_new();
  gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
  gtk_widget_show ( item );

  /* Now with icons */
  item = gtk_image_menu_item_new_with_mnemonic ( _("_Zoom to Fit Map") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(georef_layer_zoom_to_fit), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );

  item = gtk_image_menu_item_new_with_mnemonic ( _("_Goto Map Center") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(georef_layer_goto_center), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );

  item = gtk_image_menu_item_new_with_mnemonic ( _("_Export to World File") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_HARDDISK, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(georef_layer_export_params), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );
}


static gpointer georef_layer_move_create ( VikWindow *vw, VikViewport *vvp)
{
  return vvp;
}

static gboolean georef_layer_move_release ( VikGeorefLayer *vgl, GdkEventButton *event, VikViewport *vvp )
{
  if (!vgl || vgl->vl.type != VIK_LAYER_GEOREF)
    return FALSE;

  if ( vgl->click_x != -1 )
  {
    vgl->corner.easting += (event->x - vgl->click_x) * vik_viewport_get_xmpp (vvp);
    vgl->corner.northing -= (event->y - vgl->click_y) * vik_viewport_get_ympp (vvp);
    vik_layer_emit_update ( VIK_LAYER(vgl) );
    return TRUE;
  }
  return FALSE; /* I didn't move anything on this layer! */
}

static gpointer georef_layer_zoom_create ( VikWindow *vw, VikViewport *vvp)
{
  return vvp;
}

static gboolean georef_layer_zoom_press ( VikGeorefLayer *vgl, GdkEventButton *event, VikViewport *vvp )
{
  if (!vgl || vgl->vl.type != VIK_LAYER_GEOREF)
    return FALSE;
  if ( event->button == 1 )
  {
    if ( vgl->mpp_easting < (VIK_VIEWPORT_MAX_ZOOM / 1.05) && vgl->mpp_northing < (VIK_VIEWPORT_MAX_ZOOM / 1.05) )
    {
      vgl->mpp_easting *= 1.01;
      vgl->mpp_northing *= 1.01;
    }
  }
  else
  {
    if ( vgl->mpp_easting > (VIK_VIEWPORT_MIN_ZOOM * 1.05) && vgl->mpp_northing > (VIK_VIEWPORT_MIN_ZOOM * 1.05) )
    {
      vgl->mpp_easting /= 1.01;
      vgl->mpp_northing /= 1.01;
    }
  }
  vik_viewport_set_xmpp ( vvp, vgl->mpp_easting );
  vik_viewport_set_ympp ( vvp, vgl->mpp_northing );
  vik_layer_emit_update ( VIK_LAYER(vgl) );
  return TRUE;
}

static gboolean georef_layer_move_press ( VikGeorefLayer *vgl, GdkEventButton *event, VikViewport *vvp )
{
  if (!vgl || vgl->vl.type != VIK_LAYER_GEOREF)
    return FALSE;
  vgl->click_x = event->x;
  vgl->click_y = event->y;
  return TRUE;
}
