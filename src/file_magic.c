/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2016, Rob Norris <rw_norris@hotmail.com>
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
#include "file_magic.h"
#include "file.h"
#ifdef HAVE_MAGIC_H
#include <magic.h>
#endif
#include <string.h>
#include <gmodule.h>

/**
 * file_magic_check:
 * @filename:     The file
 * @magic_string: Type of file (this part matched against the magic library response)
 * @extension:    Optional extension to try when Magic Library is not available
 *
 * Returns: Whether the file is of the specified type.
 *  Uses the Magic library if available to determine the file against the supplied type.
 *  Otherwise uses a rudimentary file extension check.
 */
gboolean file_magic_check ( const gchar *filename, const gchar *magic_string, const gchar *extension )
{
	gboolean is_requested_file_type = FALSE;
#ifdef HAVE_MAGIC_H
#ifdef MAGIC_VERSION
	// Or magic_version() if available - probably need libmagic 5.18 or so
	//  (can't determine exactly which version the versioning became available)
	g_debug ("%s: magic version: %d", __FUNCTION__, MAGIC_VERSION );
#endif
	magic_t myt = magic_open ( MAGIC_CONTINUE|MAGIC_ERROR|MAGIC_MIME );
	if ( myt ) {
#ifdef WINDOWS
		// We have to 'package' the magic database ourselves :(
		gchar *dir = g_win32_get_package_installation_directory_of_module ( NULL );
		g_debug ( "%s: Running on Windows - %s", __FUNCTION__, dir );
		//  --> %PROGRAM FILES%\Viking\magic.mgc
		//int ml = magic_load ( myt, ".\\magic.mgc" );
		gchar *mgc = g_build_filename ( dir, "magic.mgc", NULL);

		//int ml = magic_load ( myt, ".\\magic.mgc" );
		int ml = magic_load ( myt, mgc );

		g_free ( mgc );
		g_free ( dir );
#else
		// Use system default
		int ml = magic_load ( myt, NULL );
#endif
		if ( ml == 0 ) {
#ifdef WINDOWS
			GError *err = NULL;
			char *magic_filename = g_locale_from_utf8 ( filename, -1, NULL, NULL, &err );
			if ( err ) {
				g_warning ( "%s: UTF8 issues for '%s' %s", __FUNCTION__, filename, err->message );
				g_error_free ( err );
			}
#else
			char *magic_filename = g_strdup ( filename );
#endif
			const char* magic = magic_file ( myt, magic_filename );
			g_debug ("%s: magic output: %s for file '%s'", __FUNCTION__, magic, magic_filename );
			if ( magic )
				if ( g_ascii_strncasecmp ( magic, magic_string, strlen(magic_string) ) == 0 )
					is_requested_file_type = TRUE;
			g_free ( magic_filename );
		}
		else {
			g_critical ("%s: magic load database failure", __FUNCTION__ );
		}

		magic_close ( myt );
	}
	else
#endif
		is_requested_file_type = a_file_check_ext ( filename, extension );

	return is_requested_file_type;
}
