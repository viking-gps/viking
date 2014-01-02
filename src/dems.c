/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2008, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
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
#include <glib.h>

#include "dems.h"
#include "background.h"

typedef struct {
  VikDEM *dem;
  guint ref_count;
} LoadedDEM;

GHashTable *loaded_dems = NULL;
/* filename -> DEM */

static void loaded_dem_free ( LoadedDEM *ldem )
{
  vik_dem_free ( ldem->dem );
  g_free ( ldem );
}

void a_dems_uninit ()
{
  if ( loaded_dems )
    g_hash_table_destroy ( loaded_dems );
}

/* To load a dem. if it was already loaded, will simply
 * reference the one already loaded and return it.
 */
VikDEM *a_dems_load(const gchar *filename)
{
  LoadedDEM *ldem;

  /* dems init hash table */
  if ( ! loaded_dems )
    loaded_dems = g_hash_table_new_full ( g_str_hash, g_str_equal, g_free, (GDestroyNotify) loaded_dem_free );

  ldem = (LoadedDEM *) g_hash_table_lookup ( loaded_dems, filename );
  if ( ldem ) {
    ldem->ref_count++;
    return ldem->dem;
  } else {
    VikDEM *dem = vik_dem_new_from_file ( filename );
    if ( ! dem )
      return NULL;
    ldem = g_malloc ( sizeof(LoadedDEM) );
    ldem->ref_count = 1;
    ldem->dem = dem;
    g_hash_table_insert ( loaded_dems, g_strdup(filename), ldem );
    return dem;
  }
}

void a_dems_unref(const gchar *filename)
{
  LoadedDEM *ldem = (LoadedDEM *) g_hash_table_lookup ( loaded_dems, filename );
  if ( !ldem ) {
    /* This is fine - probably means the loaded list was aborted / not completed for some reason */
    return;
  }
  ldem->ref_count--;
  if ( ldem->ref_count == 0 )
    g_hash_table_remove ( loaded_dems, filename );
}

/* to get a DEM that was already loaded.
 * assumes that its in there already,
 * although it could not be if earlier load failed.
 */
VikDEM *a_dems_get(const gchar *filename)
{
  LoadedDEM *ldem = g_hash_table_lookup ( loaded_dems, filename );
  if ( ldem )
    return ldem->dem;
  return NULL;
}


/* Load a string list (GList of strings) of dems. You have to use get to at them later.
 * When updating a list as a parameter, this should be bfore freeing the list so
 * the same DEMs won't be loaded & unloaded.
 * Modifies the list to remove DEMs which did not load.
 */

/* TODO: don't delete them when they don't exist.
 * we need to warn the user, but we should keep them in the GList.
 * we need to know that they weren't referenced though when we
 * do the a_dems_list_free().
 */
int a_dems_load_list ( GList **dems, gpointer threaddata )
{
  GList *iter = *dems;
  guint dem_count = 0;
  const guint dem_total = g_list_length ( *dems );
  while ( iter ) {
    if ( ! a_dems_load((const gchar *) (iter->data)) ) {
      GList *iter_temp = iter->next;
      g_free ( iter->data );
      (*dems) = g_list_remove_link ( (*dems), iter );
      iter = iter_temp;
    } else {
      iter = iter->next;
    }
    /* When running a thread - inform of progress */
    if ( threaddata ) {
      dem_count++;
      /* NB Progress also detects abort request via the returned value */
      int result = a_background_thread_progress ( threaddata, ((gdouble)dem_count) / dem_total );
      if ( result != 0 )
	return -1; /* Abort thread */
    }
  }
  return 0;
}

/* Takes a string list (GList of strings) of dems (filenames).
 * Unrefs all the dems (i.e. "unloads" them), then frees the
 * strings, the frees the list.
 */
void a_dems_list_free ( GList *dems )
{
  GList *iter = dems;
  while ( iter ) {
    a_dems_unref ((const gchar *)iter->data);
    g_free ( iter->data );
    iter = iter->next;
  }
  g_list_free ( dems );
}

GList *a_dems_list_copy ( GList *dems )
{
  GList *rv = g_list_copy ( dems );
  GList *iter = rv;
  while ( iter ) {
    if ( ! a_dems_load((const gchar *) (iter->data)) ) {
      GList *iter_temp = iter->next; /* delete link, don't bother strdup'ing and free'ing string */
      rv = g_list_remove_link ( rv, iter );
      iter = iter_temp;
    } else {
      iter->data = g_strdup((gchar *)iter->data); /* copy the string too. */
      iter = iter->next;
    }
  }
  return rv;
}

gint16 a_dems_list_get_elev_by_coord ( GList *dems, const VikCoord *coord )
{
  static struct UTM utm_tmp;
  static struct LatLon ll_tmp;
  GList *iter = dems;
  VikDEM *dem;
  gint elev;

  while ( iter ) {
    dem = a_dems_get ( (gchar *) iter->data );
    if ( dem ) {
      if ( dem->horiz_units == VIK_DEM_HORIZ_LL_ARCSECONDS ) {
        vik_coord_to_latlon ( coord, &ll_tmp );
        ll_tmp.lat *= 3600;
        ll_tmp.lon *= 3600;
        elev = vik_dem_get_east_north(dem, ll_tmp.lon, ll_tmp.lat);
        if ( elev != VIK_DEM_INVALID_ELEVATION )
          return elev;
      } else if ( dem->horiz_units == VIK_DEM_HORIZ_UTM_METERS ) {
        vik_coord_to_utm ( coord, &utm_tmp );
        if ( utm_tmp.zone == dem->utm_zone &&
             (elev = vik_dem_get_east_north(dem, utm_tmp.easting, utm_tmp.northing)) != VIK_DEM_INVALID_ELEVATION )
            return elev;
      }
    }
    iter = iter->next;
  }
  return VIK_DEM_INVALID_ELEVATION;
}

typedef struct {
  const VikCoord *coord;
  VikDemInterpol method;
  gint elev;
} CoordElev;

static gboolean get_elev_by_coord(gpointer key, LoadedDEM *ldem, CoordElev *ce)
{
  VikDEM *dem = ldem->dem;
  gdouble lat, lon;

  if ( dem->horiz_units == VIK_DEM_HORIZ_LL_ARCSECONDS ) {
    struct LatLon ll_tmp;
    vik_coord_to_latlon (ce->coord, &ll_tmp );
    lat = ll_tmp.lat * 3600;
    lon = ll_tmp.lon * 3600;
  } else if (dem->horiz_units == VIK_DEM_HORIZ_UTM_METERS) {
    static struct UTM utm_tmp;
    if (utm_tmp.zone != dem->utm_zone)
      return FALSE;
    vik_coord_to_utm (ce->coord, &utm_tmp);
    lat = utm_tmp.northing;
    lon = utm_tmp.easting;
  } else
    return FALSE;

  switch (ce->method) {
    case VIK_DEM_INTERPOL_NONE:
      ce->elev = vik_dem_get_east_north(dem, lon, lat);
      break;
    case VIK_DEM_INTERPOL_SIMPLE:
      ce->elev = vik_dem_get_simple_interpol(dem, lon, lat);
      break;
    case VIK_DEM_INTERPOL_BEST:
      ce->elev = vik_dem_get_shepard_interpol(dem, lon, lat);
      break;
    default: break;
  }
  return (ce->elev != VIK_DEM_INVALID_ELEVATION);
}

/* TODO: keep a (sorted) linked list of DEMs and select the best resolution one */
gint16 a_dems_get_elev_by_coord ( const VikCoord *coord, VikDemInterpol method )
{
  CoordElev ce;

  if (!loaded_dems)
    return VIK_DEM_INVALID_ELEVATION;

  ce.coord = coord;
  ce.method = method;
  ce.elev = VIK_DEM_INVALID_ELEVATION;

  if(!g_hash_table_find(loaded_dems, (GHRFunc)get_elev_by_coord, &ce))
    return VIK_DEM_INVALID_ELEVATION;
  return ce.elev;
}
