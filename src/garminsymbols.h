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

#ifndef __VIKING_GARMINSYMBOLS_H
#define __VIKING_GARMINSYMBOLS_H

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

GdkPixbuf *a_get_wp_sym ( const gchar *sym );
const gchar *a_get_hashed_sym ( const gchar *sym );
GtkListStore *a_garmin_get_sym_liststore ();
/* Use when preferences have changed to reload icons*/
void clear_garmin_icon_syms ();
void a_garmin_icons_uninit ();

G_END_DECLS

#endif
