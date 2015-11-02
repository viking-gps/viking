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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "jpg.h"
#ifdef VIK_CONFIG_GEOTAG
#include "geotag_exif.h"
#endif
#ifdef HAVE_MAGIC_H
#include <magic.h>
#endif

/**
 * a_jpg_magic_check:
 * @filename: The file
 *
 * Returns: Whether the file is a JPG.
 *  Uses Magic library if available to determine the jpgness.
 *  Otherwise uses a rudimentary extension name check.
 */
gboolean a_jpg_magic_check ( const gchar *filename )
{
	gboolean is_jpg = FALSE;
#ifdef HAVE_MAGIC_H
	magic_t myt = magic_open ( MAGIC_CONTINUE|MAGIC_ERROR|MAGIC_MIME );
	if ( myt ) {
#ifdef WINDOWS
		// We have to 'package' the magic database ourselves :(
		//  --> %PROGRAM FILES%\Viking\magic.mgc
		magic_load ( myt, "magic.mgc" );
#else
		// Use system default
		magic_load ( myt, NULL );
#endif
		const char* magic = magic_file (myt, filename);
		if ( g_strcmp0 (magic, "image/jpeg; charset=binary") == 0 )
			is_jpg = TRUE;

		magic_close ( myt );
	}
	else
#endif
		is_jpg = a_file_check_ext ( filename, ".jpg" );

	return is_jpg;
}

/**
 * Load a single JPG into a Trackwaypoint Layer as a waypoint
 *
 * @top:      The Aggregate layer that a new TRW layer may be created in
 * @filename: The JPG filename
 * @vvp:      The viewport
 *
 * Returns: Whether the loading was a success or not
 *
 * If the JPG has geotag information then the waypoint will be created with the appropriate position.
 *  Otherwise the waypoint will be positioned at the current screen center.
 * If a TRW layer is already selected the waypoint will be created in that layer.
 */
gboolean a_jpg_load_file ( VikAggregateLayer *top, const gchar *filename, VikViewport *vvp )
{
	gboolean auto_zoom = TRUE;
	VikWindow *vw = (VikWindow *)(VIK_GTK_WINDOW_FROM_LAYER(VIK_LAYER(top)));
	VikLayersPanel *vlp = vik_window_layers_panel ( vw );
	// Auto load into TrackWaypoint layer if one is selected
	VikLayer *vtl = vik_layers_panel_get_selected ( vlp );

	gboolean create_layer = FALSE;
	if ( vtl == NULL || vtl->type != VIK_LAYER_TRW ) {
		// Create layer if necessary
		vtl = vik_layer_create ( VIK_LAYER_TRW, vvp, FALSE );
		vik_layer_rename ( vtl, a_file_basename ( filename ) );
		create_layer = TRUE;
	}

	gchar *name = NULL;
	VikWaypoint *wp = NULL;
#ifdef VIK_CONFIG_GEOTAG
	wp = a_geotag_create_waypoint_from_file ( filename, vik_viewport_get_coord_mode (vvp), &name );
#endif
	if ( wp ) {
		// Create name if geotag method didn't return one
		if ( !name )
			name = g_strdup ( a_file_basename ( filename ) );
		vik_trw_layer_filein_add_waypoint ( VIK_TRW_LAYER(vtl), name, wp );
		g_free ( name );
	}
	else {
		wp = vik_waypoint_new ();
		wp->visible = TRUE;
		vik_trw_layer_filein_add_waypoint ( VIK_TRW_LAYER(vtl), (gchar*) a_file_basename(filename), wp );
		vik_waypoint_set_image ( wp, filename );
		// Simply set position to the current center
		wp->coord = *( vik_viewport_get_center ( vvp ) );
		auto_zoom = FALSE;
	}

	// Complete the setup
	vik_layer_post_read ( vtl, vvp, TRUE );
	if ( create_layer )
		vik_aggregate_layer_add_layer ( top, vtl, FALSE );
	if ( auto_zoom )
		vik_trw_layer_auto_set_view ( VIK_TRW_LAYER(vtl), vvp );

	// ATM This routine can't fail
	return TRUE;
}
