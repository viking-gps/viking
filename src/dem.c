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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <math.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "compression.h"
#include "dem.h"
#include "coords.h"
#include "fileutils.h"
#include "file_magic.h"

#define DEM_BLOCK_SIZE 1024
#define GET_COLUMN(dem,n) ((VikDEMColumn *)g_ptr_array_index( (dem)->columns, (n) ))

static gboolean get_double_and_continue ( gchar **buffer, gdouble *tmp, gboolean warn )
{
  gchar *endptr;
  *tmp = g_strtod(*buffer, &endptr);
  if ( endptr == NULL|| endptr == *buffer ) {
    if ( warn )
      g_warning(_("Invalid DEM"));
    return FALSE;
  }
  *buffer=endptr;
  return TRUE;
}


static gboolean get_int_and_continue ( gchar **buffer, gint *tmp, gboolean warn )
{
  gchar *endptr;
  *tmp = strtol(*buffer, &endptr, 10);
  if ( endptr == NULL|| endptr == *buffer ) {
    if ( warn )
      g_warning(_("Invalid DEM"));
    return FALSE;
  }
  *buffer=endptr;
  return TRUE;
}

static gboolean dem_parse_header ( gchar *buffer, VikDEM *dem )
{
  gdouble val;
  gint int_val;
  guint i;
  gchar *tmp = buffer;

  /* incomplete header */
  if ( strlen(buffer) != 1024 )
    return FALSE;

  /* fix Fortran-style exponentiation 1.0D5 -> 1.0E5 */
  while (*tmp) {
    if ( *tmp=='D')
      *tmp='E';
    tmp++;
  }

  /* skip name */
  buffer += 149;

  /* "DEM level code, pattern code, palaimetric reference system code" -- skip */
  get_int_and_continue(&buffer, &int_val, TRUE);
  get_int_and_continue(&buffer, &int_val, TRUE);
  get_int_and_continue(&buffer, &int_val, TRUE);

  /* zone */
  get_int_and_continue(&buffer, &int_val, TRUE);
  dem->utm_zone = int_val;
  /* TODO -- southern or northern hemisphere?! */
  dem->utm_letter = 'N';

  /* skip numbers 5-19  */
  for ( i = 0; i < 15; i++ ) {
    if ( ! get_double_and_continue(&buffer, &val, FALSE) ) {
      g_warning (_("Invalid DEM header"));
      return FALSE;
    }
  }

  /* number 20 -- horizontal unit code (utm/ll) */
  get_double_and_continue(&buffer, &val, TRUE);
  dem->horiz_units = val;
  get_double_and_continue(&buffer, &val, TRUE);
  /* dem->orig_vert_units = val; now done below */

  /* TODO: do this for real. these are only for 1:24k and 1:250k USGS */
  if ( dem->horiz_units == VIK_DEM_HORIZ_UTM_METERS ) {
    dem->east_scale = 10.0; /* meters */
    dem->north_scale = 10.0;
    dem->orig_vert_units = VIK_DEM_VERT_DECIMETERS;
  } else {
    dem->east_scale = 3.0; /* arcseconds */
    dem->north_scale = 3.0;
    dem->orig_vert_units = VIK_DEM_VERT_METERS;
  }

  /* skip next */
  get_double_and_continue(&buffer, &val, TRUE);

  /* now we get the four corner points. record the min and max. */
  get_double_and_continue(&buffer, &val, TRUE);
  dem->min_east = dem->max_east = val;
  get_double_and_continue(&buffer, &val, TRUE);
  dem->min_north = dem->max_north = val;

  for ( i = 0; i < 3; i++ ) {
    get_double_and_continue(&buffer, &val, TRUE);
    if ( val < dem->min_east ) dem->min_east = val;
    if ( val > dem->max_east ) dem->max_east = val;
    get_double_and_continue(&buffer, &val, TRUE);
    if ( val < dem->min_north ) dem->min_north = val;
    if ( val > dem->max_north ) dem->max_north = val;
  }

  return TRUE;
}

static void dem_parse_block_as_cont ( gchar *buffer, VikDEM *dem, gint *cur_column, gint *cur_row )
{
  gint tmp;
  while ( *cur_row < GET_COLUMN(dem, *cur_column)->n_points ) {
    if ( get_int_and_continue(&buffer, &tmp,FALSE) ) {
      if ( dem->orig_vert_units == VIK_DEM_VERT_DECIMETERS )
        GET_COLUMN(dem, *cur_column)->points[*cur_row] = (gint16) (tmp / 10);
      else
        GET_COLUMN(dem, *cur_column)->points[*cur_row] = (gint16) tmp;
    } else
      return;
    (*cur_row)++;
  }
  *cur_row = -1; /* expecting new column */
}

static void dem_parse_block_as_header ( gchar *buffer, VikDEM *dem, gint *cur_column, gint *cur_row )
{
  guint n_rows;
  gint i;
  gdouble east_west, south;
  gdouble tmp;

  /* 1 x n_rows 1 east_west south x x x DATA */

  if ( (!get_double_and_continue(&buffer, &tmp, TRUE)) || tmp != 1 ) {
    g_warning(_("Incorrect DEM Class B record: expected 1"));
    return;
  }

  /* don't need this */
  if ( !get_double_and_continue(&buffer, &tmp, TRUE ) ) return;

  /* n_rows */
  if ( !get_double_and_continue(&buffer, &tmp, TRUE ) )
    return;
  n_rows = (guint) tmp;

  if ( (!get_double_and_continue(&buffer, &tmp, TRUE)) || tmp != 1 ) {
    g_warning(_("Incorrect DEM Class B record: expected 1"));
    return;
  }

  if ( !get_double_and_continue(&buffer, &east_west, TRUE) )
    return;
  if ( !get_double_and_continue(&buffer, &south, TRUE) )
    return;

  /* next three things we don't need */
  if ( !get_double_and_continue(&buffer, &tmp, TRUE)) return;
  if ( !get_double_and_continue(&buffer, &tmp, TRUE)) return;
  if ( !get_double_and_continue(&buffer, &tmp, TRUE)) return;


  dem->n_columns ++;
  (*cur_column) ++;

  /* empty spaces for things before that were skipped */
  (*cur_row) = (south - dem->min_north) / dem->north_scale;
  if ( south > dem->max_north || (*cur_row) < 0 )
    (*cur_row) = 0;

  n_rows += *cur_row;

  g_ptr_array_add ( dem->columns, g_malloc(sizeof(VikDEMColumn)) );
  GET_COLUMN(dem,*cur_column)->east_west = east_west;
  GET_COLUMN(dem,*cur_column)->south = south;
  GET_COLUMN(dem,*cur_column)->n_points = n_rows;
  GET_COLUMN(dem,*cur_column)->points = g_malloc(sizeof(gint16)*n_rows);

  /* no information for things before that */
  for ( i = 0; i < (*cur_row); i++ )
    GET_COLUMN(dem,*cur_column)->points[i] = VIK_DEM_INVALID_ELEVATION;

  /* now just continue */
  dem_parse_block_as_cont ( buffer, dem, cur_column, cur_row );


}

static void dem_parse_block ( gchar *buffer, VikDEM *dem, gint *cur_column, gint *cur_row )
{
  /* if haven't read anything or have read all items in a columns and are expecting a new column */
  if ( *cur_column == -1 || *cur_row == -1 ) {
    dem_parse_block_as_header(buffer, dem, cur_column, cur_row);
  } else {
    dem_parse_block_as_cont(buffer, dem, cur_column, cur_row);
  }
}

static VikDEM *vik_dem_read_srtm_hgt(const gchar *file_name, const gchar *basename, gboolean zip)
{
  gint i, j;
  VikDEM *dem;
  off_t file_size;
  gint16 *dem_mem = NULL;
  gchar *dem_file = NULL;
  const gint num_rows_3sec = 1201;
  const gint num_rows_1sec = 3601;
  gint num_rows;
  GMappedFile *mf;
  gint arcsec;
  GError *error = NULL;

  dem = g_malloc(sizeof(VikDEM));

  dem->horiz_units = VIK_DEM_HORIZ_LL_ARCSECONDS;
  dem->orig_vert_units = VIK_DEM_VERT_DECIMETERS;

  /* TODO */
  dem->min_north = atoi(basename+1) * 3600;
  dem->min_east = atoi(basename+4) * 3600;
  if ( basename[0] == 'S' )
    dem->min_north = - dem->min_north;
  if ( basename[3] == 'W' )
    dem->min_east = - dem->min_east;

  dem->max_north = 3600 + dem->min_north;
  dem->max_east = 3600 + dem->min_east;

  dem->columns = g_ptr_array_new();
  dem->n_columns = 0;

  if ((mf = g_mapped_file_new(file_name, FALSE, &error)) == NULL) {
    g_critical(_("Couldn't map file %s: %s"), file_name, error->message);
    g_error_free(error);
    g_free(dem);
    return NULL;
  }
  file_size = g_mapped_file_get_length(mf);
  dem_file = g_mapped_file_get_contents(mf);

  if (zip) {
    void *unzip_mem = NULL;
    gulong ucsize;

    if ((unzip_mem = unzip_file(dem_file, &ucsize)) == NULL) {
      g_mapped_file_unref(mf);
      g_ptr_array_foreach ( dem->columns, (GFunc)g_free, NULL );
      g_ptr_array_free(dem->columns, TRUE);
      g_free(dem);
      return NULL;
    }

    dem_mem = unzip_mem;
    file_size = ucsize;
  }
  else
    dem_mem = (gint16 *)dem_file;

  if (file_size == (num_rows_3sec * num_rows_3sec * sizeof(gint16)))
    arcsec = 3;
  else if (file_size == (num_rows_1sec * num_rows_1sec * sizeof(gint16)))
    arcsec = 1;
  else {
    g_warning("%s(): file %s does not have right size", __PRETTY_FUNCTION__, basename);
    g_mapped_file_unref(mf);
    g_free(dem);
    return NULL;
  }

  num_rows = (arcsec == 3) ? num_rows_3sec : num_rows_1sec;
  dem->east_scale = dem->north_scale = arcsec;

  for ( i = 0; i < num_rows; i++ ) {
    dem->n_columns++;
    g_ptr_array_add ( dem->columns, g_malloc(sizeof(VikDEMColumn)) );
    GET_COLUMN(dem,i)->east_west = dem->min_east + arcsec*i;
    GET_COLUMN(dem,i)->south = dem->min_north;
    GET_COLUMN(dem,i)->n_points = num_rows;
    GET_COLUMN(dem,i)->points = g_malloc(sizeof(gint16)*num_rows);
  }

  int ent = 0;
  for ( i = (num_rows - 1); i >= 0; i-- ) {
    for ( j = 0; j < num_rows; j++ ) {
      GET_COLUMN(dem,j)->points[i] = GINT16_FROM_BE(dem_mem[ent]);
      ent++;
    }

  }

  if (zip)
    g_free(dem_mem);
  g_mapped_file_unref(mf);
  return dem;
}

VikDEM *vik_dem_new_from_file(const gchar *file)
{
  FILE *f=NULL;
  VikDEM *rv;
  gchar buffer[DEM_BLOCK_SIZE+1];

  /* use to record state for dem_parse_block */
  gint cur_column = -1;
  gint cur_row = -1;
  const gchar *basename = a_file_basename(file);

  if ( g_access ( file, R_OK ) != 0 )
    return NULL;

  // Not entirely sure the point of enforcing filenames must have this convention
  if ( strlen(basename) > 4 &&
       (basename[0] == 'N' || basename[0] == 'S') && (basename[3] == 'E' || basename[3] =='W') &&
       g_strrstr(file, "hgt") ) {
    gboolean is_zip_file = file_magic_check ( file, "application/zip", ".zip" );
    rv = vik_dem_read_srtm_hgt(file, basename, is_zip_file);
    return(rv);
  }

      /* Create Structure */
  rv = g_malloc(sizeof(VikDEM));

      /* Header */
  f = g_fopen(file, "r");
  if ( !f ) {
    g_free ( rv );
    return NULL;
  }
  buffer[fread(buffer, 1, DEM_BLOCK_SIZE, f)] = '\0';
  if ( ! dem_parse_header ( buffer, rv ) ) {
    g_free ( rv );
    fclose(f);
    return NULL;
  }
  /* TODO: actually use header -- i.e. GET # OF COLUMNS EXPECTED */

  rv->columns = g_ptr_array_new();
  rv->n_columns = 0;

      /* Column -- Data */
  while (! feof(f) ) {
     gchar *tmp;

     /* read block */
     buffer[fread(buffer, 1, DEM_BLOCK_SIZE, f)] = '\0';

     /* Fix Fortran-style exponentiation */
     tmp = buffer;
     while (*tmp) {
       if ( *tmp=='D')
         *tmp='E';
       tmp++;
     }

     dem_parse_block(buffer, rv, &cur_column, &cur_row);
  }

     /* TODO - class C records (right now says 'Invalid' and dies) */

  fclose(f);
  f = NULL;

  /* 24k scale */
  if ( rv->horiz_units == VIK_DEM_HORIZ_UTM_METERS && rv->n_columns >= 2 )
    rv->north_scale = rv->east_scale = GET_COLUMN(rv, 1)->east_west - GET_COLUMN(rv,0)->east_west;

  /* FIXME bug in 10m DEM's */
  if ( rv->horiz_units == VIK_DEM_HORIZ_UTM_METERS && rv->north_scale == 10 ) {
    rv->min_east -= 100;
    rv->min_north += 200;
  }


  return rv;
}

void vik_dem_free ( VikDEM *dem )
{
  guint i;
  for ( i = 0; i < dem->n_columns; i++)
    g_free ( GET_COLUMN(dem, i)->points );
  g_ptr_array_foreach ( dem->columns, (GFunc)g_free, NULL );
  g_ptr_array_free ( dem->columns, TRUE );
  g_free ( dem );
}

gint16 vik_dem_get_xy ( VikDEM *dem, guint col, guint row )
{
  if ( col < dem->n_columns )
    if ( row < GET_COLUMN(dem, col)->n_points )
      return GET_COLUMN(dem, col)->points[row];
  return VIK_DEM_INVALID_ELEVATION;
}

gint16 vik_dem_get_east_north ( VikDEM *dem, gdouble east, gdouble north )
{
  gint col, row;

  if ( east > dem->max_east || east < dem->min_east ||
      north > dem->max_north || north < dem->min_north )
    return VIK_DEM_INVALID_ELEVATION;

  col = (gint) floor((east - dem->min_east) / dem->east_scale);
  row = (gint) floor((north - dem->min_north) / dem->north_scale);

  return vik_dem_get_xy ( dem, col, row );
}

static gboolean dem_get_ref_points_elev_dist(VikDEM *dem,
    gdouble east, gdouble north, /* in seconds */
    gint16 *elevs, gint16 *dists)
{
  int i;
  int cols[4], rows[4];
  struct LatLon ll[4];
  struct LatLon pos;

  if ( east > dem->max_east || east < dem->min_east ||
      north > dem->max_north || north < dem->min_north )
    return FALSE;  /* got nothing */

  pos.lon = east/3600;
  pos.lat = north/3600;

  /* order of the data: sw, nw, ne, se */
  /* sw */
  cols[0] = (gint) floor((east - dem->min_east) / dem->east_scale);
  rows[0] = (gint) floor((north - dem->min_north) / dem->north_scale);
  ll[0].lon = (dem->min_east + dem->east_scale*cols[0])/3600;
  ll[0].lat = (dem->min_north + dem->north_scale*rows[0])/3600;
  /* nw */
  cols[1] = cols[0];
  rows[1] = rows[0] + 1;
  ll[1].lon = ll[0].lon;
  ll[1].lat = ll[0].lat + (gdouble)dem->north_scale/3600;
  /* ne */
  cols[2] = cols[0] + 1;
  rows[2] = rows[0] + 1;
  ll[2].lon = ll[0].lon + (gdouble)dem->east_scale/3600;
  ll[2].lat = ll[0].lat + (gdouble)dem->north_scale/3600;
  /* se */
  cols[3] = cols[0] + 1;
  rows[3] = rows[0];
  ll[3].lon = ll[0].lon + (gdouble)dem->east_scale/3600;
  ll[3].lat = ll[0].lat;

  for (i = 0; i < 4; i++) {
    if ((elevs[i] = vik_dem_get_xy(dem, cols[i], rows[i])) == VIK_DEM_INVALID_ELEVATION)
      return FALSE;
    dists[i] = a_coords_latlon_diff(&pos, &ll[i]);
  }

#if 0  /* debug */
  for (i = 0; i < 4; i++)
    fprintf(stderr, "%f:%f:%d:%d  ", ll[i].lat, ll[i].lon, dists[i], elevs[i]);
  fprintf(stderr, "   north_scale=%f\n", dem->north_scale);
#endif

  return TRUE;  /* all OK */
}

gint16 vik_dem_get_simple_interpol ( VikDEM *dem, gdouble east, gdouble north )
{
  int i;
  gint16 elevs[4], dists[4];

  if (!dem_get_ref_points_elev_dist(dem, east, north, elevs, dists))
    return VIK_DEM_INVALID_ELEVATION;

  for (i = 0; i < 4; i++) {
    if (dists[i] < 1) {
      return(elevs[i]);
    }
  }

  gdouble t = (gdouble)elevs[0]/dists[0] + (gdouble)elevs[1]/dists[1] + (gdouble)elevs[2]/dists[2] + (gdouble)elevs[3]/dists[3];
  gdouble b = 1.0/dists[0] + 1.0/dists[1] + 1.0/dists[2] + 1.0/dists[3];

  return(t/b);
}

gint16 vik_dem_get_shepard_interpol ( VikDEM *dem, gdouble east, gdouble north )
{
  int i;
  gint16 elevs[4], dists[4];
  gint16 max_dist;
  gdouble t = 0.0;
  gdouble b = 0.0;

  if (!dem_get_ref_points_elev_dist(dem, east, north, elevs, dists))
    return VIK_DEM_INVALID_ELEVATION;

  max_dist = 0;
  for (i = 0; i < 4; i++) {
    if (dists[i] < 1) {
      return(elevs[i]);
    }
    if (dists[i] > max_dist)
      max_dist = dists[i];
  }

  gdouble tmp;
#if 0 /* derived method by Franke & Nielson. Does not seem to work too well here */
  for (i = 0; i < 4; i++) {
    tmp = pow((1.0*(max_dist - dists[i])/max_dist*dists[i]), 2);
    t += tmp*elevs[i];
    b += tmp;
  }
#endif

  for (i = 0; i < 4; i++) {
    tmp = pow((1.0/dists[i]), 2);
    t += tmp*elevs[i];
    b += tmp;
  }

  // fprintf(stderr, "DEBUG: tmp=%f t=%f b=%f %f\n", tmp, t, b, t/b);

  return(t/b);

}

void vik_dem_east_north_to_xy ( VikDEM *dem, gdouble east, gdouble north, guint *col, guint *row )
{
  *col = (guint) floor((east - dem->min_east) / dem->east_scale);
  *row = (guint) floor((north - dem->min_north) / dem->north_scale);
}

/**
 *
 */
LatLonBBox vik_dem_get_bbox ( const VikDEM *dem )
{
  LatLonBBox bbox = {0.0, 0.0, 0.0, 0.0};

  if ( dem->horiz_units == VIK_DEM_HORIZ_LL_ARCSECONDS ) {
    bbox.north = dem->max_north / 3600.0;
    bbox.east = dem->max_east / 3600.0;
    bbox.south = dem->min_north / 3600.0;
    bbox.west = dem->min_east / 3600.0;
  } else if ( dem->horiz_units == VIK_DEM_HORIZ_UTM_METERS ) {
    struct UTM dem_northeast_utm, dem_southwest_utm;
    dem_northeast_utm.northing = dem->max_north;
    dem_northeast_utm.easting = dem->max_east;
    dem_southwest_utm.northing = dem->min_north;
    dem_southwest_utm.easting = dem->min_east;
    dem_northeast_utm.zone = dem_southwest_utm.zone = dem->utm_zone;
    dem_northeast_utm.letter = dem_southwest_utm.letter = dem->utm_letter;

    struct LatLon dem_northeast, dem_southwest;
    a_coords_utm_to_latlon(&dem_northeast_utm, &dem_northeast);
    a_coords_utm_to_latlon(&dem_southwest_utm, &dem_southwest);

    bbox.north = dem_northeast.lat;
    bbox.east  = dem_northeast.lon;
    bbox.south = dem_southwest.lat;
    bbox.west  = dem_southwest.lon;
  }
  return bbox;
}
