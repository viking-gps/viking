#include <glib.h>

#include "dems.h"

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
  g_assert ( ldem );
  ldem->ref_count --;
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
void a_dems_load_list ( GList **dems )
{
  GList *iter = *dems;
  while ( iter ) {
    if ( ! a_dems_load((const gchar *) (iter->data)) ) {
      GList *iter_temp = iter->next;
      g_free ( iter->data );
      (*dems) = g_list_remove_link ( (*dems), iter );
      iter = iter_temp;
    } else {
      iter = iter->next;
    }
  }
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
