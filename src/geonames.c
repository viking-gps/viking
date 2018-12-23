/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#include "geonames.h"

#include "vikgotoxmltool.h"
#include "vikgoto.h"

/* initialisation */
void geonames_init () {
  // Goto
  VikGotoXmlTool *geonames = VIK_GOTO_XML_TOOL ( g_object_new ( VIK_GOTO_XML_TOOL_TYPE, "label", "Geonames",
    "url-format", "http://api.geonames.org/search?q=%s&lang=en&style=short&username="VIK_CONFIG_GEONAMES_USERNAME,
    "lat-path", "/geonames/geoname/lat",
    "lon-path", "/geonames/geoname/lng",
    "desc-path", "/geonames/geoname/toponymName",
    NULL ) );
    vik_goto_register ( VIK_GOTO_TOOL ( geonames ) );
    g_object_unref ( geonames );
}

