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
#include "curl_download.h"

#define GEONAMES_SEARCH_URL_FMT "http://ws.geonames.org/searchJSON?formatted=true&style=medium&maxRows=10&lang=en&q=%s"
#define GEONAMES_SEARCH_PATTERN_2 "\"lat\": "
#define GEONAMES_SEARCH_PATTERN_1 "\"lng\": "
#define GEONAMES_COUNTRY_PATTERN "\"countryName\": \""
#define GEONAMES_LONGITUDE_PATTERN "\"lng\": "
#define GEONAMES_NAME_PATTERN "\"name\": \""
#define GEONAMES_LATITUDE_PATTERN "\"lat\": "
#define GEONAMES_SEARCH_NOT_FOUND "not understand the location"

static gchar *last_search_str = NULL;
static VikCoord *last_coord = NULL;
static gchar *last_successful_search_str = NULL;

typedef struct {
  gchar *name;
  gchar *country;
  struct LatLon ll;
} found_geoname;

found_geoname *copy_found_geoname(found_geoname *src)
{
  found_geoname *dest = (found_geoname *)g_malloc(sizeof(found_geoname));
  dest->name = g_strdup(src->name);
  dest->country = g_strdup(src->country);
  dest->ll.lat = src->ll.lat;
  dest->ll.lon = src->ll.lon;
  return(dest);
}

gchar * a_geonamessearch_get_search_string_for_this_place(VikWindow *vw)
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

static void free_list_geonames(found_geoname *geoname, gpointer userdata)
{
  g_free(geoname->name);
  g_free(geoname->country);
}

void buttonToggled(GtkCellRendererToggle* renderer, gchar* pathStr, gpointer data)
{
   GtkTreeIter iter;
   gboolean enabled;
   GtkTreePath* path = gtk_tree_path_new_from_string(pathStr);
   gtk_tree_model_get_iter(GTK_TREE_MODEL (data), &iter, path);
   gtk_tree_model_get(GTK_TREE_MODEL (data), &iter, 0, &enabled, -1);
   enabled = !enabled;
   gtk_tree_store_set(GTK_TREE_STORE (data), &iter, 0, enabled, -1);
}

GList *a_select_geoname_from_list(GtkWindow *parent, GList *geonames, gboolean multiple_selection_allowed, const gchar *title, const gchar *msg)
{
  GtkTreeIter iter;
  GtkCellRenderer *renderer;
  GtkCellRenderer *toggle_render;
  GtkWidget *view;
  found_geoname *geoname;
  gchar *latlon_string;
  int column_runner;

  GtkWidget *dialog = gtk_dialog_new_with_buttons (title,
                                                  parent,
                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_REJECT,
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_ACCEPT,
                                                  NULL);
  GtkWidget *label = gtk_label_new ( msg );
  GtkTreeStore *store;
  if (multiple_selection_allowed)
  {
    store = gtk_tree_store_new(4, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  }
  else
  {
    store = gtk_tree_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  }
  GList *geoname_runner = geonames;
  while (geoname_runner)
  { 
    geoname = (found_geoname *)geoname_runner->data;
    latlon_string = g_strdup_printf("(%f,%f)", geoname->ll.lat, geoname->ll.lon);
    gtk_tree_store_append(store, &iter, NULL);
    if (multiple_selection_allowed)
    {
      gtk_tree_store_set(store, &iter, 0, FALSE, 1, geoname->name, 2, geoname->country, 3, latlon_string, -1);
    }
    else
    {
      gtk_tree_store_set(store, &iter, 0, geoname->name, 1, geoname->country, 2, latlon_string, -1);
    }
    geoname_runner = g_list_next(geoname_runner);
    g_free(latlon_string);
  }
  view = gtk_tree_view_new();
  renderer = gtk_cell_renderer_text_new();
  column_runner = 0;
  if (multiple_selection_allowed)
  {
    toggle_render = gtk_cell_renderer_toggle_new();
    g_object_set(toggle_render, "activatable", TRUE, NULL);
    g_signal_connect(toggle_render, "toggled", (GCallback) buttonToggled, GTK_TREE_MODEL(store));
    gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(view), -1, "Select", toggle_render, "active", column_runner, NULL);
    column_runner++;
  }
  gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(view), -1, "Name", renderer, "text", column_runner, NULL);
  column_runner++;
  gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(view), -1, "Country", renderer, "text", column_runner, NULL);
  column_runner++;
  gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(view), -1, "Lat/Lon", renderer, "text", column_runner, NULL);
  gtk_tree_view_set_headers_visible( GTK_TREE_VIEW(view), TRUE);
  gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(store));
  gtk_tree_selection_set_mode( gtk_tree_view_get_selection(GTK_TREE_VIEW(view)),
      multiple_selection_allowed ? GTK_SELECTION_MULTIPLE : GTK_SELECTION_BROWSE );
  g_object_unref(store);

  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), label, FALSE, FALSE, 0);
  gtk_widget_show ( label );
  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), view, FALSE, FALSE, 0);
  gtk_widget_show ( view );
  while ( gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT )
  {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    GList *selected_geonames = NULL;

    gtk_tree_model_get_iter_first( GTK_TREE_MODEL(store), &iter);
    geoname_runner = geonames;
    while (geoname_runner)
    {
      if (multiple_selection_allowed)
      {
        // nop;
      }
      else
      {
        if (gtk_tree_selection_iter_is_selected(selection, &iter))
        {
          found_geoname *copied = copy_found_geoname(geoname_runner->data);
          selected_geonames = g_list_prepend(selected_geonames, copied);
        }
      }
      geoname_runner = g_list_next(geoname_runner);
      gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
    }
    if (selected_geonames)
    { 
      gtk_widget_destroy ( dialog );
      return (selected_geonames);
    }
    a_dialog_error_msg(parent, _("Nothing was selected"));
  }
  gtk_widget_destroy ( dialog );
  return NULL;
}

static gboolean parse_file_for_latlon(VikWindow *vw, gchar *file_name, struct LatLon *ll)
{
  gchar *text, *pat;
  GMappedFile *mf;
  gsize len;
  gboolean more = TRUE;
  gboolean found = TRUE;
  gchar lat_buf[32], lon_buf[32];
  gchar *s;
  gint fragment_len;
  GList *found_places = NULL;
  found_geoname *geoname = NULL;
  gchar **found_entries;
  gchar *entry;
  int entry_runner;

  lat_buf[0] = lon_buf[0] = '\0';

  if ((mf = g_mapped_file_new(file_name, FALSE, NULL)) == NULL) {
    g_critical(_("couldn't map temp file"));
    exit(1);
  }
  len = g_mapped_file_get_length(mf);
  text = g_mapped_file_get_contents(mf);

  if (g_strstr_len(text, len, GEONAMES_SEARCH_NOT_FOUND) != NULL) {
    more = FALSE;
  }
  found_entries = g_strsplit(text, "},", 0);
  entry_runner = 0;
  entry = found_entries[entry_runner];
  while (entry)
  {
    more = TRUE;
    geoname = (found_geoname *)g_malloc(sizeof(found_geoname));
    if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_COUNTRY_PATTERN)) == NULL) {
      more = FALSE;
    }
    else {
      pat += strlen(GEONAMES_COUNTRY_PATTERN);
      fragment_len = 0;
      s = pat;
      while (*pat != '"') {
        fragment_len++;
        pat++;
      }
      geoname -> country = g_strndup(s, fragment_len);
    }
    if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_LONGITUDE_PATTERN)) == NULL) {
      more = FALSE;
    }
    else {
      pat += strlen(GEONAMES_LONGITUDE_PATTERN);
      s = lon_buf;
      if (*pat == '-')
        *s++ = *pat++;
      while ((s < (lon_buf + sizeof(lon_buf))) && (pat < (text + len)) &&
              (g_ascii_isdigit(*pat) || (*pat == '.')))
        *s++ = *pat++;
      *s = '\0';
      if ((pat >= (text + len)) || (lon_buf[0] == '\0')) {
        more = FALSE;
      }
      geoname->ll.lon = g_ascii_strtod(lon_buf, NULL);
    }
    if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_NAME_PATTERN)) == NULL) {
      more = FALSE;
    }
    else {
      pat += strlen(GEONAMES_NAME_PATTERN);
      fragment_len = 0;
      s = pat;
      while (*pat != '"') {
        fragment_len++;
        pat++;
      }
      geoname -> name = g_strndup(s, fragment_len);
    }
    if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_LATITUDE_PATTERN)) == NULL) {
      more = FALSE;
    }
    else {
      pat += strlen(GEONAMES_LATITUDE_PATTERN);
      s = lat_buf;
      if (*pat == '-')
        *s++ = *pat++;
      while ((s < (lat_buf + sizeof(lat_buf))) && (pat < (text + len)) &&
              (g_ascii_isdigit(*pat) || (*pat == '.')))
        *s++ = *pat++;
      *s = '\0';
      if ((pat >= (text + len)) || (lat_buf[0] == '\0')) {
        more = FALSE;
      }
      geoname->ll.lat = g_ascii_strtod(lat_buf, NULL);
    }
    if (!more) {
      if (geoname) {
        g_free(geoname);
      }
    }
    else {
      found_places = g_list_prepend(found_places, geoname);
    }
    entry_runner++;
    entry = found_entries[entry_runner];
  }
  g_strfreev(found_entries);
  if (g_list_length(found_places) == 1)
  {
    geoname = (found_geoname *)found_places->data;
    ll->lat = geoname->ll.lat;
    ll->lon = geoname->ll.lon;
  }
  else
  {
    found_places = g_list_reverse(found_places);
    GList *selected = a_select_geoname_from_list(VIK_GTK_WINDOW_FROM_WIDGET(vw), found_places, FALSE, "Select place", "Select the place to go to");
    if (selected)
    {
      geoname = (found_geoname *)selected->data;
      ll->lat = geoname->ll.lat;
      ll->lon = geoname->ll.lon;
      g_list_foreach(selected, (GFunc)free_list_geonames, NULL);
    }
    else
    {
      found = FALSE;
    }
  }
  g_list_foreach(found_places, (GFunc)free_list_geonames, NULL);
  g_list_free(found_places);
  g_mapped_file_free(mf);
  return (found);
}

gchar *uri_escape(gchar *str)
{
  gchar *esc_str = g_malloc(3*strlen(str));
  gchar *dst = esc_str;
  gchar *src;

  for (src = str; *src; src++) {
    if (*src == ' ')
     *dst++ = '+';
    else if (g_ascii_isalnum(*src))
     *dst++ = *src;
    else {
      g_sprintf(dst, "%%%02X", *src);
      dst += 3;
    }
  }
  *dst = '\0';

  return(esc_str);
}

static int geonames_search_get_coord(VikWindow *vw, VikViewport *vvp, gchar *srch_str, VikCoord *coord)
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
  //uri = g_strdup_printf(GEONAMES_SEARCH_URL_FMT, srch_str);
  uri = g_strdup_printf(GEONAMES_SEARCH_URL_FMT, escaped_srch_str);

  // TODO: curl may not be available
  if (curl_download_uri(uri, tmp_file, NULL)) {  // error
    fclose(tmp_file);
    tmp_file = NULL;
    ret = -1;
    goto done;
  }

  fclose(tmp_file);

  tmp_file = NULL;
  if (!parse_file_for_latlon(vw, tmpname, &ll)) {
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

void a_geonames_search(VikWindow *vw, VikLayersPanel *vlp, VikViewport *vvp)
{
  VikCoord new_center;
  gchar *s_str;
  gboolean more = TRUE;

  do {
    s_str = a_prompt_for_search_string(vw);
    if ((!s_str) || (s_str[0] == 0)) {
      more = FALSE;
    }
    else if (!geonames_search_get_coord(vw, vvp, s_str, &new_center)) {
      vik_viewport_set_center_coord(vvp, &new_center);
      vik_layers_panel_emit_update(vlp);
      more = FALSE;
    }
/*
    else if (!prompt_try_again(vw))
        more = FALSE;
*/
    g_free(s_str);
  } while (more);
}

