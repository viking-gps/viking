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

/**
 * Options adapting GPX writing.
 */
typedef struct {
	gboolean force_ele; /// Force ele field
	gboolean force_time; /// Force time field
} GpxWritingOptions;

void a_gpx_read_file ( VikTrwLayer *trw, FILE *f );
void a_gpx_write_file ( VikTrwLayer *trw, FILE *f );
void a_gpx_write_file_options ( GpxWritingOptions *options, VikTrwLayer *trw, FILE *f );
void a_gpx_write_track_file ( const gchar *name, VikTrack *track, FILE *f );
void a_gpx_write_track_file_options ( GpxWritingOptions *options, const gchar *name, VikTrack *t, FILE *f );

#endif
