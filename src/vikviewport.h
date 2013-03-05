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

#ifndef _VIKING_VIEWPORT_H
#define _VIKING_VIEWPORT_H
/* Requires <gtk/gtk.h> or glib, and coords.h*/

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "vikcoord.h"


G_BEGIN_DECLS


#define VIK_VIEWPORT_TYPE            (vik_viewport_get_type ())
#define VIK_VIEWPORT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_VIEWPORT_TYPE, VikViewport))
#define VIK_VIEWPORT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_VIEWPORT_TYPE, VikViewportClass))
#define VIK_IS_VIEWPORT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_VIEWPORT_TYPE))
#define VIK_IS_VIEWPORT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_VIEWPORT_TYPE))

#define VIK_VIEWPORT_MAX_ZOOM 32768.0
#define VIK_VIEWPORT_MIN_ZOOM (1 / 32.0)

/* used for coord to screen etc, screen to coord */
#define VIK_VIEWPORT_UTM_WRONG_ZONE -9999999
#define VIK_VIEWPORT_OFF_SCREEN_DOUBLE -9999999.9


/* Glib type inheritance and initialization */
typedef struct _VikViewport VikViewport;
typedef struct _VikViewportClass VikViewportClass;

struct _VikViewportClass
{
  GtkDrawingAreaClass drawing_area_class;
};
GType vik_viewport_get_type ();


/* Viking initialization */
VikViewport *vik_viewport_new ();
void vik_viewport_configure_manually ( VikViewport *vvp, gint width, guint height ); /* for off-screen viewports */
gboolean vik_viewport_configure ( VikViewport *vp ); 


/* coordinate transformations */
void vik_viewport_screen_to_coord ( VikViewport *vvp, int x, int y, VikCoord *coord );
void vik_viewport_coord_to_screen ( VikViewport *vvp, const VikCoord *coord, int *x, int *y );


/* viewport scale */
void vik_viewport_set_ympp ( VikViewport *vvp, gdouble ympp );
void vik_viewport_set_xmpp ( VikViewport *vvp, gdouble xmpp );
gdouble vik_viewport_get_ympp ( VikViewport *vvp );
gdouble vik_viewport_get_xmpp ( VikViewport *vvp );
void vik_viewport_set_zoom ( VikViewport *vvp, gdouble mpp );
gdouble vik_viewport_get_zoom ( VikViewport *vvp );
void vik_viewport_zoom_in ( VikViewport *vvp );
void vik_viewport_zoom_out ( VikViewport *vvp );


/* viewport position */
const VikCoord *vik_viewport_get_center ( VikViewport *vvp );
void vik_viewport_set_center_coord ( VikViewport *vvp, const VikCoord *coord );
void vik_viewport_set_center_screen ( VikViewport *vvp, int x, int y );
void vik_viewport_center_for_zonen ( VikViewport *vvp, struct UTM *center, int zone);
gchar vik_viewport_leftmost_zone ( VikViewport *vvp );
gchar vik_viewport_rightmost_zone ( VikViewport *vvp );
void vik_viewport_set_center_utm ( VikViewport *vvp, const struct UTM *utm );
void vik_viewport_set_center_latlon ( VikViewport *vvp, const struct LatLon *ll );
void vik_viewport_corners_for_zonen ( VikViewport *vvp, int zone, VikCoord *ul, VikCoord *br );
void vik_viewport_get_min_max_lat_lon ( VikViewport *vp, gdouble *min_lat, gdouble *max_lat, gdouble *min_lon, gdouble *max_lon );


/* drawmode management */
typedef enum {
  VIK_VIEWPORT_DRAWMODE_UTM=0,
  VIK_VIEWPORT_DRAWMODE_EXPEDIA,
  VIK_VIEWPORT_DRAWMODE_MERCATOR,
  VIK_VIEWPORT_DRAWMODE_LATLON,
  VIK_VIEWPORT_NUM_DRAWMODES      /*< skip >*/
} VikViewportDrawMode;

VikCoordMode vik_viewport_get_coord_mode ( const VikViewport *vvp );
gboolean vik_viewport_is_one_zone ( VikViewport *vvp );
const gchar *vik_viewport_get_drawmode_name(VikViewport *vv, VikViewportDrawMode mode);
void vik_viewport_set_drawmode ( VikViewport *vvp, VikViewportDrawMode drawmode );
VikViewportDrawMode vik_viewport_get_drawmode ( VikViewport *vvp );
   /* Do not forget to update vik_viewport_get_drawmode_name() if you modify VikViewportDrawMode */


/* Triggers */
void vik_viewport_set_trigger ( VikViewport *vp, gpointer trigger );
gpointer vik_viewport_get_trigger ( VikViewport *vp );
void vik_viewport_snapshot_save ( VikViewport *vp );
void vik_viewport_snapshot_load ( VikViewport *vp );
void vik_viewport_set_half_drawn(VikViewport *vp, gboolean half_drawn);
gboolean vik_viewport_get_half_drawn( VikViewport *vp );


/***************************************************************************************************
 *  Drawing-related operations 
 ***************************************************************************************************/

/* Viewport buffer management/drawing to screen */
GdkPixmap *vik_viewport_get_pixmap ( VikViewport *vvp ); /* get pointer to drawing buffer */
void vik_viewport_sync ( VikViewport *vvp );             /* draw buffer to window */
void vik_viewport_pan_sync ( VikViewport *vvp, gint x_off, gint y_off );
void vik_viewport_clear ( VikViewport *vvp );
void vik_viewport_draw_pixbuf_with_alpha ( VikViewport *vvp, GdkPixbuf *pixbuf, gint alpha,
                                           gint src_x, gint src_y, gint dest_x, gint dest_y, gint w, gint h );
void vik_viewport_draw_pixbuf ( VikViewport *vvp, GdkPixbuf *pixbuf, gint src_x, gint src_y,
                              gint dest_x, gint dest_y, gint w, gint h );
gint vik_viewport_get_width ( VikViewport *vvp );
gint vik_viewport_get_height ( VikViewport *vvp );

void vik_viewport_reset_copyrights ( VikViewport *vp );
void vik_viewport_add_copyright ( VikViewport *vp, const gchar *copyright );

void vik_viewport_reset_logos ( VikViewport *vp );
void vik_viewport_add_logo ( VikViewport *vp, const GdkPixbuf *logo );

/* Viewport features */
void vik_viewport_draw_scale ( VikViewport *vvp );
void vik_viewport_set_draw_scale ( VikViewport *vvp, gboolean draw_scale );
gboolean vik_viewport_get_draw_scale ( VikViewport *vvp );
void vik_viewport_draw_copyright ( VikViewport *vvp );
void vik_viewport_draw_centermark ( VikViewport *vvp );
void vik_viewport_set_draw_centermark ( VikViewport *vvp, gboolean draw_centermark );
gboolean vik_viewport_get_draw_centermark ( VikViewport *vvp );
void vik_viewport_draw_logo ( VikViewport *vvp );
void vik_viewport_set_draw_highlight ( VikViewport *vvp, gboolean draw_highlight );
gboolean vik_viewport_get_draw_highlight ( VikViewport *vvp );

/* Color/graphics context management */
void vik_viewport_set_background_color ( VikViewport *vvp, const gchar *color );
const gchar *vik_viewport_get_background_color ( VikViewport *vvp );
GdkColor *vik_viewport_get_background_gdkcolor ( VikViewport *vvp );
void vik_viewport_set_background_gdkcolor ( VikViewport *vvp, GdkColor * );
void vik_gc_get_fg_color ( GdkGC *gc, GdkColor *dest ); /* warning: could be slow, don't use obsessively */
GdkGC *vik_viewport_new_gc ( VikViewport *vvp, const gchar *colorname, gint thickness );
GdkGC *vik_viewport_new_gc_from_color ( VikViewport *vvp, GdkColor *color, gint thickness );
GdkFunction vik_gc_get_function ( GdkGC *gc );

void vik_viewport_set_highlight_color ( VikViewport *vvp, const gchar *color );
const gchar *vik_viewport_get_highlight_color ( VikViewport *vvp );
GdkColor *vik_viewport_get_highlight_gdkcolor ( VikViewport *vvp );
void vik_viewport_set_highlight_gdkcolor ( VikViewport *vvp, GdkColor * );
GdkGC* vik_viewport_get_gc_highlight ( VikViewport *vvp );
void vik_viewport_set_highlight_thickness ( VikViewport *vvp, gint thickness );

/* Drawing primitives */
void a_viewport_clip_line ( gint *x1, gint *y1, gint *x2, gint *y2 ); /* run this before drawing a line. vik_viewport_draw_line runs it for you */
void vik_viewport_draw_line ( VikViewport *vvp, GdkGC *gc, gint x1, gint y1, gint x2, gint y2 );
void vik_viewport_draw_rectangle ( VikViewport *vvp, GdkGC *gc, gboolean filled, gint x1, gint y1, gint x2, gint y2 );
void vik_viewport_draw_string ( VikViewport *vvp, GdkFont *font, GdkGC *gc, gint x1, gint y1, const gchar *string );
void vik_viewport_draw_arc ( VikViewport *vvp, GdkGC *gc, gboolean filled, gint x, gint y, gint width, gint height, gint angle1, gint angle2 );
void vik_viewport_draw_polygon ( VikViewport *vvp, GdkGC *gc, gboolean filled, GdkPoint *points, gint npoints );
void vik_viewport_draw_layout ( VikViewport *vvp, GdkGC *gc, gint x, gint y, PangoLayout *layout );

/* Utilities */
void vik_viewport_compute_bearing ( VikViewport *vp, gint x1, gint y1, gint x2, gint y2, gdouble *angle, gdouble *baseangle );

G_END_DECLS

#endif
