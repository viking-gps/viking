/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2006-2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#include "modules.h"

#include "google.h"
#include "terraserver.h"
#include "expedia.h"
#include "osm.h"
#include "osm-traces.h"

void modules_init()
{
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
}

