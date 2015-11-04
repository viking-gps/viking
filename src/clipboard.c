/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005, Alex Foobarian <foobarian@gmail.com>
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

#include <string.h>
#include <stdlib.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib/gi18n.h>

#include "viking.h"


typedef struct {
  gpointer clipboard;
  gint pid;
  VikClipboardDataType type;
  gint subtype;
  guint16 layer_type;
  guint len;
  gchar *text;
  guint8 data[0];
} vik_clipboard_t;

static GtkTargetEntry target_table[] = {
  { "application/viking", 0, 0 },
  { "STRING", 0, 1 },
};

/*****************************************************************
 ** functions which send to the clipboard client (we are owner) **
 *****************************************************************/

static void clip_get ( GtkClipboard *c, GtkSelectionData *selection_data, guint info, gpointer p ) 
{
  vik_clipboard_t *vc = p;
  if ( info == 0 ) {
    // Viking Data Type
    //    g_print("clip_get: vc = %p, size = %d\n", vc, sizeof(*vc) + vc->len);
    gtk_selection_data_set ( selection_data, gtk_selection_data_get_target(selection_data), 8, (void *)vc, sizeof(*vc) + vc->len );
  }
  if ( info == 1 ) {
    // Should be a string, but make sure it's something
    if ( vc->text )
      gtk_selection_data_set_text ( selection_data, vc->text, -1); // string text is null terminated
  }

}

static void clip_clear ( GtkClipboard *c, gpointer p )
{
  vik_clipboard_t* vc = (vik_clipboard_t*)p;
  g_free(vc->text);
  g_free(vc);
}


/**************************************************************************
 ** functions which receive from the clipboard owner (we are the client) **
 **************************************************************************/

/* our own data type */
static void clip_receive_viking ( GtkClipboard *c, GtkSelectionData *sd, gpointer p ) 
{
  VikLayersPanel *vlp = p;
  vik_clipboard_t *vc;
  if (gtk_selection_data_get_length(sd) == -1) {
    g_warning ( _("paste failed") );
    return;
  } 
  //  g_print("clip receive: target = %s, type = %s\n", gdk_atom_name(gtk_selection_data_get_target(sd), gdk_atom_name(sd->type));
  //g_assert(!strcmp(gdk_atom_name(gtk_selection_data_get_target(sd)), target_table[0].target));

  vc = (vik_clipboard_t *)gtk_selection_data_get_data(sd);
  //  g_print("  sd->data = %p, sd->length = %d, vc->len = %d\n", sd->data, sd->length, vc->len);

  if (gtk_selection_data_get_length(sd) != sizeof(*vc) + vc->len) {
    g_warning ( _("wrong clipboard data size") );
    return;
  }

  if ( vc->type == VIK_CLIPBOARD_DATA_LAYER )
  {
    VikLayer *new_layer = vik_layer_unmarshall ( vc->data, vc->len, vik_layers_panel_get_viewport(vlp) );
    vik_layers_panel_add_layer ( vlp, new_layer );
  }
  else if ( vc->type == VIK_CLIPBOARD_DATA_SUBLAYER )
  {
    VikLayer *sel = vik_layers_panel_get_selected ( vlp );
    if ( sel && sel->type == vc->layer_type)
    {
      if ( vik_layer_get_interface(vc->layer_type)->paste_item )
        vik_layer_get_interface(vc->layer_type)->paste_item ( sel, vc->subtype, vc->data, vc->len);
    }
    else
      a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_WIDGET(GTK_WIDGET(vlp)),
				 _("The clipboard contains sublayer data for %s layers. "
				   "You must select a layer of this type to paste the clipboard data."),
				 vik_layer_get_interface(vc->layer_type)->name );
  }
}



/**
 * clip_parse_latlon:
 * @text: text containing LatLon data.
 * @coord: computed coordinates.
 *
 * Utility func to handle pasted text:
 * search for N dd.dddddd W dd.dddddd, N dd° dd.dddd W dd° dd.ddddd and so forth.
 *
 * Returns: TRUE if coordinates are set.
 */
static gboolean clip_parse_latlon ( const gchar *text, struct LatLon *coord ) 
{
  gint latdeg, londeg, latmi, lonmi;
  gdouble lats, lons;
  gdouble latm, lonm;
  gdouble lat, lon;
  gchar *cand;
  gint len, i;
  gchar *s = g_strdup(text);

  //  g_print("parsing %s\n", s);

  len = strlen(s);

  /* remove non-digits following digits; gets rid of degree symbols or whatever people use, and 
   * punctuation
   */
  for (i=0; i<len-2; i++) {
    if (g_ascii_isdigit (s[i]) && s[i+1] != '.' && !g_ascii_isdigit (s[i+1])) {
      s[i+1] = ' ';
      if (!g_ascii_isalnum (s[i+2]) && s[i+2] != '-') {
        s[i+2] = ' ';
      }
    }
  }

  /* now try reading coordinates */
  for (i=0; i<len; i++) {
    if (s[i] == 'N' || s[i] == 'S' || g_ascii_isdigit (s[i])) {
      gchar latc[2] = "SN";
      gchar lonc[2] = "WE";
      gint j, k;
      cand = s+i;

      // First try matching strings containing the cardinal directions
      for (j=0; j<2; j++) {
        for (k=0; k<2; k++) {
          // DMM
          gchar fmt1[] = "N %d%*[ ]%lf W %d%*[ ]%lf";
          gchar fmt2[] = "%d%*[ ]%lf N %d%*[ ]%lf W";
          // DDD
          gchar fmt3[] = "N %lf W %lf";
          gchar fmt4[] = "%lf N %lf W";
          // DMS
          gchar fmt5[] = "N%d%*[ ]%d%*[ ]%lf%*[ ]W%d%*[ ]%d%*[ ]%lf";

          // Substitute in 'N','E','S' or 'W' values for each attempt
          fmt1[0]  = latc[j];	  fmt1[13] = lonc[k];
          fmt2[11] = latc[j];	  fmt2[24] = lonc[k];
          fmt3[0]  = latc[j];	  fmt3[6]  = lonc[k];
          fmt4[4]  = latc[j];	  fmt4[10] = lonc[k];
          fmt5[0]  = latc[j];	  fmt5[23] = lonc[k];

          if (sscanf(cand, fmt1, &latdeg, &latm, &londeg, &lonm) == 4 ||
              sscanf(cand, fmt2, &latdeg, &latm, &londeg, &lonm) == 4) {
            lat = (j*2-1) * (latdeg + latm / 60);
            lon = (k*2-1) * (londeg + lonm / 60);
            break;
          }
          if (sscanf(cand, fmt3, &lat, &lon) == 2 ||
              sscanf(cand, fmt4, &lat, &lon) == 2) {
            lat *= (j*2-1);
            lon *= (k*2-1);
            break;
          }
          gint am = sscanf(cand, fmt5, &latdeg, &latmi, &lats, &londeg, &lonmi, &lons);
          if ( am == 6 ) {
            lat = (j*2-1) * (latdeg + latmi / 60.0 + lats / 3600.0);
            lon = (k*2-1) * (londeg + lonmi / 60.0 + lons / 3600.0);
            break;
          }
        }
        if (k!=2) break;
      }
      if (j!=2) break;

      // DMM without Cardinal directions
      if (sscanf(cand, "%d%*[ ]%lf*[ ]%d%*[ ]%lf", &latdeg, &latm, &londeg, &lonm) == 4) {
        lat = latdeg/abs(latdeg) * (abs(latdeg) + latm / 60);
        lon = londeg/abs(londeg) * (abs(londeg) + lonm / 60);
        break;
      }
      // DMS without Cardinal directions
      if (sscanf(cand, "%d%*[ ]%d%*[ ]%lf%*[ ]%d%*[ ]%d%*[ ]%lf", &latdeg, &latmi, &lats, &londeg, &lonmi, &lons) == 6) {
        lat = latdeg/abs(latdeg) * (abs(latdeg) + latm / 60.0 + lats / 3600.0);
        lon = londeg/abs(londeg) * (abs(londeg) + lonm / 60.0 + lons / 3600.0);
        break;
      }
      // Raw values
      if (sscanf(cand, "%lf %lf", &lat, &lon) == 2) {
        break;
      }
    }
  }
  g_free(s);

  /* did we get to the end without actually finding a coordinate? */
  if (i<len && lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0) {
    coord->lat = lat;
    coord->lon = lon;
    return TRUE;
  }
  return FALSE;
}

static void clip_add_wp(VikLayersPanel *vlp, struct LatLon *coord) 
{
  VikCoord vc;
  VikLayer *sel = vik_layers_panel_get_selected ( vlp );


  vik_coord_load_from_latlon ( &vc, VIK_COORD_LATLON, coord );

  if (sel && sel->type == VIK_LAYER_TRW) {
    vik_trw_layer_new_waypoint ( VIK_TRW_LAYER(sel), VIK_GTK_WINDOW_FROM_LAYER(sel), &vc );
    trw_layer_calculate_bounds_waypoints ( VIK_TRW_LAYER(sel) );
    vik_layer_emit_update ( VIK_LAYER(sel) );
  } else {
    a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_WIDGET(GTK_WIDGET(vlp)), _("In order to paste a waypoint, please select an appropriate layer to paste into."), NULL);
  }
}

static void clip_receive_text (GtkClipboard *c, const gchar *text, gpointer p)
{
  VikLayersPanel *vlp = p;

  g_debug ( "got text: %s", text );

  VikLayer *sel = vik_layers_panel_get_selected ( vlp );
  if ( sel && vik_treeview_get_editing ( sel->vt ) ) {
    GtkTreeIter iter;
    if ( vik_treeview_get_selected_iter ( sel->vt, &iter ) ) {
      // Try to sanitize input:
      gchar *name = g_strescape ( text, NULL );
      vik_layer_rename ( sel, name );
      vik_treeview_item_set_name ( sel->vt, &iter, name );
      g_free ( name );
    }
    return;
  }

  struct LatLon coord;
  if (clip_parse_latlon(text, &coord)) {
    clip_add_wp(vlp, &coord);
  }
}

static void clip_receive_html ( GtkClipboard *c, GtkSelectionData *sd, gpointer p ) 
{
  VikLayersPanel *vlp = p;
  gsize r, w;
  GError *err = NULL;
  gchar *s, *span;
  gint tag = 0, i;
  struct LatLon coord;

  if (gtk_selection_data_get_length(sd) == -1) {
    return;
  } 

  /* - copying from Mozilla seems to give html in UTF-16. */
  if (!(s =  g_convert ( (gchar *)gtk_selection_data_get_data(sd), gtk_selection_data_get_length(sd), "UTF-8", "UTF-16", &r, &w, &err))) {
    return;
  }
  //  g_print("html is %d bytes long: %s\n", gtk_selection_data_get_length(sd), s);

  /* scrape a coordinate pasted from a geocaching.com page: look for a 
   * telltale tag if possible, and then remove tags 
   */
  if (!(span = g_strstr_len(s, w, "<span id=\"LatLon\">"))) {
    span = s;
  }
  for (i=0; i<strlen(span); i++) {
    gchar ch = span[i];
    if (ch == '<') {
      tag++;
    }
    if (tag>0) {
      span[i] = ' ';
    }
    if (ch == '>') {
      if (tag>0) tag--;
    }
  }
  if (clip_parse_latlon(span, &coord)) {
    clip_add_wp(vlp, &coord);
  }

  g_free(s);
}

/**
 * clip_receive_targets:
 *
 * Deal with various data types a clipboard may hold.
 */
void clip_receive_targets ( GtkClipboard *c, GdkAtom *a, gint n, gpointer p )
{
  VikLayersPanel *vlp = p;
  gint i;

  for (i=0; i<n; i++) {
    gchar* name = gdk_atom_name(a[i]);
    //g_print("  ""%s""\n", name);
    gboolean breaktime = FALSE;
    if (!g_strcmp0(name, "text/html")) {
      gtk_clipboard_request_contents ( c, gdk_atom_intern("text/html", TRUE), clip_receive_html, vlp );
      breaktime = TRUE;
    }
    if (a[i] == GDK_TARGET_STRING) {
      gtk_clipboard_request_text ( c, clip_receive_text, vlp );
      breaktime = TRUE;
    }
    if (!g_strcmp0(name, "application/viking")) {
      gtk_clipboard_request_contents ( c, gdk_atom_intern("application/viking", TRUE), clip_receive_viking, vlp );
      breaktime = TRUE;
    }

    g_free ( name );

    if ( breaktime )
      break;
  }
}

/*********************************************************************************
 ** public functions                                                            **
 *********************************************************************************/

/**
 * a_clipboard_copy_selected:
 *
 * Make a copy of selected object and associate ourselves with the clipboard.
 */
void a_clipboard_copy_selected ( VikLayersPanel *vlp )
{
  VikLayer *sel = vik_layers_panel_get_selected ( vlp );
  GtkTreeIter iter;
  VikClipboardDataType type = VIK_CLIPBOARD_DATA_NONE;
  guint16 layer_type = 0;
  gint subtype = 0;
  guint8 *data = NULL;
  guint len = 0;
  const gchar *name = NULL;

  if ( ! sel )
    return;

  if ( !vik_treeview_get_selected_iter ( sel->vt, &iter ) )
    return;
  layer_type = sel->type;

  // Since we intercept copy and paste keyboard operations, this is called even when a cell is being edited
  if ( vik_treeview_get_editing ( sel->vt ) ) {
    type = VIK_CLIPBOARD_DATA_TEXT;
    //  I don't think we can access what is actually selected (internal to GTK) so we go for the name of the item
    // At least this is better than copying the layer data - which is even further away from what the user would be expecting...
    name = vik_treeview_item_get_name ( sel->vt, &iter );
    len = 0;
  }
  else {
    if ( vik_treeview_item_get_type ( sel->vt, &iter ) == VIK_TREEVIEW_TYPE_SUBLAYER ) {
      type = VIK_CLIPBOARD_DATA_SUBLAYER;
      if ( vik_layer_get_interface(layer_type)->copy_item) {
        subtype = vik_treeview_item_get_data(sel->vt, &iter);
        vik_layer_get_interface(layer_type)->copy_item(sel, subtype, vik_treeview_item_get_pointer(sel->vt, &iter), &data, &len );
        // This name is used in setting the text representation of the item on the clipboard.
        name = vik_treeview_item_get_name(sel->vt, &iter);
      }
    }
    else {
      gint ilen;
      type = VIK_CLIPBOARD_DATA_LAYER;
      vik_layer_marshall ( sel, &data, &ilen );
      len = ilen;
      name = vik_layer_get_name ( vik_treeview_item_get_pointer(sel->vt, &iter) );
    }
  }
  a_clipboard_copy ( type, layer_type, subtype, len, name, data );
}

void a_clipboard_copy( VikClipboardDataType type, guint16 layer_type, gint subtype, guint len, const gchar* text, guint8 * data)
{
  vik_clipboard_t * vc = g_malloc(sizeof(*vc) + len);
  GtkClipboard *c = gtk_clipboard_get ( GDK_SELECTION_CLIPBOARD );

  vc->type = type;
  vc->layer_type = layer_type;
  vc->subtype = subtype;
  vc->len = len;
  vc->text = g_strdup (text);
  if ( data ) {
    memcpy(vc->data, data, len);
    g_free(data);
  }
  vc->pid = getpid();

  // Simple clipboard copy when necessary
  if ( type == VIK_CLIPBOARD_DATA_TEXT )
    gtk_clipboard_set_text ( c, text, -1 );
  else
    gtk_clipboard_set_with_data ( c, target_table, G_N_ELEMENTS(target_table), clip_get, clip_clear, vc );
}

/**
 * a_clipboard_paste:
 *
 * To deal with multiple data types, we first request the type of data on the clipboard,
 * and handle them in the callback.
 */
gboolean a_clipboard_paste ( VikLayersPanel *vlp )
{
  GtkClipboard *c = gtk_clipboard_get ( GDK_SELECTION_CLIPBOARD );
  gtk_clipboard_request_targets ( c, clip_receive_targets, vlp );
  return TRUE;
}

/**
 *
 * Detect our own data types
 */
static void clip_determine_viking_type ( GtkClipboard *c, GtkSelectionData *sd, gpointer p )
{
  VikClipboardDataType *vdct = p;
  // Default value
  *vdct = VIK_CLIPBOARD_DATA_NONE;
  vik_clipboard_t *vc;
  if (gtk_selection_data_get_length(sd) == -1) {
    g_warning ("DETERMINING TYPE: length failure");
    return;
  }

  vc = (vik_clipboard_t *)gtk_selection_data_get_data(sd);

  if ( !vc->type )
    return;

  if ( vc->type == VIK_CLIPBOARD_DATA_LAYER ) {
    *vdct = VIK_CLIPBOARD_DATA_LAYER;
  }
  else if ( vc->type == VIK_CLIPBOARD_DATA_SUBLAYER ) {
    *vdct = VIK_CLIPBOARD_DATA_SUBLAYER;
  }
  else {
    g_warning ("DETERMINING TYPE: THIS SHOULD NEVER HAPPEN");
  }
}

static void clip_determine_type ( GtkClipboard *c, GdkAtom *a, gint n, gpointer p )
{
  gint i;
  for (i=0; i<n; i++) {
    gchar *name = gdk_atom_name(a[i]);
    // g_print("  ""%s""\n", name);
    gboolean breaktime = FALSE;
    if (!g_strcmp0(name, "application/viking")) {
      gtk_clipboard_request_contents ( c, gdk_atom_intern("application/viking", TRUE), clip_determine_viking_type, p );
      breaktime = TRUE;
    }

    g_free ( name );

    if ( breaktime )
      break;
  }
}

/**
 * a_clipboard_type:
 *
 * Return the type of data held in the clipboard if any
 */
VikClipboardDataType a_clipboard_type ( )
{
  GtkClipboard *c = gtk_clipboard_get ( GDK_SELECTION_CLIPBOARD );
  VikClipboardDataType *vcdt = g_malloc ( sizeof (VikClipboardDataType) );

  gtk_clipboard_request_targets ( c, clip_determine_type, vcdt );
  gint answer = *vcdt;
  g_free ( vcdt );
  return answer;
}
