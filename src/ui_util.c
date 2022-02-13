/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *    Viking - GPS data editor
 *    Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
 *    Copyright 2006-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *    Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *    Copyright 2011-2012 Matthew Brush <mbrush(at)codebrainz(dot)ca>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */
 /*
  * Ideally dependencies should just be on Gtk,
  * see vikutils for things that further depend on other Viking types
  * see utils for things only depend on Glib
  */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include "ui_util.h"
#include "util.h"
#include "dialog.h"
#include "settings.h"
#include "icons/icons.h"

#ifdef WINDOWS
#include <windows.h>
#endif


#ifndef WINDOWS
static gboolean spawn_command_line_async(const gchar * cmd,
                                         const gchar * arg)
{
  gchar *cmdline = NULL;
  gboolean status;

  cmdline = g_strdup_printf("%s '%s'", cmd, arg);
  g_debug("Running: %s", cmdline);

  status = g_spawn_command_line_async(cmdline, NULL);

  g_free(cmdline);

  return status;
}
#endif


// Annoyingly gtk_show_uri() doesn't work so resort to ShellExecute method
//   (non working at least in our Windows build with GTK+2.24.10 on Windows 7)
// @parent: Maybe NULL
//
void open_url(GtkWindow *parent, const gchar * url)
{
#ifdef WINDOWS
  ShellExecute(NULL, NULL, (char *) url, NULL, ".\\", 0);
#else
  gboolean use_browser = FALSE;
  if ( a_settings_get_boolean ( "use_env_browser", &use_browser ) ) {
    const gchar *browser = g_getenv("BROWSER");
    if (browser == NULL || browser[0] == '\0') {
      browser = "firefox";
    }
    if (spawn_command_line_async(browser, url)) {
      return;
    }
    else
      g_warning("Failed to run: %s on %s", browser, url);
  }
  else {
    GError *error = NULL;
#if GTK_CHECK_VERSION (3,22,0)
	(void)gtk_show_uri_on_window ( parent, url, GDK_CURRENT_TIME, &error );
#else
    (void)gtk_show_uri ( parent ? gtk_widget_get_screen(GTK_WIDGET(parent)) : NULL, url, GDK_CURRENT_TIME, &error );
#endif
    if ( error ) {
      a_dialog_error_msg_extra ( parent, _("Could not launch web browser. %s"), error->message );
      g_error_free ( error );
    }
  }
#endif
}

void new_email(GtkWindow *parent, const gchar * address)
{
  gchar *uri = g_strdup_printf("mailto:%s", address);
  GError *error = NULL;
#if GTK_CHECK_VERSION (3,22,0)
  (void)gtk_show_uri_on_window ( parent, uri, GDK_CURRENT_TIME, &error );
#else
  (void)gtk_show_uri ( gtk_widget_get_screen (GTK_WIDGET(parent)), uri, GDK_CURRENT_TIME, &error );
#endif
  if ( error ) {
    a_dialog_error_msg_extra ( parent, _("Could not create new email. %s"), error->message );
    g_error_free ( error );
  }
  /*
#ifdef WINDOWS
  ShellExecute(NULL, NULL, (char *) uri, NULL, ".\\", 0);
#else
  if (!spawn_command_line_async("xdg-email", uri))
    a_dialog_error_msg ( parent, _("Could not create new email.") );
#endif
  */
  g_free(uri);
  uri = NULL;
}

/** Creates a @c GtkButton with custom text and a stock image similar to
 * @c gtk_button_new_from_stock().
 * @param stock_id A @c GTK_STOCK_NAME string.
 * @param text Button label text, can include mnemonics.
 * @return The new @c GtkButton.
 */
GtkWidget *ui_button_new_with_image(const gchar *stock_id, const gchar *text)
{
	GtkWidget *image, *button;

	button = gtk_button_new_with_mnemonic(text);
	gtk_widget_show(button);
	image = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(button), image);
	// note: image is shown by gtk
	return button;
}

/** Reads an integer from the GTK default settings registry
 * (see http://library.gnome.org/devel/gtk/stable/GtkSettings.html).
 * @param property_name The property to read.
 * @param default_value The default value in case the value could not be read.
 * @return The value for the property if it exists, otherwise the @a default_value.
 */
gint ui_get_gtk_settings_integer(const gchar *property_name, gint default_value)
{
	if (g_object_class_find_property(G_OBJECT_GET_CLASS(G_OBJECT(
		gtk_settings_get_default())), property_name))
	{
		gint value;
		g_object_get(G_OBJECT(gtk_settings_get_default()), property_name, &value, NULL);
		return value;
	}
	else
		return default_value;
}


/** Returns a widget from a name in a component, usually created by Glade.
 * Call it with the toplevel widget in the component (i.e. a window/dialog),
 * or alternatively any widget in the component, and the name of the widget
 * you want returned.
 * @param widget Widget with the @a widget_name property set.
 * @param widget_name Name to lookup.
 * @return The widget found.
 * @see ui_hookup_widget().
 *
 */
GtkWidget *ui_lookup_widget(GtkWidget *widget, const gchar *widget_name)
{
	GtkWidget *parent, *found_widget;

	g_return_val_if_fail(widget != NULL, NULL);
	g_return_val_if_fail(widget_name != NULL, NULL);

	for (;;)
	{
		if (GTK_IS_MENU(widget))
			parent = gtk_menu_get_attach_widget(GTK_MENU(widget));
		else
			parent = gtk_widget_get_parent(widget);
		if (parent == NULL)
			parent = (GtkWidget*) g_object_get_data(G_OBJECT(widget), "GladeParentKey");
		if (parent == NULL)
			break;
		widget = parent;
	}

	found_widget = (GtkWidget*) g_object_get_data(G_OBJECT(widget), widget_name);
	if (G_UNLIKELY(found_widget == NULL))
		g_warning("Widget not found: %s", widget_name);
	return found_widget;
}

/**
 * Returns a label widget that is made selectable (i.e. the user can copy the text)
 * @param text String to display - maybe NULL
 * @return The label widget
 */
GtkWidget* ui_label_new_selectable ( const gchar* text )
{
	GtkWidget *widget = gtk_label_new ( text );
	gtk_label_set_selectable ( GTK_LABEL(widget), TRUE );
	return widget;
}

/**
 * Create a new pixbuf of the specified color and size
 */
GdkPixbuf *ui_pixbuf_new ( GdkColor *color, guint width, guint height )
{
	GdkPixbuf *pixbuf = gdk_pixbuf_new ( GDK_COLORSPACE_RGB, FALSE, 8, width, height );
	// Annoyingly the GdkColor.pixel does not give the correct color when passed to gdk_pixbuf_fill (even when alloc'ed)
	// Here is some magic found to do the conversion
	// http://www.cs.binghamton.edu/~sgreene/cs360-2011s/topics/gtk+-2.20.1/gtk/gtkcolorbutton.c
	if ( color ) {
		guint32 pixel = ((color->red & 0xff00) << 16) |
			((color->green & 0xff00) << 8) |
			(color->blue & 0xff00);
		gdk_pixbuf_fill ( pixbuf, pixel );
	}
	return pixbuf;
}

/**
 * Apply the alpha value to the specified pixbuf
 */
GdkPixbuf *ui_pixbuf_set_alpha ( GdkPixbuf *pixbuf, guint8 alpha )
{
  guchar *pixels;
  gint width, height, iii, jjj;

  if ( ! gdk_pixbuf_get_has_alpha ( pixbuf ) )
  {
    GdkPixbuf *tmp = gdk_pixbuf_add_alpha(pixbuf,FALSE,0,0,0);
    g_object_unref(G_OBJECT(pixbuf));
    pixbuf = tmp;
    if ( !pixbuf )
      return NULL;
  }

  pixels = gdk_pixbuf_get_pixels(pixbuf);
  width = gdk_pixbuf_get_width(pixbuf);
  height = gdk_pixbuf_get_height(pixbuf);

  /* r,g,b,a,r,g,b,a.... */
  for (iii = 0; iii < width; iii++) for (jjj = 0; jjj < height; jjj++)
  {
    pixels += 3;
    if ( *pixels != 0 )
      *pixels = alpha;
    pixels++;
  }
  return pixbuf;
}


/**
 * Reduce the alpha value of the specified pixbuf by alpha / 255
 */
GdkPixbuf *ui_pixbuf_scale_alpha ( GdkPixbuf *pixbuf, guint8 alpha )
{
  guchar *pixels;
  gint width, height, iii, jjj;

  if ( ! gdk_pixbuf_get_has_alpha ( pixbuf ) )
  {
    GdkPixbuf *tmp = gdk_pixbuf_add_alpha(pixbuf,FALSE,0,0,0);
    g_object_unref(G_OBJECT(pixbuf));
    pixbuf = tmp;
    if ( !pixbuf )
      return NULL;
  }

  pixels = gdk_pixbuf_get_pixels(pixbuf);
  width = gdk_pixbuf_get_width(pixbuf);
  height = gdk_pixbuf_get_height(pixbuf);

  /* r,g,b,a,r,g,b,a.... */
  for (iii = 0; iii < width; iii++) for (jjj = 0; jjj < height; jjj++)
  {
    pixels += 3;
    if ( *pixels != 0 )
      *pixels = (guint8)(((guint16)*pixels * (guint16)alpha) / 255);
    pixels++;
  }
  return pixbuf;
}



/**
 *
 */
void ui_add_recent_file ( const gchar *filename )
{
	if ( filename ) {
		GtkRecentManager *manager = gtk_recent_manager_get_default();
		GFile *file = g_file_new_for_commandline_arg ( filename );
		gchar *uri = g_file_get_uri ( file );
		if ( uri && manager )
			gtk_recent_manager_add_item ( manager, uri );
		g_object_unref( file );
		g_free (uri);
	}
}

/**
 * Clear the entry text if the specified icon is pressed
 */
static void ui_icon_clear_entry ( GtkEntry             *entry,
                                  GtkEntryIconPosition position,
                                  GdkEventButton       *event,
                                  gpointer             data )
{
	if ( position == GPOINTER_TO_INT(data) )
		gtk_entry_set_text ( entry, "" );
}

static void
text_changed_cb (GtkEntry   *entry,
                 GParamSpec *pspec,
                 gpointer   data)
{
	if ( data ) {
		gboolean has_text = gtk_entry_get_text_length(entry) > 0;
		gtk_entry_set_icon_sensitive ( entry, GPOINTER_TO_INT(data), has_text );
	}
}

/**
 * Create an entry field with an icon to clear the entry
 *
 * Ideal for entries used for getting user entered transitory data,
 *  so it is easy to delete the text and start again.
 */
GtkWidget *ui_entry_new ( const gchar *str, GtkEntryIconPosition position )
{
	GtkWidget *entry = gtk_entry_new();
	if ( str )
		gtk_entry_set_text ( GTK_ENTRY(entry), str );
	gtk_entry_set_icon_from_stock ( GTK_ENTRY(entry), position, GTK_STOCK_CLEAR );
#if GTK_CHECK_VERSION (2,20,0)
	text_changed_cb ( GTK_ENTRY(entry), NULL, GINT_TO_POINTER(position) );
	g_signal_connect ( entry, "notify::text", G_CALLBACK(text_changed_cb), GINT_TO_POINTER(position) );
#endif
	g_signal_connect ( entry, "icon-release", G_CALLBACK(ui_icon_clear_entry), GINT_TO_POINTER(position) );
	return entry;
}

/**
 * Set the entry field text
 *
 * Handles NULL, unlike gtk_entry_set_text() which complains
 */
void ui_entry_set_text ( GtkWidget *widget, const gchar *str )
{
	if ( str )
		gtk_entry_set_text ( GTK_ENTRY(widget), str );
	else
		gtk_entry_set_text ( GTK_ENTRY(widget), "" );
}

/**
 * Create a spinbutton with an icon to clear the entry
 *
 * Ideal for entries used for getting user entered transitory data,
 *  so it is easy to delete the number and start again.
 */
GtkWidget *ui_spin_button_new ( GtkAdjustment *adjustment,
                                gdouble climb_rate,
                                guint digits )
{
	GtkWidget *spin = gtk_spin_button_new ( adjustment, climb_rate, digits );
	gtk_entry_set_icon_from_stock ( GTK_ENTRY(spin), GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_CLEAR );
	g_signal_connect ( spin, "icon-release", G_CALLBACK(ui_icon_clear_entry), GINT_TO_POINTER(GTK_ENTRY_ICON_PRIMARY) );
	return spin;
}

/**
 * ui_attach_to_table:
 *
 * Create clickable link buttons if the associated entry value is a URL,
 *  otherwise use standard labels as before.
 * Since link buttons don't support pango markup in the label,
 *  the boldness settings (for labels that may be associated with a URLs) is now configured by a specific parameter
 *  otherwise markup values are shown in the link button label.
 *
 * Returns: The created widget
 */
GtkWidget *ui_attach_to_table ( GtkTable *table, int i, char *mylabel, GtkWidget *content, gchar *value_potentialURL, gboolean embolden )
{
  // Settings so the text positioning only moves around vertically when the dialog is resized
  // This also gives more room to see the text e.g. for track comment fields
  GtkWidget *ww = NULL;
  gboolean isURL = FALSE;

  if ( value_potentialURL ) {
    gchar *scheme = g_uri_parse_scheme ( value_potentialURL );
    if ( scheme )
      isURL = TRUE;
    g_free ( scheme );
  }

  if ( isURL ) {
    // NB apparently no control over label positioning & markup
    //  when in a link button :(
    ww = gtk_link_button_new_with_label ( value_potentialURL, _(mylabel) );
  } else {
    gchar *text = NULL;
    ww = gtk_label_new ( NULL );
    if ( embolden )
      text = g_strdup_printf ( "<b>%s</b>", _(mylabel) );
    else
      text = g_strdup ( _(mylabel) );
    gtk_label_set_markup ( GTK_LABEL(ww), text );
    gtk_misc_set_alignment ( GTK_MISC(ww), 1, 0.5 ); // Position text centrally in vertical plane
    g_free ( text );
  }
  gtk_table_attach ( table, ww, 0, 1, i, i+1, GTK_FILL, GTK_SHRINK, 0, 0 );
  if ( GTK_IS_MISC(content) ) {
    gtk_misc_set_alignment ( GTK_MISC(content), 0, 0.5 );
  }
  if ( GTK_IS_COLOR_BUTTON(content) || GTK_IS_COMBO_BOX(content) )
    // Buttons compressed - otherwise look weird (to me) if vertically massive
    gtk_table_attach ( table, content, 1, 2, i, i+1, GTK_FILL, GTK_SHRINK, 0, 5 );
  else {
     // Expand for comments + descriptions / labels
     gtk_table_attach_defaults ( table, content, 1, 2, i, i+1 );
     if ( GTK_IS_LABEL(content) )
       gtk_widget_set_can_focus ( content, FALSE ); // Prevent notebook auto selecting it
  }
  return ww;
}

/**
 * ui_format_1f_cell_data_func:
 *
 * General purpose column double formatting
 *
 */
void ui_format_1f_cell_data_func ( GtkTreeViewColumn *col,
                                   GtkCellRenderer   *renderer,
                                   GtkTreeModel      *model,
                                   GtkTreeIter       *iter,
                                   gpointer           user_data )
{
	gdouble value;
	gchar buf[20];
	gint column = GPOINTER_TO_INT (user_data);
	gtk_tree_model_get ( model, iter, column, &value, -1 );
	g_snprintf ( buf, sizeof(buf), "%.1f", value );
	g_object_set ( renderer, "text", buf, NULL );
}

/**
 * ui_new_column_text:
 *
 * Standard adding of a text column
 *
 */
GtkTreeViewColumn *ui_new_column_text ( const gchar *title, GtkCellRenderer *renderer, GtkWidget *view, gint column_runner )
{
	GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes ( title, renderer, "text", column_runner, NULL );
	gtk_tree_view_column_set_sort_column_id ( column, column_runner );
	gtk_tree_view_append_column ( GTK_TREE_VIEW(view), column );
	gtk_tree_view_column_set_resizable ( column, TRUE );
	return column;
}

/**
 * ui_tree_model_number_tooltip_cb:
 *
 * A simple tooltip on a widget for showing number of items displayed in the tree model
 *
 */
gboolean ui_tree_model_number_tooltip_cb ( GtkWidget    *widget,
                                           gint          x,
                                           gint          y,
                                           gboolean      keyboard_mode,
                                           GtkTooltip   *tooltip,
                                           GtkTreeModel *tree_model )
{
	gint num = gtk_tree_model_iter_n_children ( tree_model, NULL );
	if ( num ) {
		gchar *text = g_strdup_printf ( _("Items: %d"), num );
		gtk_tooltip_set_text ( tooltip, text );
		g_free ( text );
	}
	return (num != 0);
}

/**
 * Load Icons using GResource (see icons/icons.gresource.xml)
 */
void ui_load_icons ( void )
{
	const gchar *vikpath = "/org/viking-gps/viking/icons";
	// Much easier in GTK3
#if GTK_CHECK_VERSION(3, 14, 0)
	gtk_icon_theme_add_resource_path ( gtk_icon_theme_get_default(), vikpath );
#else
	GResource *icon_res = vik_icons_get_resource();
	char ** children = g_resource_enumerate_children ( icon_res, vikpath, 0, NULL );

	if ( children == NULL ) {
		g_critical ( "No icons in resources!" );
		return;
	}

	for ( int ii = 0; children[ii] != NULL; ii++ ) {
		char *path = g_strdup_printf ( "%s/%s", vikpath, children[ii] );
		GError *error = NULL;

		GdkPixbuf *pb = gdk_pixbuf_new_from_resource ( path, &error );
		if ( error ) {
			g_warning ( "%s: %s, %s", __FUNCTION__, children[ii], error->message );
			g_error_free ( error );
			break;
		}
		
		int size = gdk_pixbuf_get_height ( pb );
		// Use a normalized name (i.e. skip the '.png')
		char *name = g_strdup ( children[ii] );
		char *pos = g_strstr_len ( name, -1, "." );
		if ( pos != NULL )
			*pos = '\0';
		gtk_icon_theme_add_builtin_icon ( name, size, pb );
		g_object_unref ( pb );
		g_free ( name );
	}
	g_strfreev ( children );
#endif
}

GdkPixbuf *ui_get_icon ( const gchar *name, guint size )
{
	GError *error = NULL;
	GdkPixbuf *icon = gtk_icon_theme_load_icon ( gtk_icon_theme_get_default(), name, size, GTK_ICON_LOOKUP_FORCE_SIZE, &error );
	if ( error ) {
		// Own icons should always be defined
		g_critical ( "%s: icon '%s' - %s", __FUNCTION__, name, error->message );
		g_error_free ( error );
	}
	return icon;
}

/**
 * replacement for gdk_draw_layout()
 * note that x,y are now doubles and may need to be offset by 0.5
 */
void ui_cr_draw_layout ( cairo_t *cr, gdouble xx, gdouble yy, PangoLayout *layout )
{
	cairo_move_to ( cr, xx, yy );
	pango_cairo_show_layout ( cr, layout );
}

/**
 * replacement for gdk_draw_line()
 * note that as x,y are now doubles and describe the center of the line
 * ATM no clipping is performed and unknown what happens if ones uses
 *  very large numbers for x2, y2
 *
 * cairo_stroke() should be called after using this
 *  (or after consecutive calls to this are done as a group)
 */
void ui_cr_draw_line ( cairo_t *cr, gdouble x1, gdouble y1, gdouble x2, gdouble y2 )
{
	cairo_move_to ( cr, x1, y1 );
	cairo_line_to ( cr, x2, y2 );
}

/**
 * replacement for gdk_draw_rectangle()
 * note that as x,y are now doubles and describe the center of the line
 * ATM no clipping is performed and unknown what happens if ones uses very large numbers
 * cairo_stroke() should be called after using this
 *  (or after consecutive calls to this are done as a group)
 */
void ui_cr_draw_rectangle ( cairo_t *cr, gboolean fill, gdouble xx, gdouble yy, gdouble ww, gdouble hh )
{
	cairo_rectangle ( cr, xx, yy, ww, hh );
	if ( fill )
		cairo_fill ( cr );
}

void ui_cr_set_color ( cairo_t *cr, const gchar *name )
{
  GdkColor color;
  if ( gdk_color_parse(name, &color) ) {
    gdk_cairo_set_source_color ( cr, &color );
  }
}

// Generic dashed line
void ui_cr_set_dash ( cairo_t *cr )
{
	// ATM assume this doesn't need to be multiplied by the vvp->scale
	static const double dashed[] = {4.0};
	cairo_set_dash ( cr, dashed, 1, 0 );
}

void ui_cr_clear ( cairo_t *cr )
{
	g_return_if_fail ( cr != NULL );

	cairo_operator_t operator = cairo_get_operator ( cr );
	// Clear the buffer
	cairo_set_operator ( cr, CAIRO_OPERATOR_CLEAR );
	cairo_paint ( cr );
	// Return to the previous mode
	//  typically will be the default operator CAIRO_OPERATOR_OVER, for the normal drawing on top mode
	cairo_set_operator ( cr, operator );
}

// Typically should be used in the 'draw' event
// Draw the specified surface (whole screen)
//  which would be overlayed on any other surface
void ui_cr_surface_paint ( cairo_t *cr, cairo_surface_t *surface )
{
	g_return_if_fail ( surface != NULL );
	cairo_set_source_surface ( cr, surface, 0.0, 0.0 );
	cairo_paint ( cr );
}

// Label with solid a background and a border
void ui_cr_label_with_bg (cairo_t *cr, gint xd, gint yd, gint wd, gint hd, PangoLayout *pl)
{
	g_return_if_fail ( cr != NULL );
	ui_cr_set_color ( cr, "#cccccc" );
	ui_cr_draw_rectangle ( cr, TRUE, xd-2, yd-1, wd+4, hd+1 );
	cairo_stroke ( cr );
	ui_cr_set_color ( cr, "#000000" );
	ui_cr_draw_rectangle ( cr, FALSE, xd-2, yd-1, wd+4, hd+1 );
	cairo_stroke ( cr );
	ui_cr_draw_layout ( cr, xd, yd, pl );
}

void ui_gc_unref ( GdkGC *gc )
{
#if GTK_CHECK_VERSION (3,0,0)
	cairo_destroy ( gc );
#else
	g_object_unref ( G_OBJECT(gc) );
#endif
}
