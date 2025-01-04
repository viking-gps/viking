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

/*
 * ATM only for Unix-like systems.
 * Although potentially should work on Windows 10 1803 with GLib 2.72 onwards:
 *  https://gitlab.gnome.org/GNOME/glib/-/issues/2487
 * However unclear ATM what the fallback behaviour of this GIO capability is when run on older versions of Windows
 *  so for the moment leave as disabled on Windows build - especially since not a typical use case for Windows users.
 * The socket allows messages to be sent to the first running instance of Viking.
 *
 * Each command which is sent between two instances and should have the following scheme:
 * command\n
 * data\n
 * data\n
 * ...
 * \n
 * The first thing should be the command name followed by the data belonging to the command.
 * The end of data is a blank line.
 * Each line should be ended with \n.
 *
 * At the moment the only commands supported are 'open' and 'open-external'
 */
#include "socket.h"

static GSocketService *service = NULL;

#ifdef G_OS_UNIX
// Work around mistake in GLIB 2.72 (fixed in 2.73) see:
// https://gitlab.gnome.org/GNOME/glib/-/commit/a638b2bbd11221c3ebb89769ae18a8c3131d47a3
// However would still need a check for GLIB 2.72 (since our minimum is meant to be 2.44)
#if !GLIB_CHECK_VERSION(2,73,0)
#include <gio/gunixsocketaddress.h>
#endif
static GSocketAddress* get_socket_address(void)
{
	// A per user socket name
	gchar *socket_name = g_build_filename ( g_get_user_runtime_dir(), PACKAGE, "command-socket", NULL );
	// Using an abstract socket (vs. a file based one) means:
	// 1. The socket is automatically closed when program ends
	//   i.e. Don't need to manually delete the file when program exits
	//    (which if not done otherwise conflicts on next run)
	// 2. Shows as an understandable name (vs using G_UNIX_SOCKET_ADDRESS_ANONYMOUS)
	//   in system socket / listening output (e.g. as per command 'ss -lx')
	GSocketAddress *gsa = g_unix_socket_address_new_with_type ( socket_name, strlen(socket_name), G_UNIX_SOCKET_ADDRESS_ABSTRACT );
	g_free ( socket_name );
	return gsa;
}

/**
 * The callback to process data received on the socket
 */
static gboolean incoming_callback ( GSocketService *service,
                                    GSocketConnection *connection,
                                    GObject *object,
                                    gpointer user_data )
{
	GError *error = NULL;
	GInputStream *istream = g_io_stream_get_input_stream ( G_IO_STREAM(connection) );
	GDataInputStream *distream = g_data_input_stream_new ( istream );
	gchar *line = NULL;
	gboolean external = FALSE;
	gsize length;

	// First line is the command
	line = g_data_input_stream_read_line ( distream, &length, NULL, &error );
	if ( error ) {
		g_warning ( "%s: %s", __FUNCTION__, error->message );
		g_error_free ( error );
		error = NULL;
	}
	if ( line ) {
		g_debug ( "%s: command is %s", __FUNCTION__, line );

		// Check the command is a supported one and then action it
		if ( g_strcmp0(line, "open-external") == 0 )
			external = TRUE;

		// Any 'open*' command - but meant for 'open' and 'open-external'
		if ( g_str_has_prefix(line, "open") ) {
			g_free ( line );
			do {
				line = g_data_input_stream_read_line ( distream, &length, NULL, &error );
				if ( error ) {
					g_warning ( "%s: %s", __FUNCTION__, error->message );
					g_error_free ( error );
					error = NULL;
				}
				if ( line ) {
					g_debug ( "%s: received: \"%s\"", __FUNCTION__, line );
					if ( g_strcmp0(line, "-") == 0 )
						g_warning ( "Can not open from stdin via socket" );
					else {
						if ( g_file_test(line, G_FILE_TEST_EXISTS) ) {
							VikWindow *vw = VIK_WINDOW(user_data);
							gboolean change_filename = FALSE;
							// .vik files open in their own window
							if ( check_file_magic_vik(line) ) {
								vw = vik_window_new_window();
								change_filename = TRUE;
							}
							vik_window_open_file ( vw, line, change_filename, TRUE, TRUE, FALSE, external );
						} else {
							g_warning ( _("Can not open non-existant file=%s"), line );
						}
					}
					g_free ( line );
				}
			} while ( line );
		}
	}
	return TRUE;
}

/**
 * Attempt to create the Viking socket (and listen to it)
 * Returns whether successful or not
 */
static gboolean create_socket ( VikWindow *vw )
{
	GError *error = NULL;

	GSocketAddress *gsa = get_socket_address ();
	service = g_socket_service_new ();

	gboolean ans =
		g_socket_listener_add_address ( (GSocketListener*)service,
		                                gsa,
		                                G_SOCKET_TYPE_STREAM,
		                                G_SOCKET_PROTOCOL_DEFAULT,
		                                NULL,
		                                NULL,
		                                &error );

	if ( !ans ) {
		g_warning ( "%s: %s", __FUNCTION__, error->message );
		g_error_free ( error );
		return FALSE;
	}

	g_object_unref ( gsa );

	g_signal_connect ( service,
	                   "incoming",
	                   G_CALLBACK(incoming_callback),
	                   vw );

	g_debug ( "%s: starting service", __FUNCTION__ );
	g_socket_service_start ( service );

	return ans;
}

/**
 * Try to connect to an existing listening socket
 *  to see if there is indeed a listening socket
 * Returns whether a successful connection was made or not
 */
static gboolean connect_socket(void)
{
	GError *error = NULL;
	GSocket *gsock = g_socket_new ( G_SOCKET_FAMILY_UNIX,
	                                G_SOCKET_TYPE_STREAM,
	                                G_SOCKET_PROTOCOL_DEFAULT,
	                                &error );
	gboolean ans = FALSE;
	GSocketAddress* gsa = get_socket_address ();
	if ( gsa )
	{
		GSocketConnection *gsc = g_socket_connection_factory_create_connection ( gsock );

		gboolean ans = g_socket_connection_connect ( gsc, gsa, NULL, &error );
		if ( !ans ) {
			g_debug ( "%s: %s", __FUNCTION__, error->message );
			g_error_free ( error );
		}
		g_object_unref ( gsa );
		g_object_unref ( gsc );
	}
	g_object_unref ( gsock );

	return ans;
}
#endif

/**
 * Returns whether successful or not
 */
gboolean socket_init ( VikWindow *vw )
{
#ifdef G_OS_UNIX
	// Create socket if one not already in use
	if ( !connect_socket() ) {
		return create_socket ( vw );
	}
#endif
	return FALSE;
}

/**
 * Any clean up actions if needed
 */
void socket_uninit (void)
{
	if ( service ) {
		g_socket_listener_close ( (GSocketListener*)service );
		g_socket_service_stop ( service );
	}
}

/**
 * Returns whether successful or not
 *  (at least in the sense that the command was sent on the socket;
 *   as the file(s) specified may not exist or be supported types
 *   and the running instance of Viking may reject them)
 */
gboolean socket_open_files ( guint argc, gchar **argv, gboolean external )
{
	gboolean ans = FALSE;
#ifdef G_OS_UNIX

	if ( argc <= 1 ) {
		g_warning ( _("No files specified to open") );
		return FALSE;
	}

	GError *error = NULL;
	GSocket *gsock = g_socket_new ( G_SOCKET_FAMILY_UNIX,
	                                G_SOCKET_TYPE_STREAM,
	                                G_SOCKET_PROTOCOL_DEFAULT,
	                                &error );
	GSocketAddress* gsa = get_socket_address ();
	GSocketConnection *gsc = g_socket_connection_factory_create_connection ( gsock );

	ans = g_socket_connection_connect ( gsc, gsa, NULL, &error );
	if ( error ) {
		g_warning ( "%s: %s", __FUNCTION__, error->message );
		g_error_free ( error );
		error = NULL;
	}

	if ( ans ) {
		GOutputStream *ostream = g_io_stream_get_output_stream (G_IO_STREAM (gsc));
		GDataOutputStream *dostream = g_data_output_stream_new ( ostream );

		// The command
		g_data_output_stream_put_string ( dostream,
		                                  external ? "open-external\n" : "open\n",
		                                  NULL, NULL );
		// The files
		gchar *cwd = g_get_current_dir();
		for (guint ii = 1; ii < argc; ii++)	{
			// Note that we convert any relative file to an absolute file
			//  since the receiving viking instance can (or indeed most likely) have a different cwd
			//  and otherwise won't be able to match the relative path
			gchar *abs_name = util_make_absolute_filename ( argv[ii], cwd );
			gchar *name = g_strdup_printf ( "%s%c", abs_name ? abs_name : argv[ii], '\n' );
			g_data_output_stream_put_string ( dostream,
			                                  name,
			                                  NULL,	NULL );
			g_free ( name );
			g_free ( abs_name );
		}
		g_free ( cwd );

		// The end marker
		g_data_output_stream_put_string ( dostream,
		                                  "",
		                                  NULL, NULL );
	}

	g_object_unref ( gsock );
	g_object_unref ( gsc );
#endif
	return ans;
}
