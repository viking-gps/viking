/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2023, Rob Norris <rw_norris@hotmail.com>
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

// Flexible and Interoperable Data Transfer (FIT) Protocol
// https://developer.garmin.com/fit/protocol/
// https://developer.garmin.com/fit/file-types/

// FIT files are binary

#include "fit_sdk.h"
#include "fit.h"
#include "viking.h"
#include "garminsymbols.h"

#define FIT_MAGIC ".FIT"
#define FIT_HEADER_SIZE 12

// Unix EPOCH to FIT Epoch Time (31 Dec 1989 00:00:00 GMT)
#define FIT_EPOCH_OFFSET 631065600 // Seconds
// Unless less than this value - and then it is a relative number of seconds
// (seconds from device power on)
#define FIT_DATE_TIME_MIN 0x10000000

static guint32 g_data_size = 0;

// NB Force structure to minimum size in order to match binary representation
// Although without CRC, packed is the same as normal layout
// FIT files may omit the upfront CRC
typedef struct __attribute__((__packed__)) {
	guint8 header_size;
	guint8 protocol_version;
	guint16 profile_version;
	guint32 data_size;
	guint32 magic;
	//guint16 crc;
} header_t;

// As published on https://developer.garmin.com/fit/protocol/
//  but changed to use 'g' types
guint16 FitCRC_Get16(guint16 crc, guint8 byte)
{
   static const guint16 crc_table[16] =
   {
      0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401,
      0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400
   };
   guint16 tmp;

   // compute checksum of lower four bits of byte
   tmp = crc_table[crc & 0xF];
   crc = (crc >> 4) & 0x0FFF;
   crc = crc ^ tmp ^ crc_table[byte & 0xF];

   // now compute checksum of upper four bits of byte
   tmp = crc_table[crc & 0xF];
   crc = (crc >> 4) & 0x0FFF;
   crc = crc ^ tmp ^ crc_table[(byte >> 4) & 0xF];

   return crc;
}

gboolean a_fit_check_magic ( FILE *ff )
{
	gboolean rv = FALSE;

	// Decode the 'magic' part
	// As using (byte style ASCII) string comparsion there is no endian issue
	gchar header[FIT_HEADER_SIZE];
	if ( fread(header, 1, sizeof(header), ff) == sizeof(header) )
		if ( strncmp(header+8, FIT_MAGIC, strlen(FIT_MAGIC)) == 0 )
			rv = TRUE;
	rewind ( ff );
	return rv;
}

typedef struct __attribute__((__packed__)) {
	guint8 num;
	guint8 size;
	guint8 type;
} field_t;

typedef struct {
	guint8 reserved;
	guint8 arch;
	guint16 mesg_id;
	guint8 num_fields;
	field_t *fields;
} mesg_def_t;

static mesg_def_t g_defs[FIT_MAX_LOCAL_MESGS];

static gboolean read_uint8 ( FILE *ff, guint8 *data, gboolean trackSize )
{
	if ( fread(data, 1, sizeof(*data), ff) != sizeof(*data))
		return FALSE;
	if ( trackSize ) {
		g_data_size -= sizeof(*data);
		//g_debug ( "%s: data=%d g_data_size=%u", __FUNCTION__, *data, g_data_size );
	}
	return TRUE;
}

static gboolean read_uint16 ( FILE *ff, guint16 *data, guint8 endian, gboolean monitor_size )
{
	guint16 val;
	if ( fread(&val, 2, 1, ff) != 1)
		return FALSE;
	if ( endian == FIT_ARCH_ENDIAN_LITTLE )
		*data = GUINT16_FROM_LE(val);
	else
		*data = GUINT16_FROM_BE(val);
	if ( monitor_size ) {
		g_data_size -= sizeof(*data);
		//g_debug ( "%s: data=%d g_data_size=%u", __FUNCTION__, *data, g_data_size );
	}
	return TRUE;
}

static gboolean read_uint32 ( FILE *ff, guint32 *data, guint8 endian, gboolean monitor_size )
{
	guint32 val;
	if ( fread(&val, 4, 1, ff) != 1)
		return FALSE;
	if ( endian == FIT_ARCH_ENDIAN_LITTLE )
		*data = GINT32_FROM_LE(val);
	else
		*data = GINT32_FROM_BE(val);
	if ( monitor_size ) {
		g_data_size -= sizeof(*data);
		//g_debug ( "%s: data=%d g_data_size=%u", __FUNCTION__, *data, g_data_size );
	}
	return TRUE;
}

// As each component is 8bit - no need to cater for endian in this type
static gboolean read_field_t ( FILE *ff, field_t *data )
{
	if ( fread(data, 1, sizeof(*data), ff) != sizeof(*data))
		return FALSE;
	g_data_size -= sizeof(*data);
	//g_debug ( "%s: g_data_size=%d", __FUNCTION__, g_data_size );
	return TRUE;
}

static guint unnamed_waypoints = 0;
static guint unnamed_tracks = 0;
static guint unnamed_layers = 0;

// Current ("fit_") objects
// Easier to have as globals rather than passing around the functions
static VikTrwLayer *fit_vtl = NULL;
static VikWaypoint *fit_wp = NULL;
static VikTrackpoint *fit_tp = NULL;
static VikTrack *fit_tr = NULL;
static VikTRWMetadata *fit_md = NULL;

static void fit_add_track ()
{
	if ( fit_tr && fit_tr->trackpoints ) {
		gchar *tr_name = g_strdup_printf ( _("Track%03d"), unnamed_tracks++ );
		fit_tr->trackpoints = g_list_reverse ( fit_tr->trackpoints );
		vik_trw_layer_filein_add_track ( fit_vtl, tr_name, fit_tr );
	}
}

//
// Attempt to convert fit course point enum into (old style Garmin) waypoint symbol
//
static void fit_waypoint_symbol ( VikWaypoint *wpt, guint8 fit_course_point_enum )
{
	gchar *sym = NULL;
	switch ( fit_course_point_enum ) {
	case FIT_COURSE_POINT_GENERIC:      sym = "wpt_dot"; break;
	case FIT_COURSE_POINT_SUMMIT:       sym = "summit"; break;
	case FIT_COURSE_POINT_WATER:        sym = "water_source"; break;
	case FIT_COURSE_POINT_FOOD:         sym = "food_source"; break;
	case FIT_COURSE_POINT_DANGER:       sym = "danger"; break;
	case FIT_COURSE_POINT_LEFT:         sym = "flag_red"; break;   // Turn to 'Port' colouring!
	case FIT_COURSE_POINT_RIGHT:        sym = "flag_green"; break; // Turn to 'Starboard' colouring!
	case FIT_COURSE_POINT_STRAIGHT:     sym = "flag"; break;
	case FIT_COURSE_POINT_FIRST_AID:    sym = "f1st_aid"; break;
	case FIT_COURSE_POINT_LEFT_FORK:    sym = "diamond_red"; break;   // Turn to 'Port' colouring!
	case FIT_COURSE_POINT_RIGHT_FORK:   sym = "diamond_green"; break; // Turn to 'Starboard' colouring!
	case FIT_COURSE_POINT_SHARP_LEFT:   sym = "square_red"; break;   // Turn to 'Port' colouring!
	case FIT_COURSE_POINT_SHARP_RIGHT:  sym = "square_green"; break; // Turn to 'Starboard' colouring!
	case FIT_COURSE_POINT_SLIGHT_LEFT:  sym = "block_red"; break;   // Turn to 'Port' colouring!
	case FIT_COURSE_POINT_SLIGHT_RIGHT: sym = "block_green"; break; // Turn to 'Starboard' colouring!
	case FIT_COURSE_POINT_U_TURN:       sym = "user_exit"; break;
	case FIT_COURSE_POINT_CAMPSITE:     sym = "camp"; break;
	case FIT_COURSE_POINT_SERVICE:      sym = "fuel"; break;
	case FIT_COURSE_POINT_REST_AREA:    sym = "truck_stop"; break;
	case FIT_COURSE_POINT_MILE_MARKER:  sym = "mi_mrkr"; break;
	case FIT_COURSE_POINT_CHECKPOINT:   sym = "circle_x"; break;
	case FIT_COURSE_POINT_SHELTER:      sym = "lodge"; break;
	case FIT_COURSE_POINT_MEETING_SPOT: sym = "picnic"; break;
	case FIT_COURSE_POINT_OVERLOOK:     sym = "scenic"; break;
	case FIT_COURSE_POINT_TOILET:       sym = "restrooms"; break;
	case FIT_COURSE_POINT_SHOWER:       sym = "showers"; break;
	case FIT_COURSE_POINT_TUNNEL:       sym = "tunnel"; break;
	case FIT_COURSE_POINT_BRIDGE:       sym = "bridge"; break;
	case FIT_COURSE_POINT_CROSSING:     sym = "crossing"; break;
	case FIT_COURSE_POINT_STORE:        sym = "store"; break;
	case FIT_COURSE_POINT_NAVAID:       sym = "buoy_wht"; break;
	case FIT_COURSE_POINT_TRANSPORT:    sym = "grnd_trans"; break;
	case FIT_COURSE_POINT_ALERT:        sym = "restricted"; break;
	case FIT_COURSE_POINT_INFO:         sym = "info"; break;
	default:                            sym = "None"; break;
	}
	vik_waypoint_set_symbol ( wpt, a_get_hashed_sym(sym) );
}

gdouble semi2degrees(gint32 value)
{
	return ((gdouble)value / (gdouble)(1U<<31)) * 180.0;
}

static gboolean f_tr_newseg = FALSE;

static gboolean read_fields ( FILE *ff, int num_fields, int id, int time_offset, VikViewport *vvp )
{
	//g_debug ( "%s: fields=%d", __FUNCTION__, num_fields );
	static guint32 ts = 0;
	ts = ts + time_offset;
	static guint32 settings_ts_offset = 0;
	//static guint32 last_ts = 0;
	// c.f. 'FIT_RECORD_MESG'
	gint32 lat = FIT_SINT32_INVALID;
	gint32 lon = FIT_SINT32_INVALID;
	guint32 timestamp = FIT_UINT32_INVALID;

	guint16 alt = FIT_UINT16_INVALID;
	guint16 speed = FIT_UINT16_INVALID;
	guint8 hr  = FIT_UINT8_INVALID;
	guint8 cad = FIT_UINT8_INVALID;
	gint8 temp = FIT_SINT8_INVALID;
	guint16 pow = FIT_UINT16_INVALID;
	guint8 event = FIT_UINT8_INVALID;
	guint8 eventtype = FIT_UINT8_INVALID;

	gchar* name = NULL;

	// Read Fields...
	for ( guint8 ii = 0; ii < num_fields; ii++ ) {
		field_t field = g_defs[id].fields[ii];
		//g_debug ( "%s: field=%d size=%d", __FUNCTION__, ii, field.size );

		guint8 data8;
		guint16 data16;
		guint32 data32;
		gchar* str = NULL;
		// Read per indicated type
		//  and then depending on the size
		//   whether a singular or multiple of that type
		switch ( field.type ) {
		case FIT_BASE_TYPE_ENUM:
		case FIT_BASE_TYPE_SINT8:
		case FIT_BASE_TYPE_UINT8:
		case FIT_BASE_TYPE_UINT8Z:
			// ATM means if array of them we only use the final one
			for (guint8 jj = 0; jj < field.size; jj++)
				if ( !read_uint8(ff, &data8, TRUE) ) return FALSE;
			break;
		case FIT_BASE_TYPE_SINT16:
		case FIT_BASE_TYPE_UINT16:
			// Maybe should check field.size is rem 2
			for (guint8 jj = 0; jj < field.size/2; jj++)
				if ( !read_uint16(ff, &data16, g_defs[id].arch, TRUE) ) return FALSE;
			break;
		case FIT_BASE_TYPE_STRING:
			{
				str = g_malloc0(field.size+1); // Ensure null terminator
				if ( !str ) {
					g_warning ( "%s: Unable to allocate memory of size %d ", __FUNCTION__, field.size );
					return FALSE;
				}
				for (guint8 jj = 0; jj < field.size; jj++)
					if ( !read_uint8(ff, (guint8*)&str[jj], TRUE) ) return FALSE;
			}
			break;
		case FIT_BASE_TYPE_SINT32:
		case FIT_BASE_TYPE_UINT32:
		case FIT_BASE_TYPE_FLOAT32:
		case FIT_BASE_TYPE_UINT32Z:
			// Maybe should check field.size is rem 4
			for (guint8 jj = 0; jj < field.size/4; jj++)
				if ( !read_uint32(ff, &data32, g_defs[id].arch, TRUE) ) return FALSE;
			break;
		default:
			// So basically ignore them
			for (guint8 jj = 0; jj < field.size; jj++)
				if ( !read_uint8(ff, &data8, TRUE) ) return FALSE;
			break;
		}

		// PARSE FIELDS

		// Is 'File Id Message'
		if ( g_defs[id].mesg_id == FIT_MESG_NUM_FILE_ID ) {
			if ( field.num == FIT_FILE_ID_FIELD_NUM_TYPE ) {
				if ( !(data8 == FIT_FILE_ACTIVITY || data8 == FIT_FILE_COURSE) ) {
					// Ignore
					g_warning ( "%s: Fit File Id Type=%d not supported", __FUNCTION__, data8 );
				} else {
					// If existing track, add to layer and then create new track
					if ( fit_tr )
						fit_add_track();
					fit_vtl = VIK_TRW_LAYER(vik_layer_create ( VIK_LAYER_TRW, vvp, FALSE ));
					// Always force V1.1, since we may read in 'extended' data like cadence, etc...
					vik_trw_layer_set_gpx_version ( fit_vtl, GPX_V1_1 );
					fit_md = vik_trw_metadata_new();
					fit_tr = vik_track_new ();
					if ( data8 == FIT_FILE_COURSE )
						fit_tr->is_route = TRUE;
				}
			}

			if ( field.num == FIT_FILE_ID_FIELD_NUM_MANUFACTURER )
				g_debug ( "%s: File Manufacturer=%d", __FUNCTION__, data16 );

			if ( field.num == FIT_FILE_ID_FIELD_NUM_SERIAL_NUMBER )
				g_debug ( "%s: Serial Number=%u", __FUNCTION__, data32 );

			if ( field.num == FIT_FILE_ID_FIELD_NUM_TIME_CREATED ) {
#if GLIB_CHECK_VERSION(2,62,0)
				gint64 ts = FIT_EPOCH_OFFSET + data32;
				GDateTime* gdt = g_date_time_new_from_unix_utc ( ts );
				gchar* msg = g_date_time_format_iso8601 ( gdt );
				g_debug ( "%s: [%u] [%ld] create time=%s\n", __FUNCTION__, data32, ts, msg );
				g_free ( msg );
				g_date_time_unref ( gdt );
#endif
				settings_ts_offset = data32;
			}
		}

		// Main track information
		if ( g_defs[id].mesg_id == FIT_MESG_NUM_RECORD ) {
			if ( field.num == FIT_RECORD_FIELD_NUM_POSITION_LAT && field.size == 4 )
				lat = (gint32)data32;

			if ( field.num == FIT_RECORD_FIELD_NUM_POSITION_LONG && field.size == 4 )
				lon = (gint32)data32;

			if ( field.num == FIT_RECORD_FIELD_NUM_TIMESTAMP && field.size == 4 )
				timestamp = data32;

			if ( field.num == FIT_RECORD_FIELD_NUM_ALTITUDE && field.size == 2 )
				alt = data16;

			// 'enhanced' takes presidence over previous 'standard' value
			if ( field.num == FIT_RECORD_FIELD_NUM_ENHANCED_ALTITUDE && field.size == 2 )
				alt = data16;

			if ( field.num == FIT_RECORD_FIELD_NUM_SPEED && field.size == 2 )
				speed = data16;

			// 'enhanced' takes presidence over previous 'standard' value
			if ( field.num == FIT_RECORD_FIELD_NUM_ENHANCED_SPEED && field.size == 2 )
				speed = data16;

			if ( field.num == FIT_RECORD_FIELD_NUM_HEART_RATE && field.size == 1 )
				hr = data8;

			if ( field.num == FIT_RECORD_FIELD_NUM_CADENCE && field.size == 1 )
				cad = data8;

			if ( field.num == FIT_RECORD_FIELD_NUM_TEMPERATURE && field.size == 1 )
				temp = data8;

			if ( field.num == FIT_RECORD_FIELD_NUM_POWER && field.size == 2 )
				pow = data16;

		}

		if ( g_defs[id].mesg_id == FIT_MESG_NUM_EVENT ) {
			// ATM I can't work out the event nums in the SDK
			if ( field.num == 0 && field.size == 1 ) {
				event = data8;
			}
			if ( field.num == 1 && field.size == 1 ) {
				eventtype = data8;
			}
		}

		if ( g_defs[id].mesg_id == FIT_MESG_NUM_COURSE_POINT ) {
			if ( field.num == FIT_COURSE_POINT_FIELD_NUM_TIMESTAMP && field.size == 4 ) {
				timestamp = data32;
			}
			if ( field.num == FIT_COURSE_POINT_FIELD_NUM_POSITION_LAT && field.size == 4 )
				lat = (gint32)data32;

			if ( field.num == FIT_COURSE_POINT_FIELD_NUM_POSITION_LONG && field.size == 4 )
				lon = (gint32)data32;

			if ( field.num == FIT_COURSE_POINT_FIELD_NUM_NAME )
				name = g_strdup ( str );

			if ( field.num == FIT_COURSE_POINT_FIELD_NUM_TYPE && field.size == 1 )
				eventtype = data8;
		}

		//if ( field.type == FIT_BASE_TYPE_STRING )
		//	g_debug ( "%s: String=%s", __FUNCTION__, str );
		if ( str ) g_free ( str );
	}

	// PARSE DATA from the collected field info
	// Events before tracks, as we insert this into the trackpoint
	if ( g_defs[id].mesg_id == FIT_MESG_NUM_EVENT ) {
		if ( event == FIT_EVENT_TIMER && eventtype == FIT_EVENT_TYPE_START ) {
			f_tr_newseg = TRUE;
			g_debug ( "%s: NEWSEGMENT EVENT", __FUNCTION__ );
		}
	}

	// Main track information
	if ( g_defs[id].mesg_id == FIT_MESG_NUM_RECORD ) {
		if ( lat != FIT_SINT32_INVALID && lon != FIT_SINT32_INVALID ) {
			fit_tp = vik_trackpoint_new ();
			struct LatLon fit_ll;
			fit_ll.lat = semi2degrees ( lat );
			fit_ll.lon = semi2degrees ( lon );
			vik_coord_load_from_latlon ( &(fit_tp->coord), vik_trw_layer_get_coord_mode(fit_vtl), &fit_ll );
			if ( f_tr_newseg ) {
				fit_tp->newsegment = f_tr_newseg;
				f_tr_newseg = FALSE; // Reset
			}

			if ( alt != FIT_UINT16_INVALID )
				// Encoded as "5 * m + 500" (for both normal and enhanced), thus apply the reverse
				fit_tp->altitude = (alt / 5.0) - 500;

			if ( timestamp != FIT_UINT32_INVALID ) {
				guint32 ts = timestamp;
				if ( timestamp < FIT_DATE_TIME_MIN )
					ts = ts + settings_ts_offset;
				//last_ts = ts;
				gint64 ts64 = (gint64)ts + (gint64)FIT_EPOCH_OFFSET;
#if GLIB_CHECK_VERSION(2,62,0)
				//GDateTime* gdt = g_date_time_new_from_unix_utc ( ts64 );
				//gchar* msg = g_date_time_format_iso8601 ( gdt );
				//g_printf ( "%s: [%d] time=%s\n", __FUNCTION__, timestamp, msg );
				//g_free ( msg );
				//g_date_time_unref ( gdt );
#endif
				fit_tp->timestamp = (gdouble)ts64;
			}

			// Both normal and enhanced
			if ( speed != FIT_UINT16_INVALID )
				fit_tp->speed = (speed / 1000.0);

			if ( hr != FIT_UINT8_INVALID )
				fit_tp->heart_rate = hr;

			if ( cad != FIT_UINT8_INVALID )
				fit_tp->cadence = cad;

			if ( temp != FIT_SINT8_INVALID )
				fit_tp->temp = temp;

			if ( pow != FIT_UINT16_INVALID )
				fit_tp->power = pow;

			fit_tr->trackpoints = g_list_prepend ( fit_tr->trackpoints, fit_tp );
		}
	}

	// Waypoints
	if ( g_defs[id].mesg_id == FIT_MESG_NUM_COURSE_POINT ) {
		if ( lat != FIT_SINT32_INVALID && lon != FIT_SINT32_INVALID ) {
			fit_wp = vik_waypoint_new ();
			struct LatLon fit_ll;
			fit_ll.lat = semi2degrees ( lat );
			fit_ll.lon = semi2degrees ( lon );
			vik_coord_load_from_latlon ( &(fit_wp->coord), vik_trw_layer_get_coord_mode(fit_vtl), &fit_ll );
			gchar* wp_name = name;
			if ( !wp_name )
				wp_name = g_strdup_printf ( _("Waypoint%04d"), unnamed_waypoints++ );
			if ( eventtype != FIT_UINT8_INVALID )
				fit_waypoint_symbol ( fit_wp, eventtype );
			vik_trw_layer_filein_add_waypoint ( fit_vtl, wp_name, fit_wp );
		}
	}

	return TRUE;
}

static gboolean read_data ( FILE *ff, int id, int time_offset, VikViewport *vvp )
{
	// Read This mesg type...
	//g_debug ( "%s: id=%d - mesg id=%d", __FUNCTION__, id, g_defs[id].mesg_id );
	if ( g_defs[id].reserved )
		return read_fields ( ff, g_defs[id].num_fields, id, time_offset, vvp );
	else {
		g_warning ( "%s: Data id %d encountered before definition", __FUNCTION__, id );
		return FALSE;
	}
}

static gboolean read_data_msg ( FILE *ff, guint8 header, int time_offset, VikViewport *vvp )
{
	int local_id = header & FIT_HDR_TYPE_MASK;
	//g_debug ( "%s: id=%d ", __FUNCTION__, local_id );
	if ( local_id > FIT_HDR_TYPE_MASK )
	{
		g_warning ( "%s: id=%d is bigger than maximum allowed %d", __FUNCTION__, local_id, FIT_HDR_TYPE_MASK);
		return FALSE;
	}

	return read_data ( ff, local_id, time_offset, vvp );
}

static gboolean read_msg_type_def ( FILE *ff, guint8 header )
{
	int local_id = header & FIT_HDR_TYPE_MASK;

	// Instead of trying to read mesg_def_t;
	// Do one by one
	guint8 reserved; // Read byte from file but otherwise basically ignore
	if ( !read_uint8(ff, &reserved, TRUE) ) return FALSE;
	// Use internally as whether this message id been 'defined' yet
	// Messages can be redefined according to FIT protocol
	// Normally not done, but perhaps if the file needs to store more message types than FIT_MAX_LOCAL_MESGS allows
	//  then the only way is to override a previous definition
	if ( g_defs[local_id].reserved )
		g_debug ( "%s: ID [%d] REDEFINED!!", __FUNCTION__, local_id );
	g_defs[local_id].reserved = TRUE;

	if ( !read_uint8(ff, &g_defs[local_id].arch, TRUE) ) return FALSE;

	if ( !read_uint16(ff, &g_defs[local_id].mesg_id, g_defs[local_id].arch, TRUE) ) return FALSE;

	g_debug ( "%s: Defining id=%u as %u", __FUNCTION__, local_id, g_defs[local_id].mesg_id );

	if ( !read_uint8(ff, &g_defs[local_id].num_fields, TRUE) ) return FALSE;

	field_t *fields = g_malloc0 ( sizeof(field_t) * g_defs[local_id].num_fields );
	g_defs[local_id].fields = fields;

	for ( guint ii = 0; ii < g_defs[local_id].num_fields; ii++ ) {
		if ( !read_field_t(ff, &fields[ii]) ) return FALSE;
		//g_debug ( "%s: Field[%d] num, size, type= %d %d 0x%02x", __FUNCTION__, ii, fields[ii].num, fields[ii].size, fields[ii].type );
	}

	if ( header & FIT_HDR_DEV_DATA_BIT ) {
		guint8 dev_num_fields;
		if ( !read_uint8(ff, &dev_num_fields, TRUE) ) return FALSE;
		//g_debug ( "%s: dev_num_fields=%d", __FUNCTION__, dev_num_fields );

		// Read in one block and otherwise ignore
		gchar *dev_buffer = g_malloc ( sizeof(field_t)*dev_num_fields );
		if ( fread(dev_buffer, 1, sizeof(field_t)*dev_num_fields, ff) != sizeof(field_t)*dev_num_fields )
			return FALSE;
		g_data_size -= sizeof(field_t)*dev_num_fields;
		//g_debug ( "%s: g_data_size=%d", __FUNCTION__, g_data_size );
	}

	return TRUE;
}

static gboolean read_compressed_time_rec ( FILE *ff, guint8 header, VikViewport *vvp )
{
	//g_debug ( "%s: ", __FUNCTION__ );
	return read_data_msg ( ff, header, header & FIT_HDR_TIME_OFFSET_MASK, vvp );
}

static gboolean read_record ( FILE *ff, VikViewport *vvp )
{
	// Data/Msg Header is 1 byte
	guint8 header;
	if ( !read_uint8(ff, &header, TRUE) ) return FALSE;

	if ( header & FIT_HDR_TIME_REC_BIT )
		return read_compressed_time_rec ( ff, header, vvp );
	// Otherwise 'Normal' header kinds:
	else if ( header & FIT_HDR_TYPE_DEF_BIT )
		return read_msg_type_def ( ff, header );
	else
		return read_data_msg ( ff, header, 0, vvp );
}

static header_t read_header ( FILE *ff )
{
	header_t header = { 0, 0, 0, 0, 0 };
	// Very simple if on a Little Endian machine as all multi-byte
	//  values are by protocol definition in LE order,
	//  so can just map the entire header structure in one go. e.g.:
	//fread ( &header, 1, sizeof(header), ff );

	// However to be fully compatible...

	// Annoyingly compiler warns about taking address of packed member of 'struct <anonymous>' may result in an unaligned pointer value
	// so repeat variables and assign at the end, instead of using struct directly
	guint8 header_size;
	guint8 protocol_version;
	guint16 profile_version;
	guint32 data_size;
	guint32 magic;
	if ( !read_uint8(ff, &header_size, FALSE) ) goto FAIL;
	if ( !read_uint8(ff, &protocol_version, FALSE) ) goto FAIL;
	if ( !read_uint16(ff, &profile_version, FIT_ARCH_ENDIAN_LITTLE, FALSE) ) goto FAIL;
	if ( !read_uint32(ff, &data_size, FIT_ARCH_ENDIAN_LITTLE, FALSE) ) goto FAIL;
	if ( !read_uint32(ff, &magic, FIT_ARCH_ENDIAN_LITTLE, FALSE) ) goto FAIL;
	header.header_size = header_size;
	header.protocol_version = protocol_version;
	header.profile_version = profile_version;
	header.data_size = data_size;
	header.magic = magic;
	goto FINISH;
FAIL:
	g_warning ( "%s: Read Header failed", __FUNCTION__ );
	header.data_size = 0;
FINISH:
	return header;
}

header_t get_header ( FILE *ff )
{
	header_t header = { 0, 0, 0, 0, 0 };

	// NB very first byte is the size of the Header
	// Check header size is as we support
	guint8 hdr_size;
	if ( fread(&hdr_size, 1, sizeof(hdr_size), ff) != sizeof(hdr_size) ) {
		g_warning ( "%s: Header read failure", __FUNCTION__ );
		return header;
	}

	// Allow for a missing CRC
	if ( hdr_size == FIT_HEADER_SIZE || (hdr_size == FIT_HEADER_SIZE+2) ) {

		rewind ( ff );
		// NB Should not fail as already performed a_fit_check_magic()
		header = read_header ( ff );
		if ( header.data_size == 0 )
			return header;

		// Does it have the CRC?
		if ( hdr_size > FIT_HEADER_SIZE ) {

			guint16 crc;
			if ( !read_uint16(ff, &crc, FIT_ARCH_ENDIAN_LITTLE, FALSE) ) {
				g_warning ( "%s: Read CRC failed", __FUNCTION__ );
				// Fake the data size so caller can detect failure.
				header.data_size = 0;
				return header;
			}
			g_debug ( "%s: HAS CRC = %d", __FUNCTION__, crc );
			// Check the CRC if it is not 0 (which is allowed)
			if ( crc != 0 ) {
				guint16 hh = 0;
				rewind ( ff );
				for ( guint8 ii = 0; ii < FIT_HEADER_SIZE; ii++ ) {
					guint8 byte;
					if ( !read_uint8(ff, &byte, FALSE) ) {
						g_warning ( "%s: CRC check reading failed", __FUNCTION__ );
						header.data_size = 0;
						return header;
					}
					hh = FitCRC_Get16 ( hh, byte );
				};
				// Only warn, carry on to attempt to read the file even if CRC value not as expected
				if ( hh != crc ) {
					g_warning ( "%s: Header CRC check failure: expected=%d vs calculated= %d", __FUNCTION__, crc, hh );
				}
				// Return to end of header
				(void)fseek ( ff, hdr_size, SEEK_SET );
			}
		}
	} else
		g_warning ( "%s: Unexpected header size=%d", __FUNCTION__, hdr_size );
	return header;
}


/**
 * Returns TRUE on a successful file read
 *   NB The file of course could contain no actual geo data that we can use!
 * NB2 Filename is used in case a name from within the file itself can not be found
 *   as file access is via the FILE* stream methods
 */
gboolean a_fit_read_file ( VikAggregateLayer *val, VikViewport *vvp, FILE *ff, const gchar* filename )
{
	gboolean ans = FALSE;

	for ( guint8 ii = 0; ii < FIT_HDR_TYPE_MASK+1; ii++ )
		g_defs[ii].reserved = FALSE;

	header_t header = get_header ( ff );
	g_debug ( "%s: Protocol=%d", __FUNCTION__, header.protocol_version );
	g_debug ( "%s: Profile=%d", __FUNCTION__, header.profile_version );
	g_debug ( "%s: Data size=%d", __FUNCTION__, header.data_size );

	unnamed_waypoints = 1;
	unnamed_tracks = 1;
	unnamed_layers = 1;
	f_tr_newseg = FALSE;

	fit_vtl = NULL;
	fit_tp = NULL;
	fit_wp = NULL;
	fit_tr = NULL;
	fit_md = NULL;

	// Keep decoding until nothing left
	g_data_size = header.data_size;
	while ( g_data_size ) {
		if ( !read_record(ff, vvp) ) {
			g_warning ( "%s: data size not read =%d", __FUNCTION__, g_data_size );
			return FALSE;
		}
	}

	// TODO - support 'chained' fit files.
	// Not found any examples to test with, so probably would end up with multiple tracks,
	//  rather than say mulitple TRW layers, however that should be good enough.
	if ( fit_vtl ) {
		fit_add_track ();
		if ( vik_trw_layer_is_empty(fit_vtl) ) {
			// free up layer
			g_warning ( "%s: No useable geo data found in %s", __FUNCTION__, vik_layer_get_name(VIK_LAYER(fit_vtl)) );
			g_object_unref ( fit_vtl );
		} else {
			// Add it
			gchar *name = g_strdup_printf ( "%s", a_file_basename(filename) );
			vik_layer_rename ( VIK_LAYER(fit_vtl), name );
			g_free ( name );
			vik_layer_post_read ( VIK_LAYER(fit_vtl), vvp, TRUE );
			vik_aggregate_layer_add_layer ( val, VIK_LAYER(fit_vtl), FALSE );
			vik_trw_layer_set_metadata ( fit_vtl, fit_md );
			vik_trw_layer_auto_set_view ( fit_vtl, vvp );
			ans = TRUE;
		}
	}

	return ans;
}
