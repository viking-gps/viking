/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2015, Rob Norris <rw_norris@hotmail.com>
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
#ifndef __VIKING_MAPNIK_IF_H
#define __VIKING_MAPNIK_IF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

typedef struct _MapnikInterface MapnikInterface;

void mapnik_interface_initialize (const char *plugins_dir, const char* font_dir, int font_dir_recurse);

MapnikInterface* mapnik_interface_new ();
void mapnik_interface_free (MapnikInterface* mi);

gchar* mapnik_interface_load_map_file ( MapnikInterface* mi,
                                        const gchar *filename,
                                        guint width,
                                        guint height );

GdkPixbuf* mapnik_interface_render ( MapnikInterface* mi, double lat_tl, double lon_tl, double lat_br, double lon_br );

gchar* mapnik_interface_get_copyright ( MapnikInterface* mi );

GArray* mapnik_interface_get_parameters ( MapnikInterface* mi );

gchar * mapnik_interface_about ( void );

#ifdef __cplusplus
}
#endif

#endif
