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
#include "coords.h"
#include "vikcoord.h"
#include "vikwaypoint.h"
#include "globals.h"
#include <glib/gi18n.h>

VikWaypoint *vik_waypoint_new()
{
  VikWaypoint *wp = g_malloc0 ( sizeof ( VikWaypoint ) );
  wp->altitude = VIK_DEFAULT_ALTITUDE;
  wp->name = g_strdup(_("Waypoint"));
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

void vik_waypoint_set_symbol(VikWaypoint *wp, const gchar *symname)
{
  if ( wp->symbol )
    g_free ( wp->symbol );

  if ( symname && symname[0] != '\0' )
    wp->symbol = g_strdup(symname);
  else
    wp->symbol = NULL;
}

void vik_waypoint_free(VikWaypoint *wp)
{
  if ( wp->comment )
    g_free ( wp->comment );
  if ( wp->description )
    g_free ( wp->description );
  if ( wp->image )
    g_free ( wp->image );
  if ( wp->symbol )
    g_free ( wp->symbol );
  g_free ( wp );
}

VikWaypoint *vik_waypoint_copy(const VikWaypoint *wp)
{
  VikWaypoint *new_wp = vik_waypoint_new();
  new_wp->coord = wp->coord;
  new_wp->visible = wp->visible;
  new_wp->altitude = wp->altitude;
  vik_waypoint_set_name(new_wp,wp->name);
  vik_waypoint_set_comment(new_wp,wp->comment);
  vik_waypoint_set_description(new_wp,wp->description);
  vik_waypoint_set_image(new_wp,wp->image);
  vik_waypoint_set_symbol(new_wp,wp->symbol);
  return new_wp;
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
  vwm_append(wp->image);
  vwm_append(wp->symbol);

  *data = b->data;
  *datalen = b->len;
  g_byte_array_free(b, FALSE);
#undef vwm_append
}

/*
 * Take a byte array and convert it into a Waypoint
 */
VikWaypoint *vik_waypoint_unmarshall (guint8 *data, guint datalen)
{
  guint len;
  VikWaypoint *new_wp = vik_waypoint_new();
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
  vwu_get(new_wp->image); 
  vwu_get(new_wp->symbol);
  
  return new_wp;
#undef vwu_get
}

