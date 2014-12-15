/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2011, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#include <glib/gi18n.h>

#include "bing.h"
#include "vikmapslayer.h"
#include "bingmapsource.h"
#include "vikwebtoolcenter.h"
#include "vikexttools.h"
#include "map_ids.h"

/** API key registered by Guilhem Bonnefille */
#define API_KEY "AqsTAipaBBpKLXhcaGgP8kceYukatmtDLS1x0CXEhRZnpl1RELF9hlI8j4mNIkrE"

/* initialisation */
void bing_init () {
	VikMapSource *bing_aerial = VIK_MAP_SOURCE
	  (bing_map_source_new_with_id (MAP_ID_BING_AERIAL, _("Bing Aerial"), API_KEY));

	maps_layer_register_map_source (bing_aerial);

	// Allow opening web location
	VikWebtoolCenter *webtool = NULL;
	webtool = vik_webtool_center_new_with_members ( _("Bing"), "http://www.bing.com/maps/?v=2&cp=%s~%s&lvl=%d" );
	vik_ext_tools_register ( VIK_EXT_TOOL ( webtool ) );
	g_object_unref ( webtool );
}
