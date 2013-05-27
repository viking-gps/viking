/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2008, Evan Battaglia <gtoevan@gmx.net>
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
#ifndef __VIKING_DEMS_H
#define __VIKING_DEMS_H

#include "dem.h"
#include "vikcoord.h"

G_BEGIN_DECLS

typedef enum {
  VIK_DEM_INTERPOL_NONE = 0,
  VIK_DEM_INTERPOL_SIMPLE,
  VIK_DEM_INTERPOL_BEST,
} VikDemInterpol;

void a_dems_uninit ();
VikDEM *a_dems_load(const gchar *filename);
void a_dems_unref(const gchar *filename);
VikDEM *a_dems_get(const gchar *filename);
int a_dems_load_list ( GList **dems, gpointer threaddata );
void a_dems_list_free ( GList *dems );
GList *a_dems_list_copy ( GList *dems );
gint16 a_dems_list_get_elev_by_coord ( GList *dems, const VikCoord *coord );
gint16 a_dems_get_elev_by_coord ( const VikCoord *coord, VikDemInterpol method);

G_END_DECLS

#endif


