/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _VIKING_GEOTAG_EXIF_H
#define _VIKING_GEOTAG_EXIF_H

#include "vikwaypoint.h"
#include "vikcoord.h"

G_BEGIN_DECLS

VikWaypoint* a_geotag_create_waypoint_from_file ( const gchar *filename, VikCoordMode vcmode, gchar **name );

VikWaypoint* a_geotag_waypoint_positioned ( const gchar *filename, VikCoord coord, gdouble alt, gchar **name, VikWaypoint *wp );

gchar* a_geotag_get_exif_date_from_file ( const gchar *filename, gboolean *has_GPS_info );

gint a_geotag_write_exif_gps ( const gchar *filename, VikCoord coord, gdouble alt, gboolean no_change_mtime );

G_END_DECLS

#endif // _VIKING_GEOTAG_EXIF_H
