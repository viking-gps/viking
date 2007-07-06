#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <math.h>
#include <stdlib.h>

#include "dem.h"
#include "file.h"

#define DEM_BLOCK_SIZE 1024
#define GET_COLUMN(dem,n) ((VikDEMColumn *)g_ptr_array_index( (dem)->columns, (n) ))

static gboolean get_double_and_continue ( gchar **buffer, gdouble *tmp, gboolean warn )
{
  gchar *endptr;
  *tmp = g_strtod(*buffer, &endptr);
  if ( endptr == NULL|| endptr == *buffer ) {
    if ( warn )
      g_warning("Invalid DEM");
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
      g_warning("Invalid DEM");
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
      g_warning ("Invalid DEM header");
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
    g_warning("Incorrect DEM Class B record: expected 1");
    return;
  }

  /* don't need this */
  if ( !get_double_and_continue(&buffer, &tmp, TRUE ) ) return;

  /* n_rows */
  if ( !get_double_and_continue(&buffer, &tmp, TRUE ) )
    return;
  n_rows = (guint) tmp;

  if ( (!get_double_and_continue(&buffer, &tmp, TRUE)) || tmp != 1 ) {
    g_warning("Incorrect DEM Class B record: expected 1");
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

static VikDEM *vik_dem_read_srtm_hgt(FILE *f, const gchar *basename)
{
  gint16 elev;
  gint i, j;
  VikDEM *dem;


  dem = g_malloc(sizeof(VikDEM));

  dem->horiz_units = VIK_DEM_HORIZ_LL_ARCSECONDS;
  dem->orig_vert_units = VIK_DEM_VERT_DECIMETERS;
  dem->east_scale = dem->north_scale = 3;

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

  for ( i = 0; i < 1201; i++ ) {
    dem->n_columns++;
    g_ptr_array_add ( dem->columns, g_malloc(sizeof(VikDEMColumn)) );
    GET_COLUMN(dem,i)->east_west = dem->min_east + 3*i;
    GET_COLUMN(dem,i)->south = dem->min_north;
    GET_COLUMN(dem,i)->n_points = 1201;
    GET_COLUMN(dem,i)->points = g_malloc(sizeof(gint16)*1201);
  }

  for ( i = 1200; i >= 0; i-- ) {
    for ( j = 0; j < 1201; j++ ) {
      if ( feof(f) ) {
        g_warning("Incorrect SRTM file: unexpected EOF");
        g_print("%d %d\n", i, j);
        return dem;
      }
      fread(&elev, sizeof(elev), 1, f);
      gchar *x = (gchar *) &elev;
      gchar tmp;
      tmp=x[0];
      x[0]=x[1];
      x[1]=tmp;
      GET_COLUMN(dem,j)->points[i] = elev;
    }

  }

  return dem;
}

#define IS_SRTM_HGT(fn) (strlen((fn))==11 && (fn)[7]=='.' && (fn)[8]=='h' && (fn)[9]=='g' && (fn)[10]=='t' && ((fn)[0]=='N' || (fn)[0]=='S') && ((fn)[3]=='E' || (fn)[3]=='W'))

VikDEM *vik_dem_new_from_file(const gchar *file)
{
  FILE *f;
  VikDEM *rv;
  gchar buffer[DEM_BLOCK_SIZE+1];

  /* use to record state for dem_parse_block */
  gint cur_column = -1;
  gint cur_row = -1;
  const gchar *basename = a_file_basename(file);

      /* FILE IO */
  f = fopen(file, "r");
  if ( !f )
    return NULL;

  if ( strlen(basename)==11 && basename[7]=='.' && basename[8]=='h' && basename[9]=='g' && basename[10]=='t' &&
       (basename[0] == 'N' || basename[0] == 'S') && (basename[3] == 'E' || basename[3] =='W'))
    return vik_dem_read_srtm_hgt(f, basename);

      /* Create Structure */
  rv = g_malloc(sizeof(VikDEM));

      /* Header */
  buffer[fread(buffer, 1, DEM_BLOCK_SIZE, f)] = '\0';
  if ( ! dem_parse_header ( buffer, rv ) ) {
    g_free ( rv );
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

void vik_dem_east_north_to_xy ( VikDEM *dem, gdouble east, gdouble north, guint *col, guint *row )
{
  *col = (gint) floor((east - dem->min_east) / dem->east_scale);
  *row = (gint) floor((north - dem->min_north) / dem->north_scale);
  if ( *col < 0 ) *col = 0;
  if ( *row < 0 ) *row = 0;
}

