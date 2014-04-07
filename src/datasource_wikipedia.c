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
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "acquire.h"
#include "geonamessearch.h"

static gboolean datasource_wikipedia_process ( VikTrwLayer *vtl, const gchar *cmd, const gchar *extra, BabelStatusFunc status_cb, acq_dialog_widgets_t *adw );

VikDataSourceInterface vik_datasource_wikipedia_interface = {
  N_("Create Waypoints from Wikipedia Articles"),
  N_("Wikipedia Waypoints"),
  VIK_DATASOURCE_AUTO_LAYER_MANAGEMENT,
  VIK_DATASOURCE_INPUTTYPE_NONE,
  FALSE,
  FALSE, // Not even using the dialog
  FALSE, // Own method for getting data - does not fit encapsulation with current thread logic
  (VikDataSourceInitFunc)               NULL,
  (VikDataSourceCheckExistenceFunc)	    NULL,
  (VikDataSourceAddSetupWidgetsFunc)    NULL,
  (VikDataSourceGetCmdStringFunc)       NULL,
  (VikDataSourceProcessFunc)            datasource_wikipedia_process,
  (VikDataSourceProgressFunc)           NULL,
  (VikDataSourceAddProgressWidgetsFunc) NULL,
  (VikDataSourceCleanupFunc)            NULL,
  (VikDataSourceOffFunc)                NULL,

  NULL,
  0,
  NULL,
  NULL,
  0
};

/**
 * Process selected files and try to generate waypoints storing them in the given vtl
 */
static gboolean datasource_wikipedia_process ( VikTrwLayer *vtl, const gchar *cmd, const gchar *extra, BabelStatusFunc status_cb, acq_dialog_widgets_t *adw )
{
	struct LatLon maxmin[2] = { {0.0,0.0}, {0.0,0.0} };

	// Note the order is max part first then min part - thus reverse order of use in min_max function:
	vik_viewport_get_min_max_lat_lon ( adw->vvp, &maxmin[1].lat, &maxmin[0].lat, &maxmin[1].lon, &maxmin[0].lon );

	if ( vtl ) {
		a_geonames_wikipedia_box ( adw->vw, vtl, maxmin );
		return TRUE;
	}
	else
		return FALSE;
}
