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

#ifndef __VIKING_BACKGROUND_H
#define __VIKING_BACKGROUND_H

#include <glib.h>
#include <gtk/gtk.h>

#include "vikstatus.h"

typedef void(*vik_thr_free_func)(gpointer);
typedef void(*vik_thr_func)(gpointer,gpointer);

/* the new way */
void a_background_thread ( GtkWindow *parent, const gchar *message, vik_thr_func func, gpointer userdata, vik_thr_free_func userdata_free_func, vik_thr_free_func userdata_cancel_cleanup_func, gint number_items );
int a_background_thread_progress ( gpointer callbackdata, gdouble fraction );
int a_background_testcancel ( gpointer callbackdata );
void a_background_show_window ();
void a_background_init ();
void a_background_uninit ();
void a_background_add_status(VikStatusbar *vs);
void a_background_remove_status(VikStatusbar *vs);

#endif
