/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Quy Tonthat <qtonthat@gmail.com>
 * Copyright (C) 2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "util.h"
#include "curl_download.h"

#include "googlesearch.h"

/* Compatibility */
#if ! GLIB_CHECK_VERSION(2,22,0)
#define g_mapped_file_unref g_mapped_file_free
#endif

#define GOOGLE_GOTO_URL_FMT "http://maps.google.com/maps?q=%s&output=js"
#define GOOGLE_GOTO_PATTERN_1 "{center:{lat:"
#define GOOGLE_GOTO_PATTERN_2 ",lng:"
#define GOOGLE_GOTO_NOT_FOUND "not understand the location"

static DownloadMapOptions googlesearch_options = { FALSE, FALSE, "http://maps.google.com/", 2, a_check_map_file };

static void google_goto_tool_finalize ( GObject *gob );

static gchar *google_goto_tool_get_url_format ( VikGotoTool *self );
static DownloadMapOptions *google_goto_tool_get_download_options ( VikGotoTool *self );
static gboolean google_goto_tool_parse_file_for_latlon(VikGotoTool *self, gchar *filename, struct LatLon *ll);

G_DEFINE_TYPE (GoogleGotoTool, google_goto_tool, VIK_GOTO_TOOL_TYPE)

static void google_goto_tool_class_init ( GoogleGotoToolClass *klass )
{
  GObjectClass *object_class;
  VikGotoToolClass *parent_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = google_goto_tool_finalize;

  parent_class = VIK_GOTO_TOOL_CLASS (klass);

  parent_class->get_url_format = google_goto_tool_get_url_format;
  parent_class->get_download_options = google_goto_tool_get_download_options;
  parent_class->parse_file_for_latlon = google_goto_tool_parse_file_for_latlon;
}

GoogleGotoTool *google_goto_tool_new ()
{
  return GOOGLE_GOTO_TOOL ( g_object_new ( GOOGLE_GOTO_TOOL_TYPE, "label", "Google", NULL ) );
}

static void google_goto_tool_init ( GoogleGotoTool *vlp )
{
}

static void google_goto_tool_finalize ( GObject *gob )
{
  G_OBJECT_GET_CLASS(gob)->finalize(gob);
}

static gboolean google_goto_tool_parse_file_for_latlon(VikGotoTool *self, gchar *file_name, struct LatLon *ll)
{
  gchar *text, *pat;
  GMappedFile *mf;
  gsize len;
  gboolean found = TRUE;
  gchar lat_buf[32], lon_buf[32];
  gchar *s;

  lat_buf[0] = lon_buf[0] = '\0';

  if ((mf = g_mapped_file_new(file_name, FALSE, NULL)) == NULL) {
    g_critical(_("couldn't map temp file"));
    return FALSE;
  }
  len = g_mapped_file_get_length(mf);
  text = g_mapped_file_get_contents(mf);

  if (g_strstr_len(text, len, GOOGLE_GOTO_NOT_FOUND) != NULL) {
    found = FALSE;
    goto done;
  }

  if ((pat = g_strstr_len(text, len, GOOGLE_GOTO_PATTERN_1)) == NULL) {
    found = FALSE;
    goto done;
  }
  pat += strlen(GOOGLE_GOTO_PATTERN_1);
  s = lat_buf;
  if (*pat == '-')
    *s++ = *pat++;
  while ((s < (lat_buf + sizeof(lat_buf))) && (pat < (text + len)) &&
          (g_ascii_isdigit(*pat) || (*pat == '.')))
    *s++ = *pat++;
  *s = '\0';
  if ((pat >= (text + len)) || (lat_buf[0] == '\0')) {
    found = FALSE;
    goto done;
  }

  if (strncmp(pat, GOOGLE_GOTO_PATTERN_2, strlen(GOOGLE_GOTO_PATTERN_2))) {
      found = FALSE;
      goto done;
  }

  pat += strlen(GOOGLE_GOTO_PATTERN_2);
  s = lon_buf;

  if (*pat == '-')
    *s++ = *pat++;
  while ((s < (lon_buf + sizeof(lon_buf))) && (pat < (text + len)) &&
          (g_ascii_isdigit(*pat) || (*pat == '.')))
    *s++ = *pat++;
  *s = '\0';
  if ((pat >= (text + len)) || (lon_buf[0] == '\0')) {
    found = FALSE;
    goto done;
  }

  ll->lat = g_ascii_strtod(lat_buf, NULL);
  ll->lon = g_ascii_strtod(lon_buf, NULL);

done:
  g_mapped_file_unref(mf);
  return (found);

}

static gchar *google_goto_tool_get_url_format ( VikGotoTool *self )
{
  return GOOGLE_GOTO_URL_FMT;
}

DownloadMapOptions *google_goto_tool_get_download_options ( VikGotoTool *self )
{
  return &googlesearch_options;
}
