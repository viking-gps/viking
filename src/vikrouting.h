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
 *
 */
#ifndef _VIKING_ROUTING_H
#define _VIKING_ROUTING_H

#include <glib.h>

#include "vikroutingengine.h"

G_BEGIN_DECLS

/* Default */
gboolean vik_routing_default_find ( VikTrwLayer *vt, struct LatLon start, struct LatLon end );

/* Routing engines management */
void vik_routing_prefs_init();
void vik_routing_register( VikRoutingEngine *engine );
void vik_routing_unregister_all ();
void vik_routing_foreach_engine ( GFunc func, gpointer user_data );

/* UI */
typedef gboolean (*Predicate)( gpointer data, gpointer user_data );
GtkWidget *vik_routing_ui_selector_new ( Predicate func, gpointer user_data );
VikRoutingEngine *vik_routing_ui_selector_get_nth ( GtkWidget *combo, int pos );

/* Needs to be visible to display info about which routing engine is getting the route in viktrwlayer.c  */
VikRoutingEngine * vik_routing_default_engine ( void );


G_END_DECLS

#endif
