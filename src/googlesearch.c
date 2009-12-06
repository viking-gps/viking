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
 * Created by Quy Tonthat <qtonthat@gmail.com>
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

#define GOOGLE_SEARCH_URL_FMT "http://maps.google.com/maps?q=%s&output=js"
#define GOOGLE_SEARCH_PATTERN_1 "{center:{lat:"
#define GOOGLE_SEARCH_PATTERN_2 ",lng:"
#define GOOGLE_SEARCH_NOT_FOUND "not understand the location"

static DownloadOptions googlesearch_options = { "http://maps.google.com/", 0, a_check_map_file };

static void google_search_tool_class_init ( GoogleSearchToolClass *klass );
static void google_search_tool_init ( GoogleSearchTool *vwd );

static void google_search_tool_finalize ( GObject *gob );

static int google_search_tool_get_coord ( VikSearchTool *self, VikWindow *vw, VikViewport *vvp, gchar *srch_str, VikCoord *coord );

GType google_search_tool_get_type()
{
  static GType w_type = 0;

  if (!w_type)
  {
    static const GTypeInfo w_info = 
    {
      sizeof (GoogleSearchToolClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      (GClassInitFunc) google_search_tool_class_init,
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (GoogleSearchTool),
      0,
      (GInstanceInitFunc) google_search_tool_init,
    };
    w_type = g_type_register_static ( VIK_SEARCH_TOOL_TYPE, "GoogleSearchTool", &w_info, 0 );
  }

  return w_type;
}

static void google_search_tool_class_init ( GoogleSearchToolClass *klass )
{
  GObjectClass *object_class;
  VikSearchToolClass *parent_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = google_search_tool_finalize;

  parent_class = VIK_SEARCH_TOOL_CLASS (klass);

  parent_class->get_coord = google_search_tool_get_coord;
}

GoogleSearchTool *google_search_tool_new ()
{
  return GOOGLE_SEARCH_TOOL ( g_object_new ( GOOGLE_SEARCH_TOOL_TYPE, "label", "Google", NULL ) );
}

static void google_search_tool_init ( GoogleSearchTool *vlp )
{
}

static void google_search_tool_finalize ( GObject *gob )
{
  G_OBJECT_GET_CLASS(gob)->finalize(gob);
}

static gboolean parse_file_for_latlon(gchar *file_name, struct LatLon *ll)
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
    exit(1);
  }
  len = g_mapped_file_get_length(mf);
  text = g_mapped_file_get_contents(mf);

  if (g_strstr_len(text, len, GOOGLE_SEARCH_NOT_FOUND) != NULL) {
    found = FALSE;
    goto done;
  }

  if ((pat = g_strstr_len(text, len, GOOGLE_SEARCH_PATTERN_1)) == NULL) {
    found = FALSE;
    goto done;
  }
  pat += strlen(GOOGLE_SEARCH_PATTERN_1);
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

  if (strncmp(pat, GOOGLE_SEARCH_PATTERN_2, strlen(GOOGLE_SEARCH_PATTERN_2))) {
      found = FALSE;
      goto done;
  }

  pat += strlen(GOOGLE_SEARCH_PATTERN_2);
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
  g_mapped_file_free(mf);
  return (found);

}

static int google_search_tool_get_coord ( VikSearchTool *self, VikWindow *vw, VikViewport *vvp, gchar *srch_str, VikCoord *coord )
{
  FILE *tmp_file;
  int tmp_fd;
  gchar *tmpname;
  gchar *uri;
  gchar *escaped_srch_str;
  int ret = 0;  /* OK */
  struct LatLon ll;

  g_debug("%s: raw search: %s", __FUNCTION__, srch_str);

  escaped_srch_str = uri_escape(srch_str);

  g_debug("%s: escaped search: %s", __FUNCTION__, escaped_srch_str);

  if ((tmp_fd = g_file_open_tmp ("vikgsearch.XXXXXX", &tmpname, NULL)) == -1) {
    g_critical(_("couldn't open temp file"));
    exit(1);
  }

  tmp_file = fdopen(tmp_fd, "r+");
  //uri = g_strdup_printf(GOOGLE_SEARCH_URL_FMT, srch_str);
  uri = g_strdup_printf(GOOGLE_SEARCH_URL_FMT, escaped_srch_str);

  /* TODO: curl may not be available */
  if (curl_download_uri(uri, tmp_file, &googlesearch_options)) {  /* error */
    fclose(tmp_file);
    tmp_file = NULL;
    ret = -1;
    goto done;
  }

  fclose(tmp_file);
  tmp_file = NULL;
  if (!parse_file_for_latlon(tmpname, &ll)) {
    ret = -1;
    goto done;
  }

  vik_coord_load_from_latlon ( coord, vik_viewport_get_coord_mode(vvp), &ll );

done:
  g_free(escaped_srch_str);
  g_free(uri);
  g_remove(tmpname);
  g_free(tmpname);
  return ret;
}
