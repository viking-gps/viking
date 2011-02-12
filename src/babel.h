/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2005, Alex Foobarian <foobarian@gmail.com>
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

#ifndef _VIKING_BABEL_H
#define _VIKING_BABEL_H

#include <glib.h>

#include "viktrwlayer.h"

typedef enum {
  BABEL_DIAG_OUTPUT,
  BABEL_DONE,
} BabelProgressCode;

typedef void (*BabelStatusFunc)(BabelProgressCode, gpointer, gpointer);

/*
 * a_babel_convert modifies data in a trw layer using gpsbabel filters.  This routine is synchronous;
 * that is, it will block the calling program until the conversion is done.  To avoid blocking, call
 * this routine from a worker thread.  The arguments are as follows:
 * 
 * vt              The TRW layer to modify.  All data will be deleted, and replaced by what gpsbabel outputs.
 * 
 * babelargs       A string containing gpsbabel command line filter options.  No file types or names should
 *                 be specified.
 * 
 * cb		   A callback function, called with the following status codes:
 *                   BABEL_DIAG_OUTPUT: a line of diagnostic output is available.  The pointer is to a 
 *                                      NUL-terminated line of diagnostic output from gpsbabel.
 *                   BABEL_DIAG_DONE: gpsbabel finished,
 *                 or NULL if no callback is needed.
 */
gboolean a_babel_convert( VikTrwLayer *vt, const char *babelargs, BabelStatusFunc cb, gpointer user_data );

/*
 * a_babel_convert_from loads data into a trw layer from a file, using gpsbabel.  This routine is synchronous;
 * that is, it will block the calling program until the conversion is done.  To avoid blocking, call
 * this routine from a worker thread. The arguments are as follows:
 * 
 * vt              The TRW layer to place data into.  Duplicate items will be overwritten. 
 * 
 * babelargs       A string containing gpsbabel command line options.  In addition to any filters, this string
 *                 must include the input file type (-i) option. 
 * 
 * cb		   Optional callback function. Same usage as in a_babel_convert.
 */
gboolean a_babel_convert_from( VikTrwLayer *vt, const char *babelargs, BabelStatusFunc cb, const char *file, gpointer user_data );
gboolean a_babel_convert_from_shellcommand ( VikTrwLayer *vt, const char *input_cmd, const char *input_file_type, BabelStatusFunc cb, gpointer user_data );
gboolean a_babel_convert_from_url ( VikTrwLayer *vt, const char *url, const char *input_type, BabelStatusFunc cb, gpointer user_data );
gboolean a_babel_convert_to( VikTrwLayer *vt, const char *babelargs, BabelStatusFunc cb, const char *file, gpointer user_data );


#endif
