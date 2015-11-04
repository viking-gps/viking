/*
 *    Viking - GPS data editor
 *    Copyright (C) 2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *    Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
 *    Based on:
 *    Copyright (C) 2003-2007, Leandro A. F. Pereira <leandro@linuxmag.com.br>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, version 2.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */
 /*
  * Dependencies must be just on Glib
  * see ui_utils for thing that depend on Gtk
  * see vikutils for things that further depend on other Viking types
  */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include "util.h"
#include "globals.h"

#ifdef WINDOWS
#include <windows.h>
#else
#include <unistd.h>
#endif

guint util_get_number_of_cpus ()
{
#if GLIB_CHECK_VERSION (2, 36, 0)
  return g_get_num_processors();
#else
  long nprocs = 1;
#ifdef WINDOWS
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  nprocs = info.dwNumberOfProcessors;
#else
#ifdef _SC_NPROCESSORS_ONLN
  nprocs = sysconf(_SC_NPROCESSORS_ONLN);
  if (nprocs < 1)
    nprocs = 1;
#endif
#endif
  return nprocs;
#endif
}

gchar *uri_escape(gchar *str)
{
  gchar *esc_str = g_malloc(3*strlen(str));
  gchar *dst = esc_str;
  gchar *src;

  for (src = str; *src; src++) {
    if (*src == ' ')
     *dst++ = '+';
    else if (g_ascii_isalnum(*src))
     *dst++ = *src;
    else {
      g_sprintf(dst, "%%%02hhX", *src);
      dst += 3;
    }
  }
  *dst = '\0';

  return(esc_str);
}


GList * str_array_to_glist(gchar* data[])
{
  GList *gl = NULL;
  gpointer * p;
  for (p = (gpointer)data; *p; p++)
    gl = g_list_prepend(gl, *p);
  return g_list_reverse(gl);
}

/**
 * split_string_from_file_on_equals:
 *
 * @buf: the input string
 * @key: newly allocated string that is before the '='
 * @val: newly allocated string after the '='
 *
 * Designed for file line processing, so it ignores strings beginning with special
 *  characters, such as '#'; returns false in such situations.
 * Also returns false if no equals character is found
 *
 * e.g. if buf = "GPS.parameter=42"
 *   key = "GPS.parameter"
 *   val = "42"
 */
gboolean split_string_from_file_on_equals ( const gchar *buf, gchar **key, gchar **val )
{
  // comments, special characters in viking file format
  if ( buf == NULL || buf[0] == '\0' || buf[0] == '~' || buf[0] == '=' || buf[0] == '#' )
    return FALSE;

  if ( ! strchr ( buf, '=' ) )
    return FALSE;

  gchar **strv = g_strsplit ( buf, "=", 2 );

  gint gi = 0;
  gchar *str = strv[gi];
  while ( str ) {
	if ( gi == 0 )
	  *key = g_strdup ( str );
	else
	  *val = g_strdup ( str );
    gi++;
    str = strv[gi];
  }

  g_strfreev ( strv );

  // Remove newline from val and also any other whitespace
  *key = g_strstrip ( *key );
  *val = g_strstrip ( *val );
  return TRUE;
}

static GSList* deletion_list = NULL;

/**
 * util_add_to_deletion_list:
 *
 * Add a name of a file into the list that is to be deleted on program exit
 * Normally this is for files that get used asynchronously,
 *  so we don't know when it's time to delete them - other than at this program's end
 */
void util_add_to_deletion_list ( const gchar* filename )
{
	deletion_list = g_slist_append ( deletion_list, g_strdup (filename) );
}

/**
 * util_remove_all_in_deletion_list:
 *
 * Delete all the files in the deletion list
 * This should only be called on program exit
 */
void util_remove_all_in_deletion_list ( void )
{
	while ( deletion_list )
	{
		if ( g_remove ( (const char*)deletion_list->data ) )
			g_warning ( "%s: Failed to remove %s", __FUNCTION__, (char*)deletion_list->data );
		g_free ( deletion_list->data );
		deletion_list = g_slist_delete_link ( deletion_list, deletion_list );
	}
}

/**
 *  Removes characters from a string, in place.
 *
 *  @param string String to search.
 *  @param chars Characters to remove.
 *
 *  @return @a string - return value is only useful when nesting function calls, e.g.:
 *  @code str = utils_str_remove_chars(g_strdup("f_o_o"), "_"); @endcode
 *
 *  @see @c g_strdelimit.
 **/
gchar *util_str_remove_chars(gchar *string, const gchar *chars)
{
	const gchar *r;
	gchar *w = string;

	g_return_val_if_fail(string, NULL);
	if (G_UNLIKELY(EMPTY(chars)))
		return string;

	foreach_str(r, string)
	{
		if (!strchr(chars, *r))
			*w++ = *r;
	}
	*w = 0x0;
	return string;
}

/**
 * In 'extreme' debug mode don't remove temporary files
 *  thus the contents can be inspected if things go wrong
 *  with the trade off the user may need to delete tmp files manually
 * Only use this for 'occasional' downloaded temporary files that need interpretation,
 *  rather than large volume items such as Bing attributions.
 */
int util_remove ( const gchar *filename )
{
	if ( vik_debug && vik_verbose ) {
		g_warning ( "Not removing file: %s", filename );
		return 0;
	}
	else
		return g_remove ( filename );
}
