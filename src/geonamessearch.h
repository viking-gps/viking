/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2009, Hein Ragas
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
#ifndef __VIK_GEONAMESSEARCH_H
#define __VIK_GEONAMESSEARCH_H

#include "viking.h"

G_BEGIN_DECLS

/* Finding Wikipedia entries within a certain box */
void a_geonames_wikipedia_box ( VikWindow *vw, VikTrwLayer *vtl, struct LatLon maxmin[2] );

G_END_DECLS

#endif
