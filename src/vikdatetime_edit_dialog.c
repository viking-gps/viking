/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
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
#include "vikdatetime_edit_dialog.h"
#include <math.h>
#include <glib/gi18n.h>
#include "ui_util.h"
#include "dialog.h"

// Show leading zeros
static gboolean on_output ( GtkSpinButton *spin, gpointer data )
{
	GtkAdjustment *adjustment = gtk_spin_button_get_adjustment ( spin );
	gint value = (gint)gtk_adjustment_get_value ( adjustment );
	gchar *text = g_strdup_printf ( "%02d", value );
	gtk_entry_set_text ( GTK_ENTRY (spin), text );
	g_free ( text );

	return TRUE;
}

static gboolean on_output6 ( GtkSpinButton *spin, gpointer data )
{
	GtkAdjustment *adjustment = gtk_spin_button_get_adjustment ( spin );
	gint value = (gint)gtk_adjustment_get_value ( adjustment );
	gchar *text = g_strdup_printf ( "%06d", value );
	gtk_entry_set_text ( GTK_ENTRY (spin), text );
	g_free ( text );

	return TRUE;
}

typedef struct _dteDialog {
	GtkWidget *dialog;
	GtkWidget *cal;
	GtkWidget *sb_hours;
	GtkWidget *sb_minutes;
	GtkWidget *sb_seconds;
	GtkWidget *sb_usecs;
	GtkWidget *entry;
	GTimeZone *tz;
} DateTimeEditDialogT;

static void
text_changed_cb (GtkEntry   *entry,
                 GParamSpec *pspec,
                 GtkWidget  *button)
{
  gboolean has_text = gtk_entry_get_text_length(entry) > 0;
  gtk_entry_set_icon_sensitive ( entry, GTK_ENTRY_ICON_SECONDARY, has_text );
  gtk_widget_set_sensitive ( button, !has_text );
}

static gdouble get_time_from_widgets ( DateTimeEditDialogT *dte )
{
	gdouble ans = NAN;

	// Read values
	guint year = 0;
	guint month = 0;
	guint day = 0;
	guint hours = 0;
	guint minutes = 0;
	guint seconds = 0;
	guint usecs = 0;

	gtk_calendar_get_date ( GTK_CALENDAR(dte->cal), &year, &month, &day );
	hours = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(dte->sb_hours) );
	minutes = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(dte->sb_minutes) );
	seconds = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(dte->sb_seconds) );
	usecs = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(dte->sb_usecs) );
	gdouble gsecs = seconds + (gdouble)usecs/G_TIME_SPAN_SECOND;

	GDateTime *gdt_ans = g_date_time_new ( dte->tz, year, month+1, day, hours, minutes, gsecs );
	if ( gdt_ans ) {
		ans = g_date_time_to_unix ( gdt_ans );
		ans += (gdouble)g_date_time_get_microsecond(gdt_ans) / G_TIME_SPAN_SECOND;
		g_date_time_unref ( gdt_ans );
	}
	return ans;
}

// Get values from Calendar widgets
// And output text into Entry
static void cnv_apply ( DateTimeEditDialogT *dte )
{
	gdouble ans = get_time_from_widgets ( dte );

	if ( !isnan(ans) ) {
		GTimeVal timestamp;
		timestamp.tv_sec = ans;
		timestamp.tv_usec = abs((ans-(gint64)ans)*G_USEC_PER_SEC);

		gchar *time_iso8601 = g_time_val_to_iso8601 ( &timestamp );
		if ( time_iso8601 != NULL ) {
			ui_entry_set_text ( dte->entry, time_iso8601 );
		} else
			g_warning ( "%s: Could not convert timestamp (secs %ld, usecs %ld) to iso8601", __FUNCTION__, timestamp.tv_sec, timestamp.tv_usec );
		g_free ( time_iso8601 );
	}
}

// Basic calendar date parsing
static gdouble my_parse_utc ( const gchar *txt )
{
	gdouble ans = NAN;

	GDateTime *gdt_ans = NULL;
	// Special handling of just years as a YYYY value since glib doesn't
	if ( strlen(txt) == 4 ) {
		gdouble years = g_ascii_strtod ( txt, NULL );
		gdt_ans = g_date_time_new_utc ( years, 1, 1, 0, 0, 0.0 );
	} else {
		GDate *gd = g_date_new ();
		g_date_set_parse ( gd, txt );
		if ( g_date_valid(gd) )
			gdt_ans = g_date_time_new_utc ( g_date_get_year(gd), g_date_get_month(gd), g_date_get_day(gd), 0, 0, 0.0 );
		g_date_free ( gd );
	}

	if ( gdt_ans ) {
		ans = g_date_time_to_unix ( gdt_ans );
		ans += (gdouble)g_date_time_get_microsecond(gdt_ans) / G_TIME_SPAN_SECOND;
		g_date_time_unref ( gdt_ans );
	}
	return ans;
}

/**
 * vik_datetime_edit_dialog:
 * @parent:         The parent window
 * @title:          The title to use for the dialog
 * @initial_time:   The initial date/time to be shown
 * @tz:             The #GTimeZone this dialog will operate in
 *
 * Returns: A time selected by the user via this dialog
 *          Otherwise NAN if the dialog was cancelled or somehow an invalid date was encountered.
 */
gdouble vik_datetime_edit_dialog ( GtkWindow *parent, const gchar *title, gdouble initial_time, GTimeZone *tz )
{
	g_return_val_if_fail ( tz, NAN );

	GtkWidget *dialog = gtk_dialog_new_with_buttons ( title,
	                                                  parent,
	                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	                                                  GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
	                                                  GTK_STOCK_OK,     GTK_RESPONSE_ACCEPT,
	                                                  NULL );

	gtk_dialog_set_default_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
	GtkWidget *response_w = NULL;
#if GTK_CHECK_VERSION (2, 20, 0)
	response_w = gtk_dialog_get_widget_for_response ( GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT );
#endif

	GtkWidget *label;
	GtkWidget *cal = gtk_calendar_new ();

	// Set according to the given date/time + timezone for display
	GTimeVal tv = { initial_time, round((initial_time-floor(initial_time))*G_TIME_SPAN_SECOND) };
	GDateTime *gdt_in = g_date_time_new_from_timeval_utc ( &tv );
	GDateTime *gdt_tz = g_date_time_to_timezone ( gdt_in, tz );
	g_date_time_unref ( gdt_in );

	gtk_calendar_select_month ( GTK_CALENDAR(cal), g_date_time_get_month(gdt_tz)-1, g_date_time_get_year (gdt_tz) );
	gtk_calendar_select_day ( GTK_CALENDAR(cal), g_date_time_get_day_of_month(gdt_tz) );

	GtkWidget *hbox_time = gtk_hbox_new ( FALSE, 1 );

	label = gtk_label_new ( g_date_time_get_timezone_abbreviation(gdt_tz) );
	gtk_box_pack_start ( GTK_BOX(hbox_time), label, FALSE, FALSE, 5 );

	GtkWidget *sb_hours = gtk_spin_button_new_with_range ( 0.0, 23.0, 1.0 );
	gtk_box_pack_start ( GTK_BOX(hbox_time), sb_hours, FALSE, FALSE, 0 );
	gtk_spin_button_set_value ( GTK_SPIN_BUTTON(sb_hours), g_date_time_get_hour(gdt_tz) );
	g_signal_connect ( sb_hours, "output", G_CALLBACK(on_output), NULL );

	label = gtk_label_new ( ":" );
	gtk_box_pack_start ( GTK_BOX(hbox_time), label, FALSE, FALSE, 0 );

	GtkWidget *sb_minutes = gtk_spin_button_new_with_range ( 0.0, 59.0, 1.0 );
	gtk_box_pack_start ( GTK_BOX(hbox_time), sb_minutes, FALSE, FALSE, 0);
	gtk_spin_button_set_value ( GTK_SPIN_BUTTON(sb_minutes), g_date_time_get_minute(gdt_tz) );
	g_signal_connect ( sb_minutes, "output", G_CALLBACK(on_output), NULL );

	label = gtk_label_new ( ":" );
	gtk_box_pack_start(GTK_BOX(hbox_time), label, FALSE, FALSE, 0);

	GtkWidget *sb_seconds = gtk_spin_button_new_with_range ( 0.0, 59.0, 1.0 );
	gtk_box_pack_start ( GTK_BOX(hbox_time), sb_seconds, FALSE, FALSE, 0 );
	gtk_spin_button_set_value ( GTK_SPIN_BUTTON(sb_seconds), g_date_time_get_second(gdt_tz) );
	g_signal_connect ( sb_seconds, "output", G_CALLBACK(on_output), NULL );

	label = gtk_label_new ( "." );
	gtk_box_pack_start(GTK_BOX(hbox_time), label, FALSE, FALSE, 0);

	GtkWidget *sb_usecs = gtk_spin_button_new_with_range ( 0, 999999, 1000 );
	gtk_box_pack_start ( GTK_BOX(hbox_time), sb_usecs, FALSE, FALSE, 0 );
	gtk_spin_button_set_value ( GTK_SPIN_BUTTON(sb_usecs), g_date_time_get_microsecond(gdt_tz) );
	g_signal_connect ( sb_usecs, "output", G_CALLBACK(on_output6), NULL );

	GtkWidget *hbox_text = gtk_hbox_new ( FALSE, 1 );

	GtkWidget *cnv_button = gtk_button_new();
	// Button with just an image (no need for text on it)
	GtkWidget *img = gtk_image_new_from_stock ( GTK_STOCK_CONVERT, GTK_ICON_SIZE_MENU );
	gtk_button_set_image ( GTK_BUTTON(cnv_button), img );
	gtk_box_pack_start ( GTK_BOX(hbox_text), cnv_button, FALSE, FALSE, 0 );

	GtkWidget *entry = ui_entry_new ( NULL, GTK_ENTRY_ICON_SECONDARY );
	// Only enable Convert button when no text in the entry
#if GTK_CHECK_VERSION (2,20,0)
	text_changed_cb ( GTK_ENTRY(entry), NULL, cnv_button );
	g_signal_connect ( entry, "notify::text", G_CALLBACK(text_changed_cb), cnv_button );
#endif
	// 'ok' when press return in the entry
	g_signal_connect_swapped ( entry, "activate", G_CALLBACK(a_dialog_response_accept), dialog );
	gtk_widget_set_tooltip_text ( entry, _("Accepts various forms of ISO 8601") );
	gtk_box_pack_start ( GTK_BOX(hbox_text), entry, TRUE, TRUE, 0 );

	DateTimeEditDialogT dted; // No need to malloc/free as only exists as long as the dialog does
	dted.dialog = dialog;
	dted.cal = cal;
	dted.entry = entry;
	dted.sb_hours = sb_hours;
	dted.sb_minutes = sb_minutes;
	dted.sb_seconds = sb_seconds;
	dted.sb_usecs = sb_usecs;
	dted.tz = tz;
	g_signal_connect_swapped ( cnv_button, "clicked", G_CALLBACK(cnv_apply), &dted );

	gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), cal, FALSE, FALSE, 0 );
	gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), hbox_time, FALSE, FALSE, 5 );
	gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), hbox_text, FALSE, FALSE, 5 );

	if ( response_w )
		gtk_widget_grab_focus ( response_w );

	g_date_time_unref ( gdt_tz );

	gtk_widget_show_all ( dialog );

	gdouble ans = NAN;

	// Use a few gotos as this simplifies the logic flow IMHO
 run:
	if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) != GTK_RESPONSE_ACCEPT )
		goto done;

	if ( gtk_entry_get_text_length(GTK_ENTRY(entry)) > 0 ) {
		const gchar *txt = gtk_entry_get_text ( GTK_ENTRY(entry) );
		gboolean understood = FALSE;
		// Try own parsing as glib only does subset of iso8601
		//  (i.e. "iso_date must include year, month, day, hours, minutes, and seconds)
		//
		if ( gtk_entry_get_text_length(GTK_ENTRY(entry)) <= 10 ) {
			gdouble cal = my_parse_utc ( txt );
			if ( !isnan(cal) ) {
				understood = TRUE;
				ans = cal;
			}
		} else {
#if GTK_CHECK_VERSION (2, 56, 0)
			GDateTime *gdt = g_date_time_new_from_iso8601 ( txt, tz );
			if ( gdt ) {
				ans = g_date_time_to_unix ( gdt );
				ans += (gdouble)g_date_time_get_microsecond(gdt) / G_TIME_SPAN_SECOND;
				g_date_time_unref ( gdt );
				understood = TRUE;
			}
#else
			GTimeVal tv_time;
			// Entry value supercedes other widget values ...
			if ( g_time_val_from_iso8601(txt, &tv_time) ) {
				gdouble d1 = tv_time.tv_sec;
				gdouble d2 = (gdouble)tv_time.tv_usec/G_USEC_PER_SEC;
				ans = (d1 < 0) ? d1 - d2 : d1 + d2;
				understood = TRUE;
			}
#endif
		}
		if ( !understood ) {
			a_dialog_error_msg ( parent, _("Text input time was not understood") );
			goto run;
		}
		goto done;
	}

	ans = initial_time;
	gdouble get_ans = get_time_from_widgets ( &dted );
	if ( !isnan(get_ans) )
		ans = get_ans;

 done:
	gtk_widget_destroy(dialog);

	return ans;
}
