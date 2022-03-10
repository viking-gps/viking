/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2022, Rob Norris <rw_norris@hotmail.com>
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
#include "astronomy.h"
#include <time.h>
#include "degrees_converters.h"

#ifdef HAVE_LIBNOVA_LIBNOVA_H
#include <libnova/libnova.h>

// Need to know which widgets that can be shown or hidden on demand
static GList *hide_widgets = NULL;

static GtkWidget *create_table (int cnt, char *labels[], GtkWidget *contents[], gchar *value_potentialURL[], gboolean hideable[] )
{
	GtkTable *table = GTK_TABLE(gtk_table_new (cnt, 2, FALSE));
	gtk_table_set_col_spacing ( table, 0, 10 );

	g_list_free ( hide_widgets );
	hide_widgets = NULL;

	for ( guint ii=0; ii<cnt; ii++ ) {
		GtkWidget *ww = ui_attach_to_table ( table, ii, labels[ii], contents[ii], value_potentialURL[ii], TRUE );
		if ( hideable[ii] ) {
			hide_widgets = g_list_prepend ( hide_widgets, contents[ii] );
			hide_widgets = g_list_prepend ( hide_widgets, ww );
		}
	}

	return GTK_WIDGET(table);
}

/**
 * astro_get_daystart:
 *
 * By default libnova returns next rise/set pair.
 *  so for a given time it may return set/rise
 * We want the rise/set for a specific day,
 *  so need to reset the time to the beginning of the day
 *  but the notion of the day will depend on the world position/timezone
 */
double astro_get_daystart ( double JDin, VikCoord *vc )
{
	double JDdaystart = JDin;
	struct ln_date lndt;
	switch ( a_vik_get_time_ref_frame() ) {
	case VIK_TIME_REF_UTC:
		ln_get_date ( JDin, &lndt );
		lndt.hours = 0; // reset to beginning of the day
		lndt.minutes = 0; // reset to beginning of the day
		JDdaystart = ln_get_julian_day ( &lndt );
		break;
	case VIK_TIME_REF_WORLD: {
		gint32 offset_secs = 0;
		gchar *mytz = vu_get_tz_at_location ( vc );
		if ( mytz ) {
			GTimeZone *gtz = g_time_zone_new ( mytz );
			offset_secs = g_time_zone_get_offset ( gtz, 0 );
			g_time_zone_unref ( gtz );
		}
		else {
			// No results (e.g. could be in the middle of a sea)
			// Fallback to simplistic method that doesn't take into account Timezones of countries.
			struct LatLon ll;
			vik_coord_to_latlon ( vc, &ll );
			offset_secs = round ( ll.lon / 15.0 ) * 3600;
		}

		ln_get_date ( JDin, &lndt );
		lndt.hours = 0; // reset to beginning of the day
		lndt.minutes = 0; // reset to beginning of the day
		JDdaystart = ln_get_julian_day ( &lndt );

		// Apply offset to counteract effect of timezone
		JDdaystart = JDdaystart - (double)offset_secs/(24.0*60.0*60.0);
		}
		break;
	default: {
		// VIK_TIME_REF_LOCALE
		struct ln_zonedate lnz;
		ln_get_local_date ( JDin, &lnz );
		lnz.hours = 0; // reset to beginning of the day
		lnz.minutes = 0; // reset to beginning of the day
		JDdaystart = ln_get_julian_local_date ( &lnz );
		}
		break;
	}

	return JDdaystart;
}

// Free string after use
static gchar *time_string_for_julian ( double JD, const VikCoord* vc, const gchar *tz )
{
	time_t tt;
	ln_get_timet_from_julian ( JD, &tt );
	// Times don't need to be to the second
	return vu_get_time_string ( &tt, "%x %H:%M %Z", vc, tz );
}

static void potential_circumpolar_sun_tooltip ( GtkWidget *widget, int res )
{
	if ( res == 1 )
		gtk_widget_set_tooltip_text ( widget, _("Circumpolar - above horizon") );
	else if ( res == -1 )
		gtk_widget_set_tooltip_text ( widget, _("Circumpolar - below horizon") );
}

static void potential_circumpolar_moon_tooltip ( GtkWidget *widget, int res )
{
	// NB When moon is circumpolar it will be below the horizon
	if ( res == 1 )
		gtk_widget_set_tooltip_text ( widget, _("Circumpolar") );
}

// Free string after use
static gchar *get_time_string ( double JD, int res, const VikCoord* vc, const gchar *tz )
{
	gchar *msg = NULL;
	if ( res == 0 ) 
		msg = time_string_for_julian ( JD, vc, tz );
	else
		// Sun/Moon is circumpolar - so neither rises or sets
		msg = g_strdup ( "--" );
	return msg;
}

#define QTR_PHASE_START 82.5
/**
 * Convert the phase angle (Value between 0 and 180)
 *  into standard textual description
 *
 * Use own definitions of what angles consitutes what phase;
 *  as not sure of any official definition.
 */
static gchar *moon_phase_as_string ( double phase, double JD )
{
	gchar *msg = NULL;
	if ( phase < 8.0 ) {
		msg = g_strdup ( _("Full") );
	} else {
		// Need to determine which phase change direction
		//  (i.e. is it towards fullness?)
		double phase2 = ln_get_lunar_phase ( JD+1 );
		if ( phase > QTR_PHASE_START && phase < 97.5 ) {
			if ( phase2 < phase )
				msg = g_strdup ( _("First Quarter") );
			else
				msg = g_strdup ( _("Last Quarter") );
		} else {
			if ( phase2 < phase && phase > 172.0 )
				msg = g_strdup ( _("New") );
			else {
				if ( phase2 < phase ) {
					if ( phase < QTR_PHASE_START )
						msg = g_strdup ( _("Waxing Gibbous") );
					else
						msg = g_strdup ( _("Waxing Crescent") );
				} else {
					if ( phase < QTR_PHASE_START )
						msg = g_strdup ( _("Waning Gibbous") );
					else
						msg = g_strdup ( _("Waning Crescent") );
				}
			}
		}
	}
	return msg;
}

static void astro_program_open ( const gchar *date_str, const gchar *time_str, const gchar *lat_str, const gchar *lon_str, const gchar *alt_str )
{
  GError *err = NULL;
  gchar *tmp;
  gint fd = g_file_open_tmp ( "vik-astro-XXXXXX.ini", &tmp, &err );
  if (fd < 0) {
    g_warning ( "%s: Failed to open temporary file: %s", __FUNCTION__, err->message );
    g_clear_error ( &err );
    return;
  }
  gchar *cmd = g_strdup_printf ( "%s %s %s %s %s %s %s %s %s %s %s %s %s %s",
                                 a_vik_get_astro_program(), "-c", tmp, "--full-screen no", "--sky-date", date_str, "--sky-time", time_str, "--latitude", lat_str, "--longitude", lon_str, "--altitude", alt_str );
  g_message ( "%s: %s", __FUNCTION__, cmd );
  if ( ! g_spawn_command_line_async ( cmd, &err ) ) {
	  a_dialog_error_msg_extra ( NULL, _("Could not launch %s"), a_vik_get_astro_program() ); // TODO parent...
	  g_warning ( "%s", err->message );
	  g_error_free ( err );
  }
  util_add_to_deletion_list ( tmp );
  g_free ( tmp );
  g_free ( cmd );
}

// Format of stellarium lat & lon seems designed to be particularly awkward
//  who uses ' & " in the parameters for the command line?!
// -1d4'27.48"
// +53d58'16.65"
static gchar *convert_to_dms ( gdouble dec )
{
  gdouble tmp;
  gchar sign_c = ' ';
  gint val_d, val_m;
  gdouble val_s;
  gchar *result = NULL;

  if ( dec > 0 )
    sign_c = '+';
  else if ( dec < 0 )
    sign_c = '-';
  else // Nul value
    sign_c = ' ';

  // Degrees
  tmp = fabs(dec);
  val_d = (gint)tmp;

  // Minutes
  tmp = (tmp - val_d) * 60;
  val_m = (gint)tmp;

  // Seconds
  val_s = (tmp - val_m) * 60;

  // Format
  result = g_strdup_printf ( "%c%dd%d\\\'%.4f\\\"", sign_c, val_d, val_m, val_s );
  return result;
}

// Easier to manage single instance than try to pass around value
static time_t astt;

static void invoke_astro_program ( VikCoord* vc )
{
	gchar date_buf[20];
	strftime ( date_buf, sizeof(date_buf), "%Y%m%d", gmtime(&astt) );
	gchar time_buf[20];
	strftime ( time_buf, sizeof(time_buf), "%H:%M:%S", gmtime(&astt) );
	struct LatLon ll;
	vik_coord_to_latlon ( vc, &ll );
	gchar *lat_str = convert_to_dms ( ll.lat );
	gchar *lon_str = convert_to_dms ( ll.lon );
	gchar alt_buf[20];
	snprintf ( alt_buf, sizeof(alt_buf), "%d", 0 );

	astro_program_open ( date_buf, time_buf, lat_str, lon_str, alt_buf );

	g_free ( lat_str );
	g_free ( lon_str );
}

static gchar *astro_texts[] = {
	N_("Astronomical Twilight"),
	N_("Nautical Twilight"),
	N_("Civil Twilight"), // Dawn
	N_("Sun Rise"),
	N_("Sun Set"),
	N_("Civil Twilight"), // Dusk
	N_("Nautical Twilight"),
	N_("Astronomical Twilight"),
	N_("Moon Rise"),
	N_("Moon Set"),
	N_("Moon Phase"),
};

// Mark that the Astronomical and Nautical Twilight text labels can be hidden
static gboolean hideable_widgets[] = {
	TRUE,
	TRUE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	TRUE,
	TRUE,
	FALSE,
	FALSE,
	FALSE,
};

static void show_hide_widgets ( gboolean show )
{
	for ( GList *it = g_list_first(hide_widgets); it != NULL; it = g_list_next(it) ) {
		if ( show )
			gtk_widget_show ( GTK_WIDGET(it->data) );
		else
			gtk_widget_hide ( GTK_WIDGET(it->data) );
	}

}

#define VIK_SETTINGS_ASTRO_SHOW_FULL_TWILIGHT "astro_dialog_show_full_twilight"

static void twilight_toggled_cb ( GtkToggleButton *togglebutton, gpointer ignore )
{
	gboolean show = gtk_toggle_button_get_active(togglebutton);
	a_settings_set_boolean ( VIK_SETTINGS_ASTRO_SHOW_FULL_TWILIGHT, show );
	show_hide_widgets ( show );
}

static void add_sun_or_moon_value ( GtkWidget *content_as[], guint pos, double riseOrSet, int res,
									VikCoord *vc, const gchar *tz, gboolean sunTip, gboolean moonTip )
{
	gchar *msg = get_time_string ( riseOrSet, res, vc, tz );
	content_as[pos] = ui_label_new_selectable ( msg );
	g_free ( msg );
	if ( sunTip )
		potential_circumpolar_sun_tooltip ( content_as[pos], res );
	if ( moonTip )
		potential_circumpolar_moon_tooltip ( content_as[pos], res );
}

/**
 * astro_info:
 * @reset_for_day_beginning:  If the time is already at the beginning of day
 *  (e.g. invoked from the calendar) then it does not need further adjusting.
 *
 * Creates a widget table of Astronomical date for the specified (Astronomical Julian) time and position
 *
 * Note do not call gtk_widget_show_all() on this widget;
 * since this widget is managing whether certain information is shown or not
 */
GtkWidget *astro_info ( time_t att, VikCoord* vc, gboolean reset_for_day_beginning )
{
	gchar *text_value_is_URLs[G_N_ELEMENTS(astro_texts)];
	for ( guint tv = 0; tv < G_N_ELEMENTS(astro_texts); tv++ )
		text_value_is_URLs[tv] = NULL;

	// Get previous value (if any) from the settings
	gboolean show_full_twilight;
	if ( ! a_settings_get_boolean ( VIK_SETTINGS_ASTRO_SHOW_FULL_TWILIGHT, &show_full_twilight ) )
		show_full_twilight = FALSE;

	GtkWidget *cb = gtk_check_button_new_with_label ( _("Show Full Twilight Values") );
	gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON(cb), show_full_twilight );

	g_signal_connect ( G_OBJECT(cb), "toggled", G_CALLBACK(twilight_toggled_cb), NULL );

	gchar tmp_buf[50];
	GtkWidget *content_as[G_N_ELEMENTS(astro_texts)];
	guint cc_as = 0;

	struct LatLon ll;
	vik_coord_to_latlon ( vc, &ll );

	struct ln_lnlat_posn observer;
	observer.lat = ll.lat;
	observer.lng = ll.lon;

	double JD = ln_get_julian_from_timet ( &att );
	double sunJD = JD;
	if ( reset_for_day_beginning )
		sunJD = astro_get_daystart ( JD, vc );

	// Minor optimization of getting timezone once,
	//  rather than everytime for each time shown
	gchar *tz = NULL;
	if ( a_vik_get_time_ref_frame() == VIK_TIME_REF_WORLD )
		tz = vu_get_tz_at_location ( vc );

	struct ln_rst_time rst;
	int res = ln_get_solar_rst ( sunJD, &observer, &rst );

	// Separate variables to store Twilight values
	struct ln_rst_time rstNTw;
	struct ln_rst_time rstATw;
	struct ln_rst_time rstCTw;

	//return 0 for success, 1 for circumpolar (above the horizon), -1 for circumpolar (bellow the horizon)
	int resA = ln_get_solar_rst_horizon ( sunJD, &observer, LN_SOLAR_ASTRONOMICAL_HORIZON, &rstATw );
	int resN = ln_get_solar_rst_horizon ( sunJD, &observer, LN_SOLAR_NAUTIC_HORIZON, &rstNTw );
	int resC = ln_get_solar_rst_horizon ( sunJD, &observer, LN_SOLAR_CIVIL_HORIZON, &rstCTw );

	add_sun_or_moon_value ( content_as, cc_as++, rstATw.rise, resA, vc, tz, FALSE, FALSE );
	add_sun_or_moon_value ( content_as, cc_as++, rstNTw.rise, resN, vc, tz, FALSE, FALSE );
	add_sun_or_moon_value ( content_as, cc_as++, rstCTw.rise, resC, vc, tz, FALSE, FALSE );

	add_sun_or_moon_value ( content_as, cc_as++, rst.rise, res, vc, tz, TRUE, FALSE );
	add_sun_or_moon_value ( content_as, cc_as++, rst.set, res, vc, tz, TRUE, FALSE );

	add_sun_or_moon_value ( content_as, cc_as++, rstCTw.set, resC, vc, tz, FALSE, FALSE );
	add_sun_or_moon_value ( content_as, cc_as++, rstNTw.set, resN, vc, tz, FALSE, FALSE );
	add_sun_or_moon_value ( content_as, cc_as++, rstATw.set, resA, vc, tz, FALSE, FALSE );

	//return 0 for success, else 1 for circumpolar.
	res = ln_get_lunar_rst ( JD, &observer, &rst );

	// Note Moonset can be before the rise
	//  May want to consider how to rearrange the order but to change labels as well
	//  so ATM this is in the fixed order to match the labels
	add_sun_or_moon_value ( content_as, cc_as++, rst.rise, resA, vc, tz, FALSE, TRUE );
	add_sun_or_moon_value ( content_as, cc_as++, rst.set, resA, vc, tz, FALSE, TRUE );

	// Convert phase angle into textual description
	double dphase = ln_get_lunar_phase ( JD );
	gchar *sphase = moon_phase_as_string ( dphase, JD );
	if ( vik_debug )
		g_snprintf ( tmp_buf, sizeof(tmp_buf), "%s (%.1f%s) (%.0f%%)", sphase, dphase, DEGREE_SYMBOL, ln_get_lunar_disk(JD)*100 );
	else
		g_snprintf ( tmp_buf, sizeof(tmp_buf), "%s (%.0f%%)", sphase, ln_get_lunar_disk(JD)*100 );
	g_free ( sphase );
	content_as[cc_as++] = ui_label_new_selectable ( tmp_buf );

	GtkWidget *table = create_table ( cc_as, astro_texts, content_as, text_value_is_URLs, hideable_widgets );
	GtkWidget *vb = gtk_vbox_new ( FALSE, 2 );
	gtk_box_pack_start ( GTK_BOX(vb), cb, FALSE, FALSE, 2 );
	gtk_box_pack_start ( GTK_BOX(vb), table, FALSE, TRUE, 2 );

	gtk_widget_show_all ( vb );
	show_hide_widgets ( show_full_twilight );

	if ( a_vik_have_astro_program() ) {
		GtkWidget *button = gtk_button_new_with_label ( a_file_basename(a_vik_get_astro_program()) );
		ln_get_timet_from_julian ( JD, &astt );
		g_signal_connect_swapped ( G_OBJECT(button), "clicked", G_CALLBACK(invoke_astro_program), vc );
		gchar *tip = g_strdup_printf ( _("Launch %s at this date"), a_file_basename(a_vik_get_astro_program()) );
		gtk_widget_set_tooltip_text ( button, tip );
		g_free ( tip );
		gtk_box_pack_end ( GTK_BOX(vb), button, FALSE, TRUE, 2 );
		gtk_widget_show ( button );
	}

	return vb;
}
#endif // libnova
