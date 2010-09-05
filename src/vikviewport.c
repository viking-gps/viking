/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2007, Evan Battaglia <gtoevan@gmx.net>
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
#define DEFAULT_HIGHLIGHT_COLOR "#EEA500"
/* Default highlight in orange */

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

/* for ALTI_TO_MPP */
#include "globals.h"

static gdouble EASTING_OFFSET = 500000.0;

static gint PAD = 10;

static void viewport_class_init ( VikViewportClass *klass );
static void viewport_init ( VikViewport *vvp );
static void viewport_finalize ( GObject *gob );
static void viewport_utm_zone_check ( VikViewport *vvp );

static gboolean calcxy(double *x, double *y, double lg, double lt, double zero_long, double zero_lat, double pixelfact_x, double pixelfact_y, gint mapSizeX2, gint mapSizeY2 );
static gboolean calcxy_rev(double *lg, double *lt, gint x, gint y, double zero_long, double zero_lat, double pixelfact_x, double pixelfact_y, gint mapSizeX2, gint mapSizeY2 );
double calcR (double lat);

static double Radius[181];
static void viewport_init_ra();

static GObjectClass *parent_class;


struct _VikViewport {
  GtkDrawingArea drawing_area;
  GdkPixmap *scr_buffer;
  gint width, height;
  VikCoord center;
  VikCoordMode coord_mode;
  gdouble xmpp, ympp;

  GdkPixbuf *alpha_pixbuf;
  guint8 alpha_pixbuf_width;
  guint8 alpha_pixbuf_height;

  gdouble utm_zone_width;
  gboolean one_utm_zone;

  GdkGC *background_gc;
  GdkColor background_color;
  GdkGC *scale_bg_gc;

  GSList *copyrights;
  GSList *logos;

  /* Wether or not display OSD info */
  gboolean draw_scale;
  gboolean draw_centermark;
  gboolean draw_highlight;
  GdkGC *highlight_gc;
  GdkColor highlight_color;

  /* subset of coord types. lat lon can be plotted in 2 ways, google or exp. */
  VikViewportDrawMode drawmode;

  /* handy conversion factors which make google plotting extremely fast */
  gdouble google_calcx_fact;
  gdouble google_calcy_fact;
  gdouble google_calcx_rev_fact;
  gdouble google_calcy_rev_fact;

  /* trigger stuff */
  gpointer trigger;
  GdkPixmap *snapshot_buffer;
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


GType vik_viewport_get_type (void)
{
  static GType vvp_type = 0;

  if (!vvp_type)
  {
    static const GTypeInfo vvp_info = 
    {
      sizeof (VikViewportClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      (GClassInitFunc) viewport_class_init,
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikViewport),
      0,
      (GInstanceInitFunc) viewport_init,
    };
    vvp_type = g_type_register_static ( GTK_TYPE_DRAWING_AREA, "VikViewport", &vvp_info, 0 );
  }
  return vvp_type;
}

static void viewport_class_init ( VikViewportClass *klass )
{
  /* Destructor */
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = viewport_finalize;

  parent_class = g_type_class_peek_parent (klass);
}

VikViewport *vik_viewport_new ()
{
  VikViewport *vv = VIK_VIEWPORT ( g_object_new ( VIK_VIEWPORT_TYPE, NULL ) );
  return vv;
}

static void viewport_init ( VikViewport *vvp )
{
  viewport_init_ra();

  struct UTM utm;
  struct LatLon ll;
  ll.lat = a_vik_get_default_lat();
  ll.lon = a_vik_get_default_long();
  a_coords_latlon_to_utm ( &ll, &utm );

  /* TODO: not static */
  vvp->xmpp = 4.0;
  vvp->ympp = 4.0;
  vvp->coord_mode = VIK_COORD_LATLON;
  vvp->drawmode = VIK_VIEWPORT_DRAWMODE_MERCATOR;
  vvp->center.mode = VIK_COORD_LATLON;
  vvp->center.north_south = ll.lat;
  vvp->center.east_west = ll.lon;
  vvp->center.utm_zone = (int)utm.zone;
  vvp->center.utm_letter = utm.letter;
  vvp->scr_buffer = NULL;
  vvp->alpha_pixbuf = NULL;
  vvp->alpha_pixbuf_width = vvp->alpha_pixbuf_height = 0;
  vvp->utm_zone_width = 0.0;
  vvp->background_gc = NULL;
  vvp->highlight_gc = NULL;
  vvp->scale_bg_gc = NULL;

  vvp->copyrights = NULL;

  vvp->draw_scale = TRUE;
  vvp->draw_centermark = TRUE;
  vvp->draw_highlight = TRUE;

  vvp->trigger = NULL;
  vvp->snapshot_buffer = NULL;
  vvp->half_drawn = FALSE;

  g_signal_connect (G_OBJECT(vvp), "configure_event", G_CALLBACK(vik_viewport_configure), NULL);

  GTK_WIDGET_SET_FLAGS(vvp, GTK_CAN_FOCUS); /* allow VVP to have focus -- enabling key events, etc */
}

GdkColor *vik_viewport_get_background_gdkcolor ( VikViewport *vvp )
{
  GdkColor *rv = g_malloc ( sizeof ( GdkColor ) );
  *rv = vvp->background_color;
  return rv;
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
  g_assert ( vvp && vvp->background_gc );
  if ( gdk_color_parse ( colorname, &(vvp->background_color) ) )
    gdk_gc_set_rgb_fg_color ( vvp->background_gc, &(vvp->background_color) );
  else
    g_warning("%s: Failed to parse color '%s'", __FUNCTION__, colorname);
}

void vik_viewport_set_background_gdkcolor ( VikViewport *vvp, GdkColor *color )
{
  g_assert ( vvp && vvp->background_gc );
  vvp->background_color = *color;
  gdk_gc_set_rgb_fg_color ( vvp->background_gc, color );
}

GdkColor *vik_viewport_get_highlight_gdkcolor ( VikViewport *vvp )
{
  GdkColor *rv = g_malloc ( sizeof ( GdkColor ) );
  *rv = vvp->highlight_color;
  return rv;
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
  g_assert ( vvp->highlight_gc );
  gdk_color_parse ( colorname, &(vvp->highlight_color) );
  gdk_gc_set_rgb_fg_color ( vvp->highlight_gc, &(vvp->highlight_color) );
}

void vik_viewport_set_highlight_gdkcolor ( VikViewport *vvp, GdkColor *color )
{
  g_assert ( vvp->highlight_gc );
  vvp->highlight_color = *color;
  gdk_gc_set_rgb_fg_color ( vvp->highlight_gc, color );
}

GdkGC *vik_viewport_get_gc_highlight ( VikViewport *vvp )
{
  return vvp->highlight_gc;
}

GdkGC *vik_viewport_new_gc ( VikViewport *vvp, const gchar *colorname, gint thickness )
{
  GdkGC *rv = NULL;
  GdkColor color;

  rv = gdk_gc_new ( GTK_WIDGET(vvp)->window );
  if ( gdk_color_parse ( colorname, &color ) )
    gdk_gc_set_rgb_fg_color ( rv, &color );
  else
    g_warning("%s: Failed to parse color '%s'", __FUNCTION__, colorname);
  gdk_gc_set_line_attributes ( rv, thickness, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND );
  return rv;
}

GdkGC *vik_viewport_new_gc_from_color ( VikViewport *vvp, GdkColor *color, gint thickness )
{
  GdkGC *rv;

  rv = gdk_gc_new ( GTK_WIDGET(vvp)->window );
  gdk_gc_set_rgb_fg_color ( rv, color );
  gdk_gc_set_line_attributes ( rv, thickness, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND );
  return rv;
}

void vik_viewport_configure_manually ( VikViewport *vvp, gint width, guint height )
{
  vvp->width = width;
  vvp->height = height;
  if ( vvp->scr_buffer )
    g_object_unref ( G_OBJECT ( vvp->scr_buffer ) );
  vvp->scr_buffer = gdk_pixmap_new ( GTK_WIDGET(vvp)->window, vvp->width, vvp->height, -1 );

  /* TODO trigger: only if this is enabled !!! */
  if ( vvp->snapshot_buffer )
    g_object_unref ( G_OBJECT ( vvp->snapshot_buffer ) );
  vvp->snapshot_buffer = gdk_pixmap_new ( GTK_WIDGET(vvp)->window, vvp->width, vvp->height, -1 );
}


GdkPixmap *vik_viewport_get_pixmap ( VikViewport *vvp )
{
  return vvp->scr_buffer;
}

gboolean vik_viewport_configure ( VikViewport *vvp )
{
  g_return_val_if_fail ( vvp != NULL, TRUE );

  vvp->width = GTK_WIDGET(vvp)->allocation.width;
  vvp->height = GTK_WIDGET(vvp)->allocation.height;

  if ( vvp->scr_buffer )
    g_object_unref ( G_OBJECT ( vvp->scr_buffer ) );

  vvp->scr_buffer = gdk_pixmap_new ( GTK_WIDGET(vvp)->window, vvp->width, vvp->height, -1 );

  /* TODO trigger: only if enabled! */
  if ( vvp->snapshot_buffer )
    g_object_unref ( G_OBJECT ( vvp->snapshot_buffer ) );

  vvp->snapshot_buffer = gdk_pixmap_new ( GTK_WIDGET(vvp)->window, vvp->width, vvp->height, -1 );
  /* TODO trigger */

  /* this is down here so it can get a GC (necessary?) */
  if ( !vvp->background_gc )
  {
    vvp->background_gc = vik_viewport_new_gc ( vvp, DEFAULT_BACKGROUND_COLOR, 1 );
    vik_viewport_set_background_color ( vvp, DEFAULT_BACKGROUND_COLOR );
  }
  if ( ! vvp->highlight_gc )
  {
    vvp->highlight_gc = vik_viewport_new_gc ( vvp, DEFAULT_HIGHLIGHT_COLOR, 1 );
    vik_viewport_set_highlight_color ( vvp, DEFAULT_HIGHLIGHT_COLOR );
  }
  if ( !vvp->scale_bg_gc) {
    vvp->scale_bg_gc = vik_viewport_new_gc(vvp, "grey", 3);
  }

  return FALSE;	
}

static void viewport_finalize ( GObject *gob )
{
  VikViewport *vvp = VIK_VIEWPORT(gob);

  g_return_if_fail ( vvp != NULL );

  if ( vvp->scr_buffer )
    g_object_unref ( G_OBJECT ( vvp->scr_buffer ) );

  if ( vvp->snapshot_buffer )
    g_object_unref ( G_OBJECT ( vvp->snapshot_buffer ) );

  if ( vvp->alpha_pixbuf )
    g_object_unref ( G_OBJECT ( vvp->alpha_pixbuf ) );

  if ( vvp->background_gc )
    g_object_unref ( G_OBJECT ( vvp->background_gc ) );

  if ( vvp->highlight_gc )
    g_object_unref ( G_OBJECT ( vvp->highlight_gc ) );

  if ( vvp->scale_bg_gc ) {
    g_object_unref ( G_OBJECT ( vvp->scale_bg_gc ) );
    vvp->scale_bg_gc = NULL;
  }

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
  gdk_draw_rectangle(GDK_DRAWABLE(vvp->scr_buffer), vvp->background_gc, TRUE, 0, 0, vvp->width, vvp->height);
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
    PangoFontDescription *pfd;
    PangoLayout *pl;
    gchar s[128];

    vik_viewport_screen_to_coord ( vvp, 0, vvp->height, &left );
    vik_viewport_screen_to_coord ( vvp, vvp->width/SCSIZE, vvp->height, &right );

    vik_units_distance_t dist_units = a_vik_get_units_distance ();
    switch (dist_units) {
    case VIK_UNITS_DISTANCE_KILOMETRES:
      base = vik_coord_diff ( &left, &right ); // in meters
      break;
    case VIK_UNITS_DISTANCE_MILES:
      // in 0.1 miles (copes better when zoomed in as 1 mile can be too big)
      base = VIK_METERS_TO_MILES(vik_coord_diff ( &left, &right )) * 10.0;
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

    /* white background */
    vik_viewport_draw_line(vvp, vvp->scale_bg_gc, 
			 PAD, vvp->height-PAD, PAD + len, vvp->height-PAD);
    vik_viewport_draw_line(vvp, vvp->scale_bg_gc,
			 PAD, vvp->height-PAD, PAD, vvp->height-PAD-HEIGHT);
    vik_viewport_draw_line(vvp, vvp->scale_bg_gc,
			 PAD + len, vvp->height-PAD, PAD + len, vvp->height-PAD-HEIGHT);
    /* black scale */
    vik_viewport_draw_line(vvp, GTK_WIDGET(&vvp->drawing_area)->style->black_gc, 
			 PAD, vvp->height-PAD, PAD + len, vvp->height-PAD);
    vik_viewport_draw_line(vvp, GTK_WIDGET(&vvp->drawing_area)->style->black_gc, 
			 PAD, vvp->height-PAD, PAD, vvp->height-PAD-HEIGHT);
    vik_viewport_draw_line(vvp, GTK_WIDGET(&vvp->drawing_area)->style->black_gc, 
			 PAD + len, vvp->height-PAD, PAD + len, vvp->height-PAD-HEIGHT);
    if (odd%2) {
      int i;
      for (i=1; i<5; i++) {
        vik_viewport_draw_line(vvp, vvp->scale_bg_gc, 
			     PAD+i*len/5, vvp->height-PAD, PAD+i*len/5, vvp->height-PAD-((i==5)?(2*HEIGHT/3):(HEIGHT/2)));
        vik_viewport_draw_line(vvp, GTK_WIDGET(&vvp->drawing_area)->style->black_gc, 
			     PAD+i*len/5, vvp->height-PAD, PAD+i*len/5, vvp->height-PAD-((i==5)?(2*HEIGHT/3):(HEIGHT/2)));
      }
    } else {
      int i;
      for (i=1; i<10; i++) {
        vik_viewport_draw_line(vvp, vvp->scale_bg_gc,
  			     PAD+i*len/10, vvp->height-PAD, PAD+i*len/10, vvp->height-PAD-((i==5)?(2*HEIGHT/3):(HEIGHT/2)));
        vik_viewport_draw_line(vvp, GTK_WIDGET(&vvp->drawing_area)->style->black_gc, 
  			     PAD+i*len/10, vvp->height-PAD, PAD+i*len/10, vvp->height-PAD-((i==5)?(2*HEIGHT/3):(HEIGHT/2)));
      }
    }
    pl = gtk_widget_create_pango_layout (GTK_WIDGET(&vvp->drawing_area), NULL); 
    pfd = pango_font_description_from_string ("Sans 8"); // FIXME: settable option? global variable?
    pango_layout_set_font_description (pl, pfd);
    pango_font_description_free (pfd);

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
    default:
      g_critical("Houston, we've had a problem. distance=%d", dist_units);
    }
    pango_layout_set_text(pl, s, -1);
    vik_viewport_draw_layout(vvp, GTK_WIDGET(&vvp->drawing_area)->style->black_gc,
			   PAD + len + PAD, vvp->height - PAD - 10, pl);
    g_object_unref(pl);
    pl = NULL;
  }
}

void vik_viewport_draw_copyright ( VikViewport *vvp )
{
  g_return_if_fail ( vvp != NULL );

  PangoFontDescription *pfd;
  PangoLayout *pl;
  PangoRectangle ink_rect, logical_rect;
  gchar s[128] = "";

  /* compute copyrights string */
  guint len = g_slist_length ( vvp->copyrights );
  int i;
  for (i = 0 ; i < len ; i++)
  {
    gchar *copyright = g_slist_nth_data ( vvp->copyrights, i );
    strcat ( s, copyright );
    strcat ( s, " " );
  }

  /* create pango layout */
  pl = gtk_widget_create_pango_layout (GTK_WIDGET(&vvp->drawing_area), NULL); 
  pfd = pango_font_description_from_string ("Sans 8"); // FIXME: settable option? global variable?
  pango_layout_set_font_description (pl, pfd);
  pango_font_description_free (pfd);
  pfd = NULL;
  pango_layout_set_alignment ( pl, PANGO_ALIGN_RIGHT );

  /* Set the text */
  pango_layout_set_text(pl, s, -1);

  /* Use maximum of half the viewport width */
  pango_layout_set_width ( pl, ( vvp->width / 2 - PAD ) * PANGO_SCALE );
  pango_layout_get_pixel_extents(pl, &ink_rect, &logical_rect);
  vik_viewport_draw_layout(vvp, GTK_WIDGET(&vvp->drawing_area)->style->black_gc,
			   vvp->width / 2, vvp->height - PAD - logical_rect.height, pl);

  /* Free memory */
  g_object_unref(pl);
  pl = NULL;		
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
  GdkGC * black_gc = GTK_WIDGET(&vvp->drawing_area)->style->black_gc;

  /* white back ground */
  vik_viewport_draw_line(vvp, vvp->scale_bg_gc, center_x - len, center_y, center_x - gap, center_y);
  vik_viewport_draw_line(vvp, vvp->scale_bg_gc, center_x + gap, center_y, center_x + len, center_y);
  vik_viewport_draw_line(vvp, vvp->scale_bg_gc, center_x, center_y - len, center_x, center_y - gap);
  vik_viewport_draw_line(vvp, vvp->scale_bg_gc, center_x, center_y + gap, center_x, center_y + len);
  /* black fore ground */
  vik_viewport_draw_line(vvp, black_gc, center_x - len, center_y, center_x - gap, center_y);
  vik_viewport_draw_line(vvp, black_gc, center_x + gap, center_y, center_x + len, center_y);
  vik_viewport_draw_line(vvp, black_gc, center_x, center_y - len, center_x, center_y - gap);
  vik_viewport_draw_line(vvp, black_gc, center_x, center_y + gap, center_x, center_y + len);
  
}

void vik_viewport_draw_logo ( VikViewport *vvp )
{
  g_return_if_fail ( vvp != NULL );

  /* compute copyrights string */
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

void vik_viewport_sync ( VikViewport *vvp )
{
  g_return_if_fail ( vvp != NULL );
  gdk_draw_drawable(GTK_WIDGET(vvp)->window, GTK_WIDGET(vvp)->style->bg_gc[0], GDK_DRAWABLE(vvp->scr_buffer), 0, 0, 0, 0, vvp->width, vvp->height);
}

void vik_viewport_pan_sync ( VikViewport *vvp, gint x_off, gint y_off )
{
  gint x, y, wid, hei;

  g_return_if_fail ( vvp != NULL );
  gdk_draw_drawable(GTK_WIDGET(vvp)->window, GTK_WIDGET(vvp)->style->bg_gc[0], GDK_DRAWABLE(vvp->scr_buffer), 0, 0, x_off, y_off, vvp->width, vvp->height);

  if (x_off >= 0) {
    x = 0;
    wid = x_off;
  } else {
    x = vvp->width+x_off; 
    wid = -x_off;
  }
  if (y_off >= 0) {
    y = 0;
    hei = y_off;
  } else {
    y = vvp->height+y_off; 
    hei = -y_off;
  }
  gtk_widget_queue_draw_area(GTK_WIDGET(vvp), x, 0, wid, vvp->height);
  gtk_widget_queue_draw_area(GTK_WIDGET(vvp), 0, y, vvp->width, hei);
}

void vik_viewport_set_zoom ( VikViewport *vvp, gdouble xympp )
{
  g_return_if_fail ( vvp != NULL );
  if ( xympp >= VIK_VIEWPORT_MIN_ZOOM && xympp <= VIK_VIEWPORT_MAX_ZOOM )
    vvp->xmpp = vvp->ympp = xympp;

  if ( vvp->drawmode == VIK_VIEWPORT_DRAWMODE_UTM )
    viewport_utm_zone_check(vvp);
}

/* or could do factor */
void vik_viewport_zoom_in ( VikViewport *vvp )
{
  g_return_if_fail ( vvp != NULL );
  if ( vvp->xmpp >= (VIK_VIEWPORT_MIN_ZOOM*2) && vvp->ympp >= (VIK_VIEWPORT_MIN_ZOOM*2) )
  {
    vvp->xmpp /= 2;
    vvp->ympp /= 2;

    viewport_utm_zone_check(vvp);
  }
}

void vik_viewport_zoom_out ( VikViewport *vvp )
{
  g_return_if_fail ( vvp != NULL );
  if ( vvp->xmpp <= (VIK_VIEWPORT_MAX_ZOOM/2) && vvp->ympp <= (VIK_VIEWPORT_MAX_ZOOM/2) )
  {
    vvp->xmpp *= 2;
    vvp->ympp *= 2;

    viewport_utm_zone_check(vvp);
  }
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
    if ( vvp->drawmode == VIK_VIEWPORT_DRAWMODE_UTM )
      viewport_utm_zone_check(vvp);
  }
}

void vik_viewport_set_ympp ( VikViewport *vvp, gdouble ympp )
{
  if ( ympp >= VIK_VIEWPORT_MIN_ZOOM && ympp <= VIK_VIEWPORT_MAX_ZOOM ) {
    vvp->ympp = ympp;
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

void vik_viewport_set_center_latlon ( VikViewport *vvp, const struct LatLon *ll )
{
  vik_coord_load_from_latlon ( &(vvp->center), vvp->coord_mode, ll );
  if ( vvp->coord_mode == VIK_COORD_UTM )
    viewport_utm_zone_check ( vvp );
}

void vik_viewport_set_center_utm ( VikViewport *vvp, const struct UTM *utm )
{
  vik_coord_load_from_utm ( &(vvp->center), vvp->coord_mode, utm );
  if ( vvp->coord_mode == VIK_COORD_UTM )
    viewport_utm_zone_check ( vvp );
}

void vik_viewport_set_center_coord ( VikViewport *vvp, const VikCoord *coord )
{
  vvp->center = *coord;
  if ( vvp->coord_mode == VIK_COORD_UTM )
    viewport_utm_zone_check ( vvp );
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
    vik_viewport_set_center_coord ( vvp, &tmp );
  }
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
    utm->easting = ( ( x - ( vvp->width / 2) ) * vvp->xmpp ) + vvp->center.east_west;
    zone_delta = floor( (utm->easting - EASTING_OFFSET ) / vvp->utm_zone_width + 0.5 );
    utm->zone += zone_delta;
    utm->easting -= zone_delta * vvp->utm_zone_width;
    utm->northing = ( ( ( vvp->height / 2) - y ) * vvp->ympp ) + vvp->center.north_south;
  } else if ( vvp->coord_mode == VIK_COORD_LATLON ) {
    coord->mode = VIK_COORD_LATLON;
    if ( vvp->drawmode == VIK_VIEWPORT_DRAWMODE_LATLON ) {
      coord->east_west = vvp->center.east_west + (180.0 * vvp->xmpp / 65536 / 256 * (x - vvp->width/2));
      coord->north_south = vvp->center.north_south + (180.0 * vvp->ympp / 65536 / 256 * (vvp->height/2 - y));
    } else if ( vvp->drawmode == VIK_VIEWPORT_DRAWMODE_EXPEDIA )
      calcxy_rev(&(coord->east_west), &(coord->north_south), x, y, vvp->center.east_west, vvp->center.north_south, vvp->xmpp * ALTI_TO_MPP, vvp->ympp * ALTI_TO_MPP, vvp->width/2, vvp->height/2);
    else if ( vvp->drawmode == VIK_VIEWPORT_DRAWMODE_MERCATOR ) {
      /* FIXMERCATOR */
      coord->east_west = vvp->center.east_west + (180.0 * vvp->xmpp / 65536 / 256 * (x - vvp->width/2));
      coord->north_south = DEMERCLAT ( MERCLAT(vvp->center.north_south) + (180.0 * vvp->ympp / 65536 / 256 * (vvp->height/2 - y)) );

#if 0
-->	THIS IS JUNK HERE.
      *y = vvp->height/2 + (65536.0 / 180 / vvp->ympp * (MERCLAT(center->lat) - MERCLAT(ll->lat)))*256.0;

      (*y - vvp->height/2) / 256 / 65536 * 180 * vvp->ympp = (MERCLAT(center->lat) - MERCLAT(ll->lat);
      DML((180.0 * vvp->ympp / 65536 / 256 * (vvp->height/2 - y)) + ML(cl)) = ll
#endif
    }
  }
}

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

    *x = ( (utm->easting - center->easting) / vvp->xmpp ) + (vvp->width / 2) -
  	  (center->zone - utm->zone ) * vvp->utm_zone_width / vvp->xmpp;
    *y = (vvp->height / 2) - ( (utm->northing - center->northing) / vvp->ympp );
  } else if ( vvp->coord_mode == VIK_COORD_LATLON ) {
    struct LatLon *center = (struct LatLon *) &(vvp->center);
    struct LatLon *ll = (struct LatLon *) coord;
    double xx,yy;
    if ( vvp->drawmode == VIK_VIEWPORT_DRAWMODE_LATLON ) {
      /* FIXMERCATOR: Optimize */
      *x = vvp->width/2 + (65536.0 / 180 / vvp->xmpp * (ll->lon - center->lon))*256.0;
      *y = vvp->height/2 + (65536.0 / 180 / vvp->ympp * (center->lat - ll->lat))*256.0;
    } else if ( vvp->drawmode == VIK_VIEWPORT_DRAWMODE_EXPEDIA ) {
      calcxy ( &xx, &yy, center->lon, center->lat, ll->lon, ll->lat, vvp->xmpp * ALTI_TO_MPP, vvp->ympp * ALTI_TO_MPP, vvp->width / 2, vvp->height / 2 );
      *x = xx; *y = yy;
    } else if ( vvp->drawmode == VIK_VIEWPORT_DRAWMODE_MERCATOR ) {
      /* FIXMERCATOR: Optimize */
      *x = vvp->width/2 + (65536.0 / 180 / vvp->xmpp * (ll->lon - center->lon))*256.0;
      *y = vvp->height/2 + (65536.0 / 180 / vvp->ympp * (MERCLAT(center->lat) - MERCLAT(ll->lat)))*256.0;
    }
  }
}

void a_viewport_clip_line ( gint *x1, gint *y1, gint *x2, gint *y2 )
{
  if ( *x1 > 20000 || *x1 < -20000 ) {
    gdouble shrinkfactor = ABS(20000.0 / *x1);
    *x1 = *x2 + (shrinkfactor * (*x1-*x2));
    *y1 = *y2 + (shrinkfactor * (*y1-*y2));
  } else if ( *y1 > 20000 || *y1 < -20000 ) {
    gdouble shrinkfactor = ABS(20000.0 / *x1);
    *x1 = *x2 + (shrinkfactor * (*x1-*x2));
    *y1 = *y2 + (shrinkfactor * (*y1-*y2));
  } else if ( *x2 > 20000 || *x2 < -20000 ) {
    gdouble shrinkfactor = ABS(20000.0 / (gdouble)*x2);
    *x2 = *x1 + (shrinkfactor * (*x2-*x1));
    *y2 = *y1 + (shrinkfactor * (*y2-*y1));
  } else if ( *y2 > 20000 || *y2 < -20000 ) {
    gdouble shrinkfactor = ABS(20000.0 / (gdouble)*x2);
    *x2 = *x1 + (shrinkfactor * (*x2-*x1));
    *y2 = *y1 + (shrinkfactor * (*y2-*y1));
  }
}

void vik_viewport_draw_line ( VikViewport *vvp, GdkGC *gc, gint x1, gint y1, gint x2, gint y2 )
{
  if ( ! ( ( x1 < 0 && x2 < 0 ) || ( y1 < 0 && y2 < 0 ) ||
       ( x1 > vvp->width && x2 > vvp->width ) || ( y1 > vvp->height && y2 > vvp->height ) ) ) {
    /*** clipping, yeah! ***/
    a_viewport_clip_line ( &x1, &y1, &x2, &y2 );
    gdk_draw_line ( vvp->scr_buffer, gc, x1, y1, x2, y2);
  }
}

void vik_viewport_draw_rectangle ( VikViewport *vvp, GdkGC *gc, gboolean filled, gint x1, gint y1, gint x2, gint y2 )
{
  if ( x1 > -10 && x1 < vvp->width + 10 && y1 > -10 && y1 < vvp->height + 10 )
    gdk_draw_rectangle ( vvp->scr_buffer, gc, filled, x1, y1, x2, y2);
}

void vik_viewport_draw_string ( VikViewport *vvp, GdkFont *font, GdkGC *gc, gint x1, gint y1, const gchar *string )
{
  if ( x1 > -100 && x1 < vvp->width + 100 && y1 > -100 && y1 < vvp->height + 100 )
    gdk_draw_string ( vvp->scr_buffer, font, gc, x1, y1, string );
}

/* shouldn't use this -- slow -- change the alpha channel instead. */
void vik_viewport_draw_pixbuf_with_alpha ( VikViewport *vvp, GdkPixbuf *pixbuf, gint alpha,
                                           gint src_x, gint src_y, gint dest_x, gint dest_y, gint w, gint h )
{
  gint real_dest_x = MAX(dest_x,0);
  gint real_dest_y = MAX(dest_y,0);

  if ( alpha == 0 )
    return; /* don't waste your time */

  if ( w > vvp->alpha_pixbuf_width || h > vvp->alpha_pixbuf_height )
  {
    if ( vvp->alpha_pixbuf )
      g_object_unref ( G_OBJECT ( vvp->alpha_pixbuf ) );
    vvp->alpha_pixbuf_width = MAX(w,vvp->alpha_pixbuf_width);
    vvp->alpha_pixbuf_height = MAX(h,vvp->alpha_pixbuf_height);
    vvp->alpha_pixbuf = gdk_pixbuf_new ( GDK_COLORSPACE_RGB, FALSE, 8, vvp->alpha_pixbuf_width, vvp->alpha_pixbuf_height );
  }

  w = MIN(w,vvp->width - dest_x);
  h = MIN(h,vvp->height - dest_y);

  /* check that we are drawing within boundaries. */
  src_x += (real_dest_x - dest_x);
  src_y += (real_dest_y - dest_y);
  w -= (real_dest_x - dest_x);
  h -= (real_dest_y - dest_y);

  gdk_pixbuf_get_from_drawable ( vvp->alpha_pixbuf, vvp->scr_buffer, NULL,
                                 real_dest_x, real_dest_y, 0, 0, w, h );

  /* do a composite */
  gdk_pixbuf_composite ( pixbuf, vvp->alpha_pixbuf, 0, 0, w, h, -src_x, -src_y, 1, 1, 0, alpha );

  /* draw pixbuf_tmp */
  vik_viewport_draw_pixbuf ( vvp, vvp->alpha_pixbuf, 0, 0, real_dest_x, real_dest_y, w, h );
}

void vik_viewport_draw_pixbuf ( VikViewport *vvp, GdkPixbuf *pixbuf, gint src_x, gint src_y,
                              gint dest_x, gint dest_y, gint w, gint h )
{
  gdk_draw_pixbuf ( vvp->scr_buffer,
// GTK_WIDGET(vvp)->style->black_gc,
NULL,
 pixbuf,
                    src_x, src_y, dest_x, dest_y, w, h,
                    GDK_RGB_DITHER_NONE, 0, 0 );
}

void vik_viewport_draw_arc ( VikViewport *vvp, GdkGC *gc, gboolean filled, gint x, gint y, gint width, gint height, gint angle1, gint angle2 )
{
  gdk_draw_arc ( vvp->scr_buffer, gc, filled, x, y, width, height, angle1, angle2 );
}


void vik_viewport_draw_polygon ( VikViewport *vvp, GdkGC *gc, gboolean filled, GdkPoint *points, gint npoints )
{
  gdk_draw_polygon ( vvp->scr_buffer, gc, filled, points, npoints );
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
  lat = zero_lat - py / Ra;
  lon =
    zero_long -
    px / (Ra *
         cos (lat * DEG2RAD));

  dif = lat * (1 - (cos ((fabs (lon - zero_long)) * DEG2RAD)));
  lat = lat - dif / 1.5;
  lon =
    zero_long -
    px / (Ra *
              cos (lat * DEG2RAD));

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
         cos (lt*DEG2RAD) * (lg - zero_long);
    *y = Ra * (lt - zero_lat);
    dif = Ra * RAD2DEG * (1 - (cos ((DEG2RAD * (lg - zero_long)))));
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
      Radius[i+90] = calcR ( (double)i ) * DEG2RAD;
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

    lat = lat * DEG2RAD;
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

void vik_viewport_draw_layout ( VikViewport *vvp, GdkGC *gc, gint x, gint y, PangoLayout *layout )
{
  if ( x > -100 && x < vvp->width + 100 && y > -100 && y < vvp->height + 100 )
    gdk_draw_layout ( vvp->scr_buffer, gc, x, y, layout );
}

void vik_gc_get_fg_color ( GdkGC *gc, GdkColor *dest )
{
  static GdkGCValues values;
  gdk_gc_get_values ( gc, &values );
  gdk_colormap_query_color ( gdk_colormap_get_system(), values.foreground.pixel, dest );
}

GdkFunction vik_gc_get_function ( GdkGC *gc )
{
  static GdkGCValues values;
  gdk_gc_get_values ( gc, &values );
  return values.function;
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
  gdk_draw_drawable ( vp->snapshot_buffer, vp->background_gc, vp->scr_buffer, 0, 0, 0, 0, -1, -1 );
}

void vik_viewport_snapshot_load ( VikViewport *vp )
{
  gdk_draw_drawable ( vp->scr_buffer, vp->background_gc, vp->snapshot_buffer, 0, 0, 0, 0, -1, -1 );
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
    gchar *found = (gchar*)g_slist_find_custom ( vp->copyrights, copyright, (GCompareFunc)strcmp );
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
