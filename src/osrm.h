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
#ifndef _OSRM_ROUTING_H
#define _OSRM_ROUTING_H

#include <glib.h>

#include "vikroutingengine.h"

G_BEGIN_DECLS

#define OSRM_ROUTING_TYPE            (osrm_routing_get_type ())
#define OSRM_ROUTING(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OSRM_ROUTING_TYPE, OsrmRouting))
#define OSRM_ROUTING_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OSRM_ROUTING_TYPE, OsrmRoutingClass))
#define OSRM_IS_ROUTING(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OSRM_ROUTING_TYPE))
#define OSRM_IS_ROUTING_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OSRM_ROUTING_TYPE))
#define OSRM_ROUTING_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OSRM_ROUTING_TYPE, OsrmRoutingClass))


typedef struct _OsrmRouting OsrmRouting;
typedef struct _OsrmRoutingClass OsrmRoutingClass;

struct _OsrmRoutingClass
{
  VikRoutingEngineClass object_class;
};

GType osrm_routing_get_type ();

struct _OsrmRouting {
  VikRoutingEngine obj;
};

OsrmRouting *osrm_routing_new ();

G_END_DECLS

#endif
