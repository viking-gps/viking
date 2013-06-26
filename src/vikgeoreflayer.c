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

#include "viking.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

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
};

enum { PARAM_IMAGE = 0, PARAM_CE, PARAM_CN, PARAM_ME, PARAM_MN, NUM_PARAMS };

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
static gboolean georef_layer_dialog ( VikGeorefLayer **vgl, gpointer vp, GtkWindow *w );
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
    GDK_CURSOR_IS_PIXMAP, &cursor_geomove_pixbuf },

  { { "GeorefZoomTool", "vik-icon-Georef Zoom Tool",  N_("Georef Z_oom Tool"), NULL,  N_("Georef Zoom Tool"), 0 },
    (VikToolConstructorFunc) georef_layer_zoom_create, NULL, NULL, NULL,
    (VikToolMouseFunc) georef_layer_zoom_press, NULL, NULL,
    (VikToolKeyFunc) NULL,
    FALSE,
    GDK_CURSOR_IS_PIXMAP, &cursor_geozoom_pixbuf },
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

struct _VikGeorefLayer {
  VikLayer vl;
  gchar *image;
  GdkPixbuf *pixbuf;
  struct UTM corner;
  gdouble mpp_easting, mpp_northing;
  guint width, height;

  GdkPixbuf *scaled;
  guint32 scaled_width, scaled_height;

  gint click_x, click_y;
};



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
    case PARAM_ME:  vgl->mpp_easting = data.d; break;
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
  return vgl;
}

static void georef_layer_draw ( VikGeorefLayer *vgl, VikViewport *vp )
{
  if ( vik_viewport_get_drawmode(vp) != VIK_VIEWPORT_DRAWMODE_UTM )
    return;

  if ( vgl->pixbuf )
  {
    struct UTM utm_middle;
    gdouble xmpp = vik_viewport_get_xmpp(vp), ympp = vik_viewport_get_ympp(vp);
    GdkPixbuf *pixbuf = vgl->pixbuf;
    guint layer_width = vgl->width;
    guint layer_height = vgl->height;

    vik_coord_to_utm ( vik_viewport_get_center ( vp ), &utm_middle );

    guint width = vik_viewport_get_width(vp), height = vik_viewport_get_height(vp);
    gint32 x, y;
    vgl->corner.zone = utm_middle.zone;
    vgl->corner.letter = utm_middle.letter;
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
  return georef_layer_dialog ( &vgl, vp, VIK_GTK_WINDOW_FROM_WIDGET(vp) );
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
  }

  if ( !from_file )
  {
    if ( vik_viewport_get_drawmode(vp) != VIK_VIEWPORT_DRAWMODE_UTM )
    {
      a_dialog_warning_msg ( VIK_GTK_WINDOW_FROM_WIDGET(vp),
                             _("GeoRef map cannot be displayed in the current drawmode.\nSelect \"UTM Mode\" from View menu to view it.") );
    }
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
  vgl->image = g_strdup ( image );
}

static gboolean world_file_read_line ( gchar *buffer, gint size, FILE *f, GtkWidget *widget, gboolean use_value )
{
  if (!fgets ( buffer, 1024, f ))
  {
    a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_WIDGET(widget), _("Unexpected end of file reading World file.") );
    g_free ( buffer );
    fclose ( f );
    f = NULL;
    return FALSE;
  }
  if ( use_value )
  {
    gdouble val = g_strtod ( buffer, NULL );
    gtk_spin_button_set_value ( GTK_SPIN_BUTTON(widget), val > 0 ? val : -val );
  }
  return TRUE;
}

static void georef_layer_dialog_load ( GtkWidget *pass_along[4] )
{
  GtkWidget *file_selector = gtk_file_chooser_dialog_new (_("Choose World file"),
				      NULL,
				      GTK_FILE_CHOOSER_ACTION_OPEN,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      NULL);

  if ( gtk_dialog_run ( GTK_DIALOG ( file_selector ) ) == GTK_RESPONSE_ACCEPT )
  {
    FILE *f = g_fopen ( gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER(file_selector) ), "r" );
    gtk_widget_destroy ( file_selector ); 
    if ( !f )
    {
      a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_WIDGET(pass_along[0]), _("The World file you requested could not be opened for reading.") );
      return;
    }
    else
    {
      gchar *buffer = g_malloc ( 1024 * sizeof(gchar) );
      if ( world_file_read_line ( buffer, 1024, f, pass_along[0], TRUE ) && world_file_read_line ( buffer, 1024, f, pass_along[0], FALSE)
        && world_file_read_line ( buffer, 1024, f, pass_along[0], FALSE ) && world_file_read_line ( buffer, 1024, f, pass_along[1], TRUE)
        && world_file_read_line ( buffer, 1024, f, pass_along[2], TRUE ) && world_file_read_line ( buffer, 1024, f, pass_along[3], TRUE ) )
      {
        g_free ( buffer );
        fclose ( f );
	f = NULL;
      }
    }
  }
  else
    gtk_widget_destroy ( file_selector ); 
/* do your jazz
We need a
  file selection dialog
  file opener for reading, if NULL, send error_msg ( VIK_GTK_WINDOW_FROM_WIDGET(pass_along[0]) )
  does that catch directories too?
  read lines -- if not enough lines, give error.
  if anything outside, give error. define range with #define CONSTANTS
  put 'em in thar widgets, and that's it.
*/
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

/* returns TRUE if OK was pressed. */
static gboolean georef_layer_dialog ( VikGeorefLayer **vgl, gpointer vp, GtkWindow *w )
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
  GtkWidget *table, *wfp_hbox, *wfp_label, *wfp_button, *ce_label, *ce_spin, *cn_label, *cn_spin, *xlabel, *xspin, *ylabel, *yspin, *imagelabel, *imageentry;

  GtkWidget *pass_along[4];

  table = gtk_table_new ( 6, 2, FALSE );
  gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), table, TRUE, TRUE, 0 );

  wfp_hbox = gtk_hbox_new ( FALSE, 0 );
  wfp_label = gtk_label_new ( _("World File Parameters:") );
  wfp_button = gtk_button_new_with_label ( _("Load From File...") );

  gtk_box_pack_start ( GTK_BOX(wfp_hbox), wfp_label, TRUE, TRUE, 0 );
  gtk_box_pack_start ( GTK_BOX(wfp_hbox), wfp_button, FALSE, FALSE, 3 );

  ce_label = gtk_label_new ( _("Corner pixel easting:") );
  ce_spin = gtk_spin_button_new ( (GtkAdjustment *) gtk_adjustment_new ( 4, 0.0, 1500000.0, 1, 5, 0 ), 1, 4 );
  gtk_widget_set_tooltip_text ( GTK_WIDGET(ce_spin), _("the UTM \"easting\" value of the upper-left corner pixel of the map") );

  cn_label = gtk_label_new ( _("Corner pixel northing:") );
  cn_spin = gtk_spin_button_new ( (GtkAdjustment *) gtk_adjustment_new ( 4, 0.0, 9000000.0, 1, 5, 0 ), 1, 4 );
  gtk_widget_set_tooltip_text ( GTK_WIDGET(cn_spin), _("the UTM \"northing\" value of the upper-left corner pixel of the map") );

  xlabel = gtk_label_new ( _("X (easting) scale (mpp): "));
  ylabel = gtk_label_new ( _("Y (northing) scale (mpp): "));

  xspin = gtk_spin_button_new ( (GtkAdjustment *) gtk_adjustment_new ( 4, VIK_VIEWPORT_MIN_ZOOM, VIK_VIEWPORT_MAX_ZOOM, 1, 5, 0 ), 1, 8 );
  gtk_widget_set_tooltip_text ( GTK_WIDGET(xspin), _("the scale of the map in the X direction (meters per pixel)") );
  yspin = gtk_spin_button_new ( (GtkAdjustment *) gtk_adjustment_new ( 4, VIK_VIEWPORT_MIN_ZOOM, VIK_VIEWPORT_MAX_ZOOM, 1, 5, 0 ), 1, 8 );
  gtk_widget_set_tooltip_text ( GTK_WIDGET(yspin), _("the scale of the map in the Y direction (meters per pixel)") );

  imagelabel = gtk_label_new ( _("Map Image:") );
  imageentry = vik_file_entry_new (GTK_FILE_CHOOSER_ACTION_OPEN);

  if (*vgl)
  {
    gtk_spin_button_set_value ( GTK_SPIN_BUTTON(ce_spin), (*vgl)->corner.easting );
    gtk_spin_button_set_value ( GTK_SPIN_BUTTON(cn_spin), (*vgl)->corner.northing );
    gtk_spin_button_set_value ( GTK_SPIN_BUTTON(xspin), (*vgl)->mpp_easting );
    gtk_spin_button_set_value ( GTK_SPIN_BUTTON(yspin), (*vgl)->mpp_northing );
    if ( (*vgl)->image )
    vik_file_entry_set_filename ( VIK_FILE_ENTRY(imageentry), (*vgl)->image );
  }
  else
  {
    VikCoord corner_coord;
    struct UTM utm;
    vik_viewport_screen_to_coord ( VIK_VIEWPORT(vp), 0, 0, &corner_coord );
    vik_coord_to_utm ( &corner_coord, &utm );
    gtk_spin_button_set_value ( GTK_SPIN_BUTTON(ce_spin), utm.easting );
    gtk_spin_button_set_value ( GTK_SPIN_BUTTON(cn_spin), utm.northing );
    gtk_spin_button_set_value ( GTK_SPIN_BUTTON(xspin), vik_viewport_get_xmpp ( VIK_VIEWPORT(vp) ) );
    gtk_spin_button_set_value ( GTK_SPIN_BUTTON(yspin), vik_viewport_get_ympp ( VIK_VIEWPORT(vp) ) );
  }

  gtk_table_attach_defaults ( GTK_TABLE(table), imagelabel, 0, 1, 0, 1 );
  gtk_table_attach_defaults ( GTK_TABLE(table), imageentry, 1, 2, 0, 1 );
  gtk_table_attach_defaults ( GTK_TABLE(table), wfp_hbox, 0, 2, 1, 2 );
  gtk_table_attach_defaults ( GTK_TABLE(table), xlabel, 0, 1, 2, 3 );
  gtk_table_attach_defaults ( GTK_TABLE(table), xspin, 1, 2, 2, 3 );
  gtk_table_attach_defaults ( GTK_TABLE(table), ylabel, 0, 1, 3, 4 );
  gtk_table_attach_defaults ( GTK_TABLE(table), yspin, 1, 2, 3, 4 );
  gtk_table_attach_defaults ( GTK_TABLE(table), ce_label, 0, 1, 4, 5 );
  gtk_table_attach_defaults ( GTK_TABLE(table), ce_spin, 1, 2, 4, 5 );
  gtk_table_attach_defaults ( GTK_TABLE(table), cn_label, 0, 1, 5, 6 );
  gtk_table_attach_defaults ( GTK_TABLE(table), cn_spin, 1, 2, 5, 6 );

  pass_along[0] = xspin;
  pass_along[1] = yspin;
  pass_along[2] = ce_spin;
  pass_along[3] = cn_spin;
  g_signal_connect_swapped ( G_OBJECT(wfp_button), "clicked", G_CALLBACK(georef_layer_dialog_load), pass_along );

  if ( response_w )
    gtk_widget_grab_focus ( response_w );

  gtk_widget_show_all ( table );

  if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT )
  {
    if (! *vgl)
    {
      *vgl = georef_layer_new ( VIK_VIEWPORT(vp) );
       vik_layer_rename ( VIK_LAYER(*vgl), vik_georef_layer_interface.name );
    }
    (*vgl)->corner.easting = gtk_spin_button_get_value ( GTK_SPIN_BUTTON(ce_spin) );
    (*vgl)->corner.northing = gtk_spin_button_get_value ( GTK_SPIN_BUTTON(cn_spin) );
    (*vgl)->mpp_easting = gtk_spin_button_get_value ( GTK_SPIN_BUTTON(xspin) );
    (*vgl)->mpp_northing = gtk_spin_button_get_value ( GTK_SPIN_BUTTON(yspin) );
    if ( (!(*vgl)->image) || strcmp( (*vgl)->image, vik_file_entry_get_filename(VIK_FILE_ENTRY(imageentry)) ) != 0 )
    {
      georef_layer_set_image ( *vgl, vik_file_entry_get_filename(VIK_FILE_ENTRY(imageentry)) );
      georef_layer_load_image ( *vgl, VIK_VIEWPORT(vp), FALSE );
    }

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
  vik_viewport_set_center_coord ( vp, &coord );

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
