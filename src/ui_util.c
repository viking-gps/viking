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

#include "util.h"
#include "dialog.h"
#include "settings.h"

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
    gtk_show_uri ( gtk_widget_get_screen (GTK_WIDGET(parent)), url, GDK_CURRENT_TIME, &error );
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
  gtk_show_uri ( gtk_widget_get_screen (GTK_WIDGET(parent)), uri, GDK_CURRENT_TIME, &error );
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
