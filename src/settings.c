/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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
 /*
  * Sort of like the globals file, but values are automatically saved via program use.
  * Some settings are *not* intended to have any GUI controls.
  * Other settings be can used to set other GUI elements.
  *
  * ATM This is implemented using the simple (for me!) GKeyFile API - AKA an .ini file
  *  One might wish to consider the more modern alternative such as:
  *            http://developer.gnome.org/gio/2.26/GSettings.html
  * Since these settings are 'internal' I have no problem with them *not* being supported
  *  between various Viking versions, should one switch to different API/storage methods.
  * Indeed even the internal settings themselves can be liable to change.
  */
#include <glib.h>
#include "dir.h"

static GKeyFile *keyfile;

#define VIKING_INI_FILE "viking.ini"

static gboolean settings_load_from_file()
{
	GKeyFileFlags flags = G_KEY_FILE_KEEP_COMMENTS;

	GError *error = NULL;

	gchar *fn = g_build_filename ( a_get_viking_dir(), VIKING_INI_FILE, NULL );

	if ( !g_key_file_load_from_file ( keyfile, fn, flags, &error ) ) {
		g_warning ( "%s: %s", error->message, fn );
		g_free ( fn );
		g_error_free ( error );
		return FALSE;
	}

	g_free ( fn );

	return TRUE;
}

void a_settings_init()
{
	keyfile = g_key_file_new();
	settings_load_from_file();
}

/**
 * a_settings_uninit:
 *
 *  ATM: The only time settings are saved is on program exit
 *   Could change this to occur on window exit or dialog exit or have memory hash of values...?
 */
void a_settings_uninit()
{
	GError *error = NULL;
	gchar *fn = g_build_filename ( a_get_viking_dir(), VIKING_INI_FILE, NULL );
	gsize size;

	gchar *keyfilestr = g_key_file_to_data ( keyfile, &size, &error );

	if ( error ) {
		g_warning ( "%s", error->message );
		g_error_free ( error );
		goto tidy;
	}

	g_file_set_contents ( fn, keyfilestr, size, &error );
	if ( error ) {
		g_warning ( "%s: %s", error->message, fn );
		g_error_free ( error );
	}

	g_key_file_free ( keyfile );
 tidy:
	g_free ( keyfilestr );
	g_free ( fn );
}

// ATM, can't see a point in having any more than one group for various settings
#define VIKING_SETTINGS_GROUP "viking"

static gboolean settings_get_boolean ( const gchar *group, const gchar *name, gboolean *val )
{
	GError *error = NULL;
	gboolean success = TRUE;
	gboolean bb = g_key_file_get_boolean ( keyfile, group, name, &error );
	if ( error ) {
		// Only print on debug - as often may have requests for keys not in the file
		g_debug ( "%s", error->message );
		g_error_free ( error );
		success = FALSE;
	}
	*val = bb;
	return success;
}

gboolean a_settings_get_boolean ( const gchar *name, gboolean *val )
{
	return settings_get_boolean ( VIKING_SETTINGS_GROUP, name, val );
}

void a_settings_set_boolean ( const gchar *name, gboolean val )
{
	g_key_file_set_boolean ( keyfile, VIKING_SETTINGS_GROUP, name, val );
}

static gboolean settings_get_string ( const gchar *group, const gchar *name, gchar **val )
{
	GError *error = NULL;
	gboolean success = TRUE;
	gchar *str = g_key_file_get_string ( keyfile, group, name, &error );
	if ( error ) {
		// Only print on debug - as often may have requests for keys not in the file
		g_debug ( "%s", error->message );
		g_error_free ( error );
		success = FALSE;
	}
	*val = str;
	return success;
}

gboolean a_settings_get_string ( const gchar *name, gchar **val )
{
	return settings_get_string ( VIKING_SETTINGS_GROUP, name, val );
}

void a_settings_set_string ( const gchar *name, const gchar *val )
{
	g_key_file_set_string ( keyfile, VIKING_SETTINGS_GROUP, name, val );
}

static gboolean settings_get_integer ( const gchar *group, const gchar *name, gint *val )
{
	GError *error = NULL;
	gboolean success = TRUE;
	gint ii = g_key_file_get_integer ( keyfile, group, name, &error );
	if ( error ) {
		// Only print on debug - as often may have requests for keys not in the file
		g_debug ( "%s", error->message );
		g_error_free ( error );
		success = FALSE;
	}
	*val = ii;
	return success;
}

gboolean a_settings_get_integer ( const gchar *name, gint *val )
{
	return settings_get_integer ( VIKING_SETTINGS_GROUP, name, val );
}

void a_settings_set_integer ( const gchar *name, gint val )
{
	g_key_file_set_integer ( keyfile, VIKING_SETTINGS_GROUP, name, val );
}

static gboolean settings_get_double ( const gchar *group, const gchar *name, gdouble *val )
{
	GError *error = NULL;
	gboolean success = TRUE;
	gdouble dd = g_key_file_get_double ( keyfile, group, name, &error );
	if ( error ) {
		// Only print on debug - as often may have requests for keys not in the file
		g_debug ( "%s", error->message );
		g_error_free ( error );
		success = FALSE;
	}
	*val = dd;
	return success;
}

gboolean a_settings_get_double ( const gchar *name, gdouble *val )
{
	return settings_get_double ( VIKING_SETTINGS_GROUP, name, val );
}

void a_settings_set_double ( const gchar *name, gdouble val )
{
	g_key_file_set_double ( keyfile, VIKING_SETTINGS_GROUP, name, val );
}

static gboolean settings_get_integer_list ( const gchar *group, const gchar *name, gint **vals, gsize *length )
{
	GError *error = NULL;
	gboolean success = TRUE;
	gint *ints = g_key_file_get_integer_list ( keyfile, group, name, length, &error );
	if ( error ) {
		// Only print on debug - as often may have requests for keys not in the file
		g_debug ( "%s", error->message );
		g_error_free ( error );
		success = FALSE;
	}
	*vals = ints;
	return success;
}

/*
 * The returned list of integers should be freed when no longer needed
 */
static gboolean a_settings_get_integer_list ( const gchar *name, gint **vals, gsize* length )
{
	return settings_get_integer_list ( VIKING_SETTINGS_GROUP, name, vals, length );
}

static void a_settings_set_integer_list ( const gchar *name, gint vals[], gsize length )
{
	g_key_file_set_integer_list ( keyfile, VIKING_SETTINGS_GROUP, name, vals, length );
}

gboolean a_settings_get_integer_list_contains ( const gchar *name, gint val )
{
	gint* vals = NULL;
	gsize length;
	// Get current list and see if the value supplied is in the list
	gboolean contains = FALSE;
	// Get current list
	if ( a_settings_get_integer_list ( name, &vals, &length ) ) {
		// See if it's not already there
		gint ii = 0;
		if ( vals && length ) {
			while ( ii < length ) {
				if ( vals[ii] == val ) {
					contains = TRUE;
					break;
				}
				ii++;
			}
			// Free old array
			g_free (vals);
		}
	}
	return contains;
}

void a_settings_set_integer_list_containing ( const gchar *name, gint val )
{
	gint* vals = NULL;
	gsize length;
	gboolean need_to_add = TRUE;
	gint ii = 0;
	// Get current list
	if ( a_settings_get_integer_list ( name, &vals, &length ) ) {
		// See if it's not already there
		if ( vals && length ) {
			while ( ii < length ) {
				if ( vals[ii] == val ) {
					need_to_add = FALSE;
					break;
				}
				ii++;
			}
		}
	}
	// Add value into array if necessary
	if ( need_to_add ) {
		// NB not bothering to sort this 'list' ATM as there is not much to be gained
		guint new_length = length + 1;
		gint new_vals[new_length];
		// Copy array
		for ( ii = 0; ii < length; ii++ ) {
			new_vals[ii] = vals[ii];
		}
		new_vals[length] = val; // Set the new value
		// Apply
		a_settings_set_integer_list ( name, new_vals, new_length );
		// Free old array
		g_free (vals);
	}
}
