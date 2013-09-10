/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2006-2008, Quy Tonthat <qtonthat@gmail.com>
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
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#include "viking.h"
#include "icons/icons.h"
#include "babel.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#ifdef VIK_CONFIG_REALTIME_GPS_TRACKING
#include <gps.h>
#include "vikutils.h"
#endif

#define DISCONNECT_UPDATE_SIGNAL(vl, val) g_signal_handlers_disconnect_matched(vl, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, val)
static VikGpsLayer *vik_gps_layer_create (VikViewport *vp);
static void vik_gps_layer_realize ( VikGpsLayer *val, VikTreeview *vt, GtkTreeIter *layer_iter );
static void vik_gps_layer_free ( VikGpsLayer *val );
static void vik_gps_layer_draw ( VikGpsLayer *val, VikViewport *vp );
static VikGpsLayer *vik_gps_layer_new ( VikViewport *vp );

static void gps_layer_marshall( VikGpsLayer *val, guint8 **data, gint *len );
static VikGpsLayer *gps_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp );
static gboolean gps_layer_set_param ( VikGpsLayer *vgl, guint16 id, VikLayerParamData data, VikViewport *vp, gboolean is_file_operation );
static VikLayerParamData gps_layer_get_param ( VikGpsLayer *vgl, guint16 id, gboolean is_file_operation );

static const gchar* gps_layer_tooltip ( VikGpsLayer *vgl );

static void gps_layer_change_coord_mode ( VikGpsLayer *val, VikCoordMode mode );
static void gps_layer_add_menu_items( VikGpsLayer *vtl, GtkMenu *menu, gpointer vlp );
static void gps_layer_drag_drop_request ( VikGpsLayer *val_src, VikGpsLayer *val_dest, GtkTreeIter *src_item_iter, GtkTreePath *dest_path );

static void gps_upload_cb( gpointer layer_and_vlp[2] );
static void gps_download_cb( gpointer layer_and_vlp[2] );
static void gps_empty_upload_cb( gpointer layer_and_vlp[2] );
static void gps_empty_download_cb( gpointer layer_and_vlp[2] );
static void gps_empty_all_cb( gpointer layer_and_vlp[2] );
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
static void gps_empty_realtime_cb( gpointer layer_and_vlp[2] );
static void gps_start_stop_tracking_cb( gpointer layer_and_vlp[2] );
static void realtime_tracking_draw(VikGpsLayer *vgl, VikViewport *vp);
static void rt_gpsd_disconnect(VikGpsLayer *vgl);
static gboolean rt_gpsd_connect(VikGpsLayer *vgl, gboolean ask_if_failed);
#endif

// Shouldn't need to use these much any more as the protocol is now saved as a string.
// They are kept for compatibility loading old .vik files
typedef enum {GARMIN_P = 0, MAGELLAN_P, DELORME_P, NAVILINK_P, OLD_NUM_PROTOCOLS} vik_gps_proto;
static gchar * protocols_args[]   = {"garmin", "magellan", "delbin", "navilink", NULL};
#ifdef WINDOWS
static gchar * params_ports[] = {"com1", "usb:", NULL};
#else
static gchar * params_ports[] = {"/dev/ttyS0", "/dev/ttyS1", "/dev/ttyUSB0", "/dev/ttyUSB1", "usb:", NULL};
#endif
/* NUM_PORTS not actually used */
/* #define NUM_PORTS (sizeof(params_ports)/sizeof(params_ports[0]) - 1) */
/* Compatibility with previous versions */
#ifdef WINDOWS
static gchar * old_params_ports[] = {"com1", "usb:", NULL};
#else
static gchar * old_params_ports[] = {"/dev/ttyS0", "/dev/ttyS1", "/dev/ttyUSB0", "/dev/ttyUSB1", "usb:", NULL};
#endif
#define OLD_NUM_PORTS (sizeof(old_params_ports)/sizeof(old_params_ports[0]) - 1)

typedef struct {
  GMutex *mutex;
  vik_gps_dir direction;
  gchar *port;
  gboolean ok;
  gint total_count;
  gint count;
  VikTrwLayer *vtl;
  VikTrack *track;
  gchar *cmd_args;
  gchar *window_title;
  GtkWidget *dialog;
  GtkWidget *status_label;
  GtkWidget *gps_label;
  GtkWidget *ver_label;
  GtkWidget *id_label;
  GtkWidget *wp_label;
  GtkWidget *trk_label;
  GtkWidget *rte_label;
  GtkWidget *progress_label;
  vik_gps_xfer_type progress_type;
  VikViewport *vvp;
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
  gboolean realtime_tracking;
#endif
} GpsSession;
static void gps_session_delete(GpsSession *sess);

static gchar *params_groups[] = {
  N_("Data Mode"),
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
  N_("Realtime Tracking Mode"),
#endif
};

enum {GROUP_DATA_MODE, GROUP_REALTIME_MODE};


static VikLayerParamData gps_protocol_default ( void )
{
  VikLayerParamData data;
  data.s = g_strdup ( "garmin" );
  return data;
}

static VikLayerParamData gps_port_default ( void )
{
  VikLayerParamData data;
  data.s = g_strdup ( "usb:" );
#ifndef WINDOWS
  /* Attempt to auto set default USB serial port entry */
  /* Ordered to make lowest device favourite if available */
  if (g_access ("/dev/ttyUSB1", R_OK) == 0) {
	if ( data.s )
	  g_free ( (gchar *)data.s );
    data.s = g_strdup ("/dev/ttyUSB1");
  }
  if (g_access ("/dev/ttyUSB0", R_OK) == 0) {
	if ( data.s )
	  g_free ( (gchar *)data.s );
    data.s = g_strdup ("/dev/ttyUSB0");
  }
#endif
  return data;
}

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
static gchar *params_vehicle_position[] = {
  N_("Keep vehicle at center"),
  N_("Keep vehicle on screen"),
  N_("Disable"),
  NULL
};
enum {
  VEHICLE_POSITION_CENTERED = 0,
  VEHICLE_POSITION_ON_SCREEN,
  VEHICLE_POSITION_NONE,
};

static VikLayerParamData moving_map_method_default ( void ) { return VIK_LPD_UINT ( VEHICLE_POSITION_ON_SCREEN ); }

static VikLayerParamData gpsd_host_default ( void )
{
  VikLayerParamData data;
  data.s = g_strdup ( "localhost" );
  return data;
}

static VikLayerParamData gpsd_port_default ( void )
{
  VikLayerParamData data;
  data.s = g_strdup ( DEFAULT_GPSD_PORT );
  return data;
}

static VikLayerParamData gpsd_retry_interval_default ( void )
{
  VikLayerParamData data;
  data.s = g_strdup ( "10" );
  return data;
}

#endif

static VikLayerParam gps_layer_params[] = {
 //	NB gps_layer_inst_init() is performed after parameter registeration
 //  thus to give the protocols some potential values use the old static list
 // TODO: find another way to use gps_layer_inst_init()?
  { VIK_LAYER_GPS, "gps_protocol", VIK_LAYER_PARAM_STRING, GROUP_DATA_MODE, N_("GPS Protocol:"), VIK_LAYER_WIDGET_COMBOBOX, protocols_args, NULL, NULL, gps_protocol_default, NULL, NULL }, // List reassigned at runtime
  { VIK_LAYER_GPS, "gps_port", VIK_LAYER_PARAM_STRING, GROUP_DATA_MODE, N_("Serial Port:"), VIK_LAYER_WIDGET_COMBOBOX, params_ports, NULL, NULL, gps_port_default, NULL, NULL },
  { VIK_LAYER_GPS, "gps_download_tracks", VIK_LAYER_PARAM_BOOLEAN, GROUP_DATA_MODE, N_("Download Tracks:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_GPS, "gps_upload_tracks", VIK_LAYER_PARAM_BOOLEAN, GROUP_DATA_MODE, N_("Upload Tracks:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_GPS, "gps_download_routes", VIK_LAYER_PARAM_BOOLEAN, GROUP_DATA_MODE, N_("Download Routes:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_GPS, "gps_upload_routes", VIK_LAYER_PARAM_BOOLEAN, GROUP_DATA_MODE, N_("Upload Routes:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_GPS, "gps_download_waypoints", VIK_LAYER_PARAM_BOOLEAN, GROUP_DATA_MODE, N_("Download Waypoints:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_GPS, "gps_upload_waypoints", VIK_LAYER_PARAM_BOOLEAN, GROUP_DATA_MODE, N_("Upload Waypoints:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
  { VIK_LAYER_GPS, "record_tracking", VIK_LAYER_PARAM_BOOLEAN, GROUP_REALTIME_MODE, N_("Recording tracks"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_GPS, "center_start_tracking", VIK_LAYER_PARAM_BOOLEAN, GROUP_REALTIME_MODE, N_("Jump to current position on start"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_false_default, NULL, NULL },
  { VIK_LAYER_GPS, "moving_map_method", VIK_LAYER_PARAM_UINT, GROUP_REALTIME_MODE, N_("Moving Map Method:"), VIK_LAYER_WIDGET_RADIOGROUP_STATIC, params_vehicle_position, NULL, NULL, moving_map_method_default, NULL, NULL },
  { VIK_LAYER_GPS, "realtime_update_statusbar", VIK_LAYER_PARAM_BOOLEAN, GROUP_REALTIME_MODE, N_("Update Statusbar:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, N_("Display information in the statusbar on GPS updates"), vik_lpd_true_default, NULL, NULL },
  { VIK_LAYER_GPS, "gpsd_host", VIK_LAYER_PARAM_STRING, GROUP_REALTIME_MODE, N_("Gpsd Host:"), VIK_LAYER_WIDGET_ENTRY, NULL, NULL, NULL, gpsd_host_default, NULL, NULL },
  { VIK_LAYER_GPS, "gpsd_port", VIK_LAYER_PARAM_STRING, GROUP_REALTIME_MODE, N_("Gpsd Port:"), VIK_LAYER_WIDGET_ENTRY, NULL, NULL, NULL, gpsd_port_default, NULL, NULL },
  { VIK_LAYER_GPS, "gpsd_retry_interval", VIK_LAYER_PARAM_STRING, GROUP_REALTIME_MODE, N_("Gpsd Retry Interval (seconds):"), VIK_LAYER_WIDGET_ENTRY, NULL, NULL, NULL, gpsd_retry_interval_default, NULL, NULL },
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */
};
enum {
  PARAM_PROTOCOL=0, PARAM_PORT,
  PARAM_DOWNLOAD_TRACKS, PARAM_UPLOAD_TRACKS,
  PARAM_DOWNLOAD_ROUTES, PARAM_UPLOAD_ROUTES,
  PARAM_DOWNLOAD_WAYPOINTS, PARAM_UPLOAD_WAYPOINTS,
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
  PARAM_REALTIME_REC,
  PARAM_REALTIME_CENTER_START,
  PARAM_VEHICLE_POSITION,
  PARAM_REALTIME_UPDATE_STATUSBAR,
  PARAM_GPSD_HOST,
  PARAM_GPSD_PORT,
  PARAM_GPSD_RETRY_INTERVAL,
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */
  NUM_PARAMS};

VikLayerInterface vik_gps_layer_interface = {
  "GPS",
  N_("GPS"),
  NULL,
  &vikgpslayer_pixbuf,

  NULL,
  0,

  gps_layer_params,
  NUM_PARAMS,
  params_groups,
  sizeof(params_groups)/sizeof(params_groups[0]),

  VIK_MENU_ITEM_ALL,

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
  (VikLayerFuncSublayerTooltip)         NULL,
  (VikLayerFuncLayerTooltip)            gps_layer_tooltip,
  (VikLayerFuncLayerSelected)           NULL,

  (VikLayerFuncMarshall)		gps_layer_marshall,
  (VikLayerFuncUnmarshall)		gps_layer_unmarshall,

  (VikLayerFuncSetParam)                gps_layer_set_param,
  (VikLayerFuncGetParam)                gps_layer_get_param,

  (VikLayerFuncReadFileData)            NULL,
  (VikLayerFuncWriteFileData)           NULL,

  (VikLayerFuncDeleteItem)              NULL,
  (VikLayerFuncCutItem)                 NULL,
  (VikLayerFuncCopyItem)                NULL,
  (VikLayerFuncPasteItem)               NULL,
  (VikLayerFuncFreeCopiedItem)          NULL,
  (VikLayerFuncDragDropRequest)		gps_layer_drag_drop_request,

  (VikLayerFuncSelectClick)             NULL,
  (VikLayerFuncSelectMove)              NULL,
  (VikLayerFuncSelectRelease)           NULL,
  (VikLayerFuncSelectedViewportMenu)    NULL,
};

enum {TRW_DOWNLOAD=0, TRW_UPLOAD,
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
  TRW_REALTIME,
#endif
  NUM_TRW};
static gchar * trw_names[] = {
  N_("GPS Download"), N_("GPS Upload"),
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
  N_("GPS Realtime Tracking"),
#endif
};

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
typedef struct {
  struct gps_data_t gpsd;
  VikGpsLayer *vgl;
} VglGpsd;

typedef struct {
  struct gps_fix_t fix;
  gint satellites_used;
  gboolean dirty;   /* needs to be saved */
} GpsFix;
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */

struct _VikGpsLayer {
  VikLayer vl;
  VikTrwLayer * trw_children[NUM_TRW];
  GList * children;	/* used only for writing file */
  int cur_read_child;   /* used only for reading file */
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
  VglGpsd *vgpsd;
  gboolean realtime_tracking;  /* set/reset only by the callback */
  gboolean first_realtime_trackpoint;
  GpsFix realtime_fix;
  GpsFix last_fix;

  VikTrack *realtime_track;

  GIOChannel *realtime_io_channel;
  guint realtime_io_watch_id;
  guint realtime_retry_timer;
  GdkGC *realtime_track_gc;
  GdkGC *realtime_track_bg_gc;
  GdkGC *realtime_track_pt_gc;
  GdkGC *realtime_track_pt1_gc;
  GdkGC *realtime_track_pt2_gc;

  /* params */
  gchar *gpsd_host;
  gchar *gpsd_port;
  gint gpsd_retry_interval;
  gboolean realtime_record;
  gboolean realtime_jump_to_start;
  guint vehicle_position;
  gboolean realtime_update_statusbar;
  VikTrackpoint *trkpt;
  VikTrackpoint *trkpt_prev;
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */
  gchar *protocol;
  gchar *serial_port;
  gboolean download_tracks;
  gboolean download_routes;
  gboolean download_waypoints;
  gboolean upload_tracks;
  gboolean upload_routes;
  gboolean upload_waypoints;
};

/**
 * Overwrite the static setup with dynamically generated GPS Babel device list
 */
static void gps_layer_inst_init ( VikGpsLayer *self )
{
  gint new_proto = 0;
  // +1 for luck (i.e the NULL terminator)
  gchar **new_protocols = g_malloc(1 + g_list_length(a_babel_device_list)*sizeof(gpointer));

  GList *gl = g_list_first ( a_babel_device_list );
  while ( gl ) {
    // should be using label property but use name for now
    //  thus don't need to mess around converting label to name later on
    new_protocols[new_proto++] = ((BabelDevice*)gl->data)->name;
    gl = g_list_next ( gl );
  }
  new_protocols[new_proto] = NULL;

  vik_gps_layer_interface.params[PARAM_PROTOCOL].widget_data = new_protocols;
}

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
      (GInstanceInitFunc) gps_layer_inst_init,
    };
    val_type = g_type_register_static ( VIK_LAYER_TYPE, "VikGpsLayer", &val_info, 0 );
  }

  return val_type;
}

static VikGpsLayer *vik_gps_layer_create (VikViewport *vp)
{
  int i;

  VikGpsLayer *rv = vik_gps_layer_new (vp);
  vik_layer_rename ( VIK_LAYER(rv), vik_gps_layer_interface.name );

  for (i = 0; i < NUM_TRW; i++) {
    rv->trw_children[i] = VIK_TRW_LAYER(vik_layer_create ( VIK_LAYER_TRW, vp, NULL, FALSE ));
    vik_layer_set_menu_items_selection(VIK_LAYER(rv->trw_children[i]), VIK_MENU_ITEM_ALL & ~(VIK_MENU_ITEM_CUT|VIK_MENU_ITEM_DELETE));
  }
  return rv;
}

static const gchar* gps_layer_tooltip ( VikGpsLayer *vgl )
{
  return vgl->protocol;
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
  
  VikGpsLayer *rv = vik_gps_layer_new(vvp);
  VikLayer *child_layer;
  gint i;

  vik_layer_unmarshall_params ( VIK_LAYER(rv), data+sizeof(gint), alm_size, vvp );
  alm_next;

  i = 0;
  while (len>0 && i < NUM_TRW) {
    child_layer = vik_layer_unmarshall ( data + sizeof(gint), alm_size, vvp );
    if (child_layer) {
      rv->trw_children[i++] = (VikTrwLayer *)child_layer;
      g_signal_connect_swapped ( G_OBJECT(child_layer), "update", G_CALLBACK(vik_layer_emit_update_secondary), rv );
    }
    alm_next;
  }
  //  g_print("gps_layer_unmarshall ended with len=%d\n", len);
  g_assert(len == 0);
  return rv;
#undef alm_size
#undef alm_next
}

static gboolean gps_layer_set_param ( VikGpsLayer *vgl, guint16 id, VikLayerParamData data, VikViewport *vp, gboolean is_file_operation )
{
  switch ( id )
  {
    case PARAM_PROTOCOL:
      if (data.s) {
        g_free(vgl->protocol);
        // Backwards Compatibility: previous versions <v1.4 stored protocol as an array index
        int index = data.s[0] - '0';
        if (data.s[0] != '\0' &&
            g_ascii_isdigit (data.s[0]) &&
            data.s[1] == '\0' &&
            index < OLD_NUM_PROTOCOLS)
          // It is a single digit: activate compatibility
          vgl->protocol = g_strdup(protocols_args[index]);
        else
          vgl->protocol = g_strdup(data.s);
        g_debug("%s: %s", __FUNCTION__, vgl->protocol);
      }
      else
        g_warning(_("Unknown GPS Protocol"));
      break;
    case PARAM_PORT:
      if (data.s) {
        g_free(vgl->serial_port);
        // Backwards Compatibility: previous versions <v0.9.91 stored serial_port as an array index
        int index = data.s[0] - '0';
        if (data.s[0] != '\0' &&
            g_ascii_isdigit (data.s[0]) &&
            data.s[1] == '\0' &&
            index < OLD_NUM_PORTS)
          /* It is a single digit: activate compatibility */
          vgl->serial_port = g_strdup(old_params_ports[index]);
        else
          vgl->serial_port = g_strdup(data.s);
        g_debug("%s: %s", __FUNCTION__, vgl->serial_port);
      }
      else
        g_warning(_("Unknown serial port device"));
      break;
    case PARAM_DOWNLOAD_TRACKS:
      vgl->download_tracks = data.b;
      break;
    case PARAM_UPLOAD_TRACKS:
      vgl->upload_tracks = data.b;
      break;
    case PARAM_DOWNLOAD_ROUTES:
      vgl->download_routes = data.b;
      break;
    case PARAM_UPLOAD_ROUTES:
      vgl->upload_routes = data.b;
      break;
    case PARAM_DOWNLOAD_WAYPOINTS:
      vgl->download_waypoints = data.b;
      break;
    case PARAM_UPLOAD_WAYPOINTS:
      vgl->upload_waypoints = data.b;
      break;
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
    case PARAM_GPSD_HOST:
      if (data.s) {
        if (vgl->gpsd_host)
          g_free(vgl->gpsd_host);
        vgl->gpsd_host = g_strdup(data.s);
      }
      break;
    case PARAM_GPSD_PORT:
      if (data.s) {
        if (vgl->gpsd_port)
          g_free(vgl->gpsd_port);
        vgl->gpsd_port = g_strdup(data.s);
      }
      break;
    case PARAM_GPSD_RETRY_INTERVAL:
      vgl->gpsd_retry_interval = strtol(data.s, NULL, 10);
      break;
    case PARAM_REALTIME_REC:
      vgl->realtime_record = data.b;
      break;
    case PARAM_REALTIME_CENTER_START:
      vgl->realtime_jump_to_start = data.b;
      break;
    case PARAM_VEHICLE_POSITION:
      vgl->vehicle_position = data.u;
      break;
    case PARAM_REALTIME_UPDATE_STATUSBAR:
      vgl->realtime_update_statusbar = data.b;
      break;
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */
    default:
      g_warning("gps_layer_set_param(): unknown parameter");
  }

  return TRUE;
}

static VikLayerParamData gps_layer_get_param ( VikGpsLayer *vgl, guint16 id, gboolean is_file_operation )
{
  VikLayerParamData rv;
  switch ( id )
  {
    case PARAM_PROTOCOL:
      rv.s = vgl->protocol;
      g_debug("%s: %s", __FUNCTION__, rv.s);
      break;
    case PARAM_PORT:
      rv.s = vgl->serial_port;
      g_debug("%s: %s", __FUNCTION__, rv.s);
      break;
    case PARAM_DOWNLOAD_TRACKS:
      rv.b = vgl->download_tracks;
      break;
    case PARAM_UPLOAD_TRACKS:
      rv.b = vgl->upload_tracks;
      break;
    case PARAM_DOWNLOAD_ROUTES:
      rv.b = vgl->download_routes;
      break;
    case PARAM_UPLOAD_ROUTES:
      rv.b = vgl->upload_routes;
      break;
    case PARAM_DOWNLOAD_WAYPOINTS:
      rv.b = vgl->download_waypoints;
      break;
    case PARAM_UPLOAD_WAYPOINTS:
      rv.b = vgl->upload_waypoints;
      break;
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
    case PARAM_GPSD_HOST:
      rv.s = vgl->gpsd_host ? vgl->gpsd_host : "";
      break;
    case PARAM_GPSD_PORT:
      rv.s = vgl->gpsd_port ? vgl->gpsd_port : g_strdup(DEFAULT_GPSD_PORT);
      break;
    case PARAM_GPSD_RETRY_INTERVAL:
      rv.s = g_strdup_printf("%d", vgl->gpsd_retry_interval);
      break;
    case PARAM_REALTIME_REC:
      rv.b = vgl->realtime_record;
      break;
    case PARAM_REALTIME_CENTER_START:
      rv.b = vgl->realtime_jump_to_start;
      break;
    case PARAM_VEHICLE_POSITION:
      rv.u = vgl->vehicle_position;
      break;
    case PARAM_REALTIME_UPDATE_STATUSBAR:
      rv.u = vgl->realtime_update_statusbar;
      break;
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */
    default:
      g_warning(_("%s: unknown parameter"), __FUNCTION__);
  }

  return rv;
}

VikGpsLayer *vik_gps_layer_new (VikViewport *vp)
{
  gint i;
  VikGpsLayer *vgl = VIK_GPS_LAYER ( g_object_new ( VIK_GPS_LAYER_TYPE, NULL ) );
  vik_layer_set_type ( VIK_LAYER(vgl), VIK_LAYER_GPS );
  for (i = 0; i < NUM_TRW; i++) {
    vgl->trw_children[i] = NULL;
  }
  vgl->children = NULL;
  vgl->cur_read_child = 0;

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
  vgl->first_realtime_trackpoint = FALSE;
  vgl->trkpt = NULL;
  vgl->trkpt_prev = NULL;
  vgl->vgpsd = NULL;
  vgl->realtime_io_channel = NULL;
  vgl->realtime_io_watch_id = 0;
  vgl->realtime_retry_timer = 0;
  if ( vp ) {
    vgl->realtime_track_gc = vik_viewport_new_gc ( vp, "#203070", 2 );
    vgl->realtime_track_bg_gc = vik_viewport_new_gc ( vp, "grey", 2 );
    vgl->realtime_track_pt1_gc = vik_viewport_new_gc ( vp, "red", 2 );
    vgl->realtime_track_pt2_gc = vik_viewport_new_gc ( vp, "green", 2 );
    vgl->realtime_track_pt_gc = vgl->realtime_track_pt1_gc;
  }
  vgl->realtime_track = NULL;
#endif // VIK_CONFIG_REALTIME_GPS_TRACKING

  vik_layer_set_defaults ( VIK_LAYER(vgl), vp );

  return vgl;
}

static void vik_gps_layer_draw ( VikGpsLayer *vgl, VikViewport *vp )
{
  gint i;
  VikLayer *vl;
  VikLayer *trigger = VIK_LAYER(vik_viewport_get_trigger( vp ));

  for (i = 0; i < NUM_TRW; i++) {
    vl = VIK_LAYER(vgl->trw_children[i]);
    if (vl == trigger) {
      if ( vik_viewport_get_half_drawn ( vp ) ) {
        vik_viewport_set_half_drawn ( vp, FALSE );
        vik_viewport_snapshot_load( vp );
      } else {
        vik_viewport_snapshot_save( vp );
      }
    }
    if (!vik_viewport_get_half_drawn(vp))
      vik_layer_draw ( vl, vp );
  }
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
  if (vgl->realtime_tracking) {
    if (VIK_LAYER(vgl) == trigger) {
      if ( vik_viewport_get_half_drawn ( vp ) ) {
        vik_viewport_set_half_drawn ( vp, FALSE );
        vik_viewport_snapshot_load( vp );
      } else {
        vik_viewport_snapshot_save( vp );
      }
    }
    if (!vik_viewport_get_half_drawn(vp))
      realtime_tracking_draw(vgl, vp);
  }
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */
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

  /* Now with icons */
  item = gtk_image_menu_item_new_with_mnemonic ( _("_Upload to GPS") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_UP, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(gps_upload_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );

  item = gtk_image_menu_item_new_with_mnemonic ( _("Download from _GPS") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(gps_download_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
  item = gtk_image_menu_item_new_with_mnemonic ( vgl->realtime_tracking  ?
						 "_Stop Realtime Tracking" :
						 "_Start Realtime Tracking" );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, vgl->realtime_tracking ?
				  gtk_image_new_from_stock (GTK_STOCK_MEDIA_STOP, GTK_ICON_SIZE_MENU) :
				  gtk_image_new_from_stock (GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(gps_start_stop_tracking_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );

  item = gtk_menu_item_new();
  gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
  gtk_widget_show ( item );

  item = gtk_image_menu_item_new_with_mnemonic ( _("Empty _Realtime") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(gps_empty_realtime_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */

  item = gtk_image_menu_item_new_with_mnemonic ( _("E_mpty Upload") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(gps_empty_upload_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );

  item = gtk_image_menu_item_new_with_mnemonic ( _("_Empty Download") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(gps_empty_download_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );

  item = gtk_image_menu_item_new_with_mnemonic ( _("Empty _All") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(gps_empty_all_cb), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );

}

static void disconnect_layer_signal ( VikLayer *vl, VikGpsLayer *vgl )
{
  guint number_handlers = DISCONNECT_UPDATE_SIGNAL(vl,vgl);
  if ( number_handlers != 1 ) {
    /*
      NB It's not fatal if this gives 2 for example! Hence removal of the g_assert
      This happens when copied GPS layer is deleted (not sure why the number_handlers would be 2)
      I don't think there's any side effects and certainly better than the program just aborting
    */
    g_warning(_("Unexpected number of disconnected handlers: %d"), number_handlers);
  }
}

static void vik_gps_layer_free ( VikGpsLayer *vgl )
{
  gint i;
  for (i = 0; i < NUM_TRW; i++) {
    if (vgl->vl.realized)
      disconnect_layer_signal(VIK_LAYER(vgl->trw_children[i]), vgl);
    g_object_unref(vgl->trw_children[i]);
  }
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
  rt_gpsd_disconnect(vgl);
  if (vgl->realtime_track_gc != NULL)
    g_object_unref(vgl->realtime_track_gc);
  if (vgl->realtime_track_bg_gc != NULL)
    g_object_unref(vgl->realtime_track_bg_gc);
  if (vgl->realtime_track_pt1_gc != NULL)
    g_object_unref(vgl->realtime_track_pt1_gc);
  if (vgl->realtime_track_pt2_gc != NULL)
    g_object_unref(vgl->realtime_track_pt2_gc);
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */
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

static void vik_gps_layer_realize ( VikGpsLayer *vgl, VikTreeview *vt, GtkTreeIter *layer_iter )
{
  GtkTreeIter iter;
  int ix;

  // TODO set to garmin by default
  //if (a_babel_device_list)
  // device = ((BabelDevice*)g_list_nth_data(a_babel_device_list, last_active))->name;
  // Need to access uibuild widgets somehow....

  for (ix = 0; ix < NUM_TRW; ix++) {
    VikLayer * trw = VIK_LAYER(vgl->trw_children[ix]);
    vik_treeview_add_layer ( VIK_LAYER(vgl)->vt, layer_iter, &iter,
        _(trw_names[ix]), vgl, TRUE,
        trw, trw->type, trw->type );
    if ( ! trw->visible )
      vik_treeview_item_set_visible ( VIK_LAYER(vgl)->vt, &iter, FALSE );
    vik_layer_realize ( trw, VIK_LAYER(vgl)->vt, &iter );
    g_signal_connect_swapped ( G_OBJECT(trw), "update", G_CALLBACK(vik_layer_emit_update_secondary), vgl );
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

VikTrwLayer * vik_gps_layer_get_a_child(VikGpsLayer *vgl)
{
  g_assert ((vgl->cur_read_child >= 0) && (vgl->cur_read_child < NUM_TRW));

  VikTrwLayer * vtl = vgl->trw_children[vgl->cur_read_child];
  if (++(vgl->cur_read_child) >= NUM_TRW)
    vgl->cur_read_child = 0;
  return(vtl);
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
  gchar *dp;

  dp = gtk_tree_path_to_string(dest_path);

  /* vik_gps_layer_delete unrefs, but we don't want that here.
   * we're still using the layer. */
  g_object_ref ( vl );
  vik_gps_layer_delete(val_src, src_item_iter);

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
    const gchar *tmp_str;
    if (sess->direction == GPS_DOWN)
    {
      switch (sess->progress_type) {
      case WPT: tmp_str = ngettext("Downloading %d waypoint...", "Downloading %d waypoints...", cnt); sess->total_count = cnt; break;
      case TRK: tmp_str = ngettext("Downloading %d trackpoint...", "Downloading %d trackpoints...", cnt); sess->total_count = cnt; break;
      default:
        {
          // Maybe a gpsbabel bug/feature (upto at least v1.4.3 or maybe my Garmin device) but the count always seems x2 too many for routepoints
          gint mycnt = (cnt / 2) + 1;
          tmp_str = ngettext("Downloading %d routepoint...", "Downloading %d routepoints...", mycnt); break;
          sess->total_count = mycnt;
        }
      }
    }
    else
    {
      switch (sess->progress_type) {
      case WPT: tmp_str = ngettext("Uploading %d waypoint...", "Uploading %d waypoints...", cnt); break;
      case TRK: tmp_str = ngettext("Uploading %d trackpoint...", "Uploading %d trackpoints...", cnt); break;
      default: tmp_str = ngettext("Uploading %d routepoint...", "Uploading %d routepoints...", cnt); break;
      }
    }

    g_snprintf(s, 128, tmp_str, cnt);
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
  const gchar *tmp_str;

  gdk_threads_enter();
  g_mutex_lock(sess->mutex);
  if (sess->ok) {
    if (cnt < sess->total_count) {
      if (sess->direction == GPS_DOWN)
      {
        switch (sess->progress_type) {
        case WPT: tmp_str = ngettext("Downloaded %d out of %d waypoint...", "Downloaded %d out of %d waypoints...", sess->total_count); break;
        case TRK: tmp_str = ngettext("Downloaded %d out of %d trackpoint...", "Downloaded %d out of %d trackpoints...", sess->total_count); break;
        default: tmp_str = ngettext("Downloaded %d out of %d routepoint...", "Downloaded %d out of %d routepoints...", sess->total_count); break;
        }
      }
      else {
        switch (sess->progress_type) {
        case WPT: tmp_str = ngettext("Uploaded %d out of %d waypoint...", "Uploaded %d out of %d waypoints...", sess->total_count); break;
        case TRK: tmp_str = ngettext("Uploaded %d out of %d trackpoint...", "Uploaded %d out of %d trackpoints...", sess->total_count); break;
        default: tmp_str = ngettext("Uploaded %d out of %d routepoint...", "Uploaded %d out of %d routepoints...", sess->total_count); break;
	}
      }
      g_snprintf(s, 128, tmp_str, cnt, sess->total_count);
    } else {
      if (sess->direction == GPS_DOWN)
      {
        switch (sess->progress_type) {
        case WPT: tmp_str = ngettext("Downloaded %d waypoint", "Downloaded %d waypoints", cnt); break;
        case TRK: tmp_str = ngettext("Downloaded %d trackpoint", "Downloaded %d trackpoints", cnt); break;
        default: tmp_str = ngettext("Downloaded %d routepoint", "Downloaded %d routepoints", cnt); break;
	}
      }
      else {
        switch (sess->progress_type) {
        case WPT: tmp_str = ngettext("Uploaded %d waypoint", "Uploaded %d waypoints", cnt); break;
        case TRK: tmp_str = ngettext("Uploaded %d trackpoint", "Uploaded %d trackpoints", cnt); break;
        default: tmp_str = ngettext("Uploaded %d routepoint", "Uploaded %d routepoints", cnt); break;
	}
      }
      g_snprintf(s, 128, tmp_str, cnt);
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
    g_snprintf(s, 256, _("GPS Device: %s"), info);
    gtk_label_set_text ( GTK_LABEL(sess->gps_label), s );
  }
  g_mutex_unlock(sess->mutex);
  gdk_threads_leave();
}

/*
 * Common processing for GPS Device information
 * It doesn't matter whether we're uploading or downloading
 */
static void process_line_for_gps_info ( const gchar *line, GpsSession *sess )
{
  if (strstr(line, "PRDDAT")) {
    gchar **tokens = g_strsplit(line, " ", 0);
    gchar info[128];
    int ilen = 0;
    int i;
    int n_tokens = 0;

    while (tokens[n_tokens])
      n_tokens++;

    // I'm not entirely clear what information this is trying to get...
    //  Obviously trying to decipher some kind of text/naming scheme
    //  Anyway this will be superceded if there is 'Unit:' information
    if (n_tokens > 8) {
      for (i=8; tokens[i] && ilen < sizeof(info)-2 && strcmp(tokens[i], "00"); i++) {
        guint ch;
        sscanf(tokens[i], "%x", &ch);
        info[ilen++] = ch;
      }
      info[ilen++] = 0;
      set_gps_info(info, sess);
    }
    g_strfreev(tokens);
  }

  /* eg: "Unit:\teTrex Legend HCx Software Version 2.90\n" */
  if (strstr(line, "Unit:")) {
    gchar **tokens = g_strsplit(line, "\t", 0);
    int n_tokens = 0;
    while (tokens[n_tokens])
      n_tokens++;

    if (n_tokens > 1) {
      set_gps_info(tokens[1], sess);
    }
    g_strfreev(tokens);
  }
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

    gdk_threads_enter();
    g_mutex_lock(sess->mutex);
    if (sess->ok) {
      gtk_label_set_text ( GTK_LABEL(sess->status_label), _("Status: Working...") );
    }
    g_mutex_unlock(sess->mutex);
    gdk_threads_leave();

    /* tells us the type of items that will follow */
    if (strstr(line, "Xfer Wpt")) {
      sess->progress_label = sess->wp_label;
      sess->progress_type = WPT;
    }
    if (strstr(line, "Xfer Trk")) {
      sess->progress_label = sess->trk_label;
      sess->progress_type = TRK;
    }
    if (strstr(line, "Xfer Rte")) {
      sess->progress_label = sess->rte_label;
      sess->progress_type = RTE;
    }

    process_line_for_gps_info ( line, sess );

    if (strstr(line, "RECORD")) {
      int lsb, msb, cnt;

      if (strlen(line) > 20) {
        sscanf(line+17, "%x", &lsb); 
        sscanf(line+20, "%x", &msb);
        cnt = lsb + msb * 256;
        set_total_count(cnt, sess);
        sess->count = 0;
      }
    }
    if ( strstr(line, "WPTDAT") || strstr(line, "TRKHDR") || strstr(line, "TRKDAT") || strstr(line, "RTEHDR") || strstr(line, "RTEWPT") ) {
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

    gdk_threads_enter();
    g_mutex_lock(sess->mutex);
    if (sess->ok) {
      gtk_label_set_text ( GTK_LABEL(sess->status_label), _("Status: Working...") );
    }
    g_mutex_unlock(sess->mutex);
    gdk_threads_leave();

    process_line_for_gps_info ( line, sess );

    if (strstr(line, "RECORD")) { 
      int lsb, msb;

      if (strlen(line) > 20) {
        sscanf(line+17, "%x", &lsb); 
        sscanf(line+20, "%x", &msb);
        cnt = lsb + msb * 256;
        /* set_total_count(cnt, sess); */
        sess->count = 0;
      }
    }
    if ( strstr(line, "WPTDAT")) {
      if (sess->count == 0) {
        sess->progress_label = sess->wp_label;
        sess->progress_type = WPT;
        set_total_count(cnt, sess);
      }
      sess->count++;
      set_current_count(sess->count, sess);
    }
    if ( strstr(line, "RTEHDR") || strstr(line, "RTEWPT") ) {
       if (sess->count == 0) {
         sess->progress_label = sess->rte_label;
         sess->progress_type = RTE;
         // Maybe a gpsbabel bug/feature (upto at least v1.4.3 or maybe my Garmin device) but the count always seems x2 too many for routepoints
         // Anyway since we're uploading - we should know how many points we're going to put!
         cnt = (cnt / 2) + 1;
         set_total_count(cnt, sess);
       }
       sess->count++;
       set_current_count(sess->count, sess);
    }
    if ( strstr(line, "TRKHDR") || strstr(line, "TRKDAT") ) {
      if (sess->count == 0) {
        sess->progress_label = sess->trk_label;
	sess->progress_type = TRK;
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
    result = a_babel_convert_from (sess->vtl, sess->cmd_args, sess->port,
        (BabelStatusFunc) gps_download_progress_func, sess, NULL);
  else {
    result = a_babel_convert_to (sess->vtl, sess->track, sess->cmd_args, sess->port,
        (BabelStatusFunc) gps_upload_progress_func, sess);
  }

  if (!result) {
    gtk_label_set_text ( GTK_LABEL(sess->status_label), _("Error: couldn't find gpsbabel.") );
  } 
  else {
    g_mutex_lock(sess->mutex);
    if (sess->ok) {
      gtk_label_set_text ( GTK_LABEL(sess->status_label), _("Done.") );
      gtk_dialog_set_response_sensitive ( GTK_DIALOG(sess->dialog), GTK_RESPONSE_ACCEPT, TRUE );
      gtk_dialog_set_response_sensitive ( GTK_DIALOG(sess->dialog), GTK_RESPONSE_REJECT, FALSE );

      /* Do not change the view if we are following the current GPS position */
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
      if (!sess->realtime_tracking)
#endif
      {
	if ( sess->vvp && sess->direction == GPS_DOWN ) {
	  /* View the data available */
	  vik_trw_layer_auto_set_view ( sess->vtl, sess->vvp) ;
	  vik_layer_emit_update ( VIK_LAYER(sess->vtl) ); // NB update from background thread
	}
      }
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
  g_thread_exit(NULL);
}

/**
 * vik_gps_comm:
 * @vtl: The TrackWaypoint layer to operate on
 * @track: Operate on a particular track when specified
 * @dir: The direction of the transfer
 * @protocol: The GPS device communication protocol
 * @port: The GPS serial port
 * @tracking: If tracking then viewport display update will be skipped
 * @vvp: A viewport is required as the display may get updated
 * @vlp: A layers panel is needed for uploading as the items maybe modified
 * @do_tracks: Whether tracks shoud be processed
 * @do_waypoints: Whether waypoints shoud be processed
 * @turn_off: Whether we should attempt to turn off the GPS device after the transfer (only some devices support this)
 *
 * Talk to a GPS Device using a thread which updates a dialog with the progress
 */
gint vik_gps_comm ( VikTrwLayer *vtl,
                    VikTrack *track,
                    vik_gps_dir dir,
                    gchar *protocol,
                    gchar *port,
                    gboolean tracking,
                    VikViewport *vvp,
                    VikLayersPanel *vlp,
                    gboolean do_tracks,
                    gboolean do_routes,
                    gboolean do_waypoints,
		    gboolean turn_off )
{
  GpsSession *sess = g_malloc(sizeof(GpsSession));
  char *tracks = NULL;
  char *routes = NULL;
  char *waypoints = NULL;

  sess->mutex = g_mutex_new();
  sess->direction = dir;
  sess->vtl = vtl;
  sess->track = track;
  sess->port = g_strdup(port);
  sess->ok = TRUE;
  sess->window_title = (dir == GPS_DOWN) ? _("GPS Download") : _("GPS Upload");
  sess->vvp = vvp;

  // This must be done inside the main thread as the uniquify causes screen updates
  //  (originally performed this nearer the point of upload in the thread)
  if ( dir == GPS_UP ) {
    // Enforce unique names in the layer upload to the GPS device
    // NB this may only be a Garmin device restriction (and may be not every Garmin device either...)
    // Thus this maintains the older code in built restriction
    if ( ! vik_trw_layer_uniquify ( sess->vtl, vlp ) )
      vik_statusbar_set_message ( vik_window_get_statusbar (VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(sess->vtl))), VIK_STATUSBAR_INFO,
				  _("Warning - GPS Upload items may overwrite each other") );
  }

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
  sess->realtime_tracking = tracking;
#endif

  if (do_tracks)
    tracks = "-t";
  else
    tracks = "";
  if (do_routes)
    routes = "-r";
  else
    routes = "";
  if (do_waypoints)
    waypoints = "-w";
  else
    waypoints = "";

  sess->cmd_args = g_strdup_printf("-D 9 %s %s %s -%c %s",
				   tracks, routes, waypoints, (dir == GPS_DOWN) ? 'i' : 'o', protocol);
  tracks = NULL;
  waypoints = NULL;

  // Only create dialog if we're going to do some transferring
  if ( do_tracks || do_waypoints || do_routes ) {
    sess->dialog = gtk_dialog_new_with_buttons ( "", VIK_GTK_WINDOW_FROM_LAYER(vtl), 0, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL );
    gtk_dialog_set_response_sensitive ( GTK_DIALOG(sess->dialog),
                                        GTK_RESPONSE_ACCEPT, FALSE );
    gtk_window_set_title ( GTK_WINDOW(sess->dialog), sess->window_title );

    sess->status_label = gtk_label_new (_("Status: detecting gpsbabel"));
    gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(sess->dialog))), sess->status_label, FALSE, FALSE, 5 );
    gtk_widget_show_all(sess->status_label);

    sess->gps_label = gtk_label_new (_("GPS device: N/A"));
    sess->ver_label = gtk_label_new ("");
    sess->id_label = gtk_label_new ("");
    sess->wp_label = gtk_label_new ("");
    sess->trk_label = gtk_label_new ("");
    sess->rte_label = gtk_label_new ("");

    gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(sess->dialog))), sess->gps_label, FALSE, FALSE, 5 );
    gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(sess->dialog))), sess->wp_label, FALSE, FALSE, 5 );
    gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(sess->dialog))), sess->trk_label, FALSE, FALSE, 5 );
    gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(sess->dialog))), sess->rte_label, FALSE, FALSE, 5 );

    gtk_widget_show_all(sess->dialog);

    sess->progress_label = sess->wp_label;
    sess->total_count = -1;

    // Starting gps read/write thread
#if GLIB_CHECK_VERSION (2, 32, 0)
    g_thread_try_new ( "gps_comm_thread", (GThreadFunc)gps_comm_thread, sess, NULL );
#else
    g_thread_create ( (GThreadFunc)gps_comm_thread, sess, FALSE, NULL );
#endif

    gtk_dialog_set_default_response ( GTK_DIALOG(sess->dialog), GTK_RESPONSE_ACCEPT );
    gtk_dialog_run(GTK_DIALOG(sess->dialog));

    gtk_widget_destroy(sess->dialog);
  }
  else {
    if ( !turn_off )
      a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("No GPS items selected for transfer.") );
  }

  g_mutex_lock(sess->mutex);

  if (sess->ok) {
    sess->ok = FALSE;   /* tell thread to stop */
    g_mutex_unlock(sess->mutex);
  }
  else {
    if ( turn_off ) {
      // No need for thread for powering off device (should be quick operation...) - so use babel command directly:
      gchar *device_off = g_strdup_printf("-i %s,%s", protocol, "power_off");
      gboolean result = a_babel_convert_from (NULL, (const char*)device_off, (const char*)port, NULL, NULL, NULL);
      if ( !result )
        a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_LAYER(vtl), _("Could not turn off device.") );
      g_free ( device_off );
    }
    g_mutex_unlock(sess->mutex);
    gps_session_delete(sess);
  }

  return 0;
}

static void gps_upload_cb( gpointer layer_and_vlp[2] )
{
  VikGpsLayer *vgl = (VikGpsLayer *)layer_and_vlp[0];
  VikLayersPanel *vlp = VIK_LAYERS_PANEL(layer_and_vlp[1]);
  VikTrwLayer *vtl = vgl->trw_children[TRW_UPLOAD];
  VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vgl));
  VikViewport *vvp = vik_window_viewport(vw);
  vik_gps_comm(vtl, NULL, GPS_UP, vgl->protocol, vgl->serial_port, FALSE, vvp, vlp, vgl->upload_tracks, vgl->upload_routes, vgl->upload_waypoints, FALSE);
}

static void gps_download_cb( gpointer layer_and_vlp[2] )
{
  VikGpsLayer *vgl = (VikGpsLayer *)layer_and_vlp[0];
  VikTrwLayer *vtl = vgl->trw_children[TRW_DOWNLOAD];
  VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vgl));
  VikViewport *vvp = vik_window_viewport(vw);
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
  vik_gps_comm(vtl, NULL, GPS_DOWN, vgl->protocol, vgl->serial_port, vgl->realtime_tracking, vvp, NULL, vgl->download_tracks, vgl->download_routes, vgl->download_waypoints, FALSE);
#else
  vik_gps_comm(vtl, NULL, GPS_DOWN, vgl->protocol, vgl->serial_port, FALSE, vvp, NULL, vgl->download_tracks, vgl->download_routes, vgl->download_waypoints, FALSE);
#endif
}

static void gps_empty_upload_cb( gpointer layer_and_vlp[2] )
{
  VikGpsLayer *vgl = (VikGpsLayer *)layer_and_vlp[0];
  // Get confirmation from the user
  if ( ! a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_WIDGET(layer_and_vlp[1]),
			      _("Are you sure you want to delete GPS Upload data?"),
			      NULL ) )
    return;
  vik_trw_layer_delete_all_waypoints ( vgl-> trw_children[TRW_UPLOAD]);
  vik_trw_layer_delete_all_tracks ( vgl-> trw_children[TRW_UPLOAD]);
  vik_trw_layer_delete_all_routes ( vgl-> trw_children[TRW_UPLOAD]);
}

static void gps_empty_download_cb( gpointer layer_and_vlp[2] )
{
  VikGpsLayer *vgl = (VikGpsLayer *)layer_and_vlp[0];
  // Get confirmation from the user
  if ( ! a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_WIDGET(layer_and_vlp[1]),
			      _("Are you sure you want to delete GPS Download data?"),
			      NULL ) )
    return;
  vik_trw_layer_delete_all_waypoints ( vgl-> trw_children[TRW_DOWNLOAD]);
  vik_trw_layer_delete_all_tracks ( vgl-> trw_children[TRW_DOWNLOAD]);
  vik_trw_layer_delete_all_routes ( vgl-> trw_children[TRW_DOWNLOAD]);
}

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
static void gps_empty_realtime_cb( gpointer layer_and_vlp[2] )
{
  VikGpsLayer *vgl = (VikGpsLayer *)layer_and_vlp[0];
  // Get confirmation from the user
  if ( ! a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_WIDGET(layer_and_vlp[1]),
			      _("Are you sure you want to delete GPS Realtime data?"),
			      NULL ) )
    return;
  vik_trw_layer_delete_all_waypoints ( vgl-> trw_children[TRW_REALTIME]);
  vik_trw_layer_delete_all_tracks ( vgl-> trw_children[TRW_REALTIME]);
}
#endif

static void gps_empty_all_cb( gpointer layer_and_vlp[2] )
{
  VikGpsLayer *vgl = (VikGpsLayer *)layer_and_vlp[0];
  // Get confirmation from the user
  if ( ! a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_WIDGET(layer_and_vlp[1]),
			      _("Are you sure you want to delete All GPS data?"),
			      NULL ) )
    return;
  vik_trw_layer_delete_all_waypoints ( vgl-> trw_children[TRW_UPLOAD]);
  vik_trw_layer_delete_all_tracks ( vgl-> trw_children[TRW_UPLOAD]);
  vik_trw_layer_delete_all_routes ( vgl-> trw_children[TRW_UPLOAD]);
  vik_trw_layer_delete_all_waypoints ( vgl-> trw_children[TRW_DOWNLOAD]);
  vik_trw_layer_delete_all_tracks ( vgl-> trw_children[TRW_DOWNLOAD]);
  vik_trw_layer_delete_all_routes ( vgl-> trw_children[TRW_DOWNLOAD]);
#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
  vik_trw_layer_delete_all_waypoints ( vgl-> trw_children[TRW_REALTIME]);
  vik_trw_layer_delete_all_tracks ( vgl-> trw_children[TRW_REALTIME]);
#endif
}

#if defined (VIK_CONFIG_REALTIME_GPS_TRACKING) && defined (GPSD_API_MAJOR_VERSION)
static void realtime_tracking_draw(VikGpsLayer *vgl, VikViewport *vp)
{
  struct LatLon ll;
  VikCoord nw, se;
  struct LatLon lnw, lse;
  vik_viewport_screen_to_coord ( vp, -20, -20, &nw );
  vik_viewport_screen_to_coord ( vp, vik_viewport_get_width(vp)+20, vik_viewport_get_width(vp)+20, &se );
  vik_coord_to_latlon ( &nw, &lnw );
  vik_coord_to_latlon ( &se, &lse );
  if ( vgl->realtime_fix.fix.latitude > lse.lat &&
       vgl->realtime_fix.fix.latitude < lnw.lat &&
       vgl->realtime_fix.fix.longitude > lnw.lon &&
       vgl->realtime_fix.fix.longitude < lse.lon &&
       !isnan (vgl->realtime_fix.fix.track) ) {
    VikCoord gps;
    gint x, y;
    gint half_back_x, half_back_y;
    gint half_back_bg_x, half_back_bg_y;
    gint pt_x, pt_y;
    gint ptbg_x;
    gint side1_x, side1_y, side2_x, side2_y;
    gint side1bg_x, side1bg_y, side2bg_x, side2bg_y;

    ll.lat = vgl->realtime_fix.fix.latitude;
    ll.lon = vgl->realtime_fix.fix.longitude;
    vik_coord_load_from_latlon ( &gps, vik_viewport_get_coord_mode(vp), &ll);
    vik_viewport_coord_to_screen ( vp, &gps, &x, &y );

    gdouble heading_cos = cos(DEG2RAD(vgl->realtime_fix.fix.track));
    gdouble heading_sin = sin(DEG2RAD(vgl->realtime_fix.fix.track));

    half_back_y = y+8*heading_cos;
    half_back_x = x-8*heading_sin;
    half_back_bg_y = y+10*heading_cos;
    half_back_bg_x = x-10*heading_sin;

    pt_y = half_back_y-24*heading_cos;
    pt_x = half_back_x+24*heading_sin;
    //ptbg_y = half_back_bg_y-28*heading_cos;
    ptbg_x = half_back_bg_x+28*heading_sin;

    side1_y = half_back_y+9*heading_sin;
    side1_x = half_back_x+9*heading_cos;
    side1bg_y = half_back_bg_y+11*heading_sin;
    side1bg_x = half_back_bg_x+11*heading_cos;

    side2_y = half_back_y-9*heading_sin;
    side2_x = half_back_x-9*heading_cos;
    side2bg_y = half_back_bg_y-11*heading_sin;
    side2bg_x = half_back_bg_x-11*heading_cos;

     GdkPoint trian[3] = { { pt_x, pt_y }, {side1_x, side1_y}, {side2_x, side2_y} };
     GdkPoint trian_bg[3] = { { ptbg_x, pt_y }, {side1bg_x, side1bg_y}, {side2bg_x, side2bg_y} };

     vik_viewport_draw_polygon ( vp, vgl->realtime_track_bg_gc, TRUE, trian_bg, 3 );
     vik_viewport_draw_polygon ( vp, vgl->realtime_track_gc, TRUE, trian, 3 );
     vik_viewport_draw_rectangle ( vp,
         (vgl->realtime_fix.fix.mode > MODE_2D) ? vgl->realtime_track_pt2_gc : vgl->realtime_track_pt1_gc,
         TRUE, x-2, y-2, 4, 4 );
     //vgl->realtime_track_pt_gc = (vgl->realtime_track_pt_gc == vgl->realtime_track_pt1_gc) ? vgl->realtime_track_pt2_gc : vgl->realtime_track_pt1_gc;
  }
}

static VikTrackpoint* create_realtime_trackpoint(VikGpsLayer *vgl, gboolean forced)
{
    struct LatLon ll;
    GList *last_tp;

    /* Note that fix.time is a double, but it should not affect the precision
       for most GPS */
    time_t cur_timestamp = vgl->realtime_fix.fix.time;
    time_t last_timestamp = vgl->last_fix.fix.time;

    if (cur_timestamp < last_timestamp) {
      return NULL;
    }

    if (vgl->realtime_record && vgl->realtime_fix.dirty) {
      gboolean replace = FALSE;
      int heading = isnan(vgl->realtime_fix.fix.track) ? 0 : (int)floor(vgl->realtime_fix.fix.track);
      int last_heading = isnan(vgl->last_fix.fix.track) ? 0 : (int)floor(vgl->last_fix.fix.track);
      int alt = isnan(vgl->realtime_fix.fix.altitude) ? VIK_DEFAULT_ALTITUDE : floor(vgl->realtime_fix.fix.altitude);
      int last_alt = isnan(vgl->last_fix.fix.altitude) ? VIK_DEFAULT_ALTITUDE : floor(vgl->last_fix.fix.altitude);
      if (((last_tp = g_list_last(vgl->realtime_track->trackpoints)) != NULL) &&
          (vgl->realtime_fix.fix.mode > MODE_2D) &&
          (vgl->last_fix.fix.mode <= MODE_2D) &&
          ((cur_timestamp - last_timestamp) < 2)) {
        g_free(last_tp->data);
        vgl->realtime_track->trackpoints = g_list_delete_link(vgl->realtime_track->trackpoints, last_tp);
        replace = TRUE;
      }
      if (replace ||
          ((cur_timestamp != last_timestamp) &&
          ((forced || 
            ((heading < last_heading) && (heading < (last_heading - 3))) || 
            ((heading > last_heading) && (heading > (last_heading + 3))) ||
            ((alt != VIK_DEFAULT_ALTITUDE) && (alt != last_alt)))))) {
        /* TODO: check for new segments */
        VikTrackpoint *tp = vik_trackpoint_new();
        tp->newsegment = FALSE;
        tp->has_timestamp = TRUE;
        tp->timestamp = vgl->realtime_fix.fix.time;
        tp->altitude = alt;
        /* speed only available for 3D fix. Check for NAN when use this speed */
        tp->speed = vgl->realtime_fix.fix.speed;  
        tp->course = vgl->realtime_fix.fix.track;
        tp->nsats = vgl->realtime_fix.satellites_used;
        tp->fix_mode = vgl->realtime_fix.fix.mode;

        ll.lat = vgl->realtime_fix.fix.latitude;
        ll.lon = vgl->realtime_fix.fix.longitude;
        vik_coord_load_from_latlon(&tp->coord,
             vik_trw_layer_get_coord_mode(vgl->trw_children[TRW_REALTIME]), &ll);

        vik_track_add_trackpoint ( vgl->realtime_track, tp, TRUE ); // Ensure bounds is recalculated
        vgl->realtime_fix.dirty = FALSE;
        vgl->realtime_fix.satellites_used = 0;
        vgl->last_fix = vgl->realtime_fix;
        return tp;
      }
    }
    return NULL;
}

#define VIK_SETTINGS_GPS_STATUSBAR_FORMAT "gps_statusbar_format"

static void update_statusbar ( VikGpsLayer *vgl, VikWindow *vw )
{
  gchar *statusbar_format_code = NULL;
  gboolean need2free = FALSE;
  if ( !a_settings_get_string ( VIK_SETTINGS_GPS_STATUSBAR_FORMAT, &statusbar_format_code ) ) {
    // Otherwise use default
    statusbar_format_code = g_strdup ( "GSA" );
    need2free = TRUE;
  }

  gchar *msg = vu_trackpoint_formatted_message ( statusbar_format_code, vgl->trkpt, vgl->trkpt_prev, vgl->realtime_track );
  vik_statusbar_set_message ( vik_window_get_statusbar (vw), VIK_STATUSBAR_INFO, msg );
  g_free ( msg );

  if ( need2free )
    g_free ( statusbar_format_code );

}

static void gpsd_raw_hook(VglGpsd *vgpsd, gchar *data)
{
  gboolean update_all = FALSE;
  VikGpsLayer *vgl = vgpsd->vgl;

  if (!vgl->realtime_tracking) {
    g_warning("%s: receiving GPS data while not in realtime mode", __PRETTY_FUNCTION__);
    return;
  }

  if ((vgpsd->gpsd.fix.mode >= MODE_2D) &&
      !isnan(vgpsd->gpsd.fix.latitude) &&
      !isnan(vgpsd->gpsd.fix.longitude)) {

    VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vgl));
    VikViewport *vvp = vik_window_viewport(vw);
    vgl->realtime_fix.fix = vgpsd->gpsd.fix;
    vgl->realtime_fix.satellites_used = vgpsd->gpsd.satellites_used;
    vgl->realtime_fix.dirty = TRUE;

    struct LatLon ll;
    VikCoord vehicle_coord;

    ll.lat = vgl->realtime_fix.fix.latitude;
    ll.lon = vgl->realtime_fix.fix.longitude;
    vik_coord_load_from_latlon(&vehicle_coord,
           vik_trw_layer_get_coord_mode(vgl->trw_children[TRW_REALTIME]), &ll);

    if ((vgl->vehicle_position == VEHICLE_POSITION_CENTERED) ||
        (vgl->realtime_jump_to_start && vgl->first_realtime_trackpoint)) {
      vik_viewport_set_center_coord(vvp, &vehicle_coord);
      update_all = TRUE;
    }
    else if (vgl->vehicle_position == VEHICLE_POSITION_ON_SCREEN) {
      const int hdiv = 6;
      const int vdiv = 6;
      const int px = 20; /* adjust ment in pixels to make sure vehicle is inside the box */
      gint width = vik_viewport_get_width(vvp);
      gint height = vik_viewport_get_height(vvp);
      gint vx, vy;

      vik_viewport_coord_to_screen(vvp, &vehicle_coord, &vx, &vy);
      update_all = TRUE;
      if (vx < (width/hdiv))
        vik_viewport_set_center_screen(vvp, vx - width/2 + width/hdiv + px, vy);
      else if (vx > (width - width/hdiv))
        vik_viewport_set_center_screen(vvp, vx + width/2 - width/hdiv - px, vy);
      else if (vy < (height/vdiv))
        vik_viewport_set_center_screen(vvp, vx, vy - height/2 + height/vdiv + px);
      else if (vy > (height - height/vdiv))
        vik_viewport_set_center_screen(vvp, vx, vy + height/2 - height/vdiv - px);
      else
        update_all = FALSE;
    }

    vgl->first_realtime_trackpoint = FALSE;

    vgl->trkpt = create_realtime_trackpoint ( vgl, FALSE );

    if ( vgl->trkpt ) {
      if ( vgl->realtime_update_statusbar )
	update_statusbar ( vgl, vw );
      vgl->trkpt_prev = vgl->trkpt;
    }

    vik_layer_emit_update ( update_all ? VIK_LAYER(vgl) : VIK_LAYER(vgl->trw_children[TRW_REALTIME]) ); // NB update from background thread
  }
}

static gboolean gpsd_data_available(GIOChannel *source, GIOCondition condition, gpointer data)
{
  VikGpsLayer *vgl = data;
  if (condition == G_IO_IN) {
#if GPSD_API_MAJOR_VERSION == 3 || GPSD_API_MAJOR_VERSION == 4
    if (!gps_poll(&vgl->vgpsd->gpsd)) {
#elif GPSD_API_MAJOR_VERSION == 5
    if (gps_read(&vgl->vgpsd->gpsd) > -1) {
      // Reuse old function to perform operations on the new GPS data
      gpsd_raw_hook(vgl->vgpsd, NULL);
#else
      // Broken compile
#endif
      return TRUE;
    }
    else {
      g_warning("Disconnected from gpsd. Trying to reconnect");
      rt_gpsd_disconnect(vgl);
      rt_gpsd_connect(vgl, FALSE);
    }
  }
  return FALSE; /* no further calling */
}

static gchar *make_track_name(VikTrwLayer *vtl)
{
  const gchar basename[] = "REALTIME";
  const gint bufsize = sizeof(basename) + 5;
  gchar *name = g_malloc(bufsize);
  strcpy(name, basename);
  gint i = 2;

  while (vik_trw_layer_get_track(vtl, name) != NULL) {
    g_snprintf(name, bufsize, "%s#%d", basename, i);
    i++;
  }
  return(name);

}

static gboolean rt_gpsd_try_connect(gpointer *data)
{
  VikGpsLayer *vgl = (VikGpsLayer *)data;
#if GPSD_API_MAJOR_VERSION == 3
  struct gps_data_t *gpsd = gps_open(vgl->gpsd_host, vgl->gpsd_port);

  if (gpsd == NULL) {
#elif GPSD_API_MAJOR_VERSION == 4
  vgl->vgpsd = g_malloc(sizeof(VglGpsd));

  if (gps_open_r(vgl->gpsd_host, vgl->gpsd_port, /*(struct gps_data_t *)*/vgl->vgpsd) != 0) {
#elif GPSD_API_MAJOR_VERSION == 5
  vgl->vgpsd = g_malloc(sizeof(VglGpsd));
  if (gps_open(vgl->gpsd_host, vgl->gpsd_port, &vgl->vgpsd->gpsd) != 0) {
#else
// Delibrately break compilation...
#endif
    g_warning("Failed to connect to gpsd at %s (port %s). Will retry in %d seconds",
                     vgl->gpsd_host, vgl->gpsd_port, vgl->gpsd_retry_interval);
    return TRUE;   /* keep timer running */
  }

#if GPSD_API_MAJOR_VERSION == 3
  vgl->vgpsd = realloc(gpsd, sizeof(VglGpsd));
#endif
  vgl->vgpsd->vgl = vgl;

  vgl->realtime_fix.dirty = vgl->last_fix.dirty = FALSE;
  /* track alt/time graph uses VIK_DEFAULT_ALTITUDE (0.0) as invalid */
  vgl->realtime_fix.fix.altitude = vgl->last_fix.fix.altitude = VIK_DEFAULT_ALTITUDE;
  vgl->realtime_fix.fix.speed = vgl->last_fix.fix.speed = NAN;

  if (vgl->realtime_record) {
    VikTrwLayer *vtl = vgl->trw_children[TRW_REALTIME];
    vgl->realtime_track = vik_track_new();
    vgl->realtime_track->visible = TRUE;
    vik_trw_layer_add_track(vtl, make_track_name(vtl), vgl->realtime_track);
  }

#if GPSD_API_MAJOR_VERSION == 3 || GPSD_API_MAJOR_VERSION == 4
  gps_set_raw_hook(&vgl->vgpsd->gpsd, gpsd_raw_hook);
#endif

  vgl->realtime_io_channel = g_io_channel_unix_new(vgl->vgpsd->gpsd.gps_fd);
  vgl->realtime_io_watch_id = g_io_add_watch( vgl->realtime_io_channel,
                    G_IO_IN | G_IO_ERR | G_IO_HUP, gpsd_data_available, vgl);

#if GPSD_API_MAJOR_VERSION == 3
  gps_query(&vgl->vgpsd->gpsd, "w+x");
#endif
#if GPSD_API_MAJOR_VERSION == 4 || GPSD_API_MAJOR_VERSION == 5
  gps_stream(&vgl->vgpsd->gpsd, WATCH_ENABLE, NULL);
#endif

  return FALSE;  /* no longer called by timeout */
}

static gboolean rt_ask_retry(VikGpsLayer *vgl)
{
  GtkWidget *dialog = gtk_message_dialog_new (VIK_GTK_WINDOW_FROM_LAYER(vgl),
                          GTK_DIALOG_DESTROY_WITH_PARENT,
                          GTK_MESSAGE_QUESTION,
                          GTK_BUTTONS_YES_NO,
                          "Failed to connect to gpsd at %s (port %s)\n"
                          "Should Viking keep trying (every %d seconds)?",
                          vgl->gpsd_host, vgl->gpsd_port,
                          vgl->gpsd_retry_interval);

  gint res = gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
  return (res == GTK_RESPONSE_YES);
}

static gboolean rt_gpsd_connect(VikGpsLayer *vgl, gboolean ask_if_failed)
{
  vgl->realtime_retry_timer = 0;
  if (rt_gpsd_try_connect((gpointer *)vgl)) {
    if (vgl->gpsd_retry_interval <= 0) {
      g_warning("Failed to connect to gpsd but will not retry because retry intervel was set to %d (which is 0 or negative)", vgl->gpsd_retry_interval);
      return FALSE;
    }
    else if (ask_if_failed && !rt_ask_retry(vgl))
      return FALSE;
    else
      vgl->realtime_retry_timer = g_timeout_add_seconds(vgl->gpsd_retry_interval,
        (GSourceFunc)rt_gpsd_try_connect, (gpointer *)vgl);
  }
  return TRUE;
}

static void rt_gpsd_disconnect(VikGpsLayer *vgl)
{
  if (vgl->realtime_retry_timer) {
    g_source_remove(vgl->realtime_retry_timer);
    vgl->realtime_retry_timer = 0;
  }
  if (vgl->realtime_io_watch_id) {
    g_source_remove(vgl->realtime_io_watch_id);
    vgl->realtime_io_watch_id = 0;
  }
  if (vgl->realtime_io_channel) {
    GError *error = NULL;
    g_io_channel_shutdown (vgl->realtime_io_channel, FALSE, &error);
    vgl->realtime_io_channel = NULL;
  }
  if (vgl->vgpsd) {
#if GPSD_API_MAJOR_VERSION == 4 || GPSD_API_MAJOR_VERSION == 5
    gps_stream(&vgl->vgpsd->gpsd, WATCH_DISABLE, NULL);
#endif
    gps_close(&vgl->vgpsd->gpsd);
#if GPSD_API_MAJOR_VERSION == 3
    free(vgl->vgpsd);
#elif GPSD_API_MAJOR_VERSION == 4 || GPSD_API_MAJOR_VERSION == 5
    g_free(vgl->vgpsd);
#endif
    vgl->vgpsd = NULL;
  }

  if (vgl->realtime_record && vgl->realtime_track) {
    if ((vgl->realtime_track->trackpoints == NULL) || (vgl->realtime_track->trackpoints->next == NULL))
      vik_trw_layer_delete_track(vgl->trw_children[TRW_REALTIME], vgl->realtime_track);
    vgl->realtime_track = NULL;
  }
}

static void gps_start_stop_tracking_cb( gpointer layer_and_vlp[2])
{
  VikGpsLayer *vgl = (VikGpsLayer *)layer_and_vlp[0];
  vgl->realtime_tracking = (vgl->realtime_tracking == FALSE);

  /* Make sure we are still in the boat with libgps */
  g_assert((VIK_GPS_MODE_2D == MODE_2D) && (VIK_GPS_MODE_3D == MODE_3D));

  if (vgl->realtime_tracking) {
    vgl->first_realtime_trackpoint = TRUE;
    if (!rt_gpsd_connect(vgl, TRUE)) {
      vgl->first_realtime_trackpoint = FALSE;
      vgl->realtime_tracking = FALSE;
      vgl->trkpt = NULL;
    }
  }
  else {  /* stop realtime tracking */
    vgl->first_realtime_trackpoint = FALSE;
    vgl->trkpt = NULL;
    rt_gpsd_disconnect(vgl);
  }
}
#endif /* VIK_CONFIG_REALTIME_GPS_TRACKING */
