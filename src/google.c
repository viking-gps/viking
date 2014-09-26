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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>

#include "globals.h"
#include "google.h"
#include "vikexttools.h"
#include "vikwebtoolcenter.h"
#include "vikgoto.h"
#include "googlesearch.h"
#include "vikrouting.h"
#include "vikroutingwebengine.h"
#include "babel.h"

void google_init () {
  // Webtools
  VikWebtoolCenter *webtool = vik_webtool_center_new_with_members ( _("Google"), "http://maps.google.com/maps?f=q&geocode=&ie=UTF8&ll=%s,%s&z=%d&iwloc=addr" );
  vik_ext_tools_register ( VIK_EXT_TOOL ( webtool ) );
  g_object_unref ( webtool );

  // Goto
  GoogleGotoTool *gototool = google_goto_tool_new (  );
  vik_goto_register ( VIK_GOTO_TOOL ( gototool ) );
  g_object_unref ( gototool );

  // Routing
  /* Google Directions service as routing engine.
   * 
   * Technical details are available here:
   * https://developers.google.com/maps/documentation/directions/#DirectionsResponses
   *
   * gpsbabel supports this format.
   */
  if ( a_babel_available() ) {
    VikRoutingEngine *routing = g_object_new ( VIK_ROUTING_WEB_ENGINE_TYPE,
      "id", "google",
      "label", "Google",
      "format", "google",
      "url-base", "http://maps.google.com/maps?output=js&q=",
      "url-start-ll", "from:%s,%s",
      "url-stop-ll", "+to:%s,%s",
      "url-start-dir", "from:%s",
      "url-stop-dir", "+to:%s",
      "referer", "http://maps.google.com/",
      NULL);
    vik_routing_register ( VIK_ROUTING_ENGINE ( routing ) );
    g_object_unref ( routing );
  }
}
