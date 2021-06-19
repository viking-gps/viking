/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
 *
 * Lat/Lon plotting functions calcxy* are from GPSDrive
 * GPSDrive Copyright (C) 2001-2004 Fritz Ganter <ganter@ganter.at>
 *
 * Multiple UTM zone patch by Kit Transue <notlostyet@didactek.com>
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

#define DEFAULT_BACKGROUND_COLOR "#CCCCCC"

#include <gtk/gtk.h>
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "coords.h"
#include "vikcoord.h"
#include "vikwindow.h"
#include "vikviewport.h"
#include "mapcoord.h"
#include "ui_util.h"
#include "viklayerspanel.h"

/* for ALTI_TO_MPP */
#include "globals.h"
#include "settings.h"
#include "dialog.h"

gdouble mercator_factor ( gdouble x, guint scale )
{
  return (65536.0 / 180 / x) * 256.0 * scale;
}

static gdouble EASTING_OFFSET = 500000.0;

static gint PAD = 10;

static void viewport_finalize ( GObject *gob );
static void viewport_utm_zone_check ( VikViewport *vvp );
static void update_centers ( VikViewport *vvp );
static void free_centers ( VikViewport *vvp, guint start );

static gboolean calcxy(double *x, double *y, double lg, double lt, double zero_long, double zero_lat, double pixelfact_x, double pixelfact_y, gint mapSizeX2, gint mapSizeY2 );
static gboolean calcxy_rev(double *lg, double *lt, gint x, gint y, double zero_long, double zero_lat, double pixelfact_x, double pixelfact_y, gint mapSizeX2, gint mapSizeY2 );
double calcR (double lat);

static double Radius[181];
static void viewport_init_ra();

static GObjectClass *parent_class;

struct _VikViewport {
  GtkDrawingArea drawing_area;
  // cairo types only used in GTK3+ versions
  cairo_t *crt;
  // The main surface for pixbuf, tracks and so on
  cairo_surface_t *surface_main;
  // There can only be one
  cairo_surface_t *surface_tool;
  cairo_t *cr_tool;
#if !GTK_CHECK_VERSION (3,0,0)
  GdkPixmap *scr_buffer;
#endif
  gint width, height;
  gint width_2, height_2; // Half of the normal width and height
  VikCoord center;
  VikCoordMode coord_mode;
  gdouble xmpp, ympp;
  gdouble xmfactor, ymfactor;
  guint scale;          // Permanent scale regardless of the zoom level
  GList *centers;         // The history of requested positions (of VikCoord type)
  guint centers_index;    // current position within the history list
  guint centers_max;      // configurable maximum size of the history list
  guint centers_radius;   // Metres

  gdouble utm_zone_width;
  gboolean one_utm_zone;

  // Remember in GTK3 GdkGC-->cairo_t
  //  and they just refer back to the crt
  GdkGC *background_gc;
  GdkGC *scale_bg_gc;
  GdkGC *black_gc;
  GdkGC *highlight_gc;

  GdkColor background_color;
  GdkColor scale_bg_color;
  GdkColor black_color;
  GdkColor highlight_color;

  GSList *copyrights;
  GSList *logos;

  /* Wether or not display OSD info */
  gboolean draw_scale;
  gboolean draw_centermark;
  gboolean draw_highlight;

  /* subset of coord types. lat lon can be plotted in 2 ways, google or exp. */
  VikViewportDrawMode drawmode;

  /* trigger stuff */
  gpointer trigger;
#if !GTK_CHECK_VERSION (3,0,0)
  GdkPixmap *snapshot_buffer;
#endif
  gboolean half_drawn;
};

static gdouble
viewport_utm_zone_width ( VikViewport *vvp )
{
  if ( vvp->coord_mode == VIK_COORD_UTM ) {
    struct LatLon ll;

    /* get latitude of screen bottom */
    struct UTM utm = *((struct UTM *)(vik_viewport_get_center ( vvp )));
    utm.northing -= vvp -> height * vvp -> ympp / 2;
    a_coords_utm_to_latlon ( &utm, &ll );

    /* boundary */
    ll.lon = (utm.zone - 1) * 6 - 180 ;
    a_coords_latlon_to_utm ( &ll, &utm);
    return fabs ( utm.easting - EASTING_OFFSET ) * 2;
  } else
    return 0.0;
}

enum {
  VW_UPDATED_CENTER_SIGNAL = 0,
  VW_LAST_SIGNAL,
};
static guint viewport_signals[VW_LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (VikViewport, vik_viewport, GTK_TYPE_DRAWING_AREA)

static void
vik_viewport_class_init ( VikViewportClass *klass )
{
  /* Destructor */
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = viewport_finalize;

  parent_class = g_type_class_peek_parent (klass);

  viewport_signals[VW_UPDATED_CENTER_SIGNAL] = g_signal_new ( "updated_center", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (VikViewportClass, updated_center), NULL, NULL,
      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

VikViewport *vik_viewport_new ()
{
  VikViewport *vv = VIK_VIEWPORT ( g_object_new ( VIK_VIEWPORT_TYPE, NULL ) );
  return vv;
}

#define VIK_SETTINGS_VIEW_LAST_LATITUDE "viewport_last_latitude"
#define VIK_SETTINGS_VIEW_LAST_LONGITUDE "viewport_last_longitude"
#define VIK_SETTINGS_VIEW_LAST_ZOOM_X "viewport_last_zoom_xpp"
#define VIK_SETTINGS_VIEW_LAST_ZOOM_Y "viewport_last_zoom_ypp"
#define VIK_SETTINGS_VIEW_HISTORY_SIZE "viewport_history_size"
#define VIK_SETTINGS_VIEW_HISTORY_DIFF_DIST "viewport_history_diff_dist"
#define VIK_SETTINGS_VIEW_SCALE "viewport_scale"

// Hacky method to enable to return a scale value
// Mostly for places in code, such as initializers, where they have no knowledge of any vvp in use.
// Even if there were multiple ones - it's unlikely they would have different scales.
static VikViewport* default_vvp = NULL;

guint vik_viewport_get_scale ( VikViewport *vvp )
{
  if ( vvp )
    return vvp->scale;
  else if ( default_vvp )
    return default_vvp->scale;
  else
    return 1;
}

void init_scale ( VikViewport *vvp )
{
  // The default scale
  vvp->scale = 1;
  // GTK3+ required
  // gtk_widget_get_scale_factor (GTK_WIDGET(x));
  // Further note the scale can change during runtime
  // ATM Just initialize only
  gint res = gdk_screen_get_resolution ( gdk_screen_get_default() );
  g_debug ( "%s: Screen Resolution is '%d'", __FUNCTION__, res );
  if ( res > 50 ) {
    vvp->scale = round (res / 96.0);
    g_debug ( "%s: Scale set to '%d'", __FUNCTION__, vvp->scale );
  }
  // Allow override by user
  gint tmp = vvp->scale;
  if ( a_settings_get_integer ( VIK_SETTINGS_VIEW_SCALE, &tmp ) )
    vvp->scale = (guint)tmp;
}

static void
vik_viewport_init ( VikViewport *vvp )
{
  viewport_init_ra();

  struct UTM utm;
  struct LatLon ll;
  ll.lat = a_vik_get_default_lat();
  ll.lon = a_vik_get_default_long();
  gdouble zoom_x = 4.0;
  gdouble zoom_y = 4.0;

  if ( a_vik_get_startup_method ( ) == VIK_STARTUP_METHOD_LAST_LOCATION ) {
    gdouble lat, lon, dzoom;
    if ( a_settings_get_double ( VIK_SETTINGS_VIEW_LAST_LATITUDE, &lat ) )
      ll.lat = lat;
    if ( a_settings_get_double ( VIK_SETTINGS_VIEW_LAST_LONGITUDE, &lon ) )
      ll.lon = lon;
    if ( a_settings_get_double ( VIK_SETTINGS_VIEW_LAST_ZOOM_X, &dzoom ) )
      zoom_x = dzoom;
    if ( a_settings_get_double ( VIK_SETTINGS_VIEW_LAST_ZOOM_Y, &dzoom ) )
      zoom_y = dzoom;
  }

  a_coords_latlon_to_utm ( &ll, &utm );

  vvp->xmpp = zoom_x;
  vvp->ympp = zoom_y;
  init_scale ( vvp );
  vvp->xmfactor = mercator_factor ( vvp->xmpp, vvp->scale );
  vvp->ymfactor = mercator_factor ( vvp->ympp, vvp->scale );
  vvp->coord_mode = VIK_COORD_LATLON;
  vvp->drawmode = VIK_VIEWPORT_DRAWMODE_MERCATOR;
  vvp->center.mode = VIK_COORD_LATLON;
  vvp->center.north_south = ll.lat;
  vvp->center.east_west = ll.lon;
  vvp->center.utm_zone = (int)utm.zone;
  vvp->center.utm_letter = utm.letter;
  vvp->utm_zone_width = 0.0;
#if !GTK_CHECK_VERSION (3,0,0)
  vvp->scr_buffer = NULL;
#endif
  vvp->background_gc = NULL;
  vvp->highlight_gc = NULL;
  vvp->black_gc = NULL;
  vvp->scale_bg_gc = NULL;

  vvp->copyrights = NULL;
  vvp->centers = NULL;
  vvp->centers_index = 0;
  vvp->centers_max = 20;
  gint tmp = vvp->centers_max;
  if ( a_settings_get_integer ( VIK_SETTINGS_VIEW_HISTORY_SIZE, &tmp ) )
    vvp->centers_max = tmp;
  vvp->centers_radius = 500;
  if ( a_settings_get_integer ( VIK_SETTINGS_VIEW_HISTORY_DIFF_DIST, &tmp ) )
    vvp->centers_radius = tmp;

  vvp->draw_scale = TRUE;
  vvp->draw_centermark = TRUE;
  vvp->draw_highlight = TRUE;

  vvp->trigger = NULL;
  vvp->highlight_color = a_vik_get_startup_highlight_color();
#if GTK_CHECK_VERSION (3,0,0)
  //(void)gdk_color_parse ( DEFAULT_BACKGROUND_COLOR, &(vvp->background_color) );
  // or
  /*
  GdkRGBA *rgbaBC; // Background Colour
  GtkStyleContext *gsc = gtk_widget_get_style_context ( gtk_widget_get_toplevel(GTK_WIDGET(vvp)) );
  gtk_style_context_get ( gsc, gtk_style_context_get_state(gsc), GTK_STYLE_PROPERTY_BACKGROUND_COLOR, &rgbaBC, NULL );
  vvp->background_color = (GdkColor){ rgbaBC->red * 256, rgbaBC->green * 256, rgbaBC->blue * 256 };
  gdk_rgba_free ( rgbaBC );
  */
  //vik_viewport_set_background_color ( vvp, DEFAULT_BACKGROUND_COLOR );
#else
  vvp->snapshot_buffer = NULL;
#endif
  vvp->half_drawn = FALSE;

  // Initiate center history
  update_centers ( vvp );

  g_signal_connect (G_OBJECT(vvp), "configure_event", G_CALLBACK(vik_viewport_configure), NULL);

  // allow VVP to have focus -- enabling key events, etc...
  gtk_widget_set_can_focus ( GTK_WIDGET(vvp), TRUE );

  default_vvp = vvp;
}

GdkColor vik_viewport_get_background_gdkcolor ( VikViewport *vvp )
{
  return vvp->background_color;
}

/* returns pointer to internal static storage, changes next time function called, use quickly */
const gchar *vik_viewport_get_background_color ( VikViewport *vvp )
{
  static gchar color[8];
  g_snprintf(color, sizeof(color), "#%.2x%.2x%.2x", (int)(vvp->background_color.red/256),(int)(vvp->background_color.green/256),(int)(vvp->background_color.blue/256));
  return color;
}

void vik_viewport_set_background_color ( VikViewport *vvp, const gchar *colorname )
{
  if ( gdk_color_parse ( colorname, &(vvp->background_color) ) ) {
#if !GTK_CHECK_VERSION (3,0,0)
    gdk_gc_set_rgb_fg_color ( vvp->background_gc, &(vvp->background_color) );
#endif
  }
  else
    g_warning("%s: Failed to parse color '%s'", __FUNCTION__, colorname);
}

void vik_viewport_set_background_gdkcolor ( VikViewport *vvp, GdkColor color )
{
  vvp->background_color = color;
#if GTK_CHECK_VERSION (3,0,0)
  gdk_cairo_set_source_color ( vvp->background_gc, &color );
#else
  gdk_gc_set_rgb_fg_color ( vvp->background_gc, &color );
#endif
}

GdkColor vik_viewport_get_highlight_gdkcolor ( VikViewport *vvp )
{
  return vvp->highlight_color;
}

/* returns pointer to internal static storage, changes next time function called, use quickly */
const gchar *vik_viewport_get_highlight_color ( VikViewport *vvp )
{
  static gchar color[8];
  g_snprintf(color, sizeof(color), "#%.2x%.2x%.2x", (int)(vvp->highlight_color.red/256),(int)(vvp->highlight_color.green/256),(int)(vvp->highlight_color.blue/256));
  return color;
}

void vik_viewport_set_highlight_color ( VikViewport *vvp, const gchar *colorname )
{
  gdk_color_parse ( colorname, &(vvp->highlight_color) );
#if !GTK_CHECK_VERSION (3,0,0)
  gdk_gc_set_rgb_fg_color ( vvp->highlight_gc, &(vvp->highlight_color) );
#endif
}

void vik_viewport_set_highlight_gdkcolor ( VikViewport *vvp, GdkColor color )
{
  vvp->highlight_color = color;
#if GTK_CHECK_VERSION (3,0,0)
  gdk_cairo_set_source_color ( vvp->highlight_gc, &color );
#else
  gdk_gc_set_rgb_fg_color ( vvp->highlight_gc, &color );
#endif
}

GdkGC *vik_viewport_get_gc_highlight ( VikViewport *vvp )
{
  return vvp->highlight_gc;
}

void vik_viewport_set_highlight_thickness ( VikViewport *vvp, gint thickness )
{
  // Otherwise same GDK_* attributes as in vik_viewport_new_gc
#if !GTK_CHECK_VERSION (3,0,0)
  gdk_gc_set_line_attributes ( vvp->highlight_gc, thickness, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND );
#endif
}

GdkGC *vik_viewport_new_gc ( VikViewport *vvp, const gchar *colorname, gint thickness )
{
  GdkGC *rv = NULL;

#if GTK_CHECK_VERSION (3,0,0)
  // At the moment create a reference rather than returning it directly
  g_return_val_if_fail ( vvp->crt != NULL, rv );
  rv = cairo_reference ( vvp->crt );
#else
  GdkColor color;
  rv = gdk_gc_new ( gtk_widget_get_window(GTK_WIDGET(vvp)) );
  if ( gdk_color_parse ( colorname, &color ) )
    gdk_gc_set_rgb_fg_color ( rv, &color );
  else
    g_warning("%s: Failed to parse color '%s'", __FUNCTION__, colorname);
  gdk_gc_set_line_attributes ( rv, thickness, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND );
#endif
  return rv;
}

GdkGC *vik_viewport_new_gc_from_color ( VikViewport *vvp, GdkColor *color, gint thickness )
{
  GdkGC *rv = NULL;

#if GTK_CHECK_VERSION (3,0,0)
  if ( vvp->crt )
    rv = cairo_reference ( vvp->crt );
#else
  rv = gdk_gc_new ( gtk_widget_get_window(GTK_WIDGET(vvp)) );
  gdk_gc_set_rgb_fg_color ( rv, color );
  gdk_gc_set_line_attributes ( rv, thickness, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND );
#endif
  return rv;
}

GdkGC* vik_viewport_get_black_gc ( VikViewport *vvp )
{
  return vvp->black_gc;
}

void configure_common ( VikViewport *vvp )
{
  if ( vvp->background_gc )
    ui_gc_unref ( vvp->background_gc );
  vvp->background_gc = vik_viewport_new_gc ( vvp, DEFAULT_BACKGROUND_COLOR, 1 );

  if ( vvp->highlight_gc )
    ui_gc_unref ( vvp->highlight_gc );
  vvp->highlight_gc = vik_viewport_new_gc_from_color ( vvp, &vvp->highlight_color, 1 );

  if ( vvp->scale_bg_gc )
    ui_gc_unref ( vvp->scale_bg_gc );
  vvp->scale_bg_gc = vik_viewport_new_gc ( vvp, "grey", 3*vvp->scale );
  gdk_color_parse ( "grey", &vvp->scale_bg_color );

  if ( vvp->black_gc )
    ui_gc_unref ( vvp->black_gc );
  vvp->black_gc = vik_viewport_new_gc ( vvp, "black", vvp->scale );
  gdk_color_parse ( "black", &vvp->black_color );
}

/**
 * A specific viewport resize with the size specified
 * Intended for temporary use, generally to enable an image snapshot of an arbitary size
 *  (not just the window or display size otherwise currently in use)
 */
void vik_viewport_configure_manually ( VikViewport *vvp, gint width, guint height )
{
  vvp->width = width;
  vvp->height = height;

  vvp->width_2 = vvp->width/2;
  vvp->height_2 = vvp->height/2;

#if GTK_CHECK_VERSION (3,0,0)
  // Need to recreate the surface each time the size changes
  //  seemingly no way via the API to resize an existing surface
  if ( vvp->surface_main )
    cairo_surface_destroy ( vvp->surface_main );
  vvp->surface_main = cairo_image_surface_create ( CAIRO_FORMAT_ARGB32, vvp->width, vvp->height );
  if ( vvp->crt )
    cairo_destroy ( vvp->crt );
  vvp->crt = cairo_create ( vvp->surface_main );
#else
  if ( vvp->scr_buffer )
    g_object_unref ( G_OBJECT ( vvp->scr_buffer ) );
  vvp->scr_buffer = gdk_pixmap_new ( gtk_widget_get_window(GTK_WIDGET(vvp)), vvp->width, vvp->height, -1 );

  /* TODO trigger: only if this is enabled !!! */
  if ( vvp->snapshot_buffer )
    g_object_unref ( G_OBJECT ( vvp->snapshot_buffer ) );
  vvp->snapshot_buffer = gdk_pixmap_new ( gtk_widget_get_window(GTK_WIDGET(vvp)), vvp->width, vvp->height, -1 );
#endif

  configure_common ( vvp );

#if GTK_CHECK_VERSION (3,0,0)
  // Performed after above gc's are reset
  vik_layers_panel_configure_layers ( vik_window_layers_panel(VIK_WINDOW_FROM_WIDGET(vvp)) );
#endif
}

/**
 * vik_viewport_get_pixbuf:
 *
 * Returns a #GdkPixbuf of the size specified (aligned from the top left)
 * Typically the size requested should be the full viewport size.
 *
 * The returned pixbuf maybe NULL,
 *  such as if the size requested is too large and the gdk_pixbuf_*() call fails.
 */
GdkPixbuf *vik_viewport_get_pixbuf ( VikViewport *vvp, gint ww, gint hh )
{
  GdkPixbuf *pixbuf;
#if GTK_CHECK_VERSION (3,0,0)
  vik_viewport_sync ( vvp, vvp->crt );
  // Force flushing the image in case otherwise not updated immediately
  cairo_surface_flush ( vvp->surface_main );
  pixbuf = gdk_pixbuf_get_from_surface ( vvp->surface_main, 0, 0, ww, hh );
#else
  pixbuf = gdk_pixbuf_get_from_drawable ( NULL, GDK_DRAWABLE(vvp->scr_buffer), NULL, 0, 0, 0, 0, ww, hh );
#endif
  return pixbuf;
}

/**
 * The returned cairo_t* should be destroyed after use,
 * inconjunction with vik_viewport_surface_tool_destroy() below.
 */
cairo_t *vik_viewport_surface_tool_create ( VikViewport *vvp )
{
  if ( vvp->surface_tool )
    cairo_surface_destroy ( vvp->surface_tool );
  vvp->surface_tool = cairo_image_surface_create ( CAIRO_FORMAT_ARGB32, vvp->width, vvp->height );
  if ( vvp->surface_tool ) {
    // Originally didn't want to know/manage this cairo reference in the viewport
    vvp->cr_tool = cairo_create ( vvp->surface_tool );
    return vvp->cr_tool;
  }
  return NULL;
}

/**
 * Returned value may be NULL
 */
cairo_surface_t *vik_viewport_surface_tool_get ( VikViewport *vvp )
{
  return vvp->surface_tool;
}

/**
 * Remove the surface once finished with the tool
 * NB you should be removing the cairo reference - as returned in
 *   vik_viewport_surface_tool_create() - as well.
 */
void vik_viewport_surface_tool_destroy ( VikViewport *vvp )
{
  if ( vvp->surface_tool )
    cairo_surface_destroy ( vvp->surface_tool );
  vvp->surface_tool = NULL; // Variable is reused so clear the value
  if ( vvp->cr_tool ) {
    cairo_destroy ( vvp->cr_tool );
  }
  vvp->cr_tool = NULL; // Variable is reused so clear the value
}

// In GTK3 haven't found an ideal way to automatically remove this surface
//  so operations that invalidate the surface i.e.
// panning (changing the viewport center) and zooming must call this
// Potentially this could be invoked by a signal mechanism
//  but ATM only used internally to vikviewport, so direct calling suffices
static void viewport_clear_surface_tool ( VikViewport *vvp )
{
#if GTK_CHECK_VERSION (3,0,0)
  if ( vvp->cr_tool )
    ui_cr_clear ( vvp->cr_tool );
#endif
}

#if !GTK_CHECK_VERSION (3,0,0)
GdkPixmap *vik_viewport_get_pixmap ( VikViewport *vvp )
{
  return vvp->scr_buffer;
}
#endif

gboolean vik_viewport_configure ( VikViewport *vvp )
{
  static gboolean first = TRUE;
  g_return_val_if_fail ( vvp != NULL, TRUE );

  GtkAllocation allocation;
  gtk_widget_get_allocation ( GTK_WIDGET(vvp), &allocation );
  gboolean changed_size = FALSE;
  if ( vvp->width != allocation.width || vvp->height != allocation.height || first )
    changed_size = TRUE;
  first = FALSE;
  vvp->width = allocation.width;
  vvp->height = allocation.height;

  vvp->width_2 = vvp->width/2;
  vvp->height_2 = vvp->height/2;

#if GTK_CHECK_VERSION (3,0,0)
  if ( changed_size ) {
    if ( vvp->crt )
      cairo_destroy ( vvp->crt );

    if ( vvp->surface_main )
      cairo_surface_destroy ( vvp->surface_main );

    // One would have thought creating cairo stuff via the gdk functions would be the obvious thing to do
    //  but for unknown reasons it doesn't actually work and nothing gets shown on the the display
    //GdkWindow * gw = gtk_widget_get_window(GTK_WIDGET(vvp));
    //vvp->surface = gdk_window_create_similar_surface ( gw, CAIRO_CONTENT_COLOR_ALPHA, vvp->width, vvp->height );
    //vvp->crt = gdk_cairo_create ( gw );
    // So instead use the raw cairo functions
    vvp->surface_main = cairo_image_surface_create ( CAIRO_FORMAT_ARGB32, vvp->width, vvp->height );
    vvp->crt = cairo_create ( vvp->surface_main );

    VikWindow *vw = VIK_WINDOW_FROM_WIDGET(vvp);
    vik_layers_panel_configure_layers ( vik_window_layers_panel(vw) );
  }
#else
  if ( vvp->scr_buffer )
    g_object_unref ( G_OBJECT ( vvp->scr_buffer ) );

  vvp->scr_buffer = gdk_pixmap_new ( gtk_widget_get_window(GTK_WIDGET(vvp)), vvp->width, vvp->height, -1 );

  /* TODO trigger: only if enabled! */
  if ( vvp->snapshot_buffer )
    g_object_unref ( G_OBJECT ( vvp->snapshot_buffer ) );

  vvp->snapshot_buffer = gdk_pixmap_new ( gtk_widget_get_window(GTK_WIDGET(vvp)), vvp->width, vvp->height, -1 );
  /* TODO trigger */
#endif

  configure_common ( vvp );

#if GTK_CHECK_VERSION (3,0,0)
  // Performed after above gc's are reset
  if ( changed_size )
    // Propagate internal configure
    vik_layers_panel_configure_layers ( vik_window_layers_panel(VIK_WINDOW_FROM_WIDGET(vvp)) );
#endif

  return FALSE;
}

static void viewport_finalize ( GObject *gob )
{
  VikViewport *vvp = VIK_VIEWPORT(gob);

  g_return_if_fail ( vvp != NULL );

  vik_viewport_reset_copyrights ( vvp );
  vik_viewport_reset_logos ( vvp );

  if ( a_vik_get_startup_method ( ) == VIK_STARTUP_METHOD_LAST_LOCATION ) {
    struct LatLon ll;
    vik_coord_to_latlon ( &(vvp->center), &ll );
    a_settings_set_double ( VIK_SETTINGS_VIEW_LAST_LATITUDE, ll.lat );
    a_settings_set_double ( VIK_SETTINGS_VIEW_LAST_LONGITUDE, ll.lon );
    a_settings_set_double ( VIK_SETTINGS_VIEW_LAST_ZOOM_X, vvp->xmpp );
    a_settings_set_double ( VIK_SETTINGS_VIEW_LAST_ZOOM_Y, vvp->ympp );
  }

  if ( vvp->centers )
    g_list_free_full ( vvp->centers, g_free );

#if !GTK_CHECK_VERSION (3,0,0)
  if ( vvp->scr_buffer )
    g_object_unref ( G_OBJECT ( vvp->scr_buffer ) );
  if ( vvp->snapshot_buffer )
    g_object_unref ( G_OBJECT ( vvp->snapshot_buffer ) );
#else
  if ( vvp->crt )
    cairo_destroy ( vvp->crt );
  if ( vvp->surface_main )
    cairo_surface_destroy ( vvp->surface_main );
#endif

  if ( vvp->background_gc )
    ui_gc_unref ( vvp->background_gc );

  if ( vvp->highlight_gc )
    ui_gc_unref ( vvp->highlight_gc );

  if ( vvp->scale_bg_gc )
    ui_gc_unref ( vvp->scale_bg_gc );

  if ( vvp->black_gc )
    ui_gc_unref ( vvp->black_gc );

  G_OBJECT_CLASS(parent_class)->finalize(gob);
}

/**
 * vik_viewport_clear:
 * @vvp: self object
 * 
 * Clear the whole viewport.
 */
void vik_viewport_clear ( VikViewport *vvp )
{
  g_return_if_fail ( vvp != NULL );
#if GTK_CHECK_VERSION (3,0,0)
  if ( vvp->crt ) {
    // Although could draw fully translucent like this
    // cairo_set_source_rgba ( vvp->crt, 0.0, 0.0, 0.0, 0.0 );
    // (which would get the default background)
    // Need to do a 'hard' reset otherwise old image may bleed through especially if pixbufs have alpha values
    // TODO really only need if loaded value from a file (and saving new file only to set if manually set);
    // i.e. main use case is manually set for using own GeoRef layer
    /*
    GdkRGBA rgba;
    rgba.red = vvp->background_color.red / 256.0;
    rgba.green = vvp->background_color.green / 256.0;
    rgba.blue = vvp->background_color.blue / 256.0;
    cairo_set_source_rgba ( vvp->crt, rgba.red, rgba.green, rgba.blue, 1.0 );
    cairo_paint ( vvp->crt );
    */
  }
  ui_cr_clear ( vvp->crt );
#else
  if ( vvp->scr_buffer )
    gdk_draw_rectangle(GDK_DRAWABLE(vvp->scr_buffer), vvp->background_gc, TRUE, 0, 0, vvp->width, vvp->height);
#endif
  vik_viewport_reset_copyrights ( vvp );
  vik_viewport_reset_logos ( vvp );
}

/**
 * vik_viewport_set_draw_scale:
 * @vvp: self
 * @draw_scale: new value
 * 
 * Enable/Disable display of scale.
 */
void vik_viewport_set_draw_scale ( VikViewport *vvp, gboolean draw_scale )
{
  vvp->draw_scale = draw_scale;
}

gboolean vik_viewport_get_draw_scale ( VikViewport *vvp )
{
  return vvp->draw_scale;
}

void vik_viewport_draw_scale ( VikViewport *vvp )
{
  g_return_if_fail ( vvp != NULL );

  if ( vvp->draw_scale ) {
    VikCoord left, right;
    gdouble unit, base, diff, old_unit, old_diff, ratio;
    gint odd, len, SCSIZE = 5, HEIGHT=10;
    PangoLayout *pl;
    gchar s[128];

    vik_viewport_screen_to_coord ( vvp, 0, vvp->height/2, &left );
    vik_viewport_screen_to_coord ( vvp, vvp->width/SCSIZE, vvp->height/2, &right );

    vik_units_distance_t dist_units = a_vik_get_units_distance ();
    switch (dist_units) {
    case VIK_UNITS_DISTANCE_KILOMETRES:
      base = vik_coord_diff ( &left, &right ); // in meters
      break;
    case VIK_UNITS_DISTANCE_MILES:
      // in 0.1 miles (copes better when zoomed in as 1 mile can be too big)
      base = VIK_METERS_TO_MILES(vik_coord_diff ( &left, &right )) * 10.0;
      break;
    case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
      // in 0.1 NM (copes better when zoomed in as 1 NM can be too big)
      base = VIK_METERS_TO_NAUTICAL_MILES(vik_coord_diff ( &left, &right )) * 10.0;
      break;
    default:
      base = 1; // Keep the compiler happy
      g_critical("Houston, we've had a problem. distance=%d", dist_units);
    }
    ratio = (vvp->width/SCSIZE)/base;

    unit = 1;
    diff = fabs(base-unit);
    old_unit = unit;
    old_diff = diff;
    odd = 1;
    while (diff <= old_diff) {
      old_unit = unit;
      old_diff = diff;
      unit = unit * (odd%2 ? 5 : 2);
      diff = fabs(base-unit);
      odd++;
    }
    unit = old_unit;
    len = unit * ratio;

    /* grey background */
    vik_viewport_draw_line(vvp, vvp->scale_bg_gc, 
                           PAD, vvp->height-PAD, PAD + len, vvp->height-PAD, &vvp->scale_bg_color, 3.0*vvp->scale);
    vik_viewport_draw_line(vvp, vvp->scale_bg_gc,
                           PAD, vvp->height-PAD, PAD, vvp->height-PAD-HEIGHT, &vvp->scale_bg_color, 3.0*vvp->scale);
    vik_viewport_draw_line(vvp, vvp->scale_bg_gc,
                           PAD + len, vvp->height-PAD, PAD + len, vvp->height-PAD-HEIGHT, &vvp->scale_bg_color, 3.0*vvp->scale);

    /* black scale */
    vik_viewport_draw_line(vvp, vvp->black_gc,
                           PAD, vvp->height-PAD, PAD + len, vvp->height-PAD, &vvp->black_color, vvp->scale);
    vik_viewport_draw_line(vvp, vvp->black_gc,
                           PAD, vvp->height-PAD, PAD, vvp->height-PAD-HEIGHT, &vvp->black_color, vvp->scale);
    vik_viewport_draw_line(vvp, vvp->black_gc,
                           PAD + len, vvp->height-PAD, PAD + len, vvp->height-PAD-HEIGHT, &vvp->black_color, vvp->scale);
    if (odd%2) {
      int i;
      for (i=1; i<5; i++) {
        vik_viewport_draw_line(vvp, vvp->scale_bg_gc, 
                               PAD+i*len/5, vvp->height-PAD, PAD+i*len/5, vvp->height-PAD-(HEIGHT/2), &vvp->black_color, vvp->scale);
        vik_viewport_draw_line(vvp, vvp->black_gc,
                               PAD+i*len/5, vvp->height-PAD, PAD+i*len/5, vvp->height-PAD-(HEIGHT/2), &vvp->black_color, vvp->scale);
      }
    } else {
      int i;
      for (i=1; i<10; i++) {
        vik_viewport_draw_line(vvp, vvp->scale_bg_gc,
                               PAD+i*len/10, vvp->height-PAD, PAD+i*len/10, vvp->height-PAD-((i==5)?(2*HEIGHT/3):(HEIGHT/2)), &vvp->black_color, vvp->scale);
        vik_viewport_draw_line(vvp, vvp->black_gc,
                               PAD+i*len/10, vvp->height-PAD, PAD+i*len/10, vvp->height-PAD-((i==5)?(2*HEIGHT/3):(HEIGHT/2)), &vvp->black_color, vvp->scale);
      }
    }

    pl = gtk_widget_create_pango_layout (GTK_WIDGET(&vvp->drawing_area), NULL); 
    pango_layout_set_font_description (pl, gtk_widget_get_style(GTK_WIDGET(&vvp->drawing_area))->font_desc);

    switch (dist_units) {
    case VIK_UNITS_DISTANCE_KILOMETRES:
      if (unit >= 1000) {
        sprintf(s, "%d km", (int)unit/1000);
      } else {
        sprintf(s, "%d m", (int)unit);
      }
      break;
    case VIK_UNITS_DISTANCE_MILES:
      // Handle units in 0.1 miles
      if (unit < 10.0) {
        sprintf(s, "%0.1f miles", unit/10.0);
      }
      else if ((int)unit == 10.0) {
        sprintf(s, "1 mile");
      }
      else {
        sprintf(s, "%d miles", (int)(unit/10.0));
      }
      break;
    case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
      // Handle units in 0.1 NM
      if (unit < 10.0) {
        sprintf(s, "%0.1f NM", unit/10.0);
      }
      else if ((int)unit == 10.0) {
        sprintf(s, "1 NM");
      }
      else {
        sprintf(s, "%d NMs", (int)(unit/10.0));
      }
      break;
    default:
      g_critical("Houston, we've had a problem. distance=%d", dist_units);
    }
    pango_layout_set_text(pl, s, -1);
    int ww, hh;
    pango_layout_get_pixel_size ( pl, &ww, &hh );
    vik_viewport_draw_layout ( vvp, vvp->black_gc, PAD + len + PAD, vvp->height - PAD - HEIGHT/2 - hh/2, pl, &vvp->black_color );
    g_object_unref(pl);
    pl = NULL;
  }
}

void vik_viewport_draw_copyright ( VikViewport *vvp )
{
  g_return_if_fail ( vvp != NULL );

  PangoLayout *pl;
  PangoRectangle ink_rect, logical_rect;
  gchar s[128] = "";

  /* compute copyrights string */
  guint len = g_slist_length ( vvp->copyrights );

  int i;
  for (i = 0 ; i < len ; i++)
  {
    // Stop when buffer is full
    int slen = strlen ( s );
    if ( slen >= 127 )
      break;

    gchar *copyright = g_slist_nth_data ( vvp->copyrights, i );

    // Only use part of this copyright that fits in the available space left
    //  remembering 1 character is left available for the appended space
    int clen = strlen ( copyright );
    if ( slen + clen > 126 ) {
      clen = 126 - slen;
    }

    strncat ( s, copyright, clen );
    strcat ( s, " " );
  }

  /* create pango layout */
  pl = gtk_widget_create_pango_layout (GTK_WIDGET(&vvp->drawing_area), NULL); 
  pango_layout_set_font_description (pl, gtk_widget_get_style(GTK_WIDGET(&vvp->drawing_area))->font_desc);
  pango_layout_set_alignment ( pl, PANGO_ALIGN_RIGHT );

  /* Set the text */
  pango_layout_set_text(pl, s, -1);

  /* Use maximum of half the viewport width */
  pango_layout_set_width ( pl, ( vvp->width / 2 ) * PANGO_SCALE );
  pango_layout_get_pixel_extents(pl, &ink_rect, &logical_rect);
  vik_viewport_draw_layout ( vvp, vvp->black_gc, vvp->width / 2, vvp->height - logical_rect.height, pl, &vvp->black_color );

  /* Free memory */
  g_object_unref(pl);
}

/**
 * vik_viewport_set_draw_centermark:
 * @vvp: self object
 * @draw_centermark: new value
 * 
 * Enable/Disable display of center mark.
 */
void vik_viewport_set_draw_centermark ( VikViewport *vvp, gboolean draw_centermark )
{
  vvp->draw_centermark = draw_centermark;
}

gboolean vik_viewport_get_draw_centermark ( VikViewport *vvp )
{
  return vvp->draw_centermark;
}

void vik_viewport_draw_centermark ( VikViewport *vvp )
{
  g_return_if_fail ( vvp != NULL );

  if ( !vvp->draw_centermark )
    return;

  const int len = 30;
  const int gap = 4;
  int center_x = vvp->width/2;
  int center_y = vvp->height/2;

  // grey background
  vik_viewport_draw_line(vvp, vvp->scale_bg_gc, center_x - len, center_y, center_x - gap, center_y, &vvp->scale_bg_color, 3.0*vvp->scale);
  vik_viewport_draw_line(vvp, vvp->scale_bg_gc, center_x + gap, center_y, center_x + len, center_y, &vvp->scale_bg_color, 3.0*vvp->scale);
  vik_viewport_draw_line(vvp, vvp->scale_bg_gc, center_x, center_y - len, center_x, center_y - gap, &vvp->scale_bg_color, 3.0*vvp->scale);
  vik_viewport_draw_line(vvp, vvp->scale_bg_gc, center_x, center_y + gap, center_x, center_y + len, &vvp->scale_bg_color, 3.0*vvp->scale);

  // black foreground
  vik_viewport_draw_line(vvp, vvp->black_gc, center_x - len, center_y, center_x - gap, center_y, &vvp->black_color, vvp->scale);
  vik_viewport_draw_line(vvp, vvp->black_gc, center_x + gap, center_y, center_x + len, center_y, &vvp->black_color, vvp->scale);
  vik_viewport_draw_line(vvp, vvp->black_gc, center_x, center_y - len, center_x, center_y - gap, &vvp->black_color, vvp->scale);
  vik_viewport_draw_line(vvp, vvp->black_gc, center_x, center_y + gap, center_x, center_y + len, &vvp->black_color, vvp->scale);
}

void vik_viewport_draw_logo ( VikViewport *vvp )
{
  g_return_if_fail ( vvp != NULL );

  guint len = g_slist_length ( vvp->logos );
  gint x = vvp->width - PAD;
  gint y = PAD;
  int i;
  for (i = 0 ; i < len ; i++)
  {
    GdkPixbuf *logo = g_slist_nth_data ( vvp->logos, i );
    gint width = gdk_pixbuf_get_width ( logo );
    gint height = gdk_pixbuf_get_height ( logo );
    vik_viewport_draw_pixbuf ( vvp, logo, 0, 0, x - width, y, width, height );
    x = x - width - PAD;
  }
}

void vik_viewport_set_draw_highlight ( VikViewport *vvp, gboolean draw_highlight )
{
  vvp->draw_highlight = draw_highlight;
}

gboolean vik_viewport_get_draw_highlight ( VikViewport *vvp )
{
  return vvp->draw_highlight;
}

/**
 * GTK2: cr is not used
 * GTK3: Remember GdkGC* is actually cairo_t*
 */
void vik_viewport_sync ( VikViewport *vvp, GdkGC *cr )
{
  g_return_if_fail ( vvp != NULL );
#if !GTK_CHECK_VERSION (3,0,0)
  gdk_draw_drawable(gtk_widget_get_window(GTK_WIDGET(vvp)), gtk_widget_get_style(GTK_WIDGET(vvp))->bg_gc[0], GDK_DRAWABLE(vvp->scr_buffer), 0, 0, 0, 0, vvp->width, vvp->height);
#else
  // Avoid using g_message() or similar in the redraw path as that updates the statusbar
  //  and then that seemingly triggers another update and so on!
  //  so prefer g_printf() for debugging purposes
  if ( cr ) {
    ui_cr_surface_paint ( cr, vvp->surface_main );

    // This *must* be performed within "draw" signal otherwise it simply doesn't actually get drawn on screen
    // Paint all other surfaces...
    if ( vvp->surface_tool )
      ui_cr_surface_paint ( cr, vvp->surface_tool );
  } else
    gtk_widget_queue_draw ( GTK_WIDGET(vvp) );
#endif
}

void vik_viewport_set_zoom ( VikViewport *vvp, gdouble xympp )
{
  g_return_if_fail ( vvp != NULL );
  if ( xympp >= VIK_VIEWPORT_MIN_ZOOM && xympp <= VIK_VIEWPORT_MAX_ZOOM ) {
    vvp->xmpp = vvp->ympp = xympp;
    // Since xmpp & ympp are the same it doesn't matter which one is used here
    vvp->xmfactor = vvp->ymfactor = mercator_factor ( vvp->xmpp, vvp->scale );
  }

  if ( vvp->drawmode == VIK_VIEWPORT_DRAWMODE_UTM )
    viewport_utm_zone_check(vvp);
  viewport_clear_surface_tool ( vvp );
}

/* or could do factor */
void vik_viewport_zoom_in ( VikViewport *vvp )
{
  g_return_if_fail ( vvp != NULL );
  if ( vvp->xmpp >= (VIK_VIEWPORT_MIN_ZOOM*2) && vvp->ympp >= (VIK_VIEWPORT_MIN_ZOOM*2) )
  {
    vvp->xmpp /= 2;
    vvp->ympp /= 2;

    vvp->xmfactor = mercator_factor ( vvp->xmpp, vvp->scale );
    vvp->ymfactor = mercator_factor ( vvp->ympp, vvp->scale );

    viewport_utm_zone_check(vvp);
  }
  viewport_clear_surface_tool ( vvp );
}

void vik_viewport_zoom_out ( VikViewport *vvp )
{
  g_return_if_fail ( vvp != NULL );
  if ( vvp->xmpp <= (VIK_VIEWPORT_MAX_ZOOM/2) && vvp->ympp <= (VIK_VIEWPORT_MAX_ZOOM/2) )
  {
    vvp->xmpp *= 2;
    vvp->ympp *= 2;

    vvp->xmfactor = mercator_factor ( vvp->xmpp, vvp->scale );
    vvp->ymfactor = mercator_factor ( vvp->ympp, vvp->scale );

    viewport_utm_zone_check(vvp);
  }
  viewport_clear_surface_tool ( vvp );
}

gdouble vik_viewport_get_zoom ( VikViewport *vvp )
{
  if ( vvp->xmpp == vvp->ympp )
    return vvp->xmpp;
  return 0.0;
}

gdouble vik_viewport_get_xmpp ( VikViewport *vvp )
{
  return vvp->xmpp;
}

gdouble vik_viewport_get_ympp ( VikViewport *vvp )
{
  return vvp->ympp;
}

void vik_viewport_set_xmpp ( VikViewport *vvp, gdouble xmpp )
{
  if ( xmpp >= VIK_VIEWPORT_MIN_ZOOM && xmpp <= VIK_VIEWPORT_MAX_ZOOM ) {
    vvp->xmpp = xmpp;
    vvp->xmfactor = mercator_factor ( vvp->xmpp, vvp->scale );
    if ( vvp->drawmode == VIK_VIEWPORT_DRAWMODE_UTM )
      viewport_utm_zone_check(vvp);
  }
}

void vik_viewport_set_ympp ( VikViewport *vvp, gdouble ympp )
{
  if ( ympp >= VIK_VIEWPORT_MIN_ZOOM && ympp <= VIK_VIEWPORT_MAX_ZOOM ) {
    vvp->ympp = ympp;
    vvp->ymfactor = mercator_factor ( vvp->ympp, vvp->scale );
    if ( vvp->drawmode == VIK_VIEWPORT_DRAWMODE_UTM )
      viewport_utm_zone_check(vvp);
  }
}


const VikCoord *vik_viewport_get_center ( VikViewport *vvp )
{
  g_return_val_if_fail ( vvp != NULL, NULL );
  return &(vvp->center);
}

/* called every time we update coordinates/zoom */
static void viewport_utm_zone_check ( VikViewport *vvp )
{
  if ( vvp->coord_mode == VIK_COORD_UTM )
  {
    struct UTM utm;
    struct LatLon ll;
    a_coords_utm_to_latlon ( (struct UTM *) &(vvp->center), &ll );
    a_coords_latlon_to_utm ( &ll, &utm );
    if ( utm.zone != vvp->center.utm_zone )
      *((struct UTM *)(&vvp->center)) = utm;

    /* misc. stuff so we don't have to check later */
    vvp->utm_zone_width = viewport_utm_zone_width ( vvp );
    vvp->one_utm_zone = ( vik_viewport_rightmost_zone(vvp) == vik_viewport_leftmost_zone(vvp) );
  }
}

/**
 * Free an individual center position in the history list
 */
static void free_center ( VikViewport *vvp, guint index )
{
  VikCoord *coord = g_list_nth_data ( vvp->centers, index );
  if ( coord )
    g_free ( coord );
  GList *gl = g_list_nth ( vvp->centers, index );
  if ( gl )
    vvp->centers = g_list_delete_link ( vvp->centers, gl );
}

/**
 * Free a set of center positions in the history list,
 *  from the indicated start index to the end of the list
 */
static void free_centers ( VikViewport *vvp, guint start )
{
  // Have to work backward since we delete items referenced by the '_nth()' values,
  //  otherwise if processed forward - removing the lower nth index entries would change the subsequent indexing
  for ( guint i = g_list_length(vvp->centers)-1; i > start; i-- )
    free_center ( vvp, i );
}

/**
 * Store the current center position into the history list
 *  and emit a signal to notify clients the list has been updated
 */
static void update_centers ( VikViewport *vvp )
{
  VikCoord *new_center = g_malloc(sizeof (VikCoord));
  *new_center = vvp->center;

  if ( vvp->centers_index ) {

    if ( vvp->centers_index == vvp->centers_max-1 ) {
      // List is full, so drop the oldest value to make room for the new one
      free_center ( vvp, 0 );
      vvp->centers_index--;
    }
    else {
      // Reset the now unused section of the list
      // Free from the index to the end
      free_centers ( vvp, vvp->centers_index+1 );
    }

  }

  // Store new position
  // NB ATM this can be the same location as the last one in the list
  vvp->centers = g_list_append ( vvp->centers, new_center );

  // Reset to the end (NB should be same as centers_index++)
  vvp->centers_index = g_list_length ( vvp->centers ) - 1;

  // Inform interested subscribers that this change has occurred
  g_signal_emit ( G_OBJECT(vvp), viewport_signals[VW_UPDATED_CENTER_SIGNAL], 0 );
}

/**
 * Show the list of forward/backward positions
 * ATM only for debug usage
 */
void vik_viewport_show_centers ( VikViewport *vvp, GtkWindow *parent )
{
  GList* node = NULL;
  GList* texts = NULL;
  gint index = 0;
  for (node = vvp->centers; node != NULL; node = g_list_next(node)) {
    gchar *lat = NULL, *lon = NULL;
    struct LatLon ll;
    vik_coord_to_latlon (node->data, &ll);
    a_coords_latlon_to_string ( &ll, &lat, &lon );
    gchar *extra = NULL;
    if ( index == vvp->centers_index-1 )
      extra = g_strdup ( " [Back]" );
    else if ( index == vvp->centers_index+1 )
      extra = g_strdup ( " [Forward]" );
    else
      extra = g_strdup ( "" );
    texts = g_list_prepend ( texts , g_strdup_printf ( "%s %s%s", lat, lon, extra ) );
    g_free ( lat );
    g_free ( lon );
    g_free ( extra );
    index++;
  }

  // NB: No i18n as this is just for debug
  // Using this function the dialog allows sorting of the list which isn't appropriate here
  //  but this doesn't matter much for debug purposes of showing stuff...
  GList *ans = a_dialog_select_from_list(parent,
                                         texts,
                                         FALSE,
                                         "Back/Forward Locations",
                                         "Back/Forward Locations");
  g_list_free_full ( ans, g_free );
  g_list_free_full ( texts, g_free );
}

/**
 * vik_viewport_go_back:
 *
 * Move back in the position history
 *
 * Returns: %TRUE one success
 */
gboolean vik_viewport_go_back ( VikViewport *vvp )
{
  // see if the current position is different from the last saved center position within a certain radius
  VikCoord *center = g_list_nth_data ( vvp->centers, vvp->centers_index );
  if ( center ) {
    // Consider an exclusion size (should it zoom level dependent, rather than a fixed value?)
    // When still near to the last saved position we'll jump over it to the one before
    if ( vik_coord_diff ( center, &vvp->center ) > vvp->centers_radius ) {

      if ( vvp->centers_index == g_list_length(vvp->centers)-1 ) {
        // Only when we haven't already moved back in the list
        // Remember where this request came from
        //   (alternatively we could insert in the list on every back attempt)
        update_centers ( vvp );
      }

    }
    // 'Go back' if possible
    // NB if we inserted a position above, then this will then move to the last saved position
    //  otherwise this will skip to the previous saved position, as it's probably somewhere else.
    if ( vvp->centers_index > 0 )
      vvp->centers_index--;
  }
  else {
    return FALSE;
  }

  VikCoord *new_center = g_list_nth_data ( vvp->centers, vvp->centers_index );
  if ( new_center ) {
    vik_viewport_set_center_coord ( vvp, new_center, FALSE );
    return TRUE;
  }
  return FALSE;
}

/**
 * vik_viewport_go_forward:
 *
 * Move forward in the position history
 *
 * Returns: %TRUE one success
 */
gboolean vik_viewport_go_forward ( VikViewport *vvp )
{
  if ( vvp->centers_index == vvp->centers_max-1 )
    return FALSE;

  vvp->centers_index++;
  VikCoord *new_center = g_list_nth_data ( vvp->centers, vvp->centers_index );
  if ( new_center ) {
    vik_viewport_set_center_coord ( vvp, new_center, FALSE );
    return TRUE;
  }
  else
    // Set to end of list
    vvp->centers_index = g_list_length(vvp->centers) - 1;

  return FALSE;
}

/**
 * vik_viewport_back_available:
 *
 * Returns: %TRUE when a previous position in the history is available
 */
gboolean vik_viewport_back_available ( const VikViewport *vvp )
{
  return ( vvp->centers_index > 0 );
}

/**
 * vik_viewport_forward_available:
 *
 * Returns: %TRUE when a next position in the history is available
 */
gboolean vik_viewport_forward_available ( const VikViewport *vvp )
{
  return ( vvp->centers_index < g_list_length(vvp->centers)-1 );
}

/**
 * vik_viewport_set_center_latlon:
 * @vvp:           The viewport to reposition.
 * @ll:            The new center position in Lat/Lon format
 * @save_position: Whether this new position should be saved into the history of positions
 *                 Normally only specific user requests should be saved (i.e. to not include Pan and Zoom repositions)
 */
void vik_viewport_set_center_latlon ( VikViewport *vvp, const struct LatLon *ll, gboolean save_position )
{
  vik_coord_load_from_latlon ( &(vvp->center), vvp->coord_mode, ll );
  if ( save_position )
    update_centers ( vvp );
  if ( vvp->coord_mode == VIK_COORD_UTM )
    viewport_utm_zone_check ( vvp );
  viewport_clear_surface_tool ( vvp );
}

/**
 * vik_viewport_set_center_utm:
 * @vvp:           The viewport to reposition.
 * @utm:           The new center position in UTM format
 * @save_position: Whether this new position should be saved into the history of positions
 *                 Normally only specific user requests should be saved (i.e. to not include Pan and Zoom repositions)
 */
void vik_viewport_set_center_utm ( VikViewport *vvp, const struct UTM *utm, gboolean save_position )
{
  vik_coord_load_from_utm ( &(vvp->center), vvp->coord_mode, utm );
  if ( save_position )
    update_centers ( vvp );
  if ( vvp->coord_mode == VIK_COORD_UTM )
    viewport_utm_zone_check ( vvp );
  viewport_clear_surface_tool ( vvp );
}

/**
 * vik_viewport_set_center_coord:
 * @vvp:           The viewport to reposition.
 * @coord:         The new center position in a VikCoord type
 * @save_position: Whether this new position should be saved into the history of positions
 *                 Normally only specific user requests should be saved (i.e. to not include Pan and Zoom repositions)
 */
void vik_viewport_set_center_coord ( VikViewport *vvp, const VikCoord *coord, gboolean save_position )
{
  vvp->center = *coord;
  if ( save_position )
    update_centers ( vvp );
  if ( vvp->coord_mode == VIK_COORD_UTM )
    viewport_utm_zone_check ( vvp );
  viewport_clear_surface_tool ( vvp );
}

void vik_viewport_corners_for_zonen ( VikViewport *vvp, int zone, VikCoord *ul, VikCoord *br )
{
  g_return_if_fail ( vvp->coord_mode == VIK_COORD_UTM );

  /* get center, then just offset */
  vik_viewport_center_for_zonen ( vvp, VIK_UTM(ul), zone );
  ul->mode = VIK_COORD_UTM;
  *br = *ul;

  ul->north_south += (vvp->ympp * vvp->height / 2);
  ul->east_west -= (vvp->xmpp * vvp->width / 2);
  br->north_south -= (vvp->ympp * vvp->height / 2);
  br->east_west += (vvp->xmpp * vvp->width / 2);
}

void vik_viewport_center_for_zonen ( VikViewport *vvp, struct UTM *center, int zone)
{
  if ( vvp->coord_mode == VIK_COORD_UTM ) {
    *center = *((struct UTM *)(vik_viewport_get_center ( vvp )));
    center->easting -= ( zone - center->zone ) * vvp->utm_zone_width;
    center->zone = zone;
  }
}

gchar vik_viewport_leftmost_zone ( VikViewport *vvp )
{
  if ( vvp->coord_mode == VIK_COORD_UTM ) {
    VikCoord coord;
    g_assert ( vvp != NULL );
    vik_viewport_screen_to_coord ( vvp, 0, 0, &coord );
    return coord.utm_zone;
  }
  return '\0';
}

gchar vik_viewport_rightmost_zone ( VikViewport *vvp )
{
  if ( vvp->coord_mode == VIK_COORD_UTM ) {
    VikCoord coord;
    g_assert ( vvp != NULL );
    vik_viewport_screen_to_coord ( vvp, vvp->width, 0, &coord );
    return coord.utm_zone;
  }
  return '\0';
}


void vik_viewport_set_center_screen ( VikViewport *vvp, int x, int y )
{
  g_return_if_fail ( vvp != NULL );
  if ( vvp->coord_mode == VIK_COORD_UTM ) {
    /* slightly optimized */
    vvp->center.east_west += vvp->xmpp * (x - (vvp->width/2));
    vvp->center.north_south += vvp->ympp * ((vvp->height/2) - y);
    viewport_utm_zone_check ( vvp );
  } else {
    VikCoord tmp;
    vik_viewport_screen_to_coord ( vvp, x, y, &tmp );
    vik_viewport_set_center_coord ( vvp, &tmp, FALSE );
  }
  viewport_clear_surface_tool ( vvp );
}

gint vik_viewport_get_width( VikViewport *vvp )
{
  g_return_val_if_fail ( vvp != NULL, 0 );
  return vvp->width;
}

gint vik_viewport_get_height( VikViewport *vvp )
{
  g_return_val_if_fail ( vvp != NULL, 0 );
  return vvp->height;
}

void vik_viewport_screen_to_coord ( VikViewport *vvp, int x, int y, VikCoord *coord )
{
  g_return_if_fail ( vvp != NULL );

  if ( vvp->coord_mode == VIK_COORD_UTM ) {
    int zone_delta;
    struct UTM *utm = (struct UTM *) coord;
    coord->mode = VIK_COORD_UTM;

    utm->zone = vvp->center.utm_zone;
    utm->letter = vvp->center.utm_letter;
    utm->easting = ( ( x - ( vvp->width_2) ) * vvp->xmpp ) + vvp->center.east_west;
    zone_delta = floor( (utm->easting - EASTING_OFFSET ) / vvp->utm_zone_width + 0.5 );
    utm->zone += zone_delta;
    utm->easting -= zone_delta * vvp->utm_zone_width;
    utm->northing = ( ( ( vvp->height_2) - y ) * vvp->ympp ) + vvp->center.north_south;
  } else if ( vvp->coord_mode == VIK_COORD_LATLON ) {
    coord->mode = VIK_COORD_LATLON;
    if ( vvp->drawmode == VIK_VIEWPORT_DRAWMODE_LATLON ) {
      coord->east_west = vvp->center.east_west + (180.0 * vvp->xmpp / 65536 / 256 * (x - vvp->width_2)) / vvp->scale;
      coord->north_south = vvp->center.north_south + (180.0 * vvp->ympp / 65536 / 256 * (vvp->height_2 - y) / vvp->scale);
    } else if ( vvp->drawmode == VIK_VIEWPORT_DRAWMODE_EXPEDIA )
      calcxy_rev(&(coord->east_west), &(coord->north_south), x, y, vvp->center.east_west, vvp->center.north_south, vvp->xmpp * ALTI_TO_MPP, vvp->ympp * ALTI_TO_MPP, vvp->width_2, vvp->height_2);
    else if ( vvp->drawmode == VIK_VIEWPORT_DRAWMODE_MERCATOR ) {
      /* This isn't called with a high frequently so less need to optimize */
      coord->east_west = vvp->center.east_west + (180.0 * vvp->xmpp / 65536 / 256 * (x - vvp->width_2) / vvp->scale);
      coord->north_south = DEMERCLAT ( MERCLAT(vvp->center.north_south) + (180.0 * vvp->ympp / 65536 / 256 * (vvp->height_2 - y)) / vvp->scale );
    }
  }
}

/*
 * Since this function is used for every drawn trackpoint - it can get called alot
 * Thus x & y position factors are calculated once on zoom changes,
 *  avoiding the need to do it here all the time.
 * For good measure the half width and height values are also pre calculated too.
 */
void vik_viewport_coord_to_screen ( VikViewport *vvp, const VikCoord *coord, int *x, int *y )
{
  static VikCoord tmp;
  g_return_if_fail ( vvp != NULL );

  if ( coord->mode != vvp->coord_mode )
  {
    g_warning ( "Have to convert in vik_viewport_coord_to_screen! This should never happen!");
    vik_coord_copy_convert ( coord, vvp->coord_mode, &tmp );
    coord = &tmp;
  }

  if ( vvp->coord_mode == VIK_COORD_UTM ) {
    struct UTM *center = (struct UTM *) &(vvp->center);
    struct UTM *utm = (struct UTM *) coord;
    if ( center->zone != utm->zone && vvp->one_utm_zone )
    {
      *x = *y = VIK_VIEWPORT_UTM_WRONG_ZONE;
      return;
    }

    *x = ( (utm->easting - center->easting) / vvp->xmpp ) + (vvp->width_2) -
  	  (center->zone - utm->zone ) * vvp->utm_zone_width / vvp->xmpp;
    *y = (vvp->height_2) - ( (utm->northing - center->northing) / vvp->ympp );
  } else if ( vvp->coord_mode == VIK_COORD_LATLON ) {
    struct LatLon *center = (struct LatLon *) &(vvp->center);
    struct LatLon *ll = (struct LatLon *) coord;
    if ( vvp->drawmode == VIK_VIEWPORT_DRAWMODE_LATLON ) {
      *x = vvp->width_2 + ( vvp->xmfactor * (ll->lon - center->lon) );
      *y = vvp->height_2 + ( vvp->ymfactor * (center->lat - ll->lat) );
    } else if ( vvp->drawmode == VIK_VIEWPORT_DRAWMODE_EXPEDIA ) {
      double xx,yy;
      calcxy ( &xx, &yy, center->lon, center->lat, ll->lon, ll->lat, vvp->xmpp * ALTI_TO_MPP, vvp->ympp * ALTI_TO_MPP, vvp->width_2, vvp->height_2 );
      *x = xx; *y = yy;
    } else if ( vvp->drawmode == VIK_VIEWPORT_DRAWMODE_MERCATOR ) {
      *x = vvp->width_2 + ( vvp->xmfactor * (ll->lon - center->lon) );
      *y = vvp->height_2 + ( vvp->ymfactor * ( MERCLAT(center->lat) - MERCLAT(ll->lat) ) );
    }
  }
}

/**
 * a_viewport_clip_line:
 * @x1: screen coord
 * @y1: screen coord
 * @x2: screen coord
 * @y2: screen coord
 *
 * Due to the seemingly undocumented behaviour of gdk_draw_line(), we need to restrict the range of values passed in.
 * So despite it accepting gints, the effective range seems to be the actually the minimum C int range (2^16).
 * This seems to be limitations coming from the X Window System.
 *
 * See http://www.rahul.net/kenton/40errs.html
 * ERROR 7. Boundary conditions.
 * "The X coordinate space is not infinite.
 *  Most drawing functions limit position, width, and height to 16 bit integers (sometimes signed, sometimes unsigned) of accuracy.
 *  Because most C compilers use 32 bit integers, Xlib will not complain if you exceed the 16 bit limit, but your results will usually not be what you expected.
 *  You should be especially careful of this if you are implementing higher level scalable graphics packages."
 *
 * This function should be called before calling gdk_draw_line().
 */
void a_viewport_clip_line ( gint *x1, gint *y1, gint *x2, gint *y2 )
{
  gboolean clip_x1, clip_y1, clip_x2, clip_y2;

  /* First check if there is a need to clip */
  clip_x1 = ( *x1 > G_MAXINT16 ) || ( *x1 < G_MININT16 );
  clip_y1 = ( *y1 > G_MAXINT16 ) || ( *y1 < G_MININT16 );
  clip_x2 = ( *x2 > G_MAXINT16 ) || ( *x2 < G_MININT16 );
  clip_y2 = ( *y2 > G_MAXINT16 ) || ( *y2 < G_MININT16 );

  if ( clip_x1 || clip_y1 || clip_x2 || clip_y2 ) {
    /* y = slope * x + offset */
    gdouble slope = (gdouble)(*y1 - *y2) / (gdouble)(*x1 - *x2);
    gdouble offset = *y1 - slope * *x1;

    if ( clip_x1 ) {
      if ( *x1 > G_MAXINT16 ) {
        *x1 = G_MAXINT16;
        *y1 = (int)round( slope * G_MAXINT16 + offset );
      } else {
        *x1 = G_MININT16;
        *y1 = (int)round( slope * G_MININT16 + offset );
      }
      clip_y1 = ( *y1 > G_MAXINT16 ) || ( *y1 < G_MININT16 );
    }
    if ( clip_y1 ) {
      if ( *y1 > G_MAXINT16 ) {
        *y1 = G_MAXINT16;
        *x1 = (int)round( ( G_MAXINT16 - offset ) / slope );
      } else {
        *y1 = G_MININT16;
        *x1 = (int)round( ( G_MININT16 - offset ) / slope );
      }
    }
    if ( clip_x2 ) {
      if ( *x2 > G_MAXINT16 ) {
        *x2 = G_MAXINT16;
        *y2 = (int)round( slope * G_MAXINT16 + offset );
      } else {
        *x2 = G_MININT16;
        *y2 = (int)round( slope * G_MININT16 + offset );
      }
      clip_y2 = ( *y2 > G_MAXINT16 ) || ( *y2 < G_MININT16 );
    }
    if ( clip_y2 ) {
      if ( *y2 > G_MAXINT16 ) {
        *y2 = G_MAXINT16;
        *x2 = (int)round( ( G_MAXINT16 - offset ) / slope );
      } else {
        *y2 = G_MININT16;
        *x2 = (int)round( ( G_MININT16 - offset ) / slope );
      }
    }
  }
}

/**
 * For GTK3 Need to pass in the color and thickness each time
 *
 */
void vik_viewport_draw_line ( VikViewport *vvp, GdkGC *gc, gint x1, gint y1, gint x2, gint y2, GdkColor *gcolor, guint thickness )
{
  //g_print ( "%s: \n", __FUNCTION__ );
  if ( ! ( ( x1 < 0 && x2 < 0 ) || ( y1 < 0 && y2 < 0 ) ||
       ( x1 > vvp->width && x2 > vvp->width ) || ( y1 > vvp->height && y2 > vvp->height ) ) ) {
    /*** clipping, yeah! ***/
    a_viewport_clip_line ( &x1, &y1, &x2, &y2 );
#if GTK_CHECK_VERSION (3,0,0)
    cairo_set_line_width ( gc, thickness );
    if ( gcolor )
      gdk_cairo_set_source_color ( gc, gcolor );
    ui_cr_draw_line ( gc, x1-0.5, y1-0.5, x2-0.5, y2-0.5 );
    cairo_stroke ( gc );
#else
    gdk_draw_line ( vvp->scr_buffer, gc, x1, y1, x2, y2);
#endif
  }
}

/**
 * For GTK3 Need to pass in the color each time
 */
void vik_viewport_draw_rectangle ( VikViewport *vvp, GdkGC *gc, gboolean filled, gint x1, gint y1, gint x2, gint y2, GdkColor *gcolor )
{
  // Using 32 as half the default waypoint image size, so this draws ensures the highlight gets done
  if ( x1 > -32 && x1 < vvp->width + 32 && y1 > -32 && y1 < vvp->height + 32 ) {
#if GTK_CHECK_VERSION (3,0,0)
    if ( gcolor )
      gdk_cairo_set_source_color ( gc, gcolor );
    ui_cr_draw_rectangle ( gc, filled, x1, y1, x2, y2 );
    cairo_stroke ( gc );
#else
    gdk_draw_rectangle ( vvp->scr_buffer, gc, filled, x1, y1, x2, y2);
#endif
  }
}

/**
 * NB Allows specifing a pixbuf with extents outside the viewport pixel limits
 * as the lower level APIs can handle out of bounds draw requests
 * NB2 For GTK3 this no longer crops the image (i.e. it does not use src_x, src_y, w or h)
 *  and simply draws the whole pixbuf.
 *  Thus use gdk_pixbuf_copy_area() or similar to produce the desired size.
 */
void vik_viewport_draw_pixbuf ( VikViewport *vvp, GdkPixbuf *pixbuf, gint src_x, gint src_y,
                              gint dest_x, gint dest_y, gint w, gint h )
{
#if GTK_CHECK_VERSION (3,0,0)
  // TODO confirm this draws with negative dest_x & dest_y values...
  gdk_cairo_set_source_pixbuf ( vvp->crt, pixbuf, dest_x, dest_y );
  // This is needed after each pixbuf is applied
  //  (i.e. can't group together a series of pixbuf requests and paint once only at the end)
  cairo_paint ( vvp->crt );
#else
  gdk_draw_pixbuf ( vvp->scr_buffer,
                    NULL,
                    pixbuf,
                    src_x, src_y, dest_x, dest_y, w, h,
                    GDK_RGB_DITHER_NONE, 0, 0 );
#endif
}

/**
 * For GTK3 Need to pass in the color each time
 * Angles passed in are 1/64th of degrees (GTK2 style)
 * Typically the caller calculates angles in Radians anyway,
 *  so if moving to GTK3+ only then could change angle parameters to gdouble radians
 */
void vik_viewport_draw_arc ( VikViewport *vvp, GdkGC *gc, gboolean filled, gint x, gint y, gint width, gint height, gint angle1, gint angle2, GdkColor *gcolor )
{
#if GTK_CHECK_VERSION (3,0,0)
  // ATM Only used for drawing circles - so height is ignored
  //  other arc drawing usage also currently uses width==height
  if ( gcolor )
    gdk_cairo_set_source_color ( gc, gcolor );
  cairo_new_sub_path ( gc );
  // Convert from 1/64ths into Radians
  gdouble a1 = angle1/64.0 * (M_PI / 180.0);
  gdouble a2 = angle2/64.0 * (M_PI / 180.0);
  cairo_arc ( gc, x+width/2.0, y+width/2.0, width/2.0, a1, a2 );
  if ( filled )
    cairo_fill ( gc );
  else
    cairo_stroke ( gc );
#else
  gdk_draw_arc ( vvp->scr_buffer, gc, filled, x, y, width, height, angle1, angle2 );
#endif
}

/**
 * For GTK3 Need to pass in the color each time
 */
void vik_viewport_draw_polygon ( VikViewport *vvp, GdkGC *gc, gboolean filled, GdkPoint *points, gint npoints, GdkColor *gcolor )
{
#if GTK_CHECK_VERSION (3,0,0)
  if ( gcolor )
    gdk_cairo_set_source_color ( gc, gcolor );
  // Using cairo no obvious draw polygon method,
  //  so a simple loop to draw between the series of points
  cairo_move_to ( gc, points[0].x, points[0].y ); // 1st point
  for ( gint nn = 0; nn < npoints; nn++ )
    cairo_line_to ( gc, points[nn].x, points[nn].y );
  if ( filled ) {
    cairo_close_path ( gc );
    cairo_fill ( gc );
  } else
    cairo_stroke ( gc );
#else
  gdk_draw_polygon ( vvp->scr_buffer, gc, filled, points, npoints );
#endif
}

VikCoordMode vik_viewport_get_coord_mode ( const VikViewport *vvp )
{
  g_assert ( vvp );
  return vvp->coord_mode;
}

static void viewport_set_coord_mode ( VikViewport *vvp, VikCoordMode mode )
{
  g_return_if_fail ( vvp != NULL );
  vvp->coord_mode = mode;
  vik_coord_convert ( &(vvp->center), mode );
}

/* Thanks GPSDrive */
static gboolean calcxy_rev(double *lg, double *lt, gint x, gint y, double zero_long, double zero_lat, double pixelfact_x, double pixelfact_y, gint mapSizeX2, gint mapSizeY2 )
{
  int px, py;
  gdouble dif, lat, lon;
  double Ra = Radius[90+(gint)zero_lat];

  px = (mapSizeX2 - x) * pixelfact_x;
  py = (-mapSizeY2 + y) * pixelfact_y;

  lat = zero_lat - py / Ra;
  lon =
    zero_long -
    px / (Ra *
         cos (DEG2RAD(lat)));

  dif = lat * (1 - (cos (DEG2RAD(fabs (lon - zero_long)))));
  lat = lat - dif / 1.5;
  lon =
    zero_long -
    px / (Ra *
              cos (DEG2RAD(lat)));

  *lt = lat;
  *lg = lon;
  return (TRUE);
}

/* Thanks GPSDrive */
static gboolean calcxy(double *x, double *y, double lg, double lt, double zero_long, double zero_lat, double pixelfact_x, double pixelfact_y, gint mapSizeX2, gint mapSizeY2 )
{
    double dif;
    double Ra;
    gint mapSizeX = 2 * mapSizeX2;
    gint mapSizeY = 2 * mapSizeY2;

    g_assert ( lt >= -90.0 && lt <= 90.0 );
//    lg *= rad2deg; // FIXME, optimize equations
//    lt *= rad2deg;
    Ra = Radius[90+(gint)lt];
    *x = Ra *
         cos (DEG2RAD(lt)) * (lg - zero_long);
    *y = Ra * (lt - zero_lat);
    dif = Ra * RAD2DEG(1 - (cos ((DEG2RAD(lg - zero_long)))));
    *y = *y + dif / 1.85;
    *x = *x / pixelfact_x;
    *y = *y / pixelfact_y;
    *x = mapSizeX2 - *x;
    *y += mapSizeY2;
    if ((*x < 0)||(*x >= mapSizeX)||(*y < 0)||(*y >= mapSizeY))
        return (FALSE);
    return (TRUE);
}

static void viewport_init_ra()
{
  static gboolean done_before = FALSE;
  if ( !done_before )
  {
    gint i;
    for ( i = -90; i <= 90; i++)
      Radius[i+90] = calcR ( DEG2RAD((double)i) );
    done_before = TRUE;
  }
}

double calcR (double lat)
{
    double a = 6378.137, r, sc, x, y, z;
    double e2 = 0.081082 * 0.081082;
    /*
     * the radius of curvature of an ellipsoidal Earth in the plane of the
     * meridian is given by
     *
     * R' = a * (1 - e^2) / (1 - e^2 * (sin(lat))^2)^(3/2)
     *
     *
     * where a is the equatorial radius, b is the polar radius, and e is
     * the eccentricity of the ellipsoid = sqrt(1 - b^2/a^2)
     *
     * a = 6378 km (3963 mi) Equatorial radius (surface to center distance)
     * b = 6356.752 km (3950 mi) Polar radius (surface to center distance) e
     * = 0.081082 Eccentricity
     */

    lat = DEG2RAD(lat);
    sc = sin (lat);
    x = a * (1.0 - e2);
    z = 1.0 - e2 * sc * sc;
    y = pow (z, 1.5);
    r = x / y;
    r = r * 1000.0;
    return r;
}

gboolean vik_viewport_is_one_zone ( VikViewport *vvp )
{
  return vvp->coord_mode == VIK_COORD_UTM && vvp->one_utm_zone;
}

void vik_viewport_draw_layout ( VikViewport *vvp, GdkGC *gc, gint x, gint y, PangoLayout *layout, GdkColor *gcolor )
{
  if ( x > -100 && x < vvp->width + 100 && y > -100 && y < vvp->height + 100 ) {
#if GTK_CHECK_VERSION (3,0,0)
    if ( gcolor )
      gdk_cairo_set_source_color ( gc, gcolor );
    ui_cr_draw_layout ( gc, x, y, layout );
#else
    gdk_draw_layout ( vvp->scr_buffer, gc, x, y, layout );
#endif
  }
}

void vik_viewport_set_drawmode ( VikViewport *vvp, VikViewportDrawMode drawmode )
{
  vvp->drawmode = drawmode;
  if ( drawmode == VIK_VIEWPORT_DRAWMODE_UTM )
    viewport_set_coord_mode ( vvp, VIK_COORD_UTM );
  else {
    viewport_set_coord_mode ( vvp, VIK_COORD_LATLON );
  }
}

VikViewportDrawMode vik_viewport_get_drawmode ( VikViewport *vvp )
{
  return vvp->drawmode;
}

/******** triggering *******/
void vik_viewport_set_trigger ( VikViewport *vp, gpointer trigger )
{
  vp->trigger = trigger;
}

gpointer vik_viewport_get_trigger ( VikViewport *vp )
{
  return vp->trigger;
}

void vik_viewport_snapshot_save ( VikViewport *vp )
{
#if !GTK_CHECK_VERSION (3,0,0)
  gdk_draw_drawable ( vp->snapshot_buffer, vp->background_gc, vp->scr_buffer, 0, 0, 0, 0, -1, -1 );
#endif
}

void vik_viewport_snapshot_load ( VikViewport *vp )
{
#if !GTK_CHECK_VERSION (3,0,0)
  gdk_draw_drawable ( vp->scr_buffer, vp->background_gc, vp->snapshot_buffer, 0, 0, 0, 0, -1, -1 );
#endif
}

void vik_viewport_set_half_drawn(VikViewport *vp, gboolean half_drawn)
{
  vp->half_drawn = half_drawn;
}

gboolean vik_viewport_get_half_drawn( VikViewport *vp )
{
  return vp->half_drawn;
}


const gchar *vik_viewport_get_drawmode_name(VikViewport *vv, VikViewportDrawMode mode)
 {
  const gchar *name = NULL;
  VikWindow *vw = NULL;
  GtkWidget *mode_button;
  GtkWidget *label;
  
  vw = VIK_WINDOW_FROM_WIDGET(vv);
  mode_button = vik_window_get_drawmode_button(vw, mode);
  label = gtk_bin_get_child(GTK_BIN(mode_button));

  name = gtk_label_get_text ( GTK_LABEL(label) );

  return name;

}

void vik_viewport_get_min_max_lat_lon ( VikViewport *vp, gdouble *min_lat, gdouble *max_lat, gdouble *min_lon, gdouble *max_lon )
{
  VikCoord tleft, tright, bleft, bright;

  vik_viewport_screen_to_coord ( vp, 0, 0, &tleft );
  vik_viewport_screen_to_coord ( vp, vik_viewport_get_width(vp), 0, &tright );
  vik_viewport_screen_to_coord ( vp, 0, vik_viewport_get_height(vp), &bleft );
  vik_viewport_screen_to_coord ( vp, vp->width, vp->height, &bright );

  vik_coord_convert(&tleft, VIK_COORD_LATLON);
  vik_coord_convert(&tright, VIK_COORD_LATLON);
  vik_coord_convert(&bleft, VIK_COORD_LATLON);
  vik_coord_convert(&bright, VIK_COORD_LATLON);

  *max_lat = MAX(tleft.north_south, tright.north_south);
  *min_lat = MIN(bleft.north_south, bright.north_south);
  *max_lon = MAX(tright.east_west, bright.east_west);
  *min_lon = MIN(tleft.east_west, bleft.east_west);
}

/**
 * vik_viewport_get_bbox:
 * @vp: self object
 *
 * Returns: The viewport area as a #LatLonBBox.
 */
LatLonBBox vik_viewport_get_bbox ( VikViewport *vp )
{
  VikCoord tleft, tright, bleft, bright;

  vik_viewport_screen_to_coord ( vp, 0, 0, &tleft );
  vik_viewport_screen_to_coord ( vp, vik_viewport_get_width(vp), 0, &tright );
  vik_viewport_screen_to_coord ( vp, 0, vik_viewport_get_height(vp), &bleft );
  vik_viewport_screen_to_coord ( vp, vp->width, vp->height, &bright );

  vik_coord_convert ( &tleft, VIK_COORD_LATLON );
  vik_coord_convert ( &tright, VIK_COORD_LATLON );
  vik_coord_convert ( &bleft, VIK_COORD_LATLON );
  vik_coord_convert ( &bright, VIK_COORD_LATLON );

  LatLonBBox bbox;
  bbox.south = MIN(bleft.north_south, bright.north_south);
  bbox.north = MAX(tleft.north_south, tright.north_south);
  bbox.east  = MAX(tright.east_west, bright.east_west);
  bbox.west  = MIN(tleft.east_west, bleft.east_west);

  return bbox;
}

void vik_viewport_reset_copyrights ( VikViewport *vp ) 
{
  g_return_if_fail ( vp != NULL );
  g_slist_foreach ( vp->copyrights, (GFunc)g_free, NULL );
  g_slist_free ( vp->copyrights );
  vp->copyrights = NULL;
}

/**
 * vik_viewport_add_copyright:
 * @vp: self object
 * @copyright: new copyright to display
 * 
 * Add a copyright to display on viewport.
 */
void vik_viewport_add_copyright ( VikViewport *vp, const gchar *copyright ) 
{
  g_return_if_fail ( vp != NULL );
  if ( copyright )
  {
    GSList *found = g_slist_find_custom ( vp->copyrights, copyright, (GCompareFunc)strcmp );
    if ( found == NULL )
    {
      gchar *duple = g_strdup ( copyright );
      vp->copyrights = g_slist_prepend ( vp->copyrights, duple );
    }
  }
}

void vik_viewport_reset_logos ( VikViewport *vp )
{
  g_return_if_fail ( vp != NULL );
  /* do not free elem */
  g_slist_free ( vp->logos );
  vp->logos = NULL;
}

void vik_viewport_add_logo ( VikViewport *vp, const GdkPixbuf *logo )
{
  g_return_if_fail ( vp != NULL );
  if ( logo )
  {
    GdkPixbuf *found = NULL; /* FIXME (GdkPixbuf*)g_slist_find_custom ( vp->logos, logo, (GCompareFunc)== ); */
    if ( found == NULL )
    {
      vp->logos = g_slist_prepend ( vp->logos, (gpointer)logo );
    }
  }
}

/**
 * vik_viewport_compute_bearing:
 * @vp: self object
 * @x1: screen coord
 * @y1: screen coord
 * @x2: screen coord
 * @y2: screen coord
 * @angle: bearing in Radian (output)
 * @baseangle: UTM base angle in Radian (output)
 * 
 * Compute bearing.
 */
void vik_viewport_compute_bearing ( VikViewport *vp, gint x1, gint y1, gint x2, gint y2, gdouble *angle, gdouble *baseangle )
{
  if ( vik_viewport_get_drawmode ( vp ) == VIK_VIEWPORT_DRAWMODE_UTM) {
    VikCoord test;
    struct LatLon ll;
    struct UTM u;
    gint tx, ty;

    vik_viewport_screen_to_coord ( vp, x1, y1, &test );
    vik_coord_to_latlon ( &test, &ll );
    ll.lat += vik_viewport_get_ympp ( vp ) * vik_viewport_get_height ( vp ) / 11000.0; // about 11km per degree latitude
    a_coords_latlon_to_utm ( &ll, &u );
    vik_coord_load_from_utm ( &test, VIK_COORD_UTM, &u );
    vik_viewport_coord_to_screen ( vp, &test, &tx, &ty );

    *baseangle = M_PI - atan2(tx-x1, ty-y1);
    *angle -= *baseangle;
  } else{
    *angle = atan2((y2-y1), (x2-x1)) + M_PI_2;
  }

  if (*angle < 0)
    *angle += 2*M_PI;
  if (*angle > 2*M_PI)
    *angle -= 2*M_PI;
}
