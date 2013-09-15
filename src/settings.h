/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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
#ifndef __SETTINGS_H
#define __SETTINGS_H

G_BEGIN_DECLS

void a_settings_init();

void a_settings_uninit();

gboolean a_settings_get_boolean ( const gchar *name, gboolean *val );

void a_settings_set_boolean ( const gchar *name, gboolean val );

gboolean a_settings_get_string ( const gchar *name, gchar **val );

void a_settings_set_string ( const gchar *name, const gchar *val );

gboolean a_settings_get_integer ( const gchar *name, gint *val );

void a_settings_set_integer ( const gchar *name, gint val );

gboolean a_settings_get_double ( const gchar *name, gdouble *val );

void a_settings_set_double ( const gchar *name, gdouble val );

/*
gboolean a_settings_get_integer_list ( const gchar *name, gint *vals, gsize* length );

void a_settings_set_integer_list ( const gchar *name, gint vals[], gsize length );
*/
gboolean a_settings_get_integer_list_contains ( const gchar *name, gint val );

void a_settings_set_integer_list_containing ( const gchar *name, gint val );

G_END_DECLS

#endif
