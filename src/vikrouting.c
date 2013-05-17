/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2012-2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
 */

/**
 * SECTION:vikrouting
 * @short_description: the routing framework
 * 
 * This module handles the list of #VikRoutingEngine.
 * It also handles the "default" functions.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "curl_download.h"
#include "babel.h"

#include "preferences.h"

#include "vikrouting.h"
#include "vikroutingengine.h"

/* params will be routing.default */
/* we have to make sure these don't collide. */
#define VIKING_ROUTING_PARAMS_GROUP_KEY "routing"
#define VIKING_ROUTING_PARAMS_NAMESPACE "routing."

/* List to register all routing engines */
static GList *routing_engine_list = NULL;

static VikLayerParam prefs[] = {
  { VIK_LAYER_NUM_TYPES, VIKING_ROUTING_PARAMS_NAMESPACE "default", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("Default engine:"), VIK_LAYER_WIDGET_COMBOBOX, NULL, NULL, NULL },
};

gchar **routing_engine_labels = NULL;
gchar **routing_engine_ids = NULL;

/**
 * vik_routing_prefs_init:
 * 
 * Initialize the preferences of the routing feature.
 */
void
vik_routing_prefs_init()
{
  a_preferences_register_group ( VIKING_ROUTING_PARAMS_GROUP_KEY, "Routing" );

  VikLayerParamData tmp;
  tmp.s = NULL;
  a_preferences_register(prefs, tmp, VIKING_ROUTING_PARAMS_GROUP_KEY);
}

static gint
search_by_id (gconstpointer a,
              gconstpointer b)
{
	const gchar *id = b;
	VikRoutingEngine *engine = (VikRoutingEngine *)a;
	gchar *engineId = vik_routing_engine_get_id (engine);
	if (id && engine)
		return strcmp(id, engineId);
	else
		return -1;
}

/**
 * vik_routing_find_engine:
 * @id: the id of the engine we are looking for
 * 
 * Returns: the found engine or %NULL
 */
VikRoutingEngine *
vik_routing_find_engine ( const gchar *id )
{
	VikRoutingEngine *engine = NULL;
	GList *elem = g_list_find_custom (routing_engine_list, id, search_by_id);
	if (elem)
		engine = elem->data;
	return engine;
}

/**
 * vik_routing_default:
 * 
 * Retrieve the default engine, based on user's preferences.
 * 
 * Returns: the default engine
 */
static VikRoutingEngine *
vik_routing_default( void )
{
  const gchar *id = a_preferences_get ( VIKING_ROUTING_PARAMS_NAMESPACE "default")->s;
  VikRoutingEngine *engine = vik_routing_find_engine(id);
  if (engine == NULL && routing_engine_list != NULL && g_list_first (routing_engine_list) != NULL)
    /* Fallback to first element */
    engine = g_list_first (routing_engine_list)->data;

  return engine;
}

/**
 * vik_routing_default_find:
 * 
 * Route computation with default engine.
 */
void
vik_routing_default_find(VikTrwLayer *vt, struct LatLon start, struct LatLon end)
{
  /* The engine */
  VikRoutingEngine *engine = vik_routing_default ( );
  /* The route computation */
  vik_routing_engine_find ( engine, vt, start, end );
}

/**
 * vik_routing_register:
 * @engine: new routing engine to register
 * 
 * Register a new routing engine.
 */
void
vik_routing_register( VikRoutingEngine *engine )
{
  gchar *label = vik_routing_engine_get_label ( engine );
  gchar *id = vik_routing_engine_get_id ( engine );
  gsize len = 0;

  /* check if id already exists in list */
  GList *elem = g_list_find_custom (routing_engine_list, id, search_by_id);
  if (elem != NULL) {
    g_debug("%s: %s already exists: update", __FUNCTION__, id);

    /* Update main list */
    g_object_unref (elem->data);
    elem->data = g_object_ref ( engine );

    /* Update GUI arrays */
    len = g_strv_length (routing_engine_labels);
    for (; len > 0 ; len--) {
      if (strcmp (routing_engine_ids[len-1], id) == 0)
			break;
	}
    /* Update the label (possibly different */
    g_free (routing_engine_labels[len-1]);
    routing_engine_labels[len-1] = g_strdup (label);
    
  } else {
    g_debug("%s: %s is new: append", __FUNCTION__, id);
    routing_engine_list = g_list_append ( routing_engine_list, g_object_ref ( engine ) );

    if (routing_engine_labels)
      len = g_strv_length (routing_engine_labels);
  
    /* Add the label */
    routing_engine_labels = g_realloc (routing_engine_labels, (len+2)*sizeof(gchar*));
    routing_engine_labels[len] = g_strdup (label);
    routing_engine_labels[len+1] = NULL;

    /* Add the id */
    routing_engine_ids = g_realloc (routing_engine_ids, (len+2)*sizeof(gchar*));
    routing_engine_ids[len] = g_strdup (id);
    routing_engine_ids[len+1] = NULL;
  
    /* Hack
       We have to ensure the mode LayerParam references the up-to-date
       GLists.
    */
    /*
    memcpy(&maps_layer_params[0].widget_data, &params_maptypes, sizeof(gpointer));
    memcpy(&maps_layer_params[0].extra_widget_data, &params_maptypes_ids, sizeof(gpointer));
    */
    prefs[0].widget_data = routing_engine_labels;
    prefs[0].extra_widget_data = routing_engine_ids;
  }
}

/**
 * vik_routing_unregister_all:
 * 
 * Unregister all registered routing engines.
 */
void
vik_routing_unregister_all ()
{
  g_list_foreach ( routing_engine_list, (GFunc) g_object_unref, NULL );
  g_strfreev ( routing_engine_labels );
  g_strfreev ( routing_engine_ids );
}
