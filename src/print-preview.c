/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/*
 * Modified for viking by Quy Tonthat <qtonthat@gmail.com>
 */

#include <gtk/gtk.h>

#if GTK_CHECK_VERSION(2,10,0)

#include "print-preview.h"


#define DRAWING_AREA_SIZE 200


enum
{
  OFFSETS_CHANGED,
  LAST_SIGNAL
};


static void      vik_print_preview_finalize         (GObject          *object);

static void      vik_print_preview_size_allocate    (GtkWidget        *widget,
                                                      GtkAllocation    *allocation,
                                                      VikPrintPreview *preview);
static void      vik_print_preview_realize          (GtkWidget        *widget);
static gboolean  vik_print_preview_event            (GtkWidget        *widget,
                                                      GdkEvent         *event,
                                                      VikPrintPreview *preview);

static gboolean  vik_print_preview_expose_event     (GtkWidget        *widget,
                                                      GdkEventExpose   *eevent,
                                                      VikPrintPreview *preview);

static gdouble   vik_print_preview_get_scale        (VikPrintPreview *preview);

static void      vik_print_preview_get_page_margins (VikPrintPreview *preview,
                                                      gdouble          *left_margin,
                                                      gdouble          *right_margin,
                                                      gdouble          *top_margin,
                                                      gdouble          *bottom_margin);

static void      print_preview_queue_draw            (VikPrintPreview *preview);


G_DEFINE_TYPE (VikPrintPreview, vik_print_preview, GTK_TYPE_ASPECT_FRAME)

#define parent_class vik_print_preview_parent_class

static guint vik_print_preview_signals[LAST_SIGNAL] = { 0 };


#define g_marshal_value_peek_double(v)   (v)->data[0].v_double

static void
marshal_VOID__DOUBLE_DOUBLE (GClosure     *closure,
                             GValue       *return_value,
                             guint         n_param_values,
                             const GValue *param_values,
                             gpointer      invocation_hint,
                             gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__DOUBLE_DOUBLE) (gpointer     data1,
                                                    gdouble      arg_1,
                                                    gdouble      arg_2,
                                                    gpointer     data2);
  register GMarshalFunc_VOID__DOUBLE_DOUBLE callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;

  g_return_if_fail (n_param_values == 3);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }

  callback = (GMarshalFunc_VOID__DOUBLE_DOUBLE) (marshal_data ?
                                                 marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_double (param_values + 1),
            g_marshal_value_peek_double (param_values + 2),
            data2);
}

static void
vik_print_preview_class_init (VikPrintPreviewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  vik_print_preview_signals[OFFSETS_CHANGED] =
    g_signal_new ("offsets-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (VikPrintPreviewClass, offsets_changed),
                  NULL, NULL,
                  marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2,
                  G_TYPE_DOUBLE,
                  G_TYPE_DOUBLE);

  object_class->finalize = vik_print_preview_finalize;

  klass->offsets_changed = NULL;
}

static void
vik_print_preview_init (VikPrintPreview *preview)
{
  preview->page               = NULL;
  preview->pixbuf             = NULL;
  preview->dragging           = FALSE;
  preview->image_offset_x     = 0.0;
  preview->image_offset_y     = 0.0;
  preview->image_offset_x_max = 0.0;
  preview->image_offset_y_max = 0.0;
  preview->image_xres         = 230.0; // 1.0
  preview->image_yres         = 230.0;
  preview->use_full_page      = FALSE;

  preview->area = gtk_drawing_area_new();
  gtk_container_add (GTK_CONTAINER (preview), preview->area);
  gtk_widget_show (preview->area);

  gtk_widget_add_events (GTK_WIDGET (preview->area), GDK_BUTTON_PRESS_MASK);

  g_signal_connect (preview->area, "size-allocate",
                    G_CALLBACK (vik_print_preview_size_allocate),
                    preview);
  g_signal_connect (preview->area, "realize",
                    G_CALLBACK (vik_print_preview_realize),
                    NULL);
  g_signal_connect (preview->area, "event",
                    G_CALLBACK (vik_print_preview_event),
                    preview);
  g_signal_connect (preview->area, "expose-event",
                    G_CALLBACK (vik_print_preview_expose_event),
                    preview);
}


static void
vik_print_preview_finalize (GObject *object)
{
  VikPrintPreview *preview = VIK_PRINT_PREVIEW (object);

  if (preview->drawable)
    {
      preview->drawable = NULL;
    }

  if (preview->pixbuf)
    {
      g_object_unref (preview->pixbuf);
      preview->pixbuf = NULL;
    }

  if (preview->page)
    {
      g_object_unref (preview->page);
      preview->page = NULL;
    }

  G_OBJECT_CLASS (vik_print_preview_parent_class)->finalize (object);
}

/**
 * vik_print_preview_new:
 * @page: page setup
 * @drawable_id: the drawable to print
 *
 * Creates a new #VikPrintPreview widget.
 *
 * Return value: the new #VikPrintPreview widget.
 **/
GtkWidget *
vik_print_preview_new (GtkPageSetup *page,
                        GdkDrawable        *drawable)
{
  VikPrintPreview *preview;
  gfloat            ratio;

  preview = g_object_new (VIK_TYPE_PRINT_PREVIEW, NULL);

  preview->drawable = drawable;

  if (page != NULL)
    preview->page = gtk_page_setup_copy (page);
  else
    preview->page = gtk_page_setup_new ();

  ratio = (gtk_page_setup_get_paper_width (preview->page, GTK_UNIT_POINTS) /
           gtk_page_setup_get_paper_height (preview->page, GTK_UNIT_POINTS));

  gtk_aspect_frame_set (GTK_ASPECT_FRAME (preview), 0.5, 0.5, ratio, FALSE);

  gtk_widget_set_size_request (preview->area,
                               DRAWING_AREA_SIZE, DRAWING_AREA_SIZE);

  return GTK_WIDGET (preview);
}

/**
 * vik_print_preview_set_image_dpi:
 * @preview: a #VikPrintPreview.
 * @xres: the X resolution
 * @yres: the Y resolution
 *
 * Sets the resolution of the image/drawable displayed by the
 * #VikPrintPreview.
 **/
void
vik_print_preview_set_image_dpi (VikPrintPreview *preview,
                                  gdouble           xres,
                                  gdouble           yres)
{
  g_return_if_fail (VIK_IS_PRINT_PREVIEW (preview));

  if (preview->image_xres != xres || preview->image_yres != yres)
    {
      preview->image_xres = xres;
      preview->image_yres = yres;

      print_preview_queue_draw (preview);
    }
}

/**
 * vik_print_preview_set_page_setup:
 * @preview: a #VikPrintPreview.
 * @page: the page setup to use
 *
 * Sets the page setup to use by the #VikPrintPreview.
 **/
void
vik_print_preview_set_page_setup (VikPrintPreview *preview,
                                   GtkPageSetup     *page)
{
  gfloat ratio;

  if (preview->page)
    g_object_unref (preview->page);

  preview->page = gtk_page_setup_copy (page);

  ratio = (gtk_page_setup_get_paper_width (page, GTK_UNIT_POINTS) /
           gtk_page_setup_get_paper_height (page, GTK_UNIT_POINTS));

  gtk_aspect_frame_set (GTK_ASPECT_FRAME (preview), 0.5, 0.5, ratio, FALSE);

  print_preview_queue_draw (preview);
}

/**
 * vik_print_preview_set_image_offsets:
 * @preview: a #VikPrintPreview.
 * @offset_x: the X offset
 * @offset_y: the Y offset
 *
 * Sets the offsets of the image/drawable displayed by the #VikPrintPreview.
 * It does not emit the "offsets-changed" signal.
 **/
void
vik_print_preview_set_image_offsets (VikPrintPreview *preview,
                                      gdouble           offset_x,
                                      gdouble           offset_y)
{
  g_return_if_fail (VIK_IS_PRINT_PREVIEW (preview));

  preview->image_offset_x = offset_x;
  preview->image_offset_y = offset_y;

  print_preview_queue_draw (preview);
}

/**
 * vik_print_preview_set_image_offsets_max:
 * @preview: a #VikPrintPreview.
 * @offset_x_max: the maximum X offset allowed
 * @offset_y_max: the maximum Y offset allowed
 *
 * Sets the maximum offsets of the image/drawable displayed by the
 * #VikPrintPreview.  It does not emit the "offsets-changed" signal.
 **/
void
vik_print_preview_set_image_offsets_max (VikPrintPreview *preview,
                                          gdouble           offset_x_max,
                                          gdouble           offset_y_max)
{
  g_return_if_fail (VIK_IS_PRINT_PREVIEW (preview));

  preview->image_offset_x_max = offset_x_max;
  preview->image_offset_y_max = offset_y_max;

  print_preview_queue_draw (preview);
}

/**
 * vik_print_preview_set_use_full_page:
 * @preview: a #VikPrintPreview.
 * @full_page: TRUE to ignore the page margins
 *
 * If @full_page is TRUE, the page margins are ignored and the full page
 * can be used to setup printing.
 **/
void
vik_print_preview_set_use_full_page (VikPrintPreview *preview,
                                      gboolean          full_page)
{
  g_return_if_fail (VIK_IS_PRINT_PREVIEW (preview));

  preview->use_full_page = full_page;

  print_preview_queue_draw (preview);
}

static void
vik_print_preview_realize (GtkWidget *widget)
{
  GdkCursor *cursor;

  cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget),
                                       GDK_FLEUR);
  gdk_window_set_cursor (widget->window, cursor);
  gdk_cursor_unref (cursor);
}

static gboolean
vik_print_preview_event (GtkWidget        *widget,
                          GdkEvent         *event,
                          VikPrintPreview *preview)
{
  static gdouble orig_offset_x = 0.0;
  static gdouble orig_offset_y = 0.0;
  static gint    start_x       = 0;
  static gint    start_y       = 0;

  gdouble        offset_x;
  gdouble        offset_y;
  gdouble        scale;

  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
      gdk_pointer_grab (widget->window, FALSE,
                        (GDK_BUTTON1_MOTION_MASK |
                         GDK_BUTTON_RELEASE_MASK),
                        NULL, NULL, event->button.time);

      orig_offset_x = preview->image_offset_x;
      orig_offset_y = preview->image_offset_y;

      start_x = event->button.x;
      start_y = event->button.y;

      preview->dragging = TRUE;
      break;

    case GDK_MOTION_NOTIFY:
      scale = vik_print_preview_get_scale (preview);

      offset_x = (orig_offset_x + (event->motion.x - start_x) / scale);
      offset_y = (orig_offset_y + (event->motion.y - start_y) / scale);

      offset_x = CLAMP (offset_x, 0, preview->image_offset_x_max);
      offset_y = CLAMP (offset_y, 0, preview->image_offset_y_max);

      if (preview->image_offset_x != offset_x ||
          preview->image_offset_y != offset_y)
        {
          vik_print_preview_set_image_offsets (preview, offset_x, offset_y);

          g_signal_emit (preview,
                         vik_print_preview_signals[OFFSETS_CHANGED], 0,
                         preview->image_offset_x, preview->image_offset_y);
        }
      break;

    case GDK_BUTTON_RELEASE:
      gdk_display_pointer_ungrab (gtk_widget_get_display (widget),
                                  event->button.time);
      start_x = start_y = 0;
      preview->dragging = FALSE;

      print_preview_queue_draw (preview);
      break;

    default:
      break;
    }

  return FALSE;
}

static GdkPixbuf *get_thumbnail(GdkDrawable *drawable, gint thumb_width, gint thumb_height)
{
  gint width, height;
  GdkPixbuf *pixbuf;
  GdkPixbuf *thumbnail;

  gdk_drawable_get_size(drawable, &width, &height);
  pixbuf = gdk_pixbuf_get_from_drawable(NULL, drawable,
                                           NULL, 0, 0, 0, 0, width, height);
  thumbnail = gdk_pixbuf_scale_simple(pixbuf, thumb_width, thumb_height,
                                      GDK_INTERP_BILINEAR);
  g_object_unref(pixbuf);
  return thumbnail;
}

static gboolean
vik_print_preview_expose_event (GtkWidget        *widget,
                                 GdkEventExpose   *eevent,
                                 VikPrintPreview *preview)
{
  gdouble  paper_width;
  gdouble  paper_height;
  gdouble  left_margin;
  gdouble  right_margin;
  gdouble  top_margin;
  gdouble  bottom_margin;
  gdouble  scale;
  cairo_t *cr;

  paper_width = gtk_page_setup_get_paper_width (preview->page,
                                                GTK_UNIT_POINTS);
  paper_height = gtk_page_setup_get_paper_height (preview->page,
                                                  GTK_UNIT_POINTS);
  vik_print_preview_get_page_margins (preview,
                                       &left_margin,
                                       &right_margin,
                                       &top_margin,
                                       &bottom_margin);

  cr = gdk_cairo_create (widget->window);

  scale = vik_print_preview_get_scale (preview);

  /* draw background */
  cairo_scale (cr, scale, scale);
  gdk_cairo_set_source_color (cr, &widget->style->white);
  cairo_rectangle (cr, 0, 0, paper_width, paper_height);
  cairo_fill (cr);

  /* draw page_margins */
  gdk_cairo_set_source_color (cr, &widget->style->black);
  cairo_rectangle (cr,
                   left_margin,
                   top_margin,
                   paper_width - left_margin - right_margin,
                   paper_height - top_margin - bottom_margin);
  cairo_stroke (cr);

  if (preview->dragging)
    {
      gint width, height;
      gdk_drawable_get_size(preview->drawable, &width, &height);
      cairo_rectangle (cr,
                       left_margin + preview->image_offset_x,
                       top_margin  + preview->image_offset_y,
                       (gdouble) width  * 72.0 / preview->image_xres,
                       (gdouble) height * 72.0 / preview->image_yres);
      cairo_stroke (cr);
    }
  else
    {
      GdkDrawable *drawable = preview->drawable;

      /* draw image */
      cairo_translate (cr,
                       left_margin + preview->image_offset_x,
                       top_margin  + preview->image_offset_y);

      if (preview->pixbuf == NULL)
        {
          gint width  = MIN (widget->allocation.width, 1024);
          gint height = MIN (widget->allocation.height, 1024);

          preview->pixbuf = get_thumbnail(drawable, width, height);
        }

      if (preview->pixbuf != NULL)
        {
          gint width, height;
          gdk_drawable_get_size(drawable, &width, &height);

          gdouble scale_x = ((gdouble) width /
                             gdk_pixbuf_get_width (preview->pixbuf));
          gdouble scale_y = ((gdouble) height /
                             gdk_pixbuf_get_height (preview->pixbuf));

          scale_x = scale_x * 72.0 / preview->image_xres;
          scale_y = scale_y * 72.0 / preview->image_yres;

          cairo_scale (cr, scale_x, scale_y);

          gdk_cairo_set_source_pixbuf (cr, preview->pixbuf, 0, 0);

          cairo_paint (cr);
        }
    }

  cairo_destroy (cr);

  return FALSE;
}

static gdouble
vik_print_preview_get_scale (VikPrintPreview* preview)
{
  gdouble scale_x;
  gdouble scale_y;

  scale_x = ((gdouble) preview->area->allocation.width /
             gtk_page_setup_get_paper_width (preview->page, GTK_UNIT_POINTS));

  scale_y = ((gdouble) preview->area->allocation.height /
             gtk_page_setup_get_paper_height (preview->page, GTK_UNIT_POINTS));

  return MIN (scale_x, scale_y);
}

static void
vik_print_preview_size_allocate (GtkWidget        *widget,
                                  GtkAllocation    *allocation,
                                  VikPrintPreview *preview)
{
  if (preview->pixbuf != NULL)
    {
      g_object_unref (preview->pixbuf);
      preview->pixbuf = NULL;
    }
}

static void
vik_print_preview_get_page_margins (VikPrintPreview *preview,
                                     gdouble          *left_margin,
                                     gdouble          *right_margin,
                                     gdouble          *top_margin,
                                     gdouble          *bottom_margin)
{
  if (preview->use_full_page)
    {
      *left_margin   = 0.0;
      *right_margin  = 0.0;
      *top_margin    = 0.0;
      *bottom_margin = 0.0;
    }
  else
    {
      *left_margin   = gtk_page_setup_get_left_margin (preview->page,
                                                       GTK_UNIT_POINTS);
      *right_margin  = gtk_page_setup_get_right_margin (preview->page,
                                                        GTK_UNIT_POINTS);
      *top_margin    = gtk_page_setup_get_top_margin (preview->page,
                                                      GTK_UNIT_POINTS);
      *bottom_margin = gtk_page_setup_get_bottom_margin (preview->page,
                                                         GTK_UNIT_POINTS);
    }
}

static void
print_preview_queue_draw (VikPrintPreview *preview)
{
  gtk_widget_queue_draw (GTK_WIDGET (preview->area));
}

#endif
