/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013 Rob Norris <rw_norris@hotmail.com>
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

// Units of each item are in SI Units
// (as returned by the appropriate internal viking track functions)
typedef struct {
	gdouble  min_alt;
	gdouble  max_alt;
	gdouble  elev_gain;
	gdouble  elev_loss;
	gdouble  length;
	gdouble  length_gaps;
	gdouble  max_speed;
	gulong   trackpoints;
	guint    segments;
	gint     duration;
	time_t   start_time;
	time_t   end_time;
	gint     count;
} track_stats;

// Early incarnations of the code had facilities to print output for multiple files
//  but has been rescoped to work on a single list of tracks for the GUI
typedef enum {
	//TS_TRACK,
	TS_TRACKS,
	//TS_FILES,
} track_stat_block;
static track_stats tracks_stats[1];

// cf with vik_track_get_minmax_alt internals
#define VIK_VAL_MIN_ALT 25000.0
#define VIK_VAL_MAX_ALT -5000.0

/**
 * Reset the specified block
 * Call this when starting to processing multiple items
 */
static void val_reset ( track_stat_block block )
{
	tracks_stats[block].min_alt     = VIK_VAL_MIN_ALT;
	tracks_stats[block].max_alt     = VIK_VAL_MAX_ALT;
	tracks_stats[block].elev_gain   = 0.0;
	tracks_stats[block].elev_loss   = 0.0;
	tracks_stats[block].length      = 0.0;
	tracks_stats[block].length_gaps = 0.0;
	tracks_stats[block].max_speed   = 0.0;
	tracks_stats[block].trackpoints = 0;
	tracks_stats[block].segments    = 0;
	tracks_stats[block].duration    = 0;
	tracks_stats[block].start_time  = 0;
	tracks_stats[block].end_time    = 0;
	tracks_stats[block].count       = 0;
}

/**
 * @val_analyse_track:
 * @trk: The track to be analyse
 *
 * Function to collect statistics, using the internal track functions
 */
static void val_analyse_track ( VikTrack *trk )
{
	//val_reset ( TS_TRACK );
	gdouble min_alt;
	gdouble max_alt;
	gdouble up;
	gdouble down;

	gdouble  length      = 0.0;
	gdouble  length_gaps = 0.0;
	gdouble  max_speed   = 0.0;
	gulong   trackpoints = 0;
	guint    segments    = 0;

	tracks_stats[TS_TRACKS].count++;

	trackpoints = vik_track_get_tp_count (trk);
	segments    = vik_track_get_segment_count (trk);
	length      = vik_track_get_length (trk);
	length_gaps = vik_track_get_length_including_gaps (trk);
	max_speed   = vik_track_get_max_speed (trk);

	int ii;
	for (ii = 0; ii < G_N_ELEMENTS(tracks_stats); ii++) {
		tracks_stats[ii].trackpoints += trackpoints;
		tracks_stats[ii].segments    += segments;
		tracks_stats[ii].length      += length;
		tracks_stats[ii].length_gaps += length_gaps;
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

	if ( trk->trackpoints && VIK_TRACKPOINT(trk->trackpoints->data)->timestamp ) {
		time_t t1, t2;
		t1 = VIK_TRACKPOINT(g_list_first(trk->trackpoints)->data)->timestamp;
		t2 = VIK_TRACKPOINT(g_list_last(trk->trackpoints)->data)->timestamp;

		// Assume never actually have a track with a time of 0 (1st Jan 1970)
		for (ii = 0; ii < G_N_ELEMENTS(tracks_stats); ii++) {
			if ( tracks_stats[ii].start_time == 0)
				tracks_stats[ii].start_time = t1;
			if ( tracks_stats[ii].end_time == 0)
				tracks_stats[ii].end_time = t2;
		}

		// Initialize to the first value
		for (ii = 0; ii < G_N_ELEMENTS(tracks_stats); ii++) {
			if (t1 < tracks_stats[ii].start_time)
				tracks_stats[ii].start_time = t1;
			if (t2 > tracks_stats[ii].end_time)
				tracks_stats[ii].end_time = t2;
		}

		for (ii = 0; ii < G_N_ELEMENTS(tracks_stats); ii++) {
			tracks_stats[ii].duration = tracks_stats[ii].duration + (int)(t2-t1);
		}
	}
}

// Could use GtkGrids but that is Gtk3+
static GtkWidget *create_table (int cnt, char *labels[], GtkWidget *contents[])
{
	GtkTable *table;
	int i;

	table = GTK_TABLE(gtk_table_new (cnt, 2, FALSE));
	gtk_table_set_col_spacing (table, 0, 10);
	for (i=0; i<cnt; i++) {
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
	return GTK_WIDGET (table);
}

static gchar *label_texts[] = {
	N_("Number of Tracks"),
	N_("Date Range"),
	N_("Total Length"),
	N_("Average Length"),
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
static GtkWidget *create_layout ( GtkWidget *content[] )
{
	int cnt = 0;
	for ( cnt = 0; cnt < G_N_ELEMENTS(label_texts); cnt++ )
		content[cnt] = gtk_label_new ( NULL );

	return create_table (cnt, label_texts, content);
}

/**
 * table_output:
 *
 * Update the given widgets table with the values from the track stats
 */
static void table_output ( track_stats ts, GtkWidget *content[] )
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
	// Test if the same day by comparing the date string of the timestamp
	GDate* gdate_start = g_date_new ();
	g_date_set_time_t ( gdate_start, ts.start_time );
	gchar time_start[32];
	g_date_strftime ( time_start, sizeof(time_start), "%x", gdate_start );
	g_date_free ( gdate_start );

	GDate* gdate_end = g_date_new ();
	g_date_set_time_t ( gdate_end, ts.end_time );
	gchar time_end[32];
	g_date_strftime ( time_end, sizeof(time_end), "%x", gdate_end );
	g_date_free ( gdate_end );

	if ( ts.start_time == ts.end_time )
		g_snprintf ( tmp_buf, sizeof(tmp_buf), _("No Data") );
	else if ( strncmp(time_start, time_end, 32) )
		g_snprintf ( tmp_buf, sizeof(tmp_buf), "%s --> %s", time_start, time_end );
	else
		g_snprintf ( tmp_buf, sizeof(tmp_buf), "%s", time_start );

	gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );

	switch (a_vik_get_units_distance ()) {
	case VIK_UNITS_DISTANCE_MILES:
		g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%.1f miles"), VIK_METERS_TO_MILES(ts.length) );
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
	default:
		//VIK_UNITS_DISTANCE_KILOMETRES
		g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%.2f km"), ts.length/(1000.0*ts.count) );
		break;
	}
	gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );

	// I'm sure this could be cleaner...
	switch (a_vik_get_units_speed()) {
	case VIK_UNITS_SPEED_MILES_PER_HOUR:
		g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%.1f mph"), (double)VIK_MPS_TO_MPH(ts.max_speed) );
		gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );
		if ( ts.duration > 0 )
			g_snprintf ( tmp_buf, sizeof(tmp_buf), ("%.1f mph"), (double)VIK_MPS_TO_MPH(ts.length/ts.duration) );
		break;
	case VIK_UNITS_SPEED_METRES_PER_SECOND:
		if ( ts.max_speed > 0 )
			g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%.2f m/s"), (double)ts.max_speed );
		gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );
		if ( ts.duration > 0 )
			g_snprintf ( tmp_buf, sizeof(tmp_buf), ("%.2f m/s"), (double)(ts.length/ts.duration) );
		else
			g_snprintf ( tmp_buf, sizeof(tmp_buf), "--" );
		break;
	case VIK_UNITS_SPEED_KNOTS:
		if ( ts.max_speed > 0 )
			g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%.2f knots\n"), (double)VIK_MPS_TO_KNOTS(ts.max_speed) );
		gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );
		if ( ts.duration > 0 )
			g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%.2f knots"), (double)VIK_MPS_TO_KNOTS(ts.length/ts.duration) );
		else
			g_snprintf ( tmp_buf, sizeof(tmp_buf), "--" );
		break;
	default:
		//VIK_UNITS_SPEED_KILOMETRES_PER_HOUR:
		if ( ts.max_speed > 0 )
			g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%.2f km/h"), (double)VIK_MPS_TO_KPH(ts.max_speed) );
		gtk_label_set_text ( GTK_LABEL(content[cnt++]), tmp_buf );
		if ( ts.duration > 0 )
			g_snprintf ( tmp_buf, sizeof(tmp_buf), _("%.2f km/h"), (double)VIK_MPS_TO_KPH(ts.length/ts.duration) );
		else
			g_snprintf ( tmp_buf, sizeof(tmp_buf), "--" );
		break;
	}
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

/**
 * val_analyse_item_maybe:
 * @vtlist: A track and the associated layer to consider for analysis
 * @data:   Whether to include invisible items
 *
 * Analyse this particular track
 *  considering whether it should be included depending on it's visibility
 */
static void val_analyse_item_maybe ( vik_trw_track_list_t *vtlist, const gpointer data )
{
	gboolean include_invisible = GPOINTER_TO_INT(data);
	VikTrack *trk = vtlist->trk;
	VikTrwLayer *vtl = vtlist->vtl;

	// Safety first - items shouldn't be deleted...
	if ( !IS_VIK_TRW_LAYER(vtl) ) return;
	if ( !trk ) return;

	if ( !include_invisible ) {
		// Skip invisible layers or sublayers
		if ( !VIK_LAYER(vtl)->visible ||
			 (trk->is_route && !vik_trw_layer_get_routes_visibility(vtl)) ||
			 (!trk->is_route && !vik_trw_layer_get_tracks_visibility(vtl)) )
			return;

		// Skip invisible tracks
		if ( !trk->visible )
			return;
	}

	val_analyse_track ( trk );
}

/**
 * val_analyse:
 * @widgets:           The widget layout
 * @tracks_and_layers: A list of #vik_trw_track_list_t
 * @include_invisible: Whether to include invisible layers and tracks
 *
 * Analyse each item in the @tracks_and_layers list
 *
 */
void val_analyse ( GtkWidget *widgets[], GList *tracks_and_layers, gboolean include_invisible )
{
	val_reset ( TS_TRACKS );

	GList *gl = g_list_first ( tracks_and_layers );
	if ( gl ) {
		g_list_foreach ( gl, (GFunc) val_analyse_item_maybe, GINT_TO_POINTER(include_invisible) );
	}

	table_output ( tracks_stats[TS_TRACKS], widgets );
}

typedef struct {
	GtkWidget **widgets;
	GtkWidget *layout;
	GtkWidget *check_button;
	GList *tracks_and_layers;
	VikLayer *vl;
	gpointer user_data;
	VikTrwlayerGetTracksAndLayersFunc get_tracks_and_layers_cb;
	VikTrwlayerAnalyseCloseFunc on_close_cb;
} analyse_cb_t;

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

	val_analyse ( acb->widgets, acb->tracks_and_layers, value );
	gtk_widget_show_all ( acb->layout );
}

#define VIK_SETTINGS_ANALYSIS_DO_INVISIBLE "track_analysis_do_invisible"

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

	//g_free ( data->layout );
	g_free ( data->widgets );
	g_list_foreach ( data->tracks_and_layers, (GFunc) g_free, NULL );
	g_list_free ( data->tracks_and_layers );

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
	//VikWindow *vw = VIK_WINDOW(window);

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

	analyse_cb_t *acb = g_malloc (sizeof(analyse_cb_t));
	acb->vl = vl;
	acb->user_data = user_data;
	acb->get_tracks_and_layers_cb = get_tracks_and_layers_cb;
	acb->on_close_cb = on_close_cb;
	acb->tracks_and_layers = get_tracks_and_layers_cb ( vl, user_data );
	acb->widgets = g_malloc ( sizeof(GtkWidget*) * G_N_ELEMENTS(label_texts) );
	acb->layout = create_layout ( acb->widgets );

	gtk_box_pack_start ( GTK_BOX(content), acb->layout, FALSE, FALSE, 0 );

	// Analysis seems reasonably quick
	//  unless you have really large numbers of tracks (i.e. many many thousands or a really slow computer)
	// One day might store stats in the track itself....
	val_analyse ( acb->widgets, acb->tracks_and_layers, include_invisible );
	
	GtkWidget *cb = gtk_check_button_new_with_label ( _("Include Invisible Items") );
	gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(cb), include_invisible );
	gtk_box_pack_start ( GTK_BOX(content), cb, FALSE, FALSE, 10);
	acb->check_button = cb;
	
	gtk_widget_show_all ( dialog );

	g_signal_connect ( G_OBJECT(cb), "toggled", G_CALLBACK(include_invisible_toggled_cb), acb );
	g_signal_connect ( G_OBJECT(dialog), "response", G_CALLBACK(analyse_close), acb );

	return dialog;
}
