/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2016 Rob Norris <rw_norris@hotmail.com>
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
#include "md5_hash.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_LIBNETTLE
#include <nettle/md5-compat.h>
#endif

/**
 * Get MD5 hash of a string using a library function
 */
char *md5_hash(const char *message)
{
	char *answer = NULL;
#ifdef HAVE_LIBNETTLE
	unsigned char result[16];
	MD5_CTX ctx;
	MD5Init ( &ctx );
	MD5Update ( &ctx, (const unsigned char*)message, strlen(message) );
	MD5Final ( &result[0], &ctx );
	/*
	g_printf ( "%s: of string '%s' is: ", __FUNCTION__, message );
	for(int i = 0; i < 16; i++)
		g_printf ( "%02x", result[i]);
	g_printf("\n");
	*/
	// Should be nicer way of converting this but I can't think of one ATM
	answer = g_strdup_printf ( "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
	                            result[0],result[1],result[2],result[3],result[4],result[5],result[6],result[7],
	                            result[8],result[9],result[10],result[11],result[12],result[13],result[14],result[15]);
#else
	// Return something that might vaguely work in case you've disabled MD5 support
	//  but you should know what you've doing and uses of it (such as thumbnail caching) now won't work well
	guint value = g_str_hash ( message );
	answer = g_strdup_printf ( "%d", value );
#endif
	return answer;
}
