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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>

#include "google.h"
#include "google-map-type.h"
#include "google-kh-map-type.h"
#include "vikmapslayer.h"


void google_init () {
  VikMapType *google_1 = VIK_MAP_TYPE(google_map_type_new_with_id(7, TYPE_GOOGLE_MAPS));
  VikMapType *google_2 = VIK_MAP_TYPE(google_map_type_new_with_id(10, TYPE_GOOGLE_TRANS));
  VikMapType *google_3 = VIK_MAP_TYPE(google_kh_map_type_new_with_id(11));
  VikMapType *google_4 = VIK_MAP_TYPE(google_map_type_new_with_id(16, TYPE_GOOGLE_TERRAIN));

  maps_layer_register_type(_("Google Maps"), 7, google_1);
  maps_layer_register_type(_("Transparent Google Maps"), 10, google_2);
  maps_layer_register_type(_("Google Satellite Images"), 11, google_3);
  maps_layer_register_type(_("Google Terrain Maps"), 16, google_4);
}
