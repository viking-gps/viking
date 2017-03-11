/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2017, Rob Norris <rw_norris@hotmail.com>
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
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <config.h>

#include "globals.h"
#include "libgeoclue.h"

void libgeoclue_print_location (GClueLocation *location)
{
	g_print ("%s:\n\tLatitude:  %f°\n\tLongitude: %f°\n\tAccuracy:  %.3f meters\n",
	         __FUNCTION__,
	         gclue_location_get_latitude (location),
	         gclue_location_get_longitude (location),
	         gclue_location_get_accuracy (location));
	const char *description = gclue_location_get_description (location);
	if (strlen (description) > 0)
		g_print ("Description: %s\n", description);
}

typedef struct {
	VikWindow *vw;
	callback func;
	struct LatLon ll;
	gdouble accuracy;
} clue_t;

static void
on_simple_ready (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
	GError *error = NULL;
	clue_t *clue = (clue_t*)user_data;
	clue->ll.lat = NAN;
	clue->ll.lon = NAN;

	GClueSimple *simple = gclue_simple_new_finish ( res, &error );
	if ( error != NULL ) {
		g_warning ( "Failed to connect to service: %s", error->message );
		g_error_free ( error );
		goto finish;
	}

	GClueLocation *location = gclue_simple_get_location ( simple );
	if ( vik_verbose )
		libgeoclue_print_location ( location );

	clue->ll.lat = gclue_location_get_latitude ( location );
	clue->ll.lon = gclue_location_get_longitude ( location );
	clue->accuracy = gclue_location_get_accuracy ( location );

#if GLIB_CHECK_VERSION(2,28,0)
	g_clear_object ( &simple );
#endif
finish:
	clue->func(clue->vw, clue->ll, clue->accuracy);
	g_free ( clue );
}

/**
 * libgeoclue_where_am_i:
 *
 * Use geoclue to get location information
 * As this is asynchronous, use a callback to inform when the process
 *  has completed.
 */
void libgeoclue_where_am_i ( VikWindow *vw, callback func )
{
	clue_t *clue = g_malloc ( sizeof(clue_t) );
	clue->vw   = vw;
	clue->func = func;

	gclue_simple_new (PACKAGE,
	                  GCLUE_ACCURACY_LEVEL_CITY,
	                  NULL,
	                  on_simple_ready,
	                  clue);
}
