/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *    Viking - GPS data editor
 *    Copyright (C) 2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *    Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, version 2.
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

#ifdef WINDOWS
#include <windows.h>
#endif

/*
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
*/

// Annoyingly gtk_show_uri() doesn't work so resort to ShellExecute method
//   (non working at least in our Windows build with GTK+2.24.10 on Windows 7)

void open_url(GtkWindow *parent, const gchar * url)
{
#ifdef WINDOWS
  ShellExecute(NULL, NULL, (char *) url, NULL, ".\\", 0);
#else
  GError *error = NULL;
  gtk_show_uri ( gtk_widget_get_screen (GTK_WIDGET(parent)), url, GDK_CURRENT_TIME, &error );
  if ( error ) {
    a_dialog_error_msg_extra ( parent, _("Could not launch web browser. %s"), error->message );
    g_error_free ( error );
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
    *pixels++ = alpha;
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
