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

#define GOOGLE_SEARCH_URL_FMT "http://maps.google.com/maps?q=%s&output=js"
#define GOOGLE_SEARCH_PATTERN_1 "{center:{lat:"
#define GOOGLE_SEARCH_PATTERN_2 ",lng:"
#define GOOGLE_SEARCH_NOT_FOUND "not understand the location"

static gchar *last_search_str = NULL;
static VikCoord *last_coord = NULL;
static gchar *last_successful_search_str = NULL;

static DownloadOptions googlesearch_options = { "http://maps.google.com/", 0, a_check_map_file };

gchar * a_googlesearch_get_search_string_for_this_place(VikWindow *vw)
{
  if (!last_coord)
    return NULL;

  VikViewport *vvp = vik_window_viewport(vw);
  const VikCoord *cur_center = vik_viewport_get_center(vvp);
  if (vik_coord_equals(cur_center, last_coord)) {
    return(last_successful_search_str);
  }
  else
    return NULL;
}

static gboolean prompt_try_again(VikWindow *vw)
{
  GtkWidget *dialog = NULL;
  gboolean ret = TRUE;

  dialog = gtk_dialog_new_with_buttons ( "", GTK_WINDOW(vw), 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL );
  gtk_window_set_title(GTK_WINDOW(dialog), _("Search"));

  GtkWidget *search_label = gtk_label_new(_("I don't know that place. Do you want another search?"));
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), search_label, FALSE, FALSE, 5 );
  gtk_widget_show_all(dialog);

  if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) != GTK_RESPONSE_ACCEPT )
    ret = FALSE;

  gtk_widget_destroy(dialog);
  return ret;
}

static gchar *  a_prompt_for_search_string(VikWindow *vw)
{
  GtkWidget *dialog = NULL;

  dialog = gtk_dialog_new_with_buttons ( "", GTK_WINDOW(vw), 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL );
  gtk_window_set_title(GTK_WINDOW(dialog), _("Search"));

  GtkWidget *search_label = gtk_label_new(_("Enter address or place name:"));
  GtkWidget *search_entry = gtk_entry_new();
  if (last_search_str)
    gtk_entry_set_text(GTK_ENTRY(search_entry), last_search_str);

  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), search_label, FALSE, FALSE, 5 );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), search_entry, FALSE, FALSE, 5 );
  gtk_widget_show_all(dialog);

  if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) != GTK_RESPONSE_ACCEPT ) {
    gtk_widget_destroy(dialog);
    return NULL;
  }

  gchar *search_str = g_strdup ( gtk_entry_get_text ( GTK_ENTRY(search_entry) ) );

  gtk_widget_destroy(dialog);

  if (search_str[0] != '\0') {
    if (last_search_str)
      g_free(last_search_str);
    last_search_str = g_strdup(search_str);
  }

  return(search_str);   /* search_str needs to be freed by caller */
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

static int google_search_get_coord(VikWindow *vw, VikViewport *vvp, gchar *srch_str, VikCoord *coord)
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

  if (last_coord)
    g_free(last_coord);
  last_coord = g_malloc(sizeof(VikCoord));
  *last_coord = *coord;
  if (last_successful_search_str)
    g_free(last_successful_search_str);
  last_successful_search_str = g_strdup(last_search_str);

done:
  g_free(escaped_srch_str);
  g_free(uri);
  g_remove(tmpname);
  g_free(tmpname);
  return ret;
}

void a_google_search(VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp)
{
  VikCoord new_center;
  gchar *s_str;
  gboolean more = TRUE;

  do {
    s_str = a_prompt_for_search_string(vw);
    if ((!s_str) || (s_str[0] == 0)) {
      more = FALSE;
    }

    else if (!google_search_get_coord(vw, vvp, s_str, &new_center)) {
      vik_viewport_set_center_coord(vvp, &new_center);
      vik_layers_panel_emit_update(vlp);
      more = FALSE;
    }
    else if (!prompt_try_again(vw))
        more = FALSE;
    g_free(s_str);
  } while (more);
}

