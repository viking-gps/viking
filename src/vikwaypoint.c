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
#include "coords.h"
#include "vikcoord.h"
#include "vikwaypoint.h"

VikWaypoint *vik_waypoint_new()
{
  VikWaypoint *wp = g_malloc ( sizeof ( VikWaypoint ) );
  wp->comment = NULL;
  wp->image = NULL;
  return wp;
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

void vik_waypoint_set_image(VikWaypoint *wp, const gchar *image)
{
  if ( wp->image )
    g_free ( wp->image );

  if ( image && image[0] != '\0' )
    wp->image = g_strdup(image);
  else
    wp->image = NULL;
}

void vik_waypoint_free(VikWaypoint *wp)
{
  if ( wp->comment )
    g_free ( wp->comment );
  if ( wp->image )
    g_free ( wp->image );
  g_free ( wp );
}

VikWaypoint *vik_waypoint_copy(const VikWaypoint *wp)
{
  VikWaypoint *new_wp = vik_waypoint_new();
  *new_wp = *wp;
  new_wp->comment = NULL; /* if the waypoint had a comment, FOR CRYING OUT LOUD DON'T FREE IT! This lousy bug took me TWO HOURS to figure out... sigh... */
  vik_waypoint_set_comment(new_wp,wp->comment);
  new_wp->image = NULL;
  vik_waypoint_set_image(new_wp,wp->image);
  return new_wp;
}
