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

/**
 * vik_datetime_edit_dialog:
 * @parent:         The parent window
 * @title:          The title to use for the dialog
 * @initial_time:   The inital date/time to be shown
 * @tz:             The #GTimeZone this dialog will operate in
 *
 * Returns: A time selected by the user via this dialog
 *          Even though a time of zero is notionally valid - consider it unlikely to be actually wanted!
 *          Thus if the time is zero then the dialog was cancelled or somehow an invalid date was encountered.
 */
time_t vik_datetime_edit_dialog ( GtkWindow *parent, const gchar *title, time_t initial_time, GTimeZone *tz )
{
	g_return_val_if_fail ( tz, 0 );

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
	GDateTime *gdt_in = g_date_time_new_from_unix_utc ( (gint64)initial_time );
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

	gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), cal, FALSE, FALSE, 0 );
	gtk_box_pack_start ( GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), hbox_time, FALSE, FALSE, 5 );

	if ( response_w )
		gtk_widget_grab_focus ( response_w );

	g_date_time_unref ( gdt_tz );

	gtk_widget_show_all ( dialog );
	if ( gtk_dialog_run ( GTK_DIALOG(dialog) ) != GTK_RESPONSE_ACCEPT ) {
		gtk_widget_destroy ( dialog );
		return 0;
	}

	// Read values
	guint year = 0;
	guint month = 0;
	guint day = 0;
	guint hours = 0;
	guint minutes = 0;
	guint seconds = 0;

	gtk_calendar_get_date ( GTK_CALENDAR(cal), &year, &month, &day );
	hours = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(sb_hours) );
	minutes = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(sb_minutes) );
	seconds = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON(sb_seconds) );

	gtk_widget_destroy(dialog);

	time_t ans = initial_time;
	GDateTime *gdt_ans = g_date_time_new ( tz, year, month+1, day, hours, minutes, (gdouble)seconds );
	if ( gdt_ans ) {
		ans = g_date_time_to_unix ( gdt_ans );
		g_date_time_unref ( gdt_ans );
	}

	return ans;
}
