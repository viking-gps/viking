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

#include <gtk/gtk.h>

GtkWidget *gtk_color_button_new_with_color ( const GdkColor *color )
{
  gchar *tmp = g_strdup_printf("#%.2x%.2x%.2x", color->red/256, color->green/256, color->blue/256 );
  GtkWidget *entry = gtk_entry_new ();
  gtk_entry_set_text ( GTK_ENTRY(entry), tmp );
  g_free ( tmp );
  return entry;
}

void gtk_color_button_get_color ( GtkWidget *widget, GdkColor *dest_color )
{
  gdk_color_parse ( gtk_entry_get_text(GTK_ENTRY(widget)), dest_color );
}

GtkWidget *GTK_COLOR_BUTTON ( GtkWidget *w )
{
  return w;
}
