/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2019 Rob Norris <rw_norris@hotmail.com>
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
 ***********************************************************
 *
 */

#include <math.h>
#include <time.h>
#include <string.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "viking.h"
#include "viktrwlayer_analysis.h"
#include "ui_util.h"

// Units of each item are in SI Units
// (as returned by the appropriate internal viking track functions)
typedef struct {
	gdouble  min_alt;
	gdouble  max_alt;
	gdouble  elev_gain;
	gdouble  elev_loss;
	gdouble  length;
	//gdouble  length_gaps;
	gdouble  max_speed;
	//gulong   trackpoints;
	//guint    segments;
	gint     duration;
	gdouble  start_time;
	gdouble  end_time;
	gint     count;
	GList    *e_list; // of guints to determine Eddington number
	// https://en.wikipedia.org/wiki/Arthur_Eddington#Eddington_number_for_cycling
} track_stats;

// Early incarnations of the code had facilities to print output for multiple files
//  but has been rescoped to work on a single list of tracks for the GUI
typedef enum {
	//TS_TRACK,
	TS_TRACKS,
	//TS_FILES,
} track_stat_block;
static track_stats tracks_stats[1];

#define YEARS_HELD 100
static track_stats tracks_years[YEARS_HELD];
static guint current_year = 2020;

// cf with vik_track_get_minmax_alt internals
#define VIK_VAL_MIN_ALT 25000.0
#define VIK_VAL_MAX_ALT -5000.0

/**
 * Reset the specified block
 */
static void reset_me ( track_stats *stats )
{
	stats->min_alt     = VIK_VAL_MIN_ALT;
	stats->max_alt     = VIK_VAL_MAX_ALT;
	stats->elev_gain   = 0.0;
	stats->elev_loss   = 0.0;
	stats->length      = 0.0;
	//stats->length_gaps = 0.0;
	stats->max_speed   = 0.0;
	//stats->trackpoints = 0;
	//stats->segments    = 0;
	stats->duration    = 0;
	stats->start_time  = NAN;
	stats->end_time    = NAN;
	stats->count       = 0;
	stats->e_list      = NULL;
}

/**
 * Reset the specified block
 * Call this when starting to processing multiple items
 */
static void val_reset ( track_stat_block block )
{
	reset_me ( &tracks_stats[block] );
}

/**
 * Reset the years info
 */
static void val_reset_years ( void )
{
	for ( guint yi = 0; yi < YEARS_HELD; yi++ )
		reset_me ( &tracks_years[yi] );
}

/**
 * @val_analyse_track:
 * @trk: The track to be analyse
 *
 * Function to collect statistics, using the internal track functions
 */
static void val_analyse_track ( VikTrack *trk, gboolean include_no_times )
{
	//val_reset ( TS_TRACK );
	gdouble min_alt, max_alt, up, down;

	//gdouble  length_gaps = vik_track_get_length_including_gaps (trk);
	gdouble  length      = 0.0;
	gdouble  max_speed   = 0.0;
	//gulong   trackpoints = vik_track_get_tp_count (trk);
	//guint    segments    = vik_track_get_segment_count (trk);

	gdouble t1 = NAN;

	// NB Subsecond resolution not needed, as just using the timestamp to get dates
	if ( trk->trackpoints && !isnan(VIK_TRACKPOINT(trk->trackpoints->data)->timestamp) ) {
		t1 = VIK_TRACKPOINT(g_list_first(trk->trackpoints)->data)->timestamp;
		gdouble t2 = VIK_TRACKPOINT(g_list_last(trk->trackpoints)->data)->timestamp;

		// Initialize to the first or smallest/largest value
		for (guint ii = 0; ii < G_N_ELEMENTS(tracks_stats); ii++) {
			if ( !isnan(t1) ) {
				if ( !isnan(tracks_stats[ii].start_time) ) {
					if ( t1 < tracks_stats[ii].start_time )
						tracks_stats[ii].start_time = t1;
				}
				else
					tracks_stats[ii].start_time = t1;
			}

			if ( !isnan(t2) ) {
				if ( !isnan(tracks_stats[ii].end_time) ) {
					if ( t2 > tracks_stats[ii].end_time )
						tracks_stats[ii].end_time = t2;
				}
				else
					tracks_stats[ii].end_time = t2;
			}

			if ( !isnan(t1) && !isnan(t2) ) {
				tracks_stats[ii].duration = tracks_stats[ii].duration + (int)(t2-t1);
			}
		}
	}

	// Only consider tracks with times (unless specified otherwise)
	//  i.e. generally a track recorded on a GPS device rather than manual/computer generated track
	if ( !isnan(t1) || include_no_times ) {

		tracks_stats[TS_TRACKS].count++;

		length    = vik_track_get_length (trk);
		max_speed = vik_track_get_max_speed (trk);

		// NB A route shouldn't have times anyway
		if ( !trk->is_route ) {
			// Eddington number will be in the current Units distance preference
			gdouble e_len;
			switch (a_vik_get_units_distance ()) {
			case VIK_UNITS_DISTANCE_MILES:          e_len = VIK_METERS_TO_MILES(length); break;
			case VIK_UNITS_DISTANCE_NAUTICAL_MILES: e_len = VIK_METERS_TO_NAUTICAL_MILES(length); break;
				//VIK_UNITS_DISTANCE_KILOMETRES
			default: e_len = length/1000.0; break;
			}
			gdouble *gd = g_malloc ( sizeof(gdouble) );
			*gd = e_len;
			tracks_stats[TS_TRACKS].e_list = g_list_prepend ( tracks_stats[TS_TRACKS].e_list, gd );
		}

		int ii;
		for (ii = 0; ii < G_N_ELEMENTS(tracks_stats); ii++) {
			//tracks_stats[ii].trackpoints += trackpoints;
			//tracks_stats[ii].segments    += segments;
			tracks_stats[ii].length      += length;
			//tracks_stats[ii].length_gaps += length_gaps;
			if ( max_speed > tracks_stats[ii].max_speed )
				tracks_stats[ii].max_speed = max_speed;
		}

		if ( vik_track_get_minmax_alt (trk, &min_alt, &max_alt) ) {
			for (ii = 0; ii < G_N_ELEMENTS(tracks_stats); ii++) {
				if ( min_alt < tracks_stats[ii].min_alt )
					tracks_stats[ii].min_alt = min_alt;
				if ( max_alt > tracks_stats[ii].max_alt )
					tracks_stats[ii].max_alt = max_alt;
			}
		}

		vik_track_get_total_elevation_gain (trk, &up, &down );

		for (ii = 0; ii < G_N_ELEMENTS(tracks_stats); ii++) {
			tracks_stats[ii].elev_gain += up;
			tracks_stats[ii].elev_loss += down;
		}
	}

	// Insert into Years data - the track must have a time
	if ( !isnan(t1) ) {
		// What Year is it?
		GDate* gdate = g_date_new ();
		g_date_set_time_t ( gdate, (time_t)t1 );
		guint trk_year = g_date_get_year ( gdate );

		// Store track info
		guint yi = current_year - trk_year;
		if ( yi < YEARS_HELD ) {
			tracks_years[yi].count++;
			tracks_years[yi].length += length;
			tracks_years[yi].elev_gain += up;
			if ( max_alt > tracks_years[yi].max_alt )
				tracks_years[yi].max_alt = max_alt;
			if ( max_speed > tracks_years[yi].max_speed )
				tracks_years[yi].max_speed = max_speed;
		}
		g_date_free ( gdate );
	}
	else
		g_debug ( "%s: %s has no time", __FUNCTION__, trk->name );
}

// Could use GtkGrids but that is Gtk3+
static GtkWidget *create_table (int cnt, char *labels[], GtkWidget *contents[], gboolean extended)
{
	GtkTable *table;
	int i;

	table = GTK_TABLE(gtk_table_new (cnt, 2, FALSE));
	gtk_table_set_col_spacing (table, 0, 10);
	for (i=0; i<cnt; i++) {
		// Hacky method to only show 4th entry (Eddington number) when wanted
		if ( !(i == 4) || extended ) {
			GtkWidget *label;
			label = gtk_label_new(NULL);
			gtk_misc_set_alignment ( GTK_MISC(label), 1, 0.5 ); // Position text centrally in vertical plane
			// All text labels are set to be in bold
			char *markup = g_markup_printf_escaped ("<b>%s:</b>", _(labels[i]) );
			gtk_label_set_markup ( GTK_LABEL(label), markup );
			g_free ( markup );
			gtk_table_attach ( table, label, 0, 1, i, i+1, GTK_FILL, GTK_EXPAND, 4, 2 );
			if (GTK_IS_MISC(contents[i])) {
				gtk_misc_set_alignment ( GTK_MISC(contents[i]), 0, 0.5 );
			}
			gtk_table_attach_defaults ( table, contents[i], 1, 2, i, i+1 );
		}
	}
	return GTK_WIDGET (table);
}

static gchar *label_texts[] = {
	N_("Number of Tracks"),
	N_("Date Range"),
	N_("Total Length"),
	N_("Average Length"),
	N_("Eddington number"), // No.4: Extended display only
	N_("Max Speed"),
	N_("Avg. Speed"),
	N_("Minimum Altitude"),
	N_("Maximum Altitude"),
	N_("Total Elevation Gain/Loss"),
	N_("Avg. Elevation Gain/Loss"),
	N_("Total Duration"),
	N_("Avg. Duration"),
};

/**
 * create_layout:
 *
 * Returns a widget to hold the stats information in a table grid layout
 */
static GtkWidget *create_layout ( GtkWidget *content[], gboolean extended )
{
	int cnt = 0;
	for ( cnt = 0; cnt < G_N_ELEMENTS(label_texts); cnt++ )
		content[cnt] = ui_label_new_selectable ( NULL );

	if ( !extended )
		cnt = cnt - 1;

	return create_table (cnt, label_texts, content, extended );
}


static gint rsort_by_distance (gconstpointer a, gconstpointer b)
{
	const gdouble* ad = (const gdouble*) a;
	const gdouble* bd = (const gdouble*) b;
	if ( *ad > *bd )
		return -1;
	else
		return 1;
}

/**
 * table_output:
 *
 * Update the given widgets table with the values from the track stats
 */
static void table_output ( track_stats ts, GtkWidget *content[], gboolean extended )
{
	int cnt = 0;

	gchar tmp_buf[64];
	g_snprintf ( tmp_buf, sizeof(tmp_buf), "%d", ts.count );
	gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );

	if ( ts.count == 0 ) {
		// Blank all other fields
		g_snprintf ( tmp_buf, sizeof(tmp_buf), "--" );
		for ( cnt = 1; cnt < G_N_ELEMENTS(label_texts); cnt++ )
			gtk_label_set_text ( GTK_LABEL(content[cnt]), tmp_buf );
		return;
	}

	// Check for potential date range
	if ( !isnan(ts.start_time) && !isnan(ts.end_time) ) {
		GDate* gdate_start = g_date_new ();
		g_date_set_time_t ( gdate_start, (time_t)ts.start_time );
		gchar time_start[32];
		g_date_strftime ( time_start, sizeof(time_start), "%x", gdate_start );
		g_date_free ( gdate_start );

		GDate* gdate_end = g_date_new ();
		g_date_set_time_t ( gdate_end, (time_t)ts.end_time );
		gchar time_end[32];
		g_date_strftime ( time_end, sizeof(time_end), "%x", gdate_end );
		g_date_free ( gdate_end );

		// Test if the same day by comparing the date string of the timestamp
		if ( strncmp(time_start, time_end, 32) )
			g_snprintf ( tmp_buf, sizeof(tmp_buf), "%s --> %s", time_start, time_end );
		else
			g_snprintf ( tmp_buf, sizeof(tmp_buf), "%s", time_start );
	} else {
		g_snprintf ( tmp_buf, sizeof(tmp_buf), _("No Data") );
	}

	gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );

	switch (a_vik_get_units_distance ()) {
	case VIK_UNITS_DISTANCE_MILES:
		g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%.1f miles"), VIK_METERS_TO_MILES(ts.length) );
		break;
	case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
		g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%.1f NM"), VIK_METERS_TO_NAUTICAL_MILES(ts.length) );
		break;
	default:
		//VIK_UNITS_DISTANCE_KILOMETRES
		g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%.1f km"), ts.length/1000.0 );
		break;
	}
	gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );

	switch (a_vik_get_units_distance ()) {
	case VIK_UNITS_DISTANCE_MILES:
		g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%.2f miles"), (VIK_METERS_TO_MILES(ts.length)/ts.count) );
		break;
	case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
		g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%.2f NM"), (VIK_METERS_TO_NAUTICAL_MILES(ts.length)/ts.count) );
		break;
	default:
		//VIK_UNITS_DISTANCE_KILOMETRES
		g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%.2f km"), ts.length/(1000.0*ts.count) );
		break;
	}
	gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );

	if ( extended ) {
		// Note that this currently is a simplified approach to calculate the Eddington number.
		// In that a per track value is used, rather than trying to work out a length per day.
		//  (i.e. doesn't combine multiple tracks for a single day or split very long tracks into days)
		tracks_stats[TS_TRACKS].e_list = g_list_sort ( tracks_stats[TS_TRACKS].e_list, rsort_by_distance );
		guint Eddington = 0;
		guint position = 0;
		for (GList *iter = g_list_first (tracks_stats[TS_TRACKS].e_list); iter != NULL; iter = g_list_next (iter)) {
			position++;
			gdouble *num = (gdouble*)iter->data;
			if ( *num > position )
				Eddington = position;
		}
		g_snprintf ( tmp_buf, sizeof(tmp_buf), ("%d"), Eddington );
		gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );
	} else
		cnt++;

	vik_units_speed_t speed_units = a_vik_get_units_speed ();
	g_snprintf ( tmp_buf, sizeof(tmp_buf), "--" );
	if ( ts.max_speed > 0 )
		vu_speed_text ( tmp_buf, sizeof(tmp_buf), speed_units, ts.max_speed, TRUE, "%.1f" );
	gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );

	g_snprintf ( tmp_buf, sizeof(tmp_buf), "--" );
	if ( ts.duration > 0 )
		vu_speed_text ( tmp_buf, sizeof(tmp_buf), speed_units, ts.length/ts.duration, TRUE, "%.1f" );
	gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );

	switch ( a_vik_get_units_height() ) {
		// Note always round off height value output since sub unit accuracy is overkill
	case VIK_UNITS_HEIGHT_FEET:
		if ( ts.min_alt != VIK_VAL_MIN_ALT )
			g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%d feet"), (int)round(VIK_METERS_TO_FEET(ts.min_alt)) );
		else
			g_snprintf ( tmp_buf, sizeof(tmp_buf), "--" );
		gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );

		if ( ts.max_alt != VIK_VAL_MAX_ALT )
			g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%d feet"), (int)round(VIK_METERS_TO_FEET(ts.max_alt)) );
		else
			g_snprintf ( tmp_buf, sizeof(tmp_buf), "--" );
		gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );

		g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%d feet / %d feet"), (int)round(VIK_METERS_TO_FEET(ts.elev_gain)), (int)round(VIK_METERS_TO_FEET(ts.elev_loss)) );
		gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );
		g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%d feet / %d feet"), (int)round(VIK_METERS_TO_FEET(ts.elev_gain/ts.count)), (int)round(VIK_METERS_TO_FEET(ts.elev_loss/ts.count)) );
		break;
	default:
		//VIK_UNITS_HEIGHT_METRES
		if ( ts.min_alt != VIK_VAL_MIN_ALT )
			g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%d m"), (int)round(ts.min_alt) );
		else
			g_snprintf ( tmp_buf, sizeof(tmp_buf), "--" );
		gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );

		if ( ts.max_alt != VIK_VAL_MAX_ALT )
			g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%d m"), (int)round(ts.max_alt) );
		else
			g_snprintf ( tmp_buf, sizeof(tmp_buf), "--" );
		gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );

		g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%d m / %d m"), (int)round(ts.elev_gain), (int)round(ts.elev_loss) );
		gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );
		g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%d m / %d m"), (int)round(ts.elev_gain/ts.count), (int)round(ts.elev_loss/ts.count) );
		break;
	}
	gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );

	gint hours;
	gint minutes;
	gint days;
	// Total Duration
	days    = (gint)(ts.duration / (60*60*24));
	hours   = (gint)floor((ts.duration - (days*60*60*24)) / (60*60));
	minutes = (gint)((ts.duration - (days*60*60*24) - (hours*60*60)) / 60);
	g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%d:%02d:%02d days:hrs:mins"), days, hours, minutes );
	gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );

	// Average Duration
	gint avg_dur = ts.duration / ts.count;
	hours   = (gint)floor(avg_dur / (60*60));
	minutes = (gint)((avg_dur - (hours*60*60)) / 60);
	g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%d:%02d hrs:mins"), hours, minutes );
	gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );
}

typedef struct {
	gboolean include_invisible;
	gboolean include_no_times;
} track_options_t;

/**
 * val_analyse_item_maybe:
 * @vtlist: A track and the associated layer to consider for analysis
 * @data:   Whether to include invisible items
 *
 * Analyse this particular track
 *  considering whether it should be included depending on it's visibility
 */
static void val_analyse_item_maybe ( vik_trw_and_track_t *vtlist, const gpointer data )
{
	track_options_t *tot = (track_options_t*)data;
	VikTrack *trk = vtlist->trk;
	VikTrwLayer *vtl = vtlist->vtl;

	// Safety first - items shouldn't be deleted...
	if ( !IS_VIK_TRW_LAYER(vtl) ) return;
	if ( !trk ) return;

	if ( !tot->include_invisible ) {
		// Skip invisible layers or sublayers
		if ( !VIK_LAYER(vtl)->visible ||
			 (trk->is_route && !vik_trw_layer_get_routes_visibility(vtl)) ||
			 (!trk->is_route && !vik_trw_layer_get_tracks_visibility(vtl)) )
			return;

		// Skip invisible tracks
		if ( !trk->visible )
			return;
	}

	val_analyse_track ( trk, tot->include_no_times );
}

/**
 * val_analyse:
 * @widgets:           The widget layout
 * @tracks_and_layers: A list of #vik_trw_and_track_t
 * @include_invisible: Whether to include invisible layers and tracks
 * @include_no_times: Whether tracks with no times should be included
 * @extended: Whether this is an extended table output
 *
 * Analyse each item in the @tracks_and_layers list
 *
 */
static void val_analyse ( GtkWidget *widgets[], GList *tracks_and_layers, gboolean include_invisible, gboolean include_no_times, gboolean extended )
{
	val_reset ( TS_TRACKS );
	val_reset_years ( );
	time_t now = time ( NULL );
	if ( now != (time_t)-1 ) {
		GDate* gdate = g_date_new ();
		g_date_set_time_t ( gdate, now );
		current_year = g_date_get_year ( gdate );
		g_date_free ( gdate );
	}

	track_options_t *tot = g_malloc0 (sizeof(track_options_t));
	tot->include_invisible = include_invisible;
	tot->include_no_times  = include_no_times;
	GList *gl = g_list_first ( tracks_and_layers );
	if ( gl ) {
		g_list_foreach ( gl, (GFunc)val_analyse_item_maybe, tot );
	}
	g_free ( tot );

	table_output ( tracks_stats[TS_TRACKS], widgets, extended );

	g_list_free_full ( tracks_stats[TS_TRACKS].e_list, g_free );

	// Years info...
	if ( vik_debug ) {
		for ( guint yi = 0; yi < YEARS_HELD; yi++ ) {
			if ( tracks_years[yi].count > 0 )
				g_printf ( "%s: %d: %d %d %5.2f %5.1f %d\n", __FUNCTION__, current_year-yi, tracks_years[yi].count, (gint)tracks_years[yi].max_alt, tracks_years[yi].max_speed, tracks_years[yi].length/1000, (gint)tracks_years[yi].elev_gain );
		}
	}
}

typedef struct {
	GtkWidget **widgets;
	GtkWidget *layout;
	GtkWidget *check_button;
	GtkWidget *check_button_times;
	GList *tracks_and_layers;
	VikLayer *vl;
	gpointer user_data;
	VikTrwlayerGetTracksAndLayersFunc get_tracks_and_layers_cb;
	VikTrwlayerAnalyseCloseFunc on_close_cb;
	gboolean extended;
	gboolean include_invisible;
	gboolean include_no_times;
	VikWindow *vw;
	GtkTreeStore *store;
} analyse_cb_t;

static gdouble get_distance_in_units ( vik_units_distance_t dist_units, gdouble dist_in )
{
	gdouble dist = dist_in;
	switch ( dist_units ) {
	case VIK_UNITS_DISTANCE_MILES:
		dist = VIK_METERS_TO_MILES(dist);
		break;
	case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
		dist = VIK_METERS_TO_NAUTICAL_MILES(dist);
		break;
	default:
		dist = dist/1000.0;
		break;
	}
	return dist;
}

#define YEARS_COLS 4

static void years_copy_all ( GtkWidget *tree_view )
{
	GString *str = g_string_new ( NULL );
	gchar sep = '\t';

	// Get info from the GTK store
	//  using this way gets the items in the ordered by the user
	GtkTreeModel *model = gtk_tree_view_get_model ( GTK_TREE_VIEW(tree_view) );
	GtkTreeIter iter;
	if ( !gtk_tree_model_get_iter_first(model, &iter) )
		return;

	gboolean cont = TRUE;
	while ( cont ) {
		gint value;
		for (guint ii=0; ii<YEARS_COLS; ii++) {
			// Most items integers
			if ( ii < YEARS_COLS-1 ) {
				gtk_tree_model_get ( model, &iter, ii, &value, -1 );
				g_string_append_printf ( str, "%d%c", value, sep );
			} else {
				// Except last which is a double
				gdouble dvalue;
				gtk_tree_model_get ( model, &iter, ii, &dvalue, -1 );
				g_string_append_printf ( str, "%.1f", dvalue );
			}
		}
		g_string_append_printf ( str, "\n" );
		cont = gtk_tree_model_iter_next ( model, &iter );
	}

	a_clipboard_copy ( VIK_CLIPBOARD_DATA_TEXT, 0, 0, 0, str->str, NULL );
	g_string_free ( str, TRUE );
}

static gboolean years_menu_popup ( GtkWidget *tree_view,
                                   GdkEventButton *event,
                                   gpointer data )
{
	GtkWidget *menu = gtk_menu_new();
	(void)vu_menu_add_item ( GTK_MENU(menu), _("_Copy Data"), GTK_STOCK_COPY, G_CALLBACK(years_copy_all), tree_view );
	gtk_widget_show_all ( menu );
	gtk_menu_popup ( GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, gtk_get_current_event_time() );
	return TRUE;
}

static gboolean years_button_pressed ( GtkWidget *tree_view,
                                       GdkEventButton *event,
                                       gpointer data )
{
	// Only on right clicks...
	if ( ! (event->type == GDK_BUTTON_PRESS && event->button == 3) )
		return FALSE;
	return years_menu_popup ( tree_view, event, data );
}

static void years_update_store ( GtkTreeStore *store )
{
	// Reset store
	gtk_tree_store_clear ( store );

	vik_units_distance_t dist_units = a_vik_get_units_distance ();
	GtkTreeIter t_iter;
	// NB Default ordering in store is in the order they are added
	//  thus for here is it in reverse time
	for ( guint yi = 0; yi < YEARS_HELD; yi++ ) {
		if ( tracks_years[yi].count > 0 ) {
			gdouble distd = get_distance_in_units ( dist_units, tracks_years[yi].length );
			guint distu = (guint)round(distd);
			distd = distd / tracks_years[yi].count;
			gtk_tree_store_append ( store, &t_iter, NULL );
			gtk_tree_store_set ( store, &t_iter,
			                     0, current_year-yi,
			                     1, tracks_years[yi].count,
			                     2, distu,
			                     3, distd,
			                     -1 );
		}
	}
}

/**
 * years_display_build:
 *
 * Setup a treeview for a table of output
 */
static void years_display_build ( analyse_cb_t *acb, GtkWidget* scrolledwindow )
{
	// It's simple storing the gdouble values in the tree store as the sort works automatically
	// Then apply specific cell data formatting (rather default double is to 6 decimal places!)
	GtkTreeStore *store = gtk_tree_store_new ( YEARS_COLS,
	                                           G_TYPE_UINT,    // 0: Year
	                                           G_TYPE_UINT,    // 1: Num Tracks
	                                           G_TYPE_UINT,    // 2: Length
	                                           G_TYPE_DOUBLE );// 3: Average Length
	acb->store = store;

	GtkWidget *view = gtk_tree_view_new();
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();

	gint column_runner = 0;
	(void)ui_new_column_text ( _("Year"), renderer, view, column_runner++ );
	(void)ui_new_column_text ( _("Tracks"), renderer, view, column_runner++ );

	GtkTreeViewColumn *column;
	vik_units_distance_t dist_units = a_vik_get_units_distance ();
	switch ( dist_units ) {
	case VIK_UNITS_DISTANCE_MILES:
		(void)ui_new_column_text ( _("Distance (miles)"), renderer, view, column_runner++ );
		column = ui_new_column_text ( _("Average Dist (miles)"), renderer, view, column_runner++ );
		break;
	case VIK_UNITS_DISTANCE_NAUTICAL_MILES:
		(void)ui_new_column_text ( _("Distance (NM)"), renderer, view, column_runner++ );
		column = ui_new_column_text ( _("Average Dist (NM)"), renderer, view, column_runner++ );
		break;
	default:
		(void)ui_new_column_text ( _("Distance (km)"), renderer, view, column_runner++ );
		column = ui_new_column_text ( _("Average Dist (km)"), renderer, view, column_runner++ );
		break;
	}
	gtk_tree_view_column_set_cell_data_func ( column, renderer, ui_format_1f_cell_data_func, GINT_TO_POINTER(column_runner-1), NULL); // Apply own formatting of the data

	gtk_tree_view_set_model ( GTK_TREE_VIEW(view), GTK_TREE_MODEL(store) );
	gtk_tree_selection_set_mode ( gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), GTK_SELECTION_NONE );
	gtk_tree_view_set_rules_hint ( GTK_TREE_VIEW(view), TRUE );

	g_signal_connect ( view, "button-press-event", G_CALLBACK(years_button_pressed), NULL );

	gtk_container_add ( GTK_CONTAINER(scrolledwindow), view );
}

static void include_no_times_toggled_cb ( GtkToggleButton *togglebutton, analyse_cb_t *acb )
{
	gboolean value = FALSE;
	if ( gtk_toggle_button_get_active ( togglebutton ) )
		value = TRUE;
	acb->include_no_times = value;

	// NB no change to the track list
	// NB2 This option has no effect on the per Year output

	vik_window_set_busy_cursor ( acb->vw );
	val_analyse ( acb->widgets, acb->tracks_and_layers, acb->include_invisible, acb->include_no_times, acb->extended );
	vik_window_clear_busy_cursor ( acb->vw );

	gtk_widget_show_all ( acb->layout );
}

static void include_invisible_toggled_cb ( GtkToggleButton *togglebutton, analyse_cb_t *acb )
{
	gboolean value = FALSE;
	if ( gtk_toggle_button_get_active ( togglebutton ) )
		value = TRUE;

	// Delete old list of items
	if ( acb->tracks_and_layers ) {
		g_list_foreach ( acb->tracks_and_layers, (GFunc) g_free, NULL );
		g_list_free ( acb->tracks_and_layers );
	}

	// Get the latest list of items to analyse
	acb->tracks_and_layers = acb->get_tracks_and_layers_cb ( acb->vl, acb->user_data );

	acb->include_invisible = value;

	vik_window_set_busy_cursor ( acb->vw );
	val_analyse ( acb->widgets, acb->tracks_and_layers, acb->include_invisible, acb->include_no_times, acb->extended );
	vik_window_clear_busy_cursor ( acb->vw );

	if ( acb->store )
		years_update_store ( acb->store );

	gtk_widget_show_all ( acb->layout );
}

#define VIK_SETTINGS_ANALYSIS_DO_INVISIBLE "track_analysis_do_invisible"
#define VIK_SETTINGS_ANALYSIS_DO_NO_TIMES "track_analysis_do_no_times"

/**
 * analyse_close:
 *
 * Multi stage closure - as we need to clear allocations made here
 *  before passing on to the callee so they know then the dialog is closed too
 */
static void analyse_close ( GtkWidget *dialog, gint resp, analyse_cb_t *data )
{
	// Save current invisible value for next time
	gboolean do_invisible = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(data->check_button) );
	a_settings_set_boolean ( VIK_SETTINGS_ANALYSIS_DO_INVISIBLE, do_invisible );

	gboolean do_no_times = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(data->check_button_times) );
	a_settings_set_boolean ( VIK_SETTINGS_ANALYSIS_DO_NO_TIMES, do_no_times );

	//g_free ( data->layout );
	g_free ( data->widgets );
	g_list_foreach ( data->tracks_and_layers, (GFunc) g_free, NULL );
	g_list_free ( data->tracks_and_layers );

	if ( data->store )
		g_object_unref ( data->store );

	if ( data->on_close_cb )
		data->on_close_cb ( dialog, resp, data->vl );

	g_free ( data );
}

/**
 * vik_trw_layer_analyse_this:
 * @window:                   A window from which the dialog will be derived
 * @name:                     The name to be shown
 * @vl:                       The #VikLayer passed on into get_tracks_and_layers_cb()
 * @user_data:                Data passed on into get_tracks_and_layers_cb()
 * @get_tracks_and_layers_cb: The function to call to construct items to be analysed
 *
 * Display a dialog with stats across many tracks
 *
 * Returns: The dialog that is created to display the analyse information
 */
GtkWidget* vik_trw_layer_analyse_this ( GtkWindow *window,
                                        const gchar *name,
                                        VikLayer *vl,
                                        gpointer user_data,
                                        VikTrwlayerGetTracksAndLayersFunc get_tracks_and_layers_cb,
                                        VikTrwlayerAnalyseCloseFunc on_close_cb )
{
	GtkWidget *dialog;
	dialog = gtk_dialog_new_with_buttons ( _("Statistics"),
	                                       window,
	                                       GTK_DIALOG_DESTROY_WITH_PARENT,
	                                       GTK_STOCK_CLOSE,     GTK_RESPONSE_CANCEL,
	                                       NULL );

	GtkWidget *name_l = gtk_label_new ( NULL );
	gchar *myname = g_markup_printf_escaped ( "<b>%s</b>", name );
	gtk_label_set_markup ( GTK_LABEL(name_l), myname );
	g_free ( myname );

	GtkWidget *content = gtk_dialog_get_content_area ( GTK_DIALOG(dialog) );
	gtk_box_pack_start ( GTK_BOX(content), name_l, FALSE, FALSE, 10);

	// Get previous value (if any) from the settings
	gboolean include_invisible;
	if ( ! a_settings_get_boolean ( VIK_SETTINGS_ANALYSIS_DO_INVISIBLE, &include_invisible ) )
		include_invisible = TRUE;

	gboolean include_no_times;
	if ( ! a_settings_get_boolean ( VIK_SETTINGS_ANALYSIS_DO_NO_TIMES, &include_no_times ) )
		include_no_times = FALSE;

	analyse_cb_t *acb = g_malloc0 (sizeof(analyse_cb_t));
	acb->vw = VIK_WINDOW(window);
	acb->vl = vl;
	acb->user_data = user_data;
	acb->get_tracks_and_layers_cb = get_tracks_and_layers_cb;
	acb->on_close_cb = on_close_cb;
	acb->tracks_and_layers = get_tracks_and_layers_cb ( vl, user_data );
	acb->widgets = g_malloc ( sizeof(GtkWidget*) * G_N_ELEMENTS(label_texts) );
	acb->extended = vl->type == VIK_LAYER_AGGREGATE;
	acb->layout = create_layout ( acb->widgets, acb->extended );
	acb->include_invisible = include_invisible;
	acb->include_no_times = include_no_times;

	// Analysis seems reasonably quick
	//  unless you have really large numbers of tracks (i.e. many many thousands or a really slow computer)
	// One day might store stats in the track itself....
	vik_window_set_busy_cursor ( acb->vw );
	val_analyse ( acb->widgets, acb->tracks_and_layers, include_invisible, include_no_times, acb->extended );
	vik_window_clear_busy_cursor ( acb->vw );

	guint num_yrs = 0;
	for ( guint yi = 0; yi < YEARS_HELD; yi++ )
		if ( tracks_years[yi].count > 0 )
			num_yrs++;

	if ( num_yrs > 1 ) {
		// Multiple Years so show per year info as well
		GtkWidget *tabs = gtk_notebook_new();
		gtk_notebook_append_page ( GTK_NOTEBOOK(tabs), acb->layout, gtk_label_new(_("Totals")) );
		gtk_box_pack_start ( GTK_BOX(content), tabs, TRUE, TRUE, 0 );

		GtkWidget *scrolledwindow = gtk_scrolled_window_new ( NULL, NULL );
		gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
		gtk_notebook_append_page ( GTK_NOTEBOOK(tabs), scrolledwindow, gtk_label_new(_("Years")) );

		years_display_build ( acb, scrolledwindow );
		years_update_store ( acb->store );
	}
	else
		gtk_box_pack_start ( GTK_BOX(content), acb->layout, TRUE, TRUE, 0 );

	GtkWidget *cb = gtk_check_button_new_with_label ( _("Include Invisible Items") );
	gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(cb), include_invisible );
	gtk_box_pack_start ( GTK_BOX(content), cb, FALSE, FALSE, 2 );
	acb->check_button = cb;

	GtkWidget *cbt = gtk_check_button_new_with_label ( _("Include Tracks With No Times") );
	gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(cbt), include_no_times );
	gtk_box_pack_start ( GTK_BOX(content), cbt, FALSE, FALSE, 2 );
	acb->check_button_times = cbt;

	gtk_widget_show_all ( dialog );

	g_signal_connect ( G_OBJECT(cb), "toggled", G_CALLBACK(include_invisible_toggled_cb), acb );
	g_signal_connect ( G_OBJECT(cbt), "toggled", G_CALLBACK(include_no_times_toggled_cb), acb );
	g_signal_connect ( G_OBJECT(dialog), "response", G_CALLBACK(analyse_close), acb );

	return dialog;
}
