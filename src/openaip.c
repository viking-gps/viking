/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2008, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#include "openaip.h"
#include "vikmapslayer.h"
#include "vikslippymapsource.h"
#include "map_ids.h"

/* initialisation */
void openaip_init ()
{
  VikMapSource *openaip_type = VIK_MAP_SOURCE(g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE,
							      "id", MAP_ID_OPENAIP,
							      "name", "OpenAIP",
							      "label", "OpenAIP",
							      "url", "https://api.tiles.openaip.net/api/data/openaip/%d/%d/%d.png?apiKey="VIK_CONFIG_OPENAIP_KEY,
							      "file-extension", ".png",
							      "zoom-min", 4,
							      "zoom-max", 14,
							      "copyright", "https://www.openaip.net",
							      "license", "CC BY-NC 4.0",
							      "license-url", "https://creativecommons.org/licenses/by-nc/4.0/",
							      NULL));

  maps_layer_register_map_source (openaip_type);
}

