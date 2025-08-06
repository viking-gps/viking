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
#ifndef __VIKING_MAPLIBRE_IF_H
#define __VIKING_MAPLIBRE_IF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

typedef struct _MaplibreInterface MaplibreInterface;

void maplibre_interface_initialize ();

MaplibreInterface* maplibre_interface_new ( guint width, guint height, const gchar* cache_file, const gchar* api_key );
void maplibre_interface_free (MaplibreInterface* mi);

gchar* maplibre_interface_load_style_file ( MaplibreInterface* mi,
                                            const gchar *filename );

GdkPixbuf* maplibre_interface_render ( MaplibreInterface* mi, double lat_tl, double lon_tl, double lat_br, double lon_br );

gchar* maplibre_interface_get_copyright ( MaplibreInterface* mi );

GArray* maplibre_interface_get_parameters ( MaplibreInterface* mi );

gchar * maplibre_interface_about ( void );

#ifdef __cplusplus
}
#endif

#endif
