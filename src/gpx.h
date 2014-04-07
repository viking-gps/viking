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

#ifndef _VIKING_GPX_H
#define _VIKING_GPX_H

#include "viktrwlayer.h"

G_BEGIN_DECLS

/**
 * Options adapting GPX writing.
 */
typedef struct {
	// NB force options only apply to trackpoints
	gboolean force_ele; /// Force ele field
	gboolean force_time; /// Force time field
	gboolean hidden; /// Write invisible tracks/waypoints (default is yes)
	gboolean is_route; /// For internal convience
} GpxWritingOptions;

gboolean a_gpx_read_file ( VikTrwLayer *trw, FILE *f );
void a_gpx_write_file ( VikTrwLayer *trw, FILE *f, GpxWritingOptions *options );
void a_gpx_write_track_file ( VikTrack *trk, FILE *f, GpxWritingOptions *options );

gchar* a_gpx_write_tmp_file ( VikTrwLayer *vtl, GpxWritingOptions *options );
gchar* a_gpx_write_track_tmp_file ( VikTrack *trk, GpxWritingOptions *options );

G_END_DECLS

#endif
