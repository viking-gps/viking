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

#include "viking.h"
#include "vikgpslayer_pixmap.h"
#include "babel.h"

#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>

#define DISCONNECT_UPDATE_SIGNAL(vl, val) g_signal_handlers_disconnect_matched(vl, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, val)
static VikGpsLayer *vik_gps_layer_create (VikViewport *vp);
static void vik_gps_layer_realize ( VikGpsLayer *val, VikTreeview *vt, GtkTreeIter *layer_iter );
static void vik_gps_layer_free ( VikGpsLayer *val );
static void vik_gps_layer_draw ( VikGpsLayer *val, gpointer data );
VikGpsLayer *vik_gps_layer_new ();

static VikGpsLayer *gps_layer_copy ( VikGpsLayer *val, gpointer vp );
static void gps_layer_marshall( VikGpsLayer *val, guint8 **data, gint *len );
static VikGpsLayer *gps_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp );
static gboolean gps_layer_set_param ( VikGpsLayer *vgl, guint16 id, VikLayerParamData data, VikViewport *vp );
static VikLayerParamData gps_layer_get_param ( VikGpsLayer *vgl, guint16 id );

static void gps_layer_change_coord_mode ( VikGpsLayer *val, VikCoordMode mode );
static void gps_layer_add_menu_items( VikGpsLayer *vtl, GtkMenu *menu, gpointer vlp );
static void gps_layer_drag_drop_request ( VikGpsLayer *val_src, VikGpsLayer *val_dest, GtkTreeIter *src_item_iter, GtkTreePath *dest_path );


static void gps_upload_cb( gpointer layer_and_vlp[2] );
static void gps_download_cb( gpointer layer_and_vlp[2] );

typedef enum {GARMIN_P = 0, MAGELLAN_P, NUM_PROTOCOLS} vik_gps_proto;
static gchar * params_protocols[] = {"Garmin", "Magellan", NULL};
static gchar * protocols_args[]   = {"garmin", "magellan"};
/*#define NUM_PROTOCOLS (sizeof(params_protocols)/sizeof(params_protocols[0]) - 1) */
static gchar * params_ports[] = {"/dev/ttyS0", "/dev/ttyS1", "/dev/ttyUSB0", "/dev/ttyUSB1", "usb:", NULL};
#define NUM_PORTS (sizeof(params_ports)/sizeof(params_ports[0]) - 1)
typedef enum {GPS_DOWN=0, GPS_UP} gps_dir;

typedef struct {
  GMutex *mutex;
  gps_dir direction;
  gchar *port;
  gboolean ok;
  gint total_count;
  gint count;
  VikTrwLayer *vtl;
  gchar *cmd_args;
  gchar * window_title;
  GtkWidget *dialog;
  GtkWidget *status_label;
  GtkWidget *gps_label;
  GtkWidget *ver_label;
  GtkWidget *id_label;
  GtkWidget *wp_label;
  GtkWidget *progress_label;
  GtkWidget *trk_label;
} GpsSession;
static void gps_session_delete(GpsSession *sess);

static VikLayerParam gps_layer_params[] = {
  { "gps_protocol", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, "GPS Protocol:", VIK_LAYER_WIDGET_COMBOBOX, params_protocols, NULL},
  { "gps_port", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, "Serial Port:", VIK_LAYER_WIDGET_COMBOBOX, params_ports, NULL},
};
enum {PARAM_PROTOCOL=0, PARAM_PORT, NUM_PARAMS};

VikLayerInterface vik_gps_layer_interface = {
  "GPS",
  &gpslayer_pixbuf,

  NULL,
  0,

  gps_layer_params,
  NUM_PARAMS,
  NULL,
  0,

  VIK_MENU_ITEM_ALL & ~(VIK_MENU_ITEM_CUT|VIK_MENU_ITEM_COPY),

  (VikLayerFuncCreate)                  vik_gps_layer_create,
  (VikLayerFuncRealize)                 vik_gps_layer_realize,
  (VikLayerFuncPostRead)                NULL,
  (VikLayerFuncFree)                    vik_gps_layer_free,

  (VikLayerFuncProperties)              NULL,
  (VikLayerFuncDraw)                    vik_gps_layer_draw,
  (VikLayerFuncChangeCoordMode)         gps_layer_change_coord_mode,

  (VikLayerFuncSetMenuItemsSelection)   NULL,
  (VikLayerFuncGetMenuItemsSelection)   NULL,

  (VikLayerFuncAddMenuItems)            gps_layer_add_menu_items,
  (VikLayerFuncSublayerAddMenuItems)    NULL,

  (VikLayerFuncSublayerRenameRequest)   NULL,
  (VikLayerFuncSublayerToggleVisible)   NULL,

  (VikLayerFuncCopy)                    gps_layer_copy,
  (VikLayerFuncMarshall)		gps_layer_marshall,
  (VikLayerFuncUnmarshall)		gps_layer_unmarshall,

  (VikLayerFuncSetParam)                gps_layer_set_param,
  (VikLayerFuncGetParam)                gps_layer_get_param,

  (VikLayerFuncReadFileData)            NULL,
  (VikLayerFuncWriteFileData)           NULL,

  (VikLayerFuncDeleteItem)              NULL,
  (VikLayerFuncCopyItem)                NULL,
  (VikLayerFuncPasteItem)               NULL,
  (VikLayerFuncFreeCopiedItem)          NULL,
  (VikLayerFuncDragDropRequest)		gps_layer_drag_drop_request,
};

enum {TRW_DOWNLOAD, TRW_UPLOAD, NUM_TRW};
static gchar * trw_names[] = {"GPS Download", "GPS Upload"};
struct _VikGpsLayer {
  VikLayer vl;
  VikTrwLayer * trw_children[NUM_TRW];
  GList * children;	/* used only for read/write file */
  /* params */
  guint protocol_id;
  guint serial_port_id;
};

GType vik_gps_layer_get_type ()
{
  static GType val_type = 0;

  if (!val_type)
  {
    static const GTypeInfo val_info =
    {
      sizeof (VikGpsLayerClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      NULL, /* class init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikGpsLayer),
      0,
      NULL /* instance init */
    };
    val_type = g_type_register_static ( VIK_LAYER_TYPE, "VikGpsLayer", &val_info, 0 );
  }

  return val_type;
}

static VikGpsLayer *vik_gps_layer_create (VikViewport *vp)
{
  VikGpsLayer *rv = vik_gps_layer_new ();
  vik_layer_rename ( VIK_LAYER(rv), vik_gps_layer_interface.name );
  rv->trw_children[TRW_UPLOAD] = VIK_TRW_LAYER(vik_layer_create ( VIK_LAYER_TRW, vp, NULL, FALSE ));
  vik_layer_set_menu_items_selection(VIK_LAYER(rv->trw_children[TRW_UPLOAD]), VIK_MENU_ITEM_ALL & ~(VIK_MENU_ITEM_CUT|VIK_MENU_ITEM_DELETE));
  rv->trw_children[TRW_DOWNLOAD] = VIK_TRW_LAYER(vik_layer_create ( VIK_LAYER_TRW, vp, NULL, FALSE ));
  vik_layer_set_menu_items_selection(VIK_LAYER(rv->trw_children[TRW_DOWNLOAD]), VIK_MENU_ITEM_ALL & ~(VIK_MENU_ITEM_CUT|VIK_MENU_ITEM_DELETE));
  return rv;
}

static VikGpsLayer *gps_layer_copy ( VikGpsLayer *vgl, gpointer vp )
{
  VikGpsLayer *rv = vik_gps_layer_new ();
  int i;

  for (i = 0; i < NUM_TRW; i++) {
    rv->trw_children[i] = (VikTrwLayer *)vik_layer_copy(VIK_LAYER(vgl->trw_children[i]), vp);
    g_signal_connect_swapped ( G_OBJECT(rv->trw_children[i]), "update", G_CALLBACK(vik_layer_emit_update), rv );
  }

  return rv;
}

/* "Copy" */
static void gps_layer_marshall( VikGpsLayer *vgl, guint8 **data, gint *datalen )
{
  VikLayer *child_layer;
  guint8 *ld; 
  gint ll;
  GByteArray* b = g_byte_array_new ();
  gint len;
  gint i;

#define alm_append(obj, sz) 	\
  len = (sz);    		\
  g_byte_array_append ( b, (guint8 *)&len, sizeof(len) );	\
  g_byte_array_append ( b, (guint8 *)(obj), len );

  vik_layer_marshall_params(VIK_LAYER(vgl), &ld, &ll);
  alm_append(ld, ll);
  g_free(ld);

  for (i = 0; i < NUM_TRW; i++) {
    child_layer = VIK_LAYER(vgl->trw_children[i]);
    vik_layer_marshall(child_layer, &ld, &ll);
    if (ld) {
      alm_append(ld, ll);
      g_free(ld);
    }
  }
  *data = b->data;
  *datalen = b->len;
  g_byte_array_free(b, FALSE);
#undef alm_append
}

/* "Paste" */
static VikGpsLayer *gps_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp )
{
#define alm_size (*(gint *)data)
#define alm_next \
  len -= sizeof(gint) + alm_size; \
  data += sizeof(gint) + alm_size;
  
  VikGpsLayer *rv = vik_gps_layer_new();
  VikLayer *child_layer;
  gint i;

  vik_layer_unmarshall_params ( VIK_LAYER(rv), data+sizeof(gint), alm_size, vvp );
  alm_next;

  i = 0;
  while (len>0 && i < NUM_TRW) {
    child_layer = vik_layer_unmarshall ( data + sizeof(gint), alm_size, vvp );
    if (child_layer) {
      rv->trw_children[i++] = (VikTrwLayer *)child_layer;
      g_signal_connect_swapped ( G_OBJECT(child_layer), "update", G_CALLBACK(vik_layer_emit_update), rv );
    }
    alm_next;
  }
  //  g_print("gps_layer_unmarshall ended with len=%d\n", len);
  g_assert(len == 0);
  return rv;
#undef alm_size
#undef alm_next
}

static gboolean gps_layer_set_param ( VikGpsLayer *vgl, guint16 id, VikLayerParamData data, VikViewport *vp )
{
  switch ( id )
  {
    case PARAM_PROTOCOL:
      if (data.u < NUM_PROTOCOLS)
        vgl->protocol_id = data.u;
      else
        g_warning("Unknown GPS Protocol");
      break;
    case PARAM_PORT:
      if (data.u < NUM_PORTS)
        vgl->serial_port_id = data.u;
      else
        g_warning("Unknown serial port device");
      break;
  }

  return TRUE;
}

static VikLayerParamData gps_layer_get_param ( VikGpsLayer *vgl, guint16 id )
{
  VikLayerParamData rv;
  switch ( id )
  {
    case PARAM_PROTOCOL:
      rv.u = vgl->protocol_id;
      break;
    case PARAM_PORT:
      rv.u = vgl->serial_port_id;
      break;
    default:
      g_warning("gps_layer_get_param(): unknown parameter");
  }

  /* fprintf(stderr, "gps_layer_get_param() called\n"); */

  return rv;
}

VikGpsLayer *vik_gps_layer_new ()
{
  gint i;
  VikGpsLayer *vgl = VIK_GPS_LAYER ( g_object_new ( VIK_GPS_LAYER_TYPE, NULL ) );
  vik_layer_init ( VIK_LAYER(vgl), VIK_LAYER_GPS );
  for (i = 0; i < NUM_TRW; i++) {
    vgl->trw_children[i] = NULL;
  }
  vgl->children = NULL;

  /* Setting params here */
  vgl->protocol_id = 0;
  vgl->serial_port_id = 0;

  return vgl;
}

static void vik_gps_layer_draw ( VikGpsLayer *vgl, gpointer data )
{
  gint i;

  for (i = 0; i < NUM_TRW; i++) {
    vik_layer_draw((VikLayer*)(vgl->trw_children[i]), data);
  }
}

static void gps_layer_change_coord_mode ( VikGpsLayer *vgl, VikCoordMode mode )
{
  gint i;
  for (i = 0; i < NUM_TRW; i++) {
    vik_layer_change_coord_mode(VIK_LAYER(vgl->trw_children[i]), mode);
  }
}

static void gps_layer_add_menu_items( VikGpsLayer *vgl, GtkMenu *menu, gpointer vlp )
{
  static gpointer pass_along[2];
  GtkWidget *item;
  pass_along[0] = vgl;
  pass_along[1] = vlp;

  item = gtk_menu_item_new();
  gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
  gtk_widget_show ( item );

  item = gtk_menu_item_new_with_label ( "Upload to GPS" );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(gps_upload_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );

  item = gtk_menu_item_new_with_label ( "Download from GPS" );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(gps_download_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );

}

static void disconnect_layer_signal ( VikLayer *vl, VikGpsLayer *vgl )
{
  g_assert(DISCONNECT_UPDATE_SIGNAL(vl,vgl)==1);
}

static void vik_gps_layer_free ( VikGpsLayer *vgl )
{
  gint i;
  for (i = 0; i < NUM_TRW; i++) {
    if (vgl->vl.realized)
      disconnect_layer_signal(VIK_LAYER(vgl->trw_children[i]), vgl);
    g_object_unref(vgl->trw_children[i]);
  }
}

static void delete_layer_iter ( VikLayer *vl )
{
  if ( vl->realized )
    vik_treeview_item_delete ( vl->vt, &(vl->iter) );
}

gboolean vik_gps_layer_delete ( VikGpsLayer *vgl, GtkTreeIter *iter )
{
  gint i;
  VikLayer *l = VIK_LAYER( vik_treeview_item_get_pointer ( VIK_LAYER(vgl)->vt, iter ) );
  gboolean was_visible = l->visible;

  vik_treeview_item_delete ( VIK_LAYER(vgl)->vt, iter );
  for (i = 0; i < NUM_TRW; i++) {
    if (VIK_LAYER(vgl->trw_children[i]) == l)
      vgl->trw_children[i] = NULL;
  }
  g_assert(DISCONNECT_UPDATE_SIGNAL(l,vgl)==1);
  g_object_unref ( l );

  return was_visible;
}

#if 0
/* returns 0 == we're good, 1 == didn't find any layers, 2 == got rejected */
guint vik_gps_layer_tool ( VikGpsLayer *val, guint16 layer_type, VikToolInterfaceFunc tool_func, GdkEventButton *event, VikViewport *vvp )
{
  GList *iter = val->children;
  gboolean found_rej = FALSE;
  if (!iter)
    return FALSE;
  while (iter->next)
    iter = iter->next;

  while ( iter )
  {
    /* if this layer "accepts" the tool call */
    if ( VIK_LAYER(iter->data)->visible && VIK_LAYER(iter->data)->type == layer_type )
    {
      if ( tool_func ( VIK_LAYER(iter->data), event, vvp ) )
        return 0;
      else
        found_rej = TRUE;
    }

    /* recursive -- try the same for the child gps layer. */
    else if ( VIK_LAYER(iter->data)->visible && VIK_LAYER(iter->data)->type == VIK_LAYER_GPS )
    {
      gint rv = vik_gps_layer_tool(VIK_GPS_LAYER(iter->data), layer_type, tool_func, event, vvp);
      if ( rv == 0 )
        return 0;
      else if ( rv == 2 )
        found_rej = TRUE;
    }
    iter = iter->prev;
  }
  return found_rej ? 2 : 1; /* no one wanted to accept the tool call in this layer */
}
#endif 

static void vik_gps_layer_realize ( VikGpsLayer *vgl, VikTreeview *vt, GtkTreeIter *layer_iter )
{
  GtkTreeIter iter;
  int ix;

  for (ix = 0; ix < NUM_TRW; ix++) {
    VikLayer * trw = VIK_LAYER(vgl->trw_children[ix]);
    vik_treeview_add_layer ( VIK_LAYER(vgl)->vt, layer_iter, &iter,
        trw_names[ix], vgl, 
        trw, trw->type, trw->type );
    if ( ! trw->visible )
      vik_treeview_item_set_visible ( VIK_LAYER(vgl)->vt, &iter, FALSE );
    vik_layer_realize ( trw, VIK_LAYER(vgl)->vt, &iter );
    g_signal_connect_swapped ( G_OBJECT(trw), "update", G_CALLBACK(vik_layer_emit_update), vgl );
  }
}

const GList *vik_gps_layer_get_children ( VikGpsLayer *vgl )
{
  int i;

  if (vgl->children == NULL) {
    for (i = NUM_TRW - 1; i >= 0; i--)
      vgl->children = g_list_prepend(vgl->children, vgl->trw_children[i]);
  }
  return vgl->children;
}

gboolean vik_gps_layer_is_empty ( VikGpsLayer *vgl )
{
  if ( vgl->trw_children[0] )
    return FALSE;
  return TRUE;
}

static void gps_layer_drag_drop_request ( VikGpsLayer *val_src, VikGpsLayer *val_dest, GtkTreeIter *src_item_iter, GtkTreePath *dest_path )
{
  VikTreeview *vt = VIK_LAYER(val_src)->vt;
  VikLayer *vl = vik_treeview_item_get_pointer(vt, src_item_iter);
  GtkTreeIter dest_iter;
  gchar *dp;
  gboolean target_exists;

  /* DEBUG */
  fprintf(stderr, "gps_layer_drag_drop_request() called\n");

  dp = gtk_tree_path_to_string(dest_path);
  target_exists = vik_treeview_get_iter_from_path_str(vt, &dest_iter, dp);

  /* vik_gps_layer_delete unrefs, but we don't want that here.
   * we're still using the layer. */
  g_object_ref ( vl );
  vik_gps_layer_delete(val_src, src_item_iter);

#ifdef XXXXXXXXXXXXXXXXXXXXXXXXX
  TODO:
  if (target_exists) {
    vik_gps_layer_insert_layer(val_dest, vl, &dest_iter);
  } else {
    vik_gps_layer_insert_layer(val_dest, vl, NULL); /* append */
  }
#endif /* XXXXXXXXXXXXXXXXXXXXXXXXX */
  g_free(dp);
}

static void gps_session_delete(GpsSession *sess)
{
  /* TODO */
  g_mutex_free(sess->mutex);
  g_free(sess->cmd_args);

  g_free(sess);

}

static void set_total_count(gint cnt, GpsSession *sess)
{
  gchar s[128];
  gdk_threads_enter();
  g_mutex_lock(sess->mutex);
  if (sess->ok) {
    g_sprintf(s, "%s %d %s...",
        (sess->direction == GPS_DOWN) ? "Downloading" : "Uploading", cnt,
        (sess->progress_label == sess->wp_label) ? "waypoints" : "trackpoints");
    gtk_label_set_text ( GTK_LABEL(sess->progress_label), s );
    gtk_widget_show ( sess->progress_label );
    sess->total_count = cnt;
  }
  g_mutex_unlock(sess->mutex);
  gdk_threads_leave();
}

static void set_current_count(gint cnt, GpsSession *sess)
{
  gchar s[128];
  gchar *dir_str = (sess->direction == GPS_DOWN) ? "Downloaded" : "Uploaded";

  gdk_threads_enter();
  g_mutex_lock(sess->mutex);
  if (sess->ok) {
    if (cnt < sess->total_count) {
      g_sprintf(s, "%s %d out of %d %s...", dir_str, cnt, sess->total_count, (sess->progress_label == sess->wp_label) ? "waypoints" : "trackpoints");
    } else {
      g_sprintf(s, "%s %d %s.", dir_str, cnt, (sess->progress_label == sess->wp_label) ? "waypoints" : "trackpoints");
    }	  
    gtk_label_set_text ( GTK_LABEL(sess->progress_label), s );
  }
  g_mutex_unlock(sess->mutex);
  gdk_threads_leave();
}

static void set_gps_info(const gchar *info, GpsSession *sess)
{
  gchar s[256];
  gdk_threads_enter();
  g_mutex_lock(sess->mutex);
  if (sess->ok) {
    g_sprintf(s, "GPS Device: %s", info);
    gtk_label_set_text ( GTK_LABEL(sess->gps_label), s );
  }
  g_mutex_unlock(sess->mutex);
  gdk_threads_leave();
}

static void gps_download_progress_func(BabelProgressCode c, gpointer data, GpsSession * sess )
{
  gchar *line;

  gdk_threads_enter ();
  g_mutex_lock(sess->mutex);
  if (!sess->ok) {
    g_mutex_unlock(sess->mutex);
    gps_session_delete(sess);
    gdk_threads_leave();
    g_thread_exit ( NULL );
  }
  g_mutex_unlock(sess->mutex);
  gdk_threads_leave ();

  switch(c) {
  case BABEL_DIAG_OUTPUT:
    line = (gchar *)data;

    /* tells us how many items there will be */
    if (strstr(line, "Xfer Wpt")) { 
      sess->progress_label = sess->wp_label;
    }
    if (strstr(line, "Xfer Trk")) { 
      sess->progress_label = sess->trk_label;
    }
    if (strstr(line, "PRDDAT")) {
      gchar **tokens = g_strsplit(line, " ", 0);
      gchar info[128];
      int ilen = 0;
      int i;

      for (i=8; tokens[i] && ilen < sizeof(info)-2 && strcmp(tokens[i], "00"); i++) {
	guint ch;
	sscanf(tokens[i], "%x", &ch);
	info[ilen++] = ch;
      }
      info[ilen++] = 0;
      set_gps_info(info, sess);
    }
    if (strstr(line, "RECORD")) { 
      int lsb, msb, cnt;

      sscanf(line+17, "%x", &lsb); 
      sscanf(line+20, "%x", &msb);
      cnt = lsb + msb * 256;
      set_total_count(cnt, sess);
      sess->count = 0;
    }
    if ( strstr(line, "WPTDAT") || strstr(line, "TRKHDR") || strstr(line, "TRKDAT") ) {
      sess->count++;
      set_current_count(sess->count, sess);
    }
    break;
  case BABEL_DONE:
    break;
  default:
    break;
  }

}

static void gps_upload_progress_func(BabelProgressCode c, gpointer data, GpsSession * sess )
{
  gchar *line;
  static int cnt = 0;

  gdk_threads_enter ();
  g_mutex_lock(sess->mutex);
  if (!sess->ok) {
    g_mutex_unlock(sess->mutex);
    gps_session_delete(sess);
    gdk_threads_leave();
    g_thread_exit ( NULL );
  }
  g_mutex_unlock(sess->mutex);
  gdk_threads_leave ();

  switch(c) {
  case BABEL_DIAG_OUTPUT:
    line = (gchar *)data;

    if (strstr(line, "PRDDAT")) {
      gchar **tokens = g_strsplit(line, " ", 0);
      gchar info[128];
      int ilen = 0;
      int i;

      for (i=8; tokens[i] && ilen < sizeof(info)-2 && strcmp(tokens[i], "00"); i++) {
	guint ch;
	sscanf(tokens[i], "%x", &ch);
	info[ilen++] = ch;
      }
      info[ilen++] = 0;
      set_gps_info(info, sess);
    }
    if (strstr(line, "RECORD")) { 
      int lsb, msb;

      sscanf(line+17, "%x", &lsb); 
      sscanf(line+20, "%x", &msb);
      cnt = lsb + msb * 256;
      /* set_total_count(cnt, sess); */
      sess->count = 0;
    }
    if ( strstr(line, "WPTDAT")) {
      if (sess->count == 0) {
        sess->progress_label = sess->wp_label;
        set_total_count(cnt, sess);
      }
      sess->count++;
      set_current_count(sess->count, sess);

    }
    if ( strstr(line, "TRKHDR") || strstr(line, "TRKDAT") ) {
      if (sess->count == 0) {
        sess->progress_label = sess->trk_label;
        set_total_count(cnt, sess);
      }
      sess->count++;
      set_current_count(sess->count, sess);
    }
    break;
  case BABEL_DONE:
    break;
  default:
    break;
  }

}

static void gps_comm_thread(GpsSession *sess)
{
  gboolean result;

  if (sess->direction == GPS_DOWN)
    result = a_babel_convert_from (sess->vtl, sess->cmd_args,
        (BabelStatusFunc) gps_download_progress_func, sess->port, sess);
  else
    result = a_babel_convert_to (sess->vtl, sess->cmd_args,
        (BabelStatusFunc) gps_upload_progress_func, sess->port, sess);

  gdk_threads_enter();
  if (!result) {
    gtk_label_set_text ( GTK_LABEL(sess->status_label), "Error: couldn't find gpsbabel." );
  } 
  else {
    g_mutex_lock(sess->mutex);
    if (sess->ok) {
      gtk_label_set_text ( GTK_LABEL(sess->status_label), "Done." );
      gtk_dialog_set_response_sensitive ( GTK_DIALOG(sess->dialog), GTK_RESPONSE_ACCEPT, TRUE );
      gtk_dialog_set_response_sensitive ( GTK_DIALOG(sess->dialog), GTK_RESPONSE_REJECT, FALSE );
    } else {
      /* canceled */
    }
    g_mutex_unlock(sess->mutex);
  }

  g_mutex_lock(sess->mutex);
  if (sess->ok) {
    sess->ok = FALSE;
    g_mutex_unlock(sess->mutex);
  }
  else {
    g_mutex_unlock(sess->mutex);
    gps_session_delete(sess);
  }
  gdk_threads_leave();
  g_thread_exit(NULL);
}

static gint gps_comm(VikTrwLayer *vtl, gps_dir dir, vik_gps_proto proto, gchar *port) {
  GpsSession *sess = g_malloc(sizeof(GpsSession));

  sess->mutex = g_mutex_new();
  sess->direction = dir;
  sess->vtl = vtl;
  sess->port = g_strdup(port);
  sess->ok = TRUE;
  sess->cmd_args = g_strdup_printf("-D 9 -t -w -%c %s",
      (dir == GPS_DOWN) ? 'i' : 'o', protocols_args[proto]);
  sess->window_title = (dir == GPS_DOWN) ? "GPS Download" : "GPS Upload";

  sess->dialog = gtk_dialog_new_with_buttons ( "", VIK_GTK_WINDOW_FROM_LAYER(vtl), 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL );
  gtk_dialog_set_response_sensitive ( GTK_DIALOG(sess->dialog),
      GTK_RESPONSE_ACCEPT, FALSE );
  gtk_window_set_title ( GTK_WINDOW(sess->dialog), sess->window_title );

  sess->status_label = gtk_label_new ("Status: detecting gpsbabel");
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(sess->dialog)->vbox),
      sess->status_label, FALSE, FALSE, 5 );
  gtk_widget_show_all(sess->status_label);

  sess->gps_label = gtk_label_new ("GPS device: N/A");
  sess->ver_label = gtk_label_new ("");
  sess->id_label = gtk_label_new ("");
  sess->wp_label = gtk_label_new ("");
  sess->trk_label = gtk_label_new ("");

  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(sess->dialog)->vbox), sess->gps_label, FALSE, FALSE, 5 );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(sess->dialog)->vbox), sess->wp_label, FALSE, FALSE, 5 );
  gtk_box_pack_start ( GTK_BOX(GTK_DIALOG(sess->dialog)->vbox), sess->trk_label, FALSE, FALSE, 5 );

  gtk_widget_show_all(sess->dialog);

  sess->progress_label = sess->wp_label;
  sess->total_count = -1;

  /* TODO: starting gps read/write thread here */
  g_thread_create((GThreadFunc)gps_comm_thread, sess, FALSE, NULL );

  gtk_dialog_run(GTK_DIALOG(sess->dialog));

  gtk_widget_destroy(sess->dialog);

  g_mutex_lock(sess->mutex);
  if (sess->ok) {
    sess->ok = FALSE;   /* tell thread to stop */
    g_mutex_unlock(sess->mutex);
  }
  else {
    g_mutex_unlock(sess->mutex);
    gps_session_delete(sess);
  }

  // fprintf(stderr, "\"gps_comm: cmd_args=%s\" port=%s\n", sess->cmd_args, sess->port);
  return 0;
}

static void gps_upload_cb( gpointer layer_and_vlp[2] )
{
  VikGpsLayer *vgl = (VikGpsLayer *)layer_and_vlp[0];
  VikTrwLayer *vtl = vgl->trw_children[TRW_UPLOAD];
  gps_comm(vtl, GPS_UP, vgl->protocol_id, params_ports[vgl->serial_port_id]);
}

static void gps_download_cb( gpointer layer_and_vlp[2] )
{
  VikGpsLayer *vgl = (VikGpsLayer *)layer_and_vlp[0];
  VikTrwLayer *vtl = vgl->trw_children[TRW_DOWNLOAD];
  gps_comm(vtl, GPS_DOWN, vgl->protocol_id, params_ports[vgl->serial_port_id]);
}
