#ifndef __VIKING_DEM_H
#define __VIKING_DEM_H

#define VIK_DEM_INVALID_ELEVATION -32768

/* unit codes */
#define VIK_DEM_HORIZ_UTM_METERS 2
#define VIK_DEM_HORIZ_LL_ARCSECONDS  3

#define VIK_DEM_VERT_DECIMETERS 2

#define VIK_DEM_VERT_METERS 1 /* wrong in 250k?	 */


typedef struct {
  guint n_columns;
  GPtrArray *columns;

  guint8 horiz_units;
  guint8 orig_vert_units; /* original, always converted to meters when loading. */
  gdouble east_scale; /* gap between samples */
  gdouble north_scale;

  gdouble min_east, min_north, max_east, max_north;

  guint8 utm_zone;
  gchar utm_letter;
} VikDEM;

typedef struct {
  /* east-west coordinate for ALL items in the column */
  gdouble east_west;

  /* coordinate of northern and southern boundaries */
  gdouble south;
//  gdouble north;

  guint n_points;
  gint16 *points;
} VikDEMColumn;


VikDEM *vik_dem_new_from_file(const gchar *file);
void vik_dem_free ( VikDEM *dem );
gint16 vik_dem_get_xy ( VikDEM *dem, guint x, guint y );

gint16 vik_dem_get_east_north ( VikDEM *dem, gdouble east, gdouble north );
gint16 vik_dem_get_simple_interpol ( VikDEM *dem, gdouble east, gdouble north );
gint16 vik_dem_get_shepard_interpol ( VikDEM *dem, gdouble east, gdouble north );
gint16 vik_dem_get_best_interpol ( VikDEM *dem, gdouble east, gdouble north );

void vik_dem_east_north_to_xy ( VikDEM *dem, gdouble east, gdouble north, guint *col, guint *row );

#endif
