/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 *
 * print.c
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
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

#include <string.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#if GTK_CHECK_VERSION(2,10,0)

#include "viking.h"
#include "print.h"
#include "print-preview.h"

typedef enum
{
  VIK_PRINT_CENTER_NONE         = 0,
  VIK_PRINT_CENTER_HORIZONTALLY,
  VIK_PRINT_CENTER_VERTICALLY,
  VIK_PRINT_CENTER_BOTH,
} PrintCenterMode;

typedef struct {
  gchar *name;
  PrintCenterMode mode;
} PrintCenterName;

static const PrintCenterName center_modes[] = {
  {N_("None"),          VIK_PRINT_CENTER_NONE},
  {N_("Horizontally"),  VIK_PRINT_CENTER_HORIZONTALLY},
  {N_("Vertically"),    VIK_PRINT_CENTER_VERTICALLY},
  {N_("Both"),          VIK_PRINT_CENTER_BOTH},
  {NULL,            -1}
};

typedef struct {
  gint                num_pages;
  gboolean            show_info_header;
  VikWindow           *vw;
  VikViewport         *vvp;
  gdouble             xmpp, ympp;  /* zoom level (meters/pixel) */
  gdouble             xres;
  gdouble             yres;
  gint                width;
  gint                height;
  gdouble             offset_x;
  gdouble             offset_y;
  PrintCenterMode     center;
  gboolean            use_full_page;
  GtkPrintOperation  *operation;
} PrintData;

static GtkWidget *create_custom_widget_cb(GtkPrintOperation *operation, PrintData *data);
static void begin_print(GtkPrintOperation *operation, GtkPrintContext *context, PrintData *data);
static void draw_page(GtkPrintOperation *print, GtkPrintContext *context, gint page_nr, PrintData *data);
static void end_print(GtkPrintOperation *operation, GtkPrintContext *context,  PrintData *data);

void a_print(VikWindow *vw, VikViewport *vvp)
{
  /* TODO: make print_settings non-static when saving_settings_to_file is
   * implemented. Keep it static for now to retain settings for each
   * viking session
   */
  static GtkPrintSettings *print_settings = NULL;

  GtkPrintOperation *print_oper;
  GtkPrintOperationResult res;
  PrintData data;

  print_oper = gtk_print_operation_new ();

  data.num_pages     = 1;
  data.vw            = vw;
  data.vvp           = vvp;
  data.offset_x      = 0;
  data.offset_y      = 0;
  data.center        = VIK_PRINT_CENTER_BOTH;
  data.use_full_page = FALSE;
  data.operation     = print_oper;

  data.xmpp          = vik_viewport_get_xmpp(vvp);
  data.ympp          = vik_viewport_get_ympp(vvp);
  data.width         = vik_viewport_get_width(vvp);
  data.height        = vik_viewport_get_height(vvp);

  data.xres = data.yres = 230;   /* FIXME */

  if (print_settings != NULL) 
    gtk_print_operation_set_print_settings (print_oper, print_settings);

  g_signal_connect (print_oper, "begin_print", G_CALLBACK (begin_print), &data);
  g_signal_connect (print_oper, "draw_page", G_CALLBACK (draw_page), &data);
  g_signal_connect (print_oper, "end-print", G_CALLBACK (end_print), &data);
  g_signal_connect (print_oper, "create-custom-widget", G_CALLBACK (create_custom_widget_cb), &data);

  gtk_print_operation_set_custom_tab_label (print_oper, _("Image Settings"));

  res = gtk_print_operation_run (print_oper,
                                 GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
                                 GTK_WINDOW (vw), NULL);

  if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
    if (print_settings != NULL)
      g_object_unref (print_settings);
    print_settings = g_object_ref (gtk_print_operation_get_print_settings (print_oper));
  }

  g_object_unref (print_oper);
}

static void begin_print(GtkPrintOperation *operation,
                        GtkPrintContext   *context,
                        PrintData         *data)
{
  // fputs("DEBUG: begin_print() called\n", stderr);
  gtk_print_operation_set_n_pages (operation, data->num_pages);
  gtk_print_operation_set_use_full_page (operation, data->use_full_page);

}

static void end_print(GtkPrintOperation *operation,
                      GtkPrintContext   *context,
                      PrintData *data)
{
  // fputs("DEBUG: end_print() called\n", stderr);

}

static void copy_row_from_rgb(guchar *surface_pixels, guchar *pixbuf_pixels, gint width)
{
  guint32 *cairo_data = (guint32 *) surface_pixels;
  guchar  *p;
  gint     i;

  for (i = 0, p = pixbuf_pixels; i < width; i++) {
    guint32 r = *p++;
    guint32 g = *p++;
    guint32 b = *p++;
    cairo_data[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
  }
}

#define INT_MULT(a,b,t)  ((t) = (a) * (b) + 0x80, ((((t) >> 8) + (t)) >> 8))
#define INT_BLEND(a,b,alpha,tmp)  (INT_MULT((a) - (b), alpha, tmp) + (b))
static void copy_row_from_rgba(guchar *surface_pixels, guchar *pixbuf_pixels, gint width)
{
  guint32 *cairo_data = (guint32 *) surface_pixels;
  guchar  *p;
  gint     i;

  for (i = 0, p = pixbuf_pixels; i < width; i++) {
    guint32 r = *p++;
    guint32 g = *p++;
    guint32 b = *p++;
    guint32 a = *p++;

    if (a != 255) {
      guint32 tmp;
      /* composite on a white background */
      r = INT_BLEND (r, 255, a, tmp);
      g = INT_BLEND (g, 255, a, tmp);
      b = INT_BLEND (b, 255, a, tmp);
    }
    cairo_data[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
  }
}

static void draw_page_cairo(GtkPrintContext *context, PrintData *data)
{
  cairo_t         *cr;
  GdkPixbuf       *pixbuf_to_draw; 
  cairo_surface_t *surface;
  guchar          *surface_pixels;
  guchar          *pixbuf_pixels;
  gint             stride;
  gint             pixbuf_stride;
  gint             pixbuf_n_channels;
  gdouble          cr_width;
  gdouble          cr_height;
  gdouble          cr_dpi_x;
  gdouble          cr_dpi_y;
  gdouble          scale_x;
  gdouble          scale_y;
  gint             y;

  cr = gtk_print_context_get_cairo_context(context);
  pixbuf_to_draw = gdk_pixbuf_get_from_drawable(NULL,
                               GDK_DRAWABLE(vik_viewport_get_pixmap(data->vvp)),
                               NULL, 0, 0, 0, 0, data->width, data->height);
  surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
                                       data->width, data->height);
  
  cr_width  = gtk_print_context_get_width  (context);
  cr_height = gtk_print_context_get_height (context);
  cr_dpi_x  = gtk_print_context_get_dpi_x  (context);
  cr_dpi_y  = gtk_print_context_get_dpi_y  (context);

  scale_x = cr_dpi_x / data->xres;
  scale_y = cr_dpi_y / data->yres;

  cairo_translate (cr,
                   data->offset_x / cr_dpi_x * 72.0,
                   data->offset_y / cr_dpi_y * 72.0);
  cairo_scale (cr, scale_x, scale_y);

  surface_pixels = cairo_image_surface_get_data (surface);
  stride = cairo_image_surface_get_stride (surface);
  pixbuf_pixels = gdk_pixbuf_get_pixels (pixbuf_to_draw);
  pixbuf_stride = gdk_pixbuf_get_rowstride(pixbuf_to_draw);
  pixbuf_n_channels = gdk_pixbuf_get_n_channels(pixbuf_to_draw);

  // fprintf(stderr, "DEBUG: %s() surface_pixels=%p pixbuf_pixels=%p size=%d surface_width=%d surface_height=%d stride=%d data_height=%d pixmap_stride=%d pixmap_nchannels=%d pixmap_bit_per_Sample=%d\n", __PRETTY_FUNCTION__, surface_pixels, pixbuf_pixels, stride * data->height, cairo_image_surface_get_width(surface), cairo_image_surface_get_height(surface), stride, data->height, gdk_pixbuf_get_rowstride(pixbuf_to_draw), gdk_pixbuf_get_n_channels(pixbuf_to_draw), gdk_pixbuf_get_bits_per_sample(pixbuf_to_draw));

  /* Assume the pixbuf has 8 bits per channel */
  for (y = 0; y < data->height; y++, surface_pixels += stride, pixbuf_pixels += pixbuf_stride) {
    switch (pixbuf_n_channels) {
      case 3:
        copy_row_from_rgb (surface_pixels, pixbuf_pixels, data->width);
        break;
      case 4:
        copy_row_from_rgba (surface_pixels, pixbuf_pixels, data->width);
        break;
    }
  }

  g_object_unref(G_OBJECT(pixbuf_to_draw));

  cairo_set_source_surface(cr, surface, 0, 0);
  cairo_rectangle(cr, 0, 0, data->width, data->height);
  cairo_fill(cr);
  cairo_surface_destroy(surface);
}

static void draw_page(GtkPrintOperation *print,
                      GtkPrintContext   *context,
                      gint               page_nr,
                      PrintData         *data)
{
  // fprintf(stderr, "DEBUG: draw_page() page_nr=%d\n", page_nr);
  draw_page_cairo(context, data);

}

/*********************** page layout gui *********************/
typedef struct
{
  PrintData       *data;
  GtkWidget       *center_combo;
  GtkWidget       *scale;
  GtkWidget       *scale_label;
  GtkWidget       *preview;
} CustomWidgetInfo;

enum
{
  BOTTOM,
  TOP,
  RIGHT,
  LEFT,
  WIDTH,
  HEIGHT
};

static gboolean scale_change_value_cb(GtkRange *range, GtkScrollType scroll, gdouble value, CustomWidgetInfo *pinfo);
static void get_page_dimensions (CustomWidgetInfo *info, gdouble *page_width, gdouble *page_height, GtkUnit unit);
static void center_changed_cb (GtkWidget *combo, CustomWidgetInfo *info);
static void get_max_offsets (CustomWidgetInfo *info, gdouble *offset_x_max, gdouble *offset_y_max);
static void update_offsets (CustomWidgetInfo *info);

static void set_scale_label(CustomWidgetInfo *pinfo, gdouble scale_val)
{
  static const gdouble inch_to_mm = 25.4;
  gchar label_text[64];

  g_snprintf(label_text, sizeof(label_text), "<i>%.0fx%0.f mm (%.0f%%)</i>",
      inch_to_mm * pinfo->data->width / pinfo->data->xres,
      inch_to_mm * pinfo->data->height / pinfo->data->yres,
      scale_val);
  gtk_label_set_markup (GTK_LABEL (pinfo->scale_label), label_text);
}

static void set_scale_value(CustomWidgetInfo *pinfo)
{
  gdouble width;
  gdouble height;
  gdouble ratio, ratio_w, ratio_h;


  get_page_dimensions (pinfo, &width, &height, GTK_UNIT_INCH);
  ratio_w = 100 * pinfo->data->width / pinfo->data->xres / width;
  ratio_h = 100 * pinfo->data->height / pinfo->data->yres / height;

  ratio = MAX(ratio_w, ratio_h);
  g_signal_handlers_block_by_func(GTK_RANGE(pinfo->scale), scale_change_value_cb, pinfo);
  gtk_range_set_value(GTK_RANGE(pinfo->scale), ratio);
  g_signal_handlers_unblock_by_func(GTK_RANGE(pinfo->scale), scale_change_value_cb, pinfo);
  set_scale_label(pinfo, ratio);
}

static void update_page_setup (CustomWidgetInfo *pinfo)
{
  gdouble paper_width;
  gdouble paper_height;
  gdouble offset_x_max, offset_y_max;
  PrintData    *data = pinfo->data;

  get_page_dimensions (pinfo, &paper_width, &paper_height, GTK_UNIT_INCH);
  if ((paper_width < (pinfo->data->width / data->xres)) ||
      (paper_height < (pinfo->data->height / data->yres))) {
    gdouble xres, yres;
    xres = (gdouble) pinfo->data->width / paper_width;
    yres = (gdouble) pinfo->data->height / paper_height;
    data->xres = data->yres = MAX(xres, yres);
    vik_print_preview_set_image_dpi (VIK_PRINT_PREVIEW (pinfo->preview),
                                  data->xres, data->yres);
  }
  get_max_offsets (pinfo, &offset_x_max, &offset_y_max);
  vik_print_preview_set_image_offsets_max (VIK_PRINT_PREVIEW (pinfo->preview),
                                            offset_x_max, offset_y_max);
  update_offsets (pinfo);
  set_scale_value(pinfo);
  if (pinfo->preview)
    vik_print_preview_set_image_offsets (VIK_PRINT_PREVIEW (pinfo->preview),
                                     pinfo->data->offset_x, pinfo->data->offset_y);

}

static void page_setup_cb (GtkWidget *widget, CustomWidgetInfo *info)
{
  PrintData *data = info->data;
  GtkPrintOperation *operation = data->operation;
  GtkPrintSettings  *settings;
  GtkPageSetup      *page_setup;
  GtkWidget         *toplevel;

  toplevel = gtk_widget_get_toplevel (widget);
  if (! GTK_WIDGET_TOPLEVEL (toplevel))
    toplevel = NULL;

  settings = gtk_print_operation_get_print_settings (operation);
  if (! settings)
    settings = gtk_print_settings_new ();

  page_setup = gtk_print_operation_get_default_page_setup (operation);

  page_setup = gtk_print_run_page_setup_dialog (GTK_WINDOW (toplevel),
                                                page_setup, settings);

  gtk_print_operation_set_default_page_setup (operation, page_setup);

  vik_print_preview_set_page_setup (VIK_PRINT_PREVIEW (info->preview),
                                     page_setup);

  update_page_setup (info);

}

static void full_page_toggled_cb (GtkWidget *widget, CustomWidgetInfo *pinfo)
{
  gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

  pinfo->data->use_full_page = active;
  update_page_setup (pinfo);
  vik_print_preview_set_use_full_page (VIK_PRINT_PREVIEW(pinfo->preview),
                                        active);
}

static void set_center_none (CustomWidgetInfo *info)
{
  info->data->center = VIK_PRINT_CENTER_NONE;

  if (info->center_combo) {
    g_signal_handlers_block_by_func (info->center_combo,
                                       center_changed_cb, info);

    info->data->center = VIK_PRINT_CENTER_NONE;
    gtk_combo_box_set_active(GTK_COMBO_BOX(info->center_combo), info->data->center);
    g_signal_handlers_unblock_by_func (info->center_combo,
                                         center_changed_cb, info);
  }
}

static void preview_offsets_changed_cb (GtkWidget *widget,
                                        gdouble    offset_x, gdouble    offset_y,
                                        CustomWidgetInfo *info)
{
  set_center_none (info);

  info->data->offset_x = offset_x;
  info->data->offset_y = offset_y;

  update_offsets (info);

}

static void get_page_dimensions (CustomWidgetInfo *info,
                                     gdouble       *page_width,
                                     gdouble       *page_height,
                                     GtkUnit        unit)
{
  GtkPageSetup *setup;

  setup = gtk_print_operation_get_default_page_setup (info->data->operation);

  *page_width = gtk_page_setup_get_paper_width (setup, unit);
  *page_height = gtk_page_setup_get_paper_height (setup, unit);

  if (!info->data->use_full_page) {
    gdouble left_margin = gtk_page_setup_get_left_margin (setup, unit);
    gdouble right_margin = gtk_page_setup_get_right_margin (setup, unit);
    gdouble top_margin = gtk_page_setup_get_top_margin (setup, unit);
    gdouble bottom_margin = gtk_page_setup_get_bottom_margin (setup, unit);

    *page_width -= left_margin + right_margin;
    *page_height -= top_margin + bottom_margin;
  }

}

static void get_max_offsets (CustomWidgetInfo *info,
                                       gdouble *offset_x_max,
                                       gdouble *offset_y_max)
{
  gdouble width;
  gdouble height;

  get_page_dimensions (info, &width, &height, GTK_UNIT_POINTS);

  *offset_x_max = width - 72.0 * info->data->width / info->data->xres;
  *offset_x_max = MAX (0, *offset_x_max);

  *offset_y_max = height - 72.0 * info->data->height / info->data->yres;
  *offset_y_max = MAX (0, *offset_y_max);
}

static void update_offsets (CustomWidgetInfo *info)
{
  PrintData *data = info->data;
  gdouble    offset_x_max;
  gdouble    offset_y_max;

  get_max_offsets (info, &offset_x_max, &offset_y_max);

  switch (data->center) {
    case VIK_PRINT_CENTER_NONE:
      if (data->offset_x > offset_x_max)
        data->offset_x = offset_x_max;
      if (data->offset_y > offset_y_max)
        data->offset_y = offset_y_max;
      break;

    case VIK_PRINT_CENTER_HORIZONTALLY:
      data->offset_x = offset_x_max / 2.0;
      break;

    case VIK_PRINT_CENTER_VERTICALLY:
      data->offset_y = offset_y_max / 2.0;
      break;

    case VIK_PRINT_CENTER_BOTH:
      data->offset_x = offset_x_max / 2.0;
      data->offset_y = offset_y_max / 2.0;
      break;
    }
}

static void center_changed_cb (GtkWidget *combo, CustomWidgetInfo *info)
{
  info->data->center = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  update_offsets (info);

  if (info->preview)
    vik_print_preview_set_image_offsets (VIK_PRINT_PREVIEW (info->preview),
                                     info->data->offset_x, info->data->offset_y);
}

static gboolean scale_change_value_cb(GtkRange     *range,
                                 GtkScrollType scroll,
                                 gdouble       value,
                                 CustomWidgetInfo  *pinfo)
{
  gdouble paper_width;
  gdouble paper_height;
  gdouble xres, yres, res;
  gdouble offset_x_max, offset_y_max;
  gdouble scale = CLAMP(value, 1, 100);

  get_page_dimensions (pinfo, &paper_width, &paper_height, GTK_UNIT_INCH);
  xres = pinfo->data->width * 100 / paper_width / scale;
  yres = pinfo->data->height * 100 / paper_height / scale;
  res = MAX(xres, yres);
  pinfo->data->xres = pinfo->data->yres = res;
  get_max_offsets (pinfo, &offset_x_max, &offset_y_max);
  update_offsets (pinfo);
  if (pinfo->preview) {
    vik_print_preview_set_image_dpi (VIK_PRINT_PREVIEW (pinfo->preview),
                                  pinfo->data->xres, pinfo->data->yres);
    vik_print_preview_set_image_offsets (VIK_PRINT_PREVIEW (pinfo->preview),
                                  pinfo->data->offset_x, pinfo->data->offset_y);
    vik_print_preview_set_image_offsets_max (VIK_PRINT_PREVIEW (pinfo->preview),
                                            offset_x_max, offset_y_max);
  }

  set_scale_label(pinfo, scale);

  return FALSE;
}

static void custom_widgets_cleanup(CustomWidgetInfo *info)
{
  g_free(info);
}

static GtkWidget *create_custom_widget_cb(GtkPrintOperation *operation, PrintData *data)
{
  GtkWidget    *layout;
  GtkWidget    *main_hbox;
  GtkWidget    *main_vbox;
  GtkWidget    *hbox;
  GtkWidget    *vbox;
  GtkWidget    *button;
  GtkWidget    *label;
  GtkPageSetup *setup;

  CustomWidgetInfo  *info = g_malloc0(sizeof(CustomWidgetInfo));
  g_signal_connect_swapped (data->operation, _("done"), G_CALLBACK (custom_widgets_cleanup), info);


  info->data = data;

  setup = gtk_print_operation_get_default_page_setup (data->operation);
  if (! setup) {
    setup = gtk_page_setup_new ();
    gtk_print_operation_set_default_page_setup (data->operation, setup);
  }

  layout = gtk_vbox_new (FALSE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (layout), 12);

  /*  main hbox  */
  main_hbox = gtk_hbox_new (FALSE, 12);
  gtk_box_pack_start (GTK_BOX (layout), main_hbox, TRUE, TRUE, 0);
  gtk_widget_show (main_hbox);

  /*  main vbox  */
  main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_box_pack_start (GTK_BOX (main_hbox), main_vbox, FALSE, FALSE, 0);
  gtk_widget_show (main_vbox);

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (main_vbox), vbox, FALSE, FALSE, 0);
  gtk_widget_show (vbox);

  /* Page Size */
  button = gtk_button_new_with_mnemonic (_("_Adjust Page Size "
                                           "and Orientation"));
  gtk_box_pack_start (GTK_BOX (main_vbox), button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (page_setup_cb),
                    info);
  gtk_widget_show (button);

  /* Center */
  GtkWidget *combo;
  const PrintCenterName *center;

  hbox = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new_with_mnemonic (_("C_enter:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  combo = gtk_combo_box_new_text ();
  for (center = center_modes; center->name; center++) {
    gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _(center->name));
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(combo), VIK_PRINT_CENTER_BOTH);
  gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0);
  gtk_widget_show (combo);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  g_signal_connect(combo, "changed",
                   G_CALLBACK(center_changed_cb), info);
  info->center_combo = combo;

  /* ignore page margins */
  button = gtk_check_button_new_with_mnemonic (_("Ignore Page _Margins"));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
                                data->use_full_page);
  gtk_box_pack_start (GTK_BOX (main_vbox), button, FALSE, FALSE, 0);
  g_signal_connect (button, "toggled",
                    G_CALLBACK (full_page_toggled_cb),
                    info);
  gtk_widget_show (button);

  /* scale */
  vbox = gtk_vbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (main_vbox), vbox, FALSE, FALSE, 0);
  gtk_widget_show (vbox);

  hbox = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new_with_mnemonic (_("Image S_ize:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  info->scale_label = label;
  gtk_box_pack_start (GTK_BOX (hbox), info->scale_label, TRUE, TRUE, 0);
  gtk_widget_show (info->scale_label);

  info->scale = gtk_hscale_new_with_range(1, 100, 1);
  gtk_box_pack_start (GTK_BOX (vbox), info->scale, TRUE, TRUE, 0);
  gtk_scale_set_draw_value(GTK_SCALE(info->scale), FALSE);
  gtk_widget_show (info->scale);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), info->scale);

  g_signal_connect(info->scale, "change_value",
                   G_CALLBACK(scale_change_value_cb), info);


  info->preview = vik_print_preview_new (setup, GDK_DRAWABLE(vik_viewport_get_pixmap(data->vvp)));
  vik_print_preview_set_use_full_page (VIK_PRINT_PREVIEW(info->preview),
                                        data->use_full_page);
  gtk_box_pack_start (GTK_BOX (main_hbox), info->preview, TRUE, TRUE, 0);
  gtk_widget_show (info->preview);

  g_signal_connect (info->preview, "offsets-changed",
                    G_CALLBACK (preview_offsets_changed_cb),
                    info);

  update_page_setup (info);

  gdouble offset_x_max, offset_y_max;
  get_max_offsets (info, &offset_x_max, &offset_y_max);
  vik_print_preview_set_image_offsets_max (VIK_PRINT_PREVIEW (info->preview),
                                            offset_x_max, offset_y_max);

  set_scale_value(info);
  
  return layout;
}

#endif
