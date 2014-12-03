/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2007-2009, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
 * Based on:
 * Copyright (C) 2003-2007, Leandro A. F. Pereira <leandro@linuxmag.com.br>
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

#ifndef _VIKING_UTIL_H
#define _VIKING_UTIL_H

#include <glib.h>

G_BEGIN_DECLS

guint util_get_number_of_cpus (void);

gchar *uri_escape(gchar *str);

GList * str_array_to_glist(gchar* data[]);

gboolean split_string_from_file_on_equals ( const gchar *buf, gchar **key, gchar **val );

void util_add_to_deletion_list ( const gchar* filename );
void util_remove_all_in_deletion_list ( void );

gchar *util_str_remove_chars(gchar *string, const gchar *chars);

/** Returns @c TRUE if @a ptr is @c NULL or @c *ptr is @c FALSE. */
#define EMPTY(ptr) \
	(!(ptr) || !*(ptr))

/** Iterates all the nodes in @a list.
 * @param node should be a (@c GList*).
 * @param list @c GList to traverse. */
#define foreach_list(node, list) \
	for (node = list; node != NULL; node = node->next)

/** Iterates all the nodes in @a list.
 * @param node should be a (@c GSList*).
 * @param list @c GSList to traverse. */
#define foreach_slist(node, list) \
	foreach_list(node, list)

/** Iterates through each character in @a string.
 * @param char_ptr Pointer to character.
 * @param string String to traverse.
 * @warning Doesn't include null terminating character. */
#define foreach_str(char_ptr, string) \
	for (char_ptr = string; *char_ptr; char_ptr++)

int util_remove ( const gchar *filename );

G_END_DECLS

#endif
