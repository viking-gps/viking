#ifndef __VIKING_DEMS_H
#define __VIKING_DEMS_H

#include "dem.h"
#include "vikcoord.h"

void a_dems_uninit ();
VikDEM *a_dems_load(const gchar *filename);
void a_dems_unref(const gchar *filename);
VikDEM *a_dems_get(const gchar *filename);
void a_dems_load_list ( GList **dems );
void a_dems_list_free ( GList *dems );
GList *a_dems_list_copy ( GList *dems );
gint16 a_dems_list_get_elev_by_coord ( GList *dems, const VikCoord *coord );

#endif
#include <glib.h>

