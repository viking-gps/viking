/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2009, Hein Ragas
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

#define GEONAMES_WIKIPEDIA_URL_FMT "http://ws.geonames.org/wikipediaBoundingBoxJSON?formatted=true&north=%s&south=%s&east=%s&west=%s"
#define GEONAMES_COUNTRY_PATTERN "\"countryName\": \""
#define GEONAMES_LONGITUDE_PATTERN "\"lng\": "
#define GEONAMES_NAME_PATTERN "\"name\": \""
#define GEONAMES_LATITUDE_PATTERN "\"lat\": "
#define GEONAMES_TITLE_PATTERN "\"title\": \""
#define GEONAMES_WIKIPEDIAURL_PATTERN "\"wikipediaUrl\": \""
#define GEONAMES_THUMBNAILIMG_PATTERN "\"thumbnailImg\": \""
#define GEONAMES_SEARCH_NOT_FOUND "not understand the location"

/* found_geoname: Type to contain data returned from GeoNames.org */

typedef struct {
  gchar *name;
  gchar *country;
  struct LatLon ll;
  gchar *desc;
} found_geoname;

found_geoname *new_found_geoname()
{
  found_geoname *ret;

  ret = (found_geoname *)g_malloc(sizeof(found_geoname));
  ret->name = NULL;
  ret->country = NULL;
  ret->desc = NULL;
  ret->ll.lat = 0.0;
  ret->ll.lon = 0.0;
  return(ret);
}

found_geoname *copy_found_geoname(found_geoname *src)
{
  found_geoname *dest = new_found_geoname();
  dest->name = g_strdup(src->name);
  dest->country = g_strdup(src->country);
  dest->ll.lat = src->ll.lat;
  dest->ll.lon = src->ll.lon;
  dest->desc = g_strdup(src->desc);
  return(dest);
}

static void free_list_geonames(found_geoname *geoname, gpointer userdata)
{
  g_free(geoname->name);
  g_free(geoname->country);
  g_free(geoname->desc);
}

void free_geoname_list(GList *found_places)
{
  g_list_foreach(found_places, (GFunc)free_list_geonames, NULL);
  g_list_free(found_places);
}

static void none_found(VikWindow *vw)
{
  GtkWidget *dialog = NULL;

  dialog = gtk_dialog_new_with_buttons ( "", GTK_WINDOW(vw), 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL );
  gtk_window_set_title(GTK_WINDOW(dialog), _("Search"));

  GtkWidget *search_label = gtk_label_new(_("No entries found!"));
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(dialog)->vbox), search_label, FALSE, FALSE, 5 );
  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
  gtk_widget_show_all(dialog);

  gtk_dialog_run ( GTK_DIALOG(dialog) );
  gtk_widget_destroy(dialog);
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
  gboolean checked;
  gboolean to_copy;

  GtkWidget *dialog = gtk_dialog_new_with_buttons (title,
                                                  parent,
                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_REJECT,
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_ACCEPT,
                                                  NULL);
  /* When something is selected then OK */
  gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
  GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
  /* Default to not apply - as initially nothing is selected! */
  response_w = gtk_dialog_get_widget_for_response ( GTK_DIALOG(dialog), GTK_RESPONSE_REJECT );
#endif
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
  if ( response_w )
    gtk_widget_grab_focus ( response_w );
  while ( gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT )
  {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    GList *selected_geonames = NULL;

    gtk_tree_model_get_iter_first( GTK_TREE_MODEL(store), &iter);
    geoname_runner = geonames;
    while (geoname_runner)
    {
      to_copy = FALSE;
      if (multiple_selection_allowed)
      {
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 0, &checked, -1);
        if (checked) {
          to_copy = TRUE;
        }
      }
      else
      {
        if (gtk_tree_selection_iter_is_selected(selection, &iter))
        {
          to_copy = TRUE;
        }
      }
      if (to_copy) {
        found_geoname *copied = copy_found_geoname(geoname_runner->data);
        selected_geonames = g_list_prepend(selected_geonames, copied);
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

GList *get_entries_from_file(gchar *file_name)
{
  gchar *text, *pat;
  GMappedFile *mf;
  gsize len;
  gboolean more = TRUE;
  gchar lat_buf[32], lon_buf[32];
  gchar *s;
  gint fragment_len;
  GList *found_places = NULL;
  found_geoname *geoname = NULL;
  gchar **found_entries;
  gchar *entry;
  int entry_runner;
  gchar *wikipedia_url = NULL;
  gchar *thumbnail_url = NULL;

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
    geoname = new_found_geoname();
    if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_COUNTRY_PATTERN))) {
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
    if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_NAME_PATTERN))) {
      pat += strlen(GEONAMES_NAME_PATTERN);
      fragment_len = 0;
      s = pat;
      while (*pat != '"') {
        fragment_len++;
        pat++;
      }
      geoname -> name = g_strndup(s, fragment_len);
    }
    if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_TITLE_PATTERN))) {
      pat += strlen(GEONAMES_TITLE_PATTERN);
      fragment_len = 0;
      s = pat;
      while (*pat != '"') {
        fragment_len++;
        pat++;
      }
      geoname -> name = g_strndup(s, fragment_len);
    }
    if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_WIKIPEDIAURL_PATTERN))) {
      pat += strlen(GEONAMES_WIKIPEDIAURL_PATTERN);
      fragment_len = 0;
      s = pat;
      while (*pat != '"') {
        fragment_len++;
        pat++;
      }
      wikipedia_url = g_strndup(s, fragment_len);
    }
    if ((pat = g_strstr_len(entry, strlen(entry), GEONAMES_THUMBNAILIMG_PATTERN))) {
      pat += strlen(GEONAMES_THUMBNAILIMG_PATTERN);
      fragment_len = 0;
      s = pat;
      while (*pat != '"') {
        fragment_len++;
        pat++;
      }
      thumbnail_url = g_strndup(s, fragment_len);
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
      if (wikipedia_url) {
        if (thumbnail_url) {
          geoname -> desc = g_strdup_printf("<a href=\"http://%s\" target=\"_blank\"><img src=\"%s\" border=\"0\"/></a>", wikipedia_url, thumbnail_url);
        }
        else {
          geoname -> desc = g_strdup_printf("<a href=\"http://%s\" target=\"_blank\">%s</a>", wikipedia_url, geoname->name);
        }
      }
      if (wikipedia_url) {
        g_free(wikipedia_url);
        wikipedia_url = NULL;
      }
      if (thumbnail_url) {
        g_free(thumbnail_url);
        thumbnail_url = NULL;
      }
      found_places = g_list_prepend(found_places, geoname);
    }
    entry_runner++;
    entry = found_entries[entry_runner];
  }
  g_strfreev(found_entries);
  found_places = g_list_reverse(found_places);
  g_mapped_file_free(mf);
  return(found_places);
}


gchar *download_url(gchar *uri)
{
  FILE *tmp_file;
  int tmp_fd;
  gchar *tmpname;

  if ((tmp_fd = g_file_open_tmp ("vikgsearch.XXXXXX", &tmpname, NULL)) == -1) {
    g_critical(_("couldn't open temp file"));
    exit(1);
  }
  tmp_file = fdopen(tmp_fd, "r+");

  // TODO: curl may not be available
  if (curl_download_uri(uri, tmp_file, NULL, 0, NULL)) {  // error
    fclose(tmp_file);
    tmp_file = NULL;
    g_remove(tmpname);
    g_free(tmpname);
    return(NULL);
  }
  fclose(tmp_file);
  tmp_file = NULL;
  return(tmpname);
}

void a_geonames_wikipedia_box(VikWindow *vw, VikTrwLayer *vtl, VikLayersPanel *vlp, struct LatLon maxmin[2])
{
  gchar *uri;
  gchar *tmpname;
  GList *wiki_places;
  GList *selected;
  GList *wp_runner;
  VikWaypoint *wiki_wp;
  found_geoname *wiki_geoname;

  /* encode doubles in a C locale */
  gchar *north = a_coords_dtostr(maxmin[0].lat);
  gchar *south = a_coords_dtostr(maxmin[1].lat);
  gchar *east = a_coords_dtostr(maxmin[0].lon);
  gchar *west = a_coords_dtostr(maxmin[1].lon);
  uri = g_strdup_printf(GEONAMES_WIKIPEDIA_URL_FMT, north, south, east, west);
  g_free(north); north = NULL;
  g_free(south); south = NULL;
  g_free(east);  east = NULL;
  g_free(west);  west = NULL;
  tmpname = download_url(uri);
  if (!tmpname) {
    none_found(vw);
    return;
  }
  wiki_places = get_entries_from_file(tmpname);
  if (g_list_length(wiki_places) == 0) {
    none_found(vw);
    return;
  }
  selected = a_select_geoname_from_list(VIK_GTK_WINDOW_FROM_WIDGET(vw), wiki_places, TRUE, "Select articles", "Select the articles you want to add.");
  wp_runner = selected;
  while (wp_runner) {
    wiki_geoname = (found_geoname *)wp_runner->data;
    wiki_wp = vik_waypoint_new();
    wiki_wp->visible = TRUE;
    vik_coord_load_from_latlon(&(wiki_wp->coord), vik_trw_layer_get_coord_mode ( vtl ), &(wiki_geoname->ll));
    vik_waypoint_set_comment(wiki_wp, wiki_geoname->desc);
    vik_trw_layer_filein_add_waypoint ( vtl, wiki_geoname->name, wiki_wp );
    wp_runner = g_list_next(wp_runner);
  }
  free_geoname_list(wiki_places);
  free_geoname_list(selected);
  g_free(uri);
  if (tmpname) {
    g_free(tmpname);
  }
  vik_layers_panel_emit_update(vlp);
}
