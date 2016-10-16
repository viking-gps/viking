/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  This file is part of the GtkHTML library.
 *
 *  Copyright 1999, 2000 Helix Code, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
*/
/*
 * Viking note: This is an extract from GtkHTML 4.10.0 for URI functions
 */
#include "gtkhtml-private.h"
#include <string.h>

gchar *
gtk_html_filename_from_uri (const gchar *uri)
{
	const gchar *relative_fpath;
	gchar *temp_uri, *temp_filename;
	gchar *retval;

	if (!uri || !*uri)
		return NULL;

	if (g_ascii_strncasecmp (uri, "file://", 7) == 0)
		return g_filename_from_uri (uri, NULL, NULL);

	if (g_ascii_strncasecmp (uri, "file:", 5) == 0) {
		/* Relative (file or other) URIs shouldn't contain the
		 * scheme prefix at all. But accept such broken URIs
		 * anyway. Whether they are URI-encoded or not is
		 * anybody's guess, assume they are.
		 */
		relative_fpath = uri + 5;
	} else {
		/* A proper relative file URI. Just do the URI-decoding. */
		relative_fpath = uri;
	}

	if (g_path_is_absolute (relative_fpath)) {
		/* The totally broken case of "file:" followed
		 * directly by an absolute pathname.
		 */
		/* file:/foo/bar.zap or file:c:/foo/bar.zap */
#ifdef G_OS_WIN32
		if (g_ascii_isalpha (relative_fpath[0]) && relative_fpath[1] == ':')
			temp_uri = g_strconcat ("file:///", relative_fpath, NULL);
		else
			temp_uri = g_strconcat ("file://", relative_fpath, NULL);
#else
		temp_uri = g_strconcat ("file://", relative_fpath, NULL);
#endif
		retval = g_filename_from_uri (temp_uri, NULL, NULL);
		g_free (temp_uri);

		return retval;
	}

	/* Create a dummy absolute file: URI and call
	 * g_filename_from_uri(), then strip off the dummy
	 * prefix.
	 */
#ifdef G_OS_WIN32
	if (g_ascii_isalpha (relative_fpath[0]) && relative_fpath[1] == ':') {
		/* file:c:relative/path/foo.bar */
		gchar drive_letter = relative_fpath[0];

		temp_uri = g_strconcat ("file:///dummy/", relative_fpath + 2, NULL);
		temp_filename = g_filename_from_uri (temp_uri, NULL, NULL);
		g_free (temp_uri);

		if (temp_filename == NULL)
			return NULL;

		g_assert (strncmp (temp_filename, G_DIR_SEPARATOR_S "dummy" G_DIR_SEPARATOR_S, 7) == 0);

		retval = g_strdup_printf ("%c:%s", drive_letter, temp_filename + 7);
		g_free (temp_filename);

		return retval;
	}
#endif
	temp_uri = g_strconcat ("file:///dummy/", relative_fpath, NULL);
	temp_filename = g_filename_from_uri (temp_uri, NULL, NULL);
	g_free (temp_uri);

	if (temp_filename == NULL)
		return NULL;

	g_assert (strncmp (temp_filename, G_DIR_SEPARATOR_S "dummy" G_DIR_SEPARATOR_S, 7) == 0);

	retval = g_strdup (temp_filename + 7);
	g_free (temp_filename);

	return retval;
}

gchar *
gtk_html_filename_to_uri (const gchar *filename)
{
	gchar *fake_filename, *fake_uri, *retval;
	const gchar dummy_prefix[] = "file:///dummy/";
	const gint dummy_prefix_len = sizeof (dummy_prefix) - 1;
#ifdef G_OS_WIN32
	gchar drive_letter = 0;
#else
	gchar *first_end, *colon;
#endif

	if (!filename || !*filename)
		return NULL;

	if (g_path_is_absolute (filename))
		return g_filename_to_uri (filename, NULL, NULL);

	/* filename is a relative path, and the corresponding URI is
	 * filename as such but URI-escaped. Instead of yet again
	 * copy-pasteing the URI-escape code from gconvert.c (or
	 * somewhere else), prefix a fake top-level directory to make
	 * it into an absolute path, call g_filename_to_uri() to turn it
	 * into a full file: URI, and then strip away the file:/// and
	 * the fake top-level directory.
	 */

#ifdef G_OS_WIN32
	if (g_ascii_isalpha (*filename) && filename[1] == ':') {
		/* A non-absolute path, but with a drive letter. Ugh. */
		drive_letter = *filename;
		filename += 2;
	}
#endif
	fake_filename = g_build_filename ("/dummy", filename, NULL);
	fake_uri = g_filename_to_uri (fake_filename, NULL, NULL);
	g_free (fake_filename);

	if (fake_uri == NULL)
		return NULL;

	g_assert (strncmp (fake_uri, dummy_prefix, dummy_prefix_len) == 0);

#ifdef G_OS_WIN32
	/* Re-insert the drive letter if we had one. Double ugh.
	 * URI-encode the colon so the drive letter isn't taken for a
	 * URI scheme!
	 */
	if (drive_letter)
		retval = g_strdup_printf ("%c%%3a%s",
					  drive_letter,
					  fake_uri + dummy_prefix_len);
	else
		retval = g_strdup (fake_uri + dummy_prefix_len);
#else
	retval = g_strdup (fake_uri + dummy_prefix_len);
#endif
	g_free (fake_uri);

#ifdef G_OS_UNIX
	/* Check if there are colons in the first component of the
	 * pathname, and URI-encode them so that the part up to the
	 * colon isn't taken for a URI scheme name! This isn't
	 * necessary on Win32 as there can't be colons in a file name
	 * in the first place.
	 */
	first_end = strchr (retval, '/');
	if (first_end == NULL)
		first_end = retval + strlen (retval);

	while ((colon = strchr (retval, ':')) != NULL && colon < first_end) {
		gchar *new_retval = g_malloc (strlen (retval) + 3);

		strncpy (new_retval, retval, colon - retval);
		strcpy (new_retval + (colon - retval), "%3a");
		strcpy (new_retval + (colon - retval) + 3, colon + 1);

		g_free (retval);
		retval = new_retval;
	}
#endif

	return retval;
}
