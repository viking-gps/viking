/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2006-2010, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#include <glib.h>
#include <glib/gstdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "modules.h"

#include "bing.h"
#include "spotmaps.h"
#include "google.h"
#include "terraserver.h"
#include "expedia.h"
#include "osm.h"
#include "osm-traces.h"
#include "bluemarble.h"
#include "geonames.h"
#include "file.h"
#include "vikmapslayer.h"
#include "vikexttools.h"
#include "vikgoto.h"
#include "vikgobjectbuilder.h"

#define VIKING_MAPS_FILE "maps.xml"
#define VIKING_EXTTOOLS_FILE "external_tools.xml"
#define VIKING_GOTOTOOLS_FILE "goto_tools.xml"

static void
modules_register_map_source(VikGobjectBuilder *self, GObject *object)
{
  g_debug (__FUNCTION__);
  VikMapSource *mapsource = VIK_MAP_SOURCE (object);
  /* FIXME label should be hosted by object */
  maps_layer_register_map_source (mapsource);
}

static void
modules_register_exttools(VikGobjectBuilder *self, GObject *object)
{
  g_debug (__FUNCTION__);
  VikExtTool *tool = VIK_EXT_TOOL (object);
  vik_ext_tools_register (tool);
}

static void
modules_register_gototools(VikGobjectBuilder *self, GObject *object)
{
  g_debug (__FUNCTION__);
  VikGotoTool *tool = VIK_GOTO_TOOL (object);
  vik_goto_register (tool);
}

static void
modules_load_config(void)
{
  /* Maps sources */
  gchar *maps = g_build_filename(a_get_viking_dir(), VIKING_MAPS_FILE, NULL);
  if (g_access (maps, R_OK) == 0)
  {
	VikGobjectBuilder *builder = vik_gobject_builder_new ();
	g_signal_connect (builder, "new-object", G_CALLBACK (modules_register_map_source), NULL);
	vik_gobject_builder_parse (builder, maps);
	g_object_unref (builder);
  }

  /* External tools */
  gchar *tools = g_build_filename(a_get_viking_dir(), VIKING_EXTTOOLS_FILE, NULL);
  if (g_access (tools, R_OK) == 0)
  {
	VikGobjectBuilder *builder = vik_gobject_builder_new ();
	g_signal_connect (builder, "new-object", G_CALLBACK (modules_register_exttools), NULL);
	vik_gobject_builder_parse (builder, tools);
	g_object_unref (builder);
  }

  /* Go-to search engines */
  gchar *go_to = g_build_filename(a_get_viking_dir(), VIKING_GOTOTOOLS_FILE, NULL);
  if (g_access (go_to, R_OK) == 0)
  {
	VikGobjectBuilder *builder = vik_gobject_builder_new ();
	g_signal_connect (builder, "new-object", G_CALLBACK (modules_register_gototools), NULL);
	vik_gobject_builder_parse (builder, go_to);
	g_object_unref (builder);
  }
}

void modules_init()
{
#ifdef VIK_CONFIG_BING
  bing_init();
#endif
#ifdef VIK_CONFIG_GOOGLE 
  google_init();
#endif
#ifdef VIK_CONFIG_EXPEDIA
  expedia_init();
#endif
#ifdef VIK_CONFIG_TERRASERVER
  terraserver_init();
#endif
#ifdef VIK_CONFIG_OPENSTREETMAP
  osm_init();
  osm_traces_init();
#endif
#ifdef VIK_CONFIG_BLUEMARBLE
  bluemarble_init();
#endif
#ifdef VIK_CONFIG_GEONAMES
  geonames_init();
#endif
#ifdef VIK_CONFIG_SPOTMAPS
  spotmaps_init();
#endif

  /* As modules are loaded, we can load configuration files */
  modules_load_config ();
}

