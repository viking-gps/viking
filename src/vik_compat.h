/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
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
#ifndef __VIKING_COMPAT_H
#define __VIKING_COMPAT_H

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

// Hide ifdef complexities of function variants here

GMutex * vik_mutex_new ();
void vik_mutex_free (GMutex *mutex);

/*
 * Since combo boxes are used in various places
 * keep the code reasonably tidy and only have one ifdef to cater for the naming variances
 */
#if GTK_CHECK_VERSION (2, 24, 0)
#define vik_combo_box_text_new gtk_combo_box_text_new
#define vik_combo_box_text_append(X,Y) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(X),Y)
#else
#define vik_combo_box_text_new gtk_combo_box_new_text
#define vik_combo_box_text_append(X,Y) gtk_combo_box_append_text(GTK_COMBO_BOX(X),Y)
#endif

G_END_DECLS

#endif
