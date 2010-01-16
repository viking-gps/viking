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

#ifndef _VIKING_MAPSLAYER_COMPAT_H
#define _VIKING_MAPSLAYER_COMPAT_H

#include "vikcoord.h"
#include "vikviewport.h"
#include "mapcoord.h"

typedef struct {
  guint8 uniq_id;
  guint16 tilesize_x;
  guint16 tilesize_y;
  guint drawmode;
  gboolean (*coord_to_mapcoord) ( const VikCoord *src, gdouble xzoom, gdouble yzoom, MapCoord *dest );
  void (*mapcoord_to_center_coord) ( MapCoord *src, VikCoord *dest );
  int (*download) ( MapCoord *src, const gchar *dest_fn, void *handle );
  void *(*download_handle_init) ( );
  void (*download_handle_cleanup) ( void *handle );
  /* TODO: constant size (yay!) */
} VikMapsLayer_MapType;

void maps_layer_register_type ( const char *label, guint id, VikMapsLayer_MapType *map_type );

#endif
