/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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
#include <string.h>
#include <math.h>
#include "coords.h"
#include "vikcoord.h"
#include "vikwaypoint.h"
#include "globals.h"
#include "garminsymbols.h"
#include "dems.h"
#include "gpx.h"
#include <glib/gi18n.h>

VikWaypoint *vik_waypoint_new()
{
  VikWaypoint *wp = g_malloc0 ( sizeof ( VikWaypoint ) );
  wp->visible = TRUE;
  wp->altitude = NAN;
  wp->name = g_strdup(_("Waypoint"));
  wp->image_direction = NAN;
  wp->timestamp = NAN;
  wp->course = NAN;
  wp->speed = NAN;
  wp->magvar = NAN;
  wp->geoidheight = NAN;
  wp->hdop = NAN;
  wp->vdop = NAN;
  wp->pdop = NAN;
  wp->ageofdgpsdata = NAN;
  wp->proximity = NAN;
  return wp;
}

// Hmmm tempted to put in new constructor
void vik_waypoint_set_name(VikWaypoint *wp, const gchar *name)
{
  if ( wp->name )
    g_free ( wp->name );

  wp->name = g_strdup(name);
}

void vik_waypoint_set_comment_no_copy(VikWaypoint *wp, gchar *comment)
{
  if ( wp->comment )
    g_free ( wp->comment );
  wp->comment = comment;
}

void vik_waypoint_set_comment(VikWaypoint *wp, const gchar *comment)
{
  if ( wp->comment )
    g_free ( wp->comment );

  if ( comment && comment[0] != '\0' )
    wp->comment = g_strdup(comment);
  else
    wp->comment = NULL;
}

void vik_waypoint_set_description(VikWaypoint *wp, const gchar *description)
{
  if ( wp->description )
    g_free ( wp->description );

  if ( description && description[0] != '\0' )
    wp->description = g_strdup(description);
  else
    wp->description = NULL;
}

void vik_waypoint_set_source(VikWaypoint *wp, const gchar *source)
{
  if ( wp->source )
    g_free ( wp->source );

  if ( source && source[0] != '\0' )
    wp->source = g_strdup(source);
  else
    wp->source = NULL;
}

void vik_waypoint_set_type(VikWaypoint *wp, const gchar *type)
{
  if ( wp->type )
    g_free ( wp->type );

  if ( type && type[0] != '\0' )
    wp->type = g_strdup(type);
  else
    wp->type = NULL;
}

void vik_waypoint_set_url(VikWaypoint *wp, const gchar *url)
{
  if ( wp->url )
    g_free ( wp->url );

  if ( url && url[0] != '\0' )
    wp->url = g_strdup(url);
  else
    wp->url = NULL;
}

void vik_waypoint_set_url_name(VikWaypoint *wp, const gchar *url_name)
{
  if ( wp->url_name )
    g_free ( wp->url_name );

  if ( url_name && url_name[0] != '\0' )
    wp->url_name = g_strdup(url_name);
  else
    wp->url_name = NULL;
}

void vik_waypoint_set_image(VikWaypoint *wp, const gchar *image)
{
  if ( wp->image )
    g_free ( wp->image );

  if ( image && image[0] != '\0' )
    wp->image = g_strdup(image);
  else
    wp->image = NULL;
  // NOTE - ATM the image (thumbnail) size is calculated on demand when needed to be first drawn
}

/**
 * Set both direction value and reference at the same time
 */
void vik_waypoint_set_image_direction_info(VikWaypoint *wp, gdouble direction, VikWaypointImageDirectionRef direction_ref)
{
  wp->image_direction = direction;
  wp->image_direction_ref = direction_ref;
}

void vik_waypoint_set_symbol(VikWaypoint *wp, const gchar *symname)
{
  const gchar *hashed_symname;

  if ( wp->symbol )
    g_free ( wp->symbol );

  // NB symbol_pixbuf is just a reference, so no need to free it

  if ( symname && symname[0] != '\0' ) {
    hashed_symname = a_get_hashed_sym ( symname );
    if ( hashed_symname )
      symname = hashed_symname;
    wp->symbol = g_strdup ( symname );
    wp->symbol_pixbuf = a_get_wp_sym ( wp->symbol );
  }
  else {
    wp->symbol = NULL;
    wp->symbol_pixbuf = NULL;
  }
}

/* Since handling of bespoke GPX extension properties for waypoints,
   the relevant XML processing and associated logic is performed here
   (rather than in gpx.c) */

// Everything in gpxx space we want to put into it's GHashTable
// Everything in wptx1 space we want to put into it's GHashTable
// The rest of the extensions is stored 'as is' in the GString
static GString *gs_ext;
static gboolean is_gpxx = FALSE;
static gboolean is_wptx1 = FALSE;
static const gchar *tag_name;

typedef enum {
  ext_unknown = 0,
  ext_wp_gpxx,
  ext_wp_gpxx_proximity,
  ext_wp_wptx1,
  ext_wp_wptx1_proximity,
} tag_type_ext;

typedef struct tag_mapping_ext {
  tag_type_ext tag_type;
  const char *tag_name;
} tag_mapping_ext;

static tag_mapping_ext extension_map[] = {
  { ext_wp_gpxx,            "gpxx:WaypointExtension" },
  { ext_wp_gpxx_proximity,  "gpxx:Proximity" },
  { ext_wp_wptx1,           "wptx1:WaypointExtension" },
  { ext_wp_wptx1_proximity, "wptx1:Proximity" },
  {0}
};

static tag_type_ext get_tag_ext_specific (const char *tt)
{
 tag_mapping_ext *tm;
 for ( tm = extension_map; tm->tag_type != 0; tm++ )
   if ( 0 == g_strcmp0(tm->tag_name, tt) )
     return tm->tag_type;
 return ext_unknown;
}

// Reprocess the extension text to extract tags we handle
static void xt_start_element ( GMarkupParseContext *context,
                               const gchar         *element_name,
                               const gchar        **attribute_names,
                               const gchar        **attribute_values,
                               gpointer             user_data,
                               GError             **error )
{
  tag_name = element_name;
  tag_type_ext tag = get_tag_ext_specific ( tag_name );
  switch ( tag ) {
  case ext_wp_gpxx: {
    VikWaypoint *wp = (VikWaypoint*)user_data;
    wp->gpxx = g_hash_table_new_full ( g_str_hash, g_str_equal, g_free, g_free );
    is_gpxx = TRUE;
    break;
  }
  case ext_wp_wptx1: {
    VikWaypoint *wp = (VikWaypoint*)user_data;
    wp->wptx1 = g_hash_table_new_full ( g_str_hash, g_str_equal, g_free, g_free );
    is_wptx1 = TRUE;
    break;
  }
  default:
    break;
  }
  if ( !is_gpxx && !is_wptx1 ) {
    // Store any other tag
    g_string_append ( gs_ext, "      <" );
    g_string_append ( gs_ext, element_name );
    for ( guint nn = 0; nn < g_strv_length((gchar**)attribute_names); nn++ )
      g_string_append_printf ( gs_ext, " %s=\"%s\"", attribute_names[nn], attribute_values[nn] );
    g_string_append ( gs_ext, ">" );
  }
}

// NB Text is not null terminated
static void xt_text ( GMarkupParseContext *context,
                      const gchar         *text,
                      gsize                text_len,
                      gpointer             user_data,
                      GError             **error )
{
  if ( is_gpxx || is_wptx1 ) {
    if ( tag_name ) {
      // NB need to avoid white-space
      gboolean add = FALSE;
      for ( guint nn = 0; nn < text_len; nn++ ) {
        if ( g_ascii_isgraph(text[nn]) ) {
          add = TRUE;
          break;
        }
      }
      if ( add ) {
        VikWaypoint *wp = (VikWaypoint*)user_data;
        gchar *txt = g_memdup ( text, text_len+1 );
        txt[text_len] = '\0';

        // Select which table is to be updated
        GHashTable *ght = wp->wptx1;
        if ( is_gpxx )
          ght = wp->gpxx;
        (void)g_hash_table_insert ( ght, g_strdup(tag_name), txt );

        // Apply (latest detected) XML value to the single proximity variable
        tag_type_ext tag = get_tag_ext_specific ( tag_name );
        switch ( tag ) {
        case ext_wp_wptx1_proximity:
        case ext_wp_gpxx_proximity:
          wp->proximity = g_ascii_strtod ( txt, NULL );
          break;
        default:
          break;
        }
      }
    }
  }
  else {
    // Store any other tag contents
    gchar *txt = g_memdup ( text, text_len+1 );
    txt[text_len] = '\0';
    gchar *tmp = a_gpx_entitize ( txt );
    g_string_append ( gs_ext, tmp );
    g_free ( txt );
    g_free ( tmp );
  }
}

static void xt_end_element ( GMarkupParseContext *context,
                             const gchar         *element_name,
                             gpointer             user_data,
                             GError             **error )
{
  // Store any other tag info
  if ( !is_gpxx && !is_wptx1 )
    g_string_append_printf ( gs_ext, "%s%s%s", "</", element_name, ">\n" );

  tag_type_ext tag = get_tag_ext_specific ( element_name );
  switch ( tag ) {
  case ext_wp_gpxx:
    is_gpxx = FALSE;
    break;
  case ext_wp_wptx1:
    is_wptx1 = FALSE;
    break;
  default:
    break;
  }
  tag_name = NULL;
}

/**
 * vik_waypoint_have_extensions:
 *
 * Waypoint extensions are now potentially stored
 *  across multiple variables
 */
gboolean vik_waypoint_have_extensions(VikWaypoint *wp)
{
  return wp->extensions || wp->gpxx || wp->wptx1;
}

// Helper function to output string for a specific schema table
static void wpt_ext_output ( VikWaypoint *wp, GString *gs, GHashTable *ght, gchar *key, gchar *ext_start, gchar *ext_end )
{
  // If we have a proximity value, ensure the specified table is aligned
  if ( !isnan(wp->proximity) ) {
    if ( !ght )
      ght = g_hash_table_new_full ( g_str_hash, g_str_equal, g_free, g_free );

    gchar buf[COORDS_STR_BUFFER_SIZE];
    a_coords_dtostr_buffer ( wp->proximity, buf );
    (void)g_hash_table_insert ( ght, g_strdup(key), g_strdup(buf) );
  } else {
    // Remove if it exists
    if ( ght )
      g_hash_table_remove ( ght, key );
  }

  if ( ght ) {
    if ( g_hash_table_size(ght) ) {
      g_string_append_printf ( gs, "\n    %s\n", ext_start );

      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init ( &iter, ght );
      while ( g_hash_table_iter_next(&iter, &key, &value) ) {
        gchar *tmp = a_gpx_entitize ( value );
        g_string_append_printf ( gs, "      <%s>%s</%s>\n", (gchar*)key, tmp, (gchar*)key );
        g_free ( tmp );
      }
      g_string_append_printf ( gs, "    %s\n", ext_end );
    }
  }
}

/**
 * vik_waypoint_get_extensions:
 *
 * For display and file save usage
 * Fully free the returned #GString after use
 */
GString *vik_waypoint_get_extensions(VikWaypoint *wp)
{
  GString *gs = g_string_new ( NULL );
  if ( wp->extensions )
    g_string_append_printf ( gs, "\n%s", wp->extensions );

  // Which styles to output depends on which Schema is to be followed
  switch ( a_vik_gpx_export_wpt_extension_type() ) {
  case VIK_GPX_EXPORT_WPT_EXT_WPTX1:
    wpt_ext_output ( wp, gs, wp->wptx1, "wptx1:Proximity",
                     "<wptx1:WaypointExtension>", "</wptx1:WaypointExtension>" );
    break;
  case VIK_GPX_EXPORT_WPT_EXT_GPXX:
    wpt_ext_output ( wp, gs, wp->gpxx, "gpxx:Proximity",
                     "<gpxx:WaypointExtension xmlns:gpxx=\"http://www.garmin.com/xmlschemas/GpxExtensions/v3\">",
                     "</gpxx:WaypointExtension>" );
    break;
  default:
    //VIK_GPX_EXPORT_WPT_EXT_ALL:
    wpt_ext_output ( wp, gs, wp->wptx1, "wptx1:Proximity",
                     "<wptx1:WaypointExtension>", "</wptx1:WaypointExtension>" );
    wpt_ext_output ( wp, gs, wp->gpxx, "gpxx:Proximity",
                     "<gpxx:WaypointExtension xmlns:gpxx=\"http://www.garmin.com/xmlschemas/GpxExtensions/v3\">",
                     "</gpxx:WaypointExtension>" );
    break;
  }
  return gs;
}

/**
 * vik_waypoint_set_extensions:
 *
 * Pass in the full XML string fragment (with the XML containing escape sequences)
 */
void vik_waypoint_set_extensions(VikWaypoint *wp, const gchar *value)
{
  if ( wp->extensions )
    g_free ( wp->extensions );

  if ( !value || strlen(value) == 0 ) {
    wp->extensions = NULL;
    return;
  }

  gs_ext = g_string_new ( NULL );

  GMarkupParser gparser;
  GMarkupParseContext *gcontext;
  gparser.start_element = &xt_start_element;
  gparser.end_element = &xt_end_element;
  gparser.text = &xt_text;
  gparser.passthrough = NULL;
  gparser.error = NULL;
  gcontext = g_markup_parse_context_new ( &gparser, 0, wp, NULL );

  // Parse xml fragment to extract extension tag values
  GError *error = NULL;
  if ( !g_markup_parse_context_parse ( gcontext, value, strlen(value), &error ) )
    g_warning ( "%s: parse error %s on:%s", __FUNCTION__, error ? error->message : "???", value );

  if ( !g_markup_parse_context_end_parse ( gcontext, &error) )
    g_warning ( "%s: error %s occurred on end of:%s", __FUNCTION__, error ? error->message : "???", value );

  if ( gs_ext->len )
    wp->extensions = g_strdup ( gs_ext->str );

  g_string_free ( gs_ext, TRUE );
  g_markup_parse_context_free ( gcontext );
}

/**
 * vik_waypoint_set_proximity:
 */
void vik_waypoint_set_proximity(VikWaypoint *wp, gdouble value)
{
  wp->proximity = value;
  // NB extension style tables are updated when getting the extension value
  // Thus ensures the relevant style schema is applied when the file saved
  //  (as every waypoint will be processed - not just those that have been changed)
}

void vik_waypoint_free(VikWaypoint *wp)
{
  if ( wp->name )
    g_free ( wp->name );
  if ( wp->comment )
    g_free ( wp->comment );
  if ( wp->description )
    g_free ( wp->description );
  if ( wp->source )
    g_free ( wp->source );
  if ( wp->url )
    g_free ( wp->url );
  if ( wp->url_name )
    g_free ( wp->url_name );
  if ( wp->type )
    g_free ( wp->type );
  if ( wp->image )
    g_free ( wp->image );
  if ( wp->symbol )
    g_free ( wp->symbol );
  if ( wp->extensions )
    g_free ( wp->extensions );
  if ( wp->gpxx )
    g_hash_table_destroy ( wp->gpxx );
  if ( wp->wptx1 )
    g_hash_table_destroy ( wp->wptx1 );
  g_free ( wp );
}

VikWaypoint *vik_waypoint_copy(const VikWaypoint *wp)
{
  VikWaypoint *new_wp = vik_waypoint_new();
  new_wp->coord = wp->coord;
  new_wp->visible = wp->visible;
  new_wp->altitude = wp->altitude;
  new_wp->timestamp = wp->timestamp;
  new_wp->course = wp->course;
  new_wp->speed = wp->speed;
  new_wp->magvar = wp->magvar;
  new_wp->geoidheight = wp->geoidheight;
  new_wp->fix_mode = wp->fix_mode;
  new_wp->nsats = wp->nsats;
  new_wp->hdop = wp->hdop;
  new_wp->vdop = wp->vdop;
  new_wp->pdop = wp->pdop;
  new_wp->ageofdgpsdata = wp->ageofdgpsdata;
  new_wp->dgpsid = wp->dgpsid;
  new_wp->proximity = wp->proximity;
  vik_waypoint_set_name(new_wp,wp->name);
  vik_waypoint_set_comment(new_wp,wp->comment);
  vik_waypoint_set_description(new_wp,wp->description);
  vik_waypoint_set_source(new_wp,wp->source);
  vik_waypoint_set_url(new_wp,wp->url);
  vik_waypoint_set_url_name(new_wp,wp->url_name);
  vik_waypoint_set_type(new_wp,wp->type);
  vik_waypoint_set_image(new_wp,wp->image);
  vik_waypoint_set_symbol(new_wp,wp->symbol);
  GString *gs = vik_waypoint_get_extensions((VikWaypoint*)wp);
  vik_waypoint_set_extensions(new_wp,gs->str);
  g_string_free(gs, TRUE);
  new_wp->image_direction = wp->image_direction;
  new_wp->image_direction_ref = wp->image_direction_ref;
  return new_wp;
}

/**
 * vik_waypoint_apply_dem_data:
 * @wp:            The Waypoint to operate on
 * @skip_existing: When TRUE, don't change the elevation if the waypoint already has a value
 *
 * Set elevation data for a waypoint using available DEM information
 *
 * Returns: TRUE if the waypoint was updated
 */
gboolean vik_waypoint_apply_dem_data ( VikWaypoint *wp, gboolean skip_existing )
{
  gboolean updated = FALSE;
  if ( !(skip_existing && !isnan(wp->altitude)) ) {
    gint16 elev = a_dems_get_elev_by_coord ( &(wp->coord), VIK_DEM_INTERPOL_BEST );
    if ( elev != VIK_DEM_INVALID_ELEVATION ) {
      wp->altitude = (gdouble)elev;
      updated = TRUE;
    }
  }
  return updated;
}

/*
 * Take a Waypoint and convert it into a byte array
 */
void vik_waypoint_marshall ( VikWaypoint *wp, guint8 **data, guint *datalen)
{
  GByteArray *b = g_byte_array_new();
  guint len;

  // This creates space for fixed sized members like gints and whatnot
  //  and copies that amount of data from the waypoint to byte array
  g_byte_array_append(b, (guint8 *)wp, sizeof(*wp));

  // This allocates space for variant sized strings
  //  and copies that amount of data from the waypoint to byte array
#define vwm_append(s) \
  len = (s) ? strlen(s)+1 : 0; \
  g_byte_array_append(b, (guint8 *)&len, sizeof(len)); \
  if (s) g_byte_array_append(b, (guint8 *)s, len);

  vwm_append(wp->name);
  vwm_append(wp->comment);
  vwm_append(wp->description);
  vwm_append(wp->source);
  vwm_append(wp->url);
  vwm_append(wp->url_name);
  vwm_append(wp->type);
  vwm_append(wp->image);
  vwm_append(wp->symbol);
  vwm_append(wp->extensions);

  // Hash table deep copy - size and then pairs of key,value
  guint sz;
  GHashTableIter iter;
  gpointer key, value;
#define vwm_append_hash(ght)                                    \
  sz = (ght) ? g_hash_table_size((ght)): 0;                     \
  g_byte_array_append ( b, (guint8*)&sz, sizeof(guint) );       \
  if ( (ght) ) {                                                \
    g_hash_table_iter_init ( &iter, (ght) );                    \
    while ( g_hash_table_iter_next (&iter, &key, &value) ) {    \
      vwm_append(key);                                          \
      vwm_append(value);                                        \
    }                                                           \
  }

  vwm_append_hash(wp->gpxx);
  vwm_append_hash(wp->wptx1);

  *data = b->data;
  *datalen = b->len;
  g_byte_array_free(b, FALSE);
#undef vwm_append
#undef vwm_append_hash
}

/*
 * Take a byte array and convert it into a Waypoint
 */
VikWaypoint *vik_waypoint_unmarshall (const guint8 *data_in, guint datalen)
{
  guint len;
  VikWaypoint *new_wp = vik_waypoint_new();
  guint8 *data = (guint8*)data_in;
  // This copies the fixed sized elements (i.e. visibility, altitude, image_width, etc...)
  memcpy(new_wp, data, sizeof(*new_wp));
  data += sizeof(*new_wp);

  // Now the variant sized strings...
#define vwu_get(s) \
  len = *(guint *)data; \
  data += sizeof(len); \
  if (len) { \
    (s) = g_strdup((gchar *)data); \
  } else { \
    (s) = NULL; \
  } \
  data += len;

  vwu_get(new_wp->name);
  vwu_get(new_wp->comment);
  vwu_get(new_wp->description);
  vwu_get(new_wp->source);
  vwu_get(new_wp->url);
  vwu_get(new_wp->url_name);
  vwu_get(new_wp->type);
  vwu_get(new_wp->image); 
  vwu_get(new_wp->symbol);
  vwu_get(new_wp->extensions);

  guint mylen;
  gchar *key;
  gchar *value;
  // Extract copy of hash table(s) if any elements found
#define vwu_get_hash(ght)                                               \
  len = *(guint *)data;                                                 \
  if ( len && len < datalen ) {                                         \
    data += sizeof(len);                                                \
    (ght) = g_hash_table_new_full ( g_str_hash, g_str_equal, g_free, g_free ); \
    mylen = len;                                                        \
    for ( guint nn = 0; nn < mylen; nn++ ) {                            \
      vwu_get(key);                                                     \
      vwu_get(value);                                                   \
      (void)g_hash_table_insert ( (ght), key, value );                  \
    }                                                                   \
  }                                                                     \
  else {                                                                \
    (ght) = NULL;                                                       \
    data += sizeof(len);                                                \
  }

  vwu_get_hash(new_wp->gpxx);
  vwu_get_hash(new_wp->wptx1);

  // Different Viking instances need their seperate versions
  //  copying to itself will get the same reference
  new_wp->symbol_pixbuf = a_get_wp_sym(new_wp->symbol);

  return new_wp;
#undef vwu_get
#undef vwu_get_hash
}
