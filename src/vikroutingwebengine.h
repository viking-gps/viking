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
#ifndef _WEB_ROUTING_H
#define _WEB_ROUTING_H

#include <glib.h>

#include "vikroutingengine.h"

G_BEGIN_DECLS

#define VIK_ROUTING_WEB_ENGINE_TYPE            (vik_routing_web_engine_get_type ())
#define VIK_ROUTING_WEB_ENGINE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_ROUTING_WEB_ENGINE_TYPE, VikRoutingWebEngine))
#define VIK_ROUTING_WEB_ENGINE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_ROUTING_WEB_ENGINE_TYPE, VikRoutingWebEngineClass))
#define VIK_IS_ROUTING_WEB_ENGINE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_ROUTING_WEB_ENGINE_TYPE))
#define VIK_IS_ROUTING_WEB_ENGINE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_ROUTING_WEB_ENGINE_TYPE))
#define VIK_ROUTING_WEB_ENGINE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VIK_ROUTING_WEB_ENGINE_TYPE, VikRoutingWebEngineClass))


typedef struct _VikRoutingWebEngine VikRoutingWebEngine;
typedef struct _VikRoutingWebEngineClass VikRoutingWebEngineClass;

struct _VikRoutingWebEngineClass
{
  VikRoutingEngineClass object_class;
};

GType vik_routing_web_engine_get_type ();

struct _VikRoutingWebEngine {
  VikRoutingEngine obj;
};

G_END_DECLS

#endif
