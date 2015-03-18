/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2006-2012, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
#include "google.h"
#include "terraserver.h"
#include "expedia.h"
#include "osm.h"
#include "osm-traces.h"
#include "bluemarble.h"
#include "geonames.h"
#include "dir.h"
#include "datasources.h"
#include "vikmapslayer.h"
#include "vikexttools.h"
#include "vikexttool_datasources.h"
#include "vikgoto.h"
#include "vikrouting.h"

/* Loadable types */
#include "vikslippymapsource.h"
#include "viktmsmapsource.h"
#include "vikwmscmapsource.h"
#include "vikwebtoolcenter.h"
#include "vikwebtoolbounds.h"
#include "vikgotoxmltool.h"
#include "vikwebtool_datasource.h"
#include "vikroutingwebengine.h"

#include "vikgobjectbuilder.h"

#ifdef HAVE_LIBMAPNIK
#include "vikmapniklayer.h"
#endif

#define VIKING_MAPS_FILE "maps.xml"
#define VIKING_EXTTOOLS_FILE "external_tools.xml"
#define VIKING_DATASOURCES_FILE "datasources.xml"
#define VIKING_GOTOTOOLS_FILE "goto_tools.xml"
#define VIKING_ROUTING_FILE "routing.xml"

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
modules_register_datasources(VikGobjectBuilder *self, GObject *object)
{
  g_debug (__FUNCTION__);
  VikExtTool *tool = VIK_EXT_TOOL (object);
  vik_ext_tool_datasources_register (tool);
}

static void
modules_register_gototools(VikGobjectBuilder *self, GObject *object)
{
  g_debug (__FUNCTION__);
  VikGotoTool *tool = VIK_GOTO_TOOL (object);
  vik_goto_register (tool);
}

static void
modules_register_routing_engine(VikGobjectBuilder *self, GObject *object)
{
  g_debug (__FUNCTION__);
  VikRoutingEngine *engine = VIK_ROUTING_ENGINE (object);
  vik_routing_register (engine);
}

static void
modules_load_config_dir(const gchar *dir)
{
  g_debug("Loading configurations from directory %s", dir);

  /* Maps sources */
  gchar *maps = g_build_filename(dir, VIKING_MAPS_FILE, NULL);
  if (g_access (maps, R_OK) == 0)
  {
	VikGobjectBuilder *builder = vik_gobject_builder_new ();
	g_signal_connect (builder, "new-object", G_CALLBACK (modules_register_map_source), NULL);
	vik_gobject_builder_parse (builder, maps);
	g_object_unref (builder);
  }
  g_free ( maps );

  /* External tools */
  gchar *tools = g_build_filename(dir, VIKING_EXTTOOLS_FILE, NULL);
  if (g_access (tools, R_OK) == 0)
  {
	VikGobjectBuilder *builder = vik_gobject_builder_new ();
	g_signal_connect (builder, "new-object", G_CALLBACK (modules_register_exttools), NULL);
	vik_gobject_builder_parse (builder, tools);
	g_object_unref (builder);
  }
  g_free ( tools );

  gchar *datasources = g_build_filename(dir, VIKING_DATASOURCES_FILE, NULL);
  if (g_access (datasources, R_OK) == 0)
  {
	VikGobjectBuilder *builder = vik_gobject_builder_new ();
	g_signal_connect (builder, "new-object", G_CALLBACK (modules_register_datasources), NULL);
	vik_gobject_builder_parse (builder, datasources);
	g_object_unref (builder);
  }
  g_free ( datasources );

  /* Go-to search engines */
  gchar *go_to = g_build_filename(dir, VIKING_GOTOTOOLS_FILE, NULL);
  if (g_access (go_to, R_OK) == 0)
  {
	VikGobjectBuilder *builder = vik_gobject_builder_new ();
	g_signal_connect (builder, "new-object", G_CALLBACK (modules_register_gototools), NULL);
	vik_gobject_builder_parse (builder, go_to);
	g_object_unref (builder);
  }
  g_free ( go_to );

  /* Routing engines */
  gchar *routing = g_build_filename(dir, VIKING_ROUTING_FILE, NULL);
  if (g_access (routing, R_OK) == 0)
  {
	VikGobjectBuilder *builder = vik_gobject_builder_new ();
	g_signal_connect (builder, "new-object", G_CALLBACK (modules_register_routing_engine), NULL);
	vik_gobject_builder_parse (builder, routing);
	g_object_unref (builder);
  }
  g_free ( routing );
}

static void
modules_load_config(void)
{
  /* Look in the directories of data path */
  gchar * * data_dirs = a_get_viking_data_path();
  /* Priority is standard one:
     left element is more important than right one.
     But our logic is to load all existing files and overwrite
     overlapping config with last recent one.
     So, we have to process directories in reverse order. */
  int nb_data_dirs = g_strv_length ( data_dirs );
  for (; nb_data_dirs > 0 ; nb_data_dirs--)
  {
    modules_load_config_dir(data_dirs[nb_data_dirs-1]);
  }
  g_strfreev(data_dirs);

  /* Check if system config is set */
  modules_load_config_dir(VIKING_SYSCONFDIR);

  const gchar *data_home = a_get_viking_data_home ();
  if (data_home)
  {
    modules_load_config_dir(data_home);
  }

  /* Check user's home config */
  modules_load_config_dir(a_get_viking_dir());
}

static void
register_loadable_types(void)
{
  /* Force registering of loadable types */
  volatile GType types[] = {
    /* Maps */
    VIK_TYPE_SLIPPY_MAP_SOURCE,
    VIK_TYPE_TMS_MAP_SOURCE,
    VIK_TYPE_WMSC_MAP_SOURCE,

    /* Goto */
    VIK_GOTO_XML_TOOL_TYPE,

    /* Tools */
    VIK_WEBTOOL_CENTER_TYPE,
    VIK_WEBTOOL_BOUNDS_TYPE,

    /* Datasource */
    VIK_WEBTOOL_DATASOURCE_TYPE,

    /* Routing */
    VIK_ROUTING_WEB_ENGINE_TYPE
  };

  /* kill 'unused variable' + argument type warnings */
  g_debug("%d types loaded", (int)sizeof(types)/(int)sizeof(GType));
}

/**
 * First stage initialization
 * Can not use a_get_preferences() yet...
 * See comment in main.c
 */
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
#ifdef VIK_CONFIG_GEOCACHES
  a_datasource_gc_init();
#endif

#ifdef HAVE_LIBMAPNIK
  vik_mapnik_layer_init();
#endif

  register_loadable_types ();

  /* As modules are loaded, we can load configuration files */
  modules_load_config ();
}

/**
 * modules_post_init:
 *
 * Secondary stage initialization
 * Can now use a_get_preferences()
 */
void modules_post_init ()
{
#ifdef HAVE_LIBMAPNIK
  vik_mapnik_layer_post_init();
#endif
}

/**
 *
 */
void modules_uninit()
{
#ifdef VIK_CONFIG_OPENSTREETMAP
  osm_traces_uninit();
#endif
#ifdef HAVE_LIBMAPNIK
  vik_mapnik_layer_uninit();
#endif
}
