/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2020, Rob Norris <rw_norris@hotmail.com>
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
#include "vikgeocluelayer.h"
#include "viking.h"
#include "libgeoclue.h"
#include "icons/icons.h"

#define GEOCLUE_FIXED_NAME "GeoClue"

static VikGeoclueLayer *vik_geoclue_layer_create ( VikViewport *vp);
static void vik_geoclue_layer_realize ( VikGeoclueLayer *vgl, VikTreeview *vt, GtkTreeIter *layer_iter );
static void vik_geoclue_layer_post_read ( VikGeoclueLayer *vgl, VikViewport *vp, gboolean from_file );
static void vik_geoclue_layer_free ( VikGeoclueLayer *vgl );
static void vik_geoclue_layer_draw ( VikGeoclueLayer *vgl, VikViewport *vp );
static VikGeoclueLayer *vik_geoclue_layer_new ( VikViewport *vp );

static void geoclue_layer_marshall( VikGeoclueLayer *vgl, guint8 **data, guint *len );
static VikGeoclueLayer *geoclue_layer_unmarshall ( guint8 *data, guint len, VikViewport *vvp );
static gboolean geoclue_layer_set_param ( VikGeoclueLayer *vgl, VikLayerSetParam *vlsp );
static VikLayerParamData geoclue_layer_get_param ( VikGeoclueLayer *vgl, guint16 id, gboolean is_file_operation );

static const gchar* geoclue_layer_tooltip ( VikGeoclueLayer *vgl );

static void geoclue_layer_change_coord_mode ( VikGeoclueLayer *vgl, VikCoordMode mode );
static void geoclue_layer_add_menu_items ( VikGeoclueLayer *vtl, GtkMenu *menu, gpointer vlp );

typedef gpointer menu_array_layer[2];
static void geoclue_empty_cb ( menu_array_layer values );
static void geoclue_start_stop_tracking_cb ( menu_array_layer values );

static gchar *params_position[] = {
	N_("Keep position at center"),
	N_("Keep position on screen"),
	N_("Disable"),
	NULL
};

typedef enum {
	POSITION_CENTERED = 0,
	POSITION_ON_SCREEN,
	POSITION_NONE,
} position_update_t;

static VikLayerParamData moving_map_method_default ( void ) { return VIK_LPD_UINT ( POSITION_CENTERED ); }

static VikLayerParamData color_default ( void ) {
	VikLayerParamData data; gdk_color_parse ( "purple", &data.c ); return data;
}

static void reset_cb ( GtkWidget *widget, gpointer ptr )
{
	a_layer_defaults_reset_show ( GEOCLUE_FIXED_NAME, ptr, VIK_LAYER_GROUP_NONE );
}

static VikLayerParamData reset_default ( void ) { return VIK_LPD_PTR(reset_cb); }

static VikLayerParam geoclue_layer_params[] = {
	{ VIK_LAYER_GEOCLUE, "auto_connect", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Auto Connect"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, N_("Automatically connect to GeoClue"), vik_lpd_true_default, NULL, NULL },
	{ VIK_LAYER_GEOCLUE, "record_tracking", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Recording tracks"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
	{ VIK_LAYER_GEOCLUE, "center_start_tracking", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Jump to current position on start"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, NULL, vik_lpd_true_default, NULL, NULL },
	{ VIK_LAYER_GEOCLUE, "moving_map_method", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Moving Map Method:"), VIK_LAYER_WIDGET_RADIOGROUP_STATIC, params_position, NULL, NULL, moving_map_method_default, NULL, NULL },
	{ VIK_LAYER_GEOCLUE, "update_statusbar", VIK_LAYER_PARAM_BOOLEAN, VIK_LAYER_GROUP_NONE, N_("Update Statusbar:"), VIK_LAYER_WIDGET_CHECKBUTTON, NULL, NULL, N_("Display information in the statusbar on GeoClue updates"), vik_lpd_true_default, NULL, NULL },
	{ VIK_LAYER_GEOCLUE, "color", VIK_LAYER_PARAM_COLOR, VIK_LAYER_GROUP_NONE, N_("Color:"), VIK_LAYER_WIDGET_COLOR, NULL, NULL, NULL, color_default, NULL, NULL },
	{ VIK_LAYER_GEOCLUE, "reset", VIK_LAYER_PARAM_PTR_DEFAULT, VIK_LAYER_GROUP_NONE, NULL,
	  VIK_LAYER_WIDGET_BUTTON, N_("Reset to Defaults"), NULL, NULL, reset_default, NULL, NULL },
};

typedef enum {
	PARAM_AUTO_CONNECT=0,
	PARAM_RECORD,
	PARAM_CENTER_START,
	PARAM_POSITION,
	PARAM_UPDATE_STATUSBAR,
	PARAM_COLOR,
	PARAM_RESET,
	NUM_PARAMS
} geoclue_param_t;

VikLayerInterface vik_geoclue_layer_interface = {
  GEOCLUE_FIXED_NAME,
  N_("GeoClue"),
  NULL,
  &vikgeocluelayer_pixbuf,

  NULL,
  0,

  geoclue_layer_params,
  NUM_PARAMS,
  NULL,
  0,

  VIK_MENU_ITEM_ALL,

  (VikLayerFuncCreate)                  vik_geoclue_layer_create,
  (VikLayerFuncRealize)                 vik_geoclue_layer_realize,
  (VikLayerFuncPostRead)                vik_geoclue_layer_post_read,
  (VikLayerFuncFree)                    vik_geoclue_layer_free,

  (VikLayerFuncProperties)              NULL,
  (VikLayerFuncDraw)                    vik_geoclue_layer_draw,
  (VikLayerFuncChangeCoordMode)         geoclue_layer_change_coord_mode,

  (VikLayerFuncGetTimestamp)            NULL,

  (VikLayerFuncSetMenuItemsSelection)   NULL,
  (VikLayerFuncGetMenuItemsSelection)   NULL,

  (VikLayerFuncAddMenuItems)            geoclue_layer_add_menu_items,
  (VikLayerFuncSublayerAddMenuItems)    NULL,

  (VikLayerFuncSublayerRenameRequest)   NULL,
  (VikLayerFuncSublayerToggleVisible)   NULL,
  (VikLayerFuncSublayerTooltip)         NULL,
  (VikLayerFuncLayerTooltip)            geoclue_layer_tooltip,
  (VikLayerFuncLayerSelected)           NULL,
  (VikLayerFuncLayerToggleVisible)      NULL,

  (VikLayerFuncMarshall)                geoclue_layer_marshall,
  (VikLayerFuncUnmarshall)              geoclue_layer_unmarshall,

  (VikLayerFuncSetParam)                geoclue_layer_set_param,
  (VikLayerFuncGetParam)                geoclue_layer_get_param,
  (VikLayerFuncChangeParam)             NULL,

  (VikLayerFuncReadFileData)            NULL,
  (VikLayerFuncWriteFileData)           NULL,

  (VikLayerFuncDeleteItem)              NULL,
  (VikLayerFuncCutItem)                 NULL,
  (VikLayerFuncCopyItem)                NULL,
  (VikLayerFuncPasteItem)               NULL,
  (VikLayerFuncFreeCopiedItem)          NULL,
  (VikLayerFuncDragDropRequest)         NULL,

  (VikLayerFuncSelectClick)             NULL,
  (VikLayerFuncSelectMove)              NULL,
  (VikLayerFuncSelectRelease)           NULL,
  (VikLayerFuncSelectedViewportMenu)    NULL,
};

struct _VikGeoclueLayer {
	VikLayer vl;
	VikTrwLayer *trw;
	gboolean tracking;
	gboolean first_trackpoint;

	GClueSimple *simple;
	GClueClient *client;

	VikCoord coord;
	gboolean coord_valid;

	VikTrack *track;
	GdkGC *track_pt_gc;

	// params
	gboolean auto_connect;
	gboolean record;
	gboolean jump_to_start;
	guint position;
	gboolean update_statusbar;
	GdkColor color;

	VikTrackpoint *trkpt;
	VikTrackpoint *trkpt_prev;
};

GType vik_geoclue_layer_get_type ()
{
  static GType val_type = 0;

  if (!val_type)
  {
    static const GTypeInfo val_info =
    {
      sizeof (VikGeoclueLayerClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      NULL, /* class init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikGeoclueLayer),
      0,
      NULL /* instance init */
    };
    val_type = g_type_register_static ( VIK_LAYER_TYPE, "VikGeoclueLayer", &val_info, 0 );
  }

  return val_type;
}

static VikGeoclueLayer *vik_geoclue_layer_create ( VikViewport *vp )
{
	VikGeoclueLayer *rv = vik_geoclue_layer_new ( vp );
	vik_layer_rename ( VIK_LAYER(rv), vik_geoclue_layer_interface.name );
	rv->trw = VIK_TRW_LAYER(vik_layer_create ( VIK_LAYER_TRW, vp, FALSE ));
	vik_layer_set_menu_items_selection ( VIK_LAYER(rv->trw), VIK_MENU_ITEM_ALL & ~(VIK_MENU_ITEM_CUT|VIK_MENU_ITEM_DELETE) );
	return rv;
}

static const gchar* geoclue_layer_tooltip ( VikGeoclueLayer *vgl )
{
	if ( vgl->tracking )
		return _("Connected");
	else
		return _("Disconnected");
}

/* "Copy" */
static void geoclue_layer_marshall( VikGeoclueLayer *vgl, guint8 **data, guint *datalen )
{
	VikLayer *layer;
	guint8 *ld; 
	guint ll;
	GByteArray* b = g_byte_array_new ();
	guint len;

#define alm_append(obj, sz) 	\
	len = (sz);												\
	g_byte_array_append ( b, (guint8 *)&len, sizeof(len) );	\
	g_byte_array_append ( b, (guint8 *)(obj), len );

	vik_layer_marshall_params(VIK_LAYER(vgl), &ld, &ll);
	alm_append(ld, ll);
	g_free(ld);

	layer = VIK_LAYER(vgl->trw);
	vik_layer_marshall(layer, &ld, &ll);
	if (ld) {
		alm_append(ld, ll);
		g_free(ld);
	}
	*data = b->data;
	*datalen = b->len;
	g_byte_array_free(b, FALSE);
#undef alm_append
}

/* "Paste" */
static VikGeoclueLayer *geoclue_layer_unmarshall ( guint8 *data, guint len, VikViewport *vvp )
{
#define alm_size (*(guint *)data)
#define alm_next \
	len -= sizeof(guint) + alm_size; \
	data += sizeof(guint) + alm_size;
  
	VikGeoclueLayer *rv = vik_geoclue_layer_new(vvp);

	vik_layer_unmarshall_params ( VIK_LAYER(rv), data+sizeof(guint), alm_size, vvp );
	alm_next;

	VikLayer *layer = vik_layer_unmarshall ( data + sizeof(guint), alm_size, vvp );
	if (layer) {
		rv->trw = (VikTrwLayer*)layer;
		// NB no need to attach signal update handler here
		//  as this will always be performed later on in vik_geoclue_layer_realize()
	}
	alm_next;
	return rv;
#undef alm_size
#undef alm_next
}

static gboolean geoclue_layer_set_param ( VikGeoclueLayer *vgl, VikLayerSetParam *vlsp )
{
	switch ( vlsp->id ) {
	case PARAM_AUTO_CONNECT:     vgl->auto_connect = vlsp->data.b; break;
	case PARAM_RECORD:           vgl->record = vlsp->data.b; break;
	case PARAM_CENTER_START:     vgl->jump_to_start = vlsp->data.b; break;
	case PARAM_POSITION:         vgl->position = vlsp->data.u; break;
	case PARAM_UPDATE_STATUSBAR: vgl->update_statusbar = vlsp->data.b; break;
	case PARAM_COLOR:            vgl->color = vlsp->data.c; break;
	default: g_warning("geoclue_layer_set_param(): unknown parameter");	break;
	}
	return TRUE;
}

static VikLayerParamData geoclue_layer_get_param ( VikGeoclueLayer *vgl, guint16 id, gboolean is_file_operation )
{
	VikLayerParamData rv;
	switch ( id ) {
	case PARAM_AUTO_CONNECT:     rv.b = vgl->auto_connect; break;
	case PARAM_RECORD:           rv.b = vgl->record; break;
	case PARAM_CENTER_START:     rv.b = vgl->jump_to_start; break;
	case PARAM_POSITION:         rv.u = vgl->position; break;
	case PARAM_UPDATE_STATUSBAR: rv.u = vgl->update_statusbar; break;
	case PARAM_COLOR:            rv.c = vgl->color; break;
	default:
		g_warning ( _("%s: unknown parameter"), __FUNCTION__ );
		break;
	}
	return rv;
}

VikGeoclueLayer *vik_geoclue_layer_new ( VikViewport *vp )
{
	VikGeoclueLayer *vgl = VIK_GEOCLUE_LAYER ( g_object_new ( VIK_GEOCLUE_LAYER_TYPE, NULL ) );
	vik_layer_set_type ( VIK_LAYER(vgl), VIK_LAYER_GEOCLUE );
	if ( vp ) {
		vgl->track_pt_gc = vik_viewport_new_gc_from_color ( vp, &(vgl->color), 2 );
	}
	vik_layer_set_defaults ( VIK_LAYER(vgl), vp );
	// Everything else is 0, FALSE or NULL
	return vgl;
}

static void tracking_draw ( VikGeoclueLayer *vgl, VikViewport *vp )
{
	guint tp_size = 4 *	vik_viewport_get_scale(vp);

	if ( vgl->coord_valid ) {
		struct LatLon ll;
		VikCoord nw, se;
		struct LatLon lnw, lse;
		vik_viewport_screen_to_coord ( vp, -20, -20, &nw );
		vik_viewport_screen_to_coord ( vp, vik_viewport_get_width(vp)+20, vik_viewport_get_width(vp)+20, &se );
		vik_coord_to_latlon ( &nw, &lnw );
		vik_coord_to_latlon ( &se, &lse );

		vik_coord_to_latlon ( &vgl->coord, &ll );

		// If it is in the viewport then draw something
		if ( ll.lat > lse.lat &&
		     ll.lat < lnw.lat &&
		     ll.lon > lnw.lon &&
		     ll.lon < lse.lon ) {
			// TODO - Draw a triangle in direction of heading...
			//        either use heading value directly from geoclue,
			//        or maybe work out heading from the previous location
			// Maybe allow configuration of triangle size?
			gint x, y;
			vik_viewport_coord_to_screen ( vp, &vgl->coord, &x, &y );
			vik_viewport_draw_rectangle ( vp, vgl->track_pt_gc, TRUE, x-tp_size, y-tp_size, tp_size*2, tp_size*2 );
		}
	}
}

static void vik_geoclue_layer_draw ( VikGeoclueLayer *vgl, VikViewport *vp )
{
	// NB I don't understand this half drawn business
	// This is just copied from vikgpslayer.c
	VikLayer *vl;
	VikLayer *trigger = VIK_LAYER(vik_viewport_get_trigger( vp ));

	vl = VIK_LAYER(vgl->trw);
	if (vl == trigger) {
		if ( vik_viewport_get_half_drawn ( vp ) ) {
			vik_viewport_set_half_drawn ( vp, FALSE );
			vik_viewport_snapshot_load( vp );
		} else {
			vik_viewport_snapshot_save( vp );
		}
	}
	if ( !vik_viewport_get_half_drawn(vp) )
		vik_layer_draw ( vl, vp );

	if ( vgl->tracking ) {
		if ( VIK_LAYER(vgl) == trigger ) {
			if ( vik_viewport_get_half_drawn ( vp ) ) {
				vik_viewport_set_half_drawn ( vp, FALSE );
				vik_viewport_snapshot_load ( vp );
			} else {
				vik_viewport_snapshot_save ( vp );
			}
		}
		if ( !vik_viewport_get_half_drawn(vp) )
			tracking_draw ( vgl, vp );
  }
}

static void geoclue_layer_change_coord_mode ( VikGeoclueLayer *vgl, VikCoordMode mode )
{
	vik_layer_change_coord_mode ( VIK_LAYER(vgl->trw), mode );
}

static void geoclue_layer_add_menu_items ( VikGeoclueLayer *vgl, GtkMenu *menu, gpointer vlp )
{
	static gpointer values[2];
	GtkWidget *item;
	values[0] = vgl;
	values[1] = vlp;

	item = gtk_menu_item_new();
	gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
	gtk_widget_show ( item );

	item = gtk_image_menu_item_new_with_mnemonic ( vgl->tracking ?
												   "_Stop Tracking" :
												   "_Start Tracking" );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, vgl->tracking ?
									gtk_image_new_from_stock (GTK_STOCK_MEDIA_STOP, GTK_ICON_SIZE_MENU) :
									gtk_image_new_from_stock (GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(geoclue_start_stop_tracking_cb), values );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );

	item = gtk_menu_item_new();
	gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
	gtk_widget_show ( item );

	item = gtk_image_menu_item_new_with_mnemonic ( _("_Empty") );
	gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU) );
	g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(geoclue_empty_cb), values );
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show ( item );
}

static void disconnect_layer_signal ( VikLayer *vl, VikGeoclueLayer *vgl )
{
	guint number_handlers = g_signal_handlers_disconnect_matched(vl, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, vgl);
	if ( number_handlers != 1 ) {
		g_critical ( _("Unexpected number of disconnected handlers: %d"), number_handlers );
	}
}

static void vik_geoclue_layer_free ( VikGeoclueLayer *vgl )
{
    if ( vgl->client )
		g_clear_object ( &vgl->client );
    if ( vgl->simple )
		g_clear_object ( &vgl->simple );
	if ( vgl->vl.realized )
		disconnect_layer_signal ( VIK_LAYER(vgl->trw), vgl );
	g_object_unref ( vgl->trw );
	if ( vgl->track_pt_gc != NULL )
		g_object_unref ( vgl->track_pt_gc );
}

gboolean vik_geoclue_layer_is_empty ( VikGeoclueLayer *vgl )
{
	if ( vgl->trw )
		return FALSE;
	return TRUE;
}

VikTrwLayer* vik_geoclue_layer_get_trw ( VikGeoclueLayer *vgl )
{
	return vgl->trw;
}

static void geoclue_empty_cb ( menu_array_layer values )
{
	VikGeoclueLayer *vgl = (VikGeoclueLayer*)values[0];
	// Get confirmation from the user
	if ( ! a_dialog_yes_or_no ( VIK_GTK_WINDOW_FROM_WIDGET(values[1]),
								_("Are you sure you want to delete geoclue data?"),
								NULL ) )
		return;
	vik_trw_layer_delete_all_waypoints ( vgl->trw );
	vik_trw_layer_delete_all_tracks ( vgl->trw );
}

static VikTrackpoint* create_trackpoint ( VikGeoclueLayer *vgl, VikCoord coord, GClueLocation *location )
{
	VikTrackpoint *tp = vik_trackpoint_new();
	tp->newsegment = FALSE;

	// NB Very unlikely you'd get times from a computer or geoclue feed from before 1970!
	// Hence not going to try to support calculations of such times

	// Default to use the time of 'now'
	GDateTime *gdt = g_date_time_new_now_utc ();
	if ( gdt ) {
		tp->timestamp = g_date_time_to_unix ( gdt );
		tp->timestamp += (gdouble)g_date_time_get_microsecond(gdt) / G_TIME_SPAN_SECOND;
		g_date_time_unref ( gdt );
	}

	// Assumed speed is in m/s
	gdouble speed = gclue_location_get_speed ( location );
	if ( speed >= 0.0 )
		tp->speed = speed;
	// Unknown what units (degrees, radians, other?) this is
	//gdouble heading = gclue_location_get_heading ( location );
	//if ( heading >= 0 )
	//	tp->course = heading;
	tp->fix_mode = VIK_GPS_MODE_2D;
	gdouble altitude = gclue_location_get_altitude ( location );
	if ( altitude != -G_MAXDOUBLE ) {
		tp->altitude = altitude;
		tp->fix_mode = VIK_GPS_MODE_3D;
	}
	GVariant *timestamp = gclue_location_get_timestamp ( location );
	if ( timestamp ) {
		// Unclear whether this time type will always be
		GTimeVal tv = { 0 };
		g_variant_get (timestamp, "(tt)", &tv.tv_sec, &tv.tv_usec);
		tp->timestamp = (gdouble)tv.tv_sec + (gdouble)tv.tv_usec / G_USEC_PER_SEC;
	}

	// Totally unclear what units this 'accuracy' is in - metres?, its own accuracy level enum?
	//tp->hdop = gclue_location_get_accuracy ( location );
	tp->coord = coord;

	vik_track_add_trackpoint ( vgl->track, tp, TRUE ); // Ensure bounds is recalculated
	return tp;
}

#define VIK_SETTINGS_GEOCLUE_STATUSBAR_FORMAT "geoclue_statusbar_format"

static void update_statusbar ( VikGeoclueLayer *vgl, VikWindow *vw )
{
	gchar *statusbar_format_code = NULL;
	gboolean need2free = FALSE;
	if ( !a_settings_get_string ( VIK_SETTINGS_GEOCLUE_STATUSBAR_FORMAT, &statusbar_format_code ) ) {
		// Otherwise use default
		statusbar_format_code = g_strdup ( "SA" );
		need2free = TRUE;
	}

	gchar *msg = vu_trackpoint_formatted_message ( statusbar_format_code, vgl->trkpt, vgl->trkpt_prev, vgl->track, NAN );
	vik_statusbar_set_message ( vik_window_get_statusbar(vw), VIK_STATUSBAR_INFO, msg );
	g_free ( msg );

	if ( need2free )
		g_free ( statusbar_format_code );
}

/**
 * make_track_name:
 *
 * returns allocated string for a new track name
 * free string after use
 */
static gchar *make_track_name ( VikTrwLayer *vtl )
{
	const gchar basename[] = N_("Track");
	const gint bufsize = sizeof(basename) + 5;
	gchar *name = g_malloc ( bufsize );
	strcpy ( name, basename );
	gint i = 2;

	while ( vik_trw_layer_get_track(vtl, name) != NULL ) {
		g_snprintf(name, bufsize, "%s#%d", basename, i);
		i++;
	}
	return name;
}

static void
notify_location ( GClueSimple *simple,
                  GParamSpec *pspec,
                  gpointer    user_data )
{
	VikGeoclueLayer *vgl = (VikGeoclueLayer*)user_data;
	gboolean update_all = FALSE;

	VikWindow *vw = VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vgl));
	VikViewport *vvp = vik_window_viewport(vw);

	GClueLocation *location = gclue_simple_get_location ( vgl->simple );
	struct LatLon ll;
	ll.lat = gclue_location_get_latitude ( location );
	ll.lon = gclue_location_get_longitude ( location );
	vik_coord_load_from_latlon ( &vgl->coord, vik_trw_layer_get_coord_mode(vgl->trw), &ll );
	vgl->coord_valid = TRUE;

	if ( (vgl->position == POSITION_CENTERED) ||
	     (vgl->jump_to_start && vgl->first_trackpoint) ) {
	  vik_viewport_set_center_coord ( vvp, &vgl->coord, FALSE );
	  update_all = TRUE;
	}
	else if ( vgl->position == POSITION_ON_SCREEN ) {
	  const int hdiv = 6;
	  const int vdiv = 6;
	  const int px = 20;
	  gint width = vik_viewport_get_width ( vvp );
	  gint height = vik_viewport_get_height ( vvp );
	  gint vx, vy;

	  vik_viewport_coord_to_screen ( vvp, &vgl->coord, &vx, &vy );
	  update_all = TRUE;
	  if ( vx < (width/hdiv) )
		  vik_viewport_set_center_screen ( vvp, vx - width/2 + width/hdiv + px, vy );
	  else if ( vx > (width - width/hdiv) )
		  vik_viewport_set_center_screen ( vvp, vx + width/2 - width/hdiv - px, vy );
	  else if ( vy < (height/vdiv) )
		  vik_viewport_set_center_screen(vvp, vx, vy - height/2 + height/vdiv + px );
	  else if ( vy > (height - height/vdiv) )
		  vik_viewport_set_center_screen ( vvp, vx, vy + height/2 - height/vdiv - px );
	  else
		  update_all = FALSE;
	}

	vgl->trkpt = vgl->record ? create_trackpoint ( vgl, vgl->coord, location ) : NULL;
	vgl->first_trackpoint = FALSE;
	
	if ( vgl->trkpt ) {
		if ( vgl->update_statusbar )
			update_statusbar ( vgl, vw );
		vgl->trkpt_prev = vgl->trkpt;
	}

	vik_layer_emit_update ( update_all ? VIK_LAYER(vgl) : VIK_LAYER(vgl->trw) ); // NB update from background thread
}

static void
notify_active ( GClueClient *client,
                GParamSpec *pspec,
                gpointer    user_data )
{
	if ( gclue_client_get_active (client) )
		return;

	g_warning ( "%s: geoclue no longer active", __FUNCTION__ );
	// Deactivate
	VikGeoclueLayer *vgl = (VikGeoclueLayer*)user_data;
	vgl->tracking = FALSE;
	vgl->coord_valid = TRUE;
	vik_statusbar_set_message ( vik_window_get_statusbar (VIK_WINDOW(VIK_GTK_WINDOW_FROM_LAYER(vgl))),
	                            VIK_STATUSBAR_INFO, _("GeoClue disabled") );
}

static void
on_simple_ready ( GObject      *source_object,
                  GAsyncResult *res,
                  gpointer      user_data )
{
	GError *error = NULL;

	VikGeoclueLayer *vgl = (VikGeoclueLayer*)user_data;

	vgl->simple = gclue_simple_new_finish ( res, &error );
	if ( error != NULL ) {
		vgl->tracking = FALSE;
		a_dialog_error_msg_extra ( VIK_GTK_WINDOW_FROM_LAYER(vgl), _("Failed to connect to GeoClue service: %s"), error->message );
		g_error_free ( error );
		goto finish;
	}

	vgl->client = gclue_simple_get_client ( vgl->simple );
	if ( !vgl->client ) {
		vgl->tracking = FALSE;
		// Possibly client not available for the requested accuracy level (we always use 'EXACT')
		//  lesser accurate levels won't make much sense for tracking positions
		a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_LAYER(vgl), _("GeoClue client unavailable") );
		goto finish;
	}
	g_object_ref ( vgl->client );
	g_debug ( "%s: Client object: %s", __FUNCTION__, g_dbus_proxy_get_object_path(G_DBUS_PROXY(vgl->client)) );

	gclue_client_set_time_threshold ( vgl->client, 1 ); // Assume this is in seconds

	if ( vik_verbose ) {
		GClueLocation *location = gclue_simple_get_location ( vgl->simple );
		libgeoclue_print_location ( location );
	}

	if ( vgl->record ) {
		VikTrwLayer *vtl = vgl->trw;
		vgl->track = vik_track_new();
		gchar *name = make_track_name ( vtl );
		vik_trw_layer_add_track ( vtl, name, vgl->track );
		g_free ( name );
	}

	vgl->first_trackpoint = TRUE;

	notify_location ( vgl->simple, NULL, vgl );

	g_signal_connect ( vgl->simple,
	                   "notify::location",
	                   G_CALLBACK(notify_location),
	                   vgl );
	g_signal_connect ( vgl->client,
	                   "notify::active",
	                   G_CALLBACK(notify_active),
	                   NULL );
finish:
	NULL;
}

static void geoclue_connect ( VikGeoclueLayer *vgl )
{
	vgl->tracking = TRUE;
	gclue_simple_new ( PACKAGE,
	                   GCLUE_ACCURACY_LEVEL_EXACT,
	                   NULL,
	                   on_simple_ready,
	                   vgl );
}

static void vik_geoclue_layer_realize ( VikGeoclueLayer *vgl, VikTreeview *vt, GtkTreeIter *layer_iter )
{
	GtkTreeIter iter;
	VikLayer *trw = VIK_LAYER(vgl->trw);
	vik_treeview_add_layer ( VIK_LAYER(vgl)->vt, layer_iter, &iter,
	                         _("GeoClue Tracking"), vgl, TRUE,
	                         trw, trw->type, trw->type, vik_layer_get_timestamp(trw) );
	if ( ! trw->visible )
		vik_treeview_item_set_visible ( VIK_LAYER(vgl)->vt, &iter, FALSE );
	vik_layer_realize ( trw, VIK_LAYER(vgl)->vt, &iter );
	g_signal_connect_swapped ( G_OBJECT(trw), "update", G_CALLBACK(vik_layer_emit_update_secondary), vgl );

	if ( vgl->auto_connect ) {
		geoclue_connect ( vgl );
	}
}

static void vik_geoclue_layer_post_read ( VikGeoclueLayer *vgl, VikViewport *vp, gboolean from_file )
{
	// Ensure bounds values are set (particularly if loading from a file)
	trw_layer_calculate_bounds_waypoints ( vgl->trw );
	trw_layer_calculate_bounds_tracks ( vgl->trw );
}

static void geoclue_start_stop_tracking_cb ( menu_array_layer values )
{
	VikGeoclueLayer *vgl = (VikGeoclueLayer*)values[0];

	if ( !vgl->tracking ) {
		geoclue_connect ( vgl );
	}
	else {
		// stop tracking
		vgl->tracking = FALSE;
		if ( vgl->client )
			g_clear_object ( &vgl->client );
		if ( vgl->simple )
			g_clear_object ( &vgl->simple );
		vgl->first_trackpoint = FALSE;
		vgl->trkpt = NULL;
	}
}
