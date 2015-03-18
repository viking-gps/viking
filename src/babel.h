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
#include "download.h"

G_BEGIN_DECLS

/**
 * BabelProgressCode:
 * @BABEL_DIAG_OUTPUT: a line of diagnostic output is available. The pointer is to a 
 *                     NULL-terminated line of diagnostic output from gpsbabel.
 * @BABEL_DONE: gpsbabel finished, or %NULL if no callback is needed.
 *
 * Used when calling #BabelStatusFunc.
 */
typedef enum {
  BABEL_DIAG_OUTPUT,
  BABEL_DONE,
} BabelProgressCode;

/**
 * BabelStatusFunc:
 *
 * Callback function.
 */
typedef void (*BabelStatusFunc)(BabelProgressCode, gpointer, gpointer);

/**
 * BabelMode:
 * 
 * Store the Read/Write support offered by gpsbabel for a given format.
 */
typedef struct {
    unsigned waypointsRead : 1;
    unsigned waypointsWrite : 1;
    unsigned tracksRead : 1;
    unsigned tracksWrite : 1;
    unsigned routesRead : 1;
    unsigned routesWrite : 1;
} BabelMode;

/**
 * BabelDevice:
 * @name: gpsbabel's identifier of the device
 * @label: human readable label
 * 
 * Representation of a supported device.
 */
typedef struct {
    BabelMode mode;
    gchar *name;
    gchar *label;
} BabelDevice;

/**
 * BabelFile:
 * @name: gpsbabel's identifier of the format
 * @ext: file's extension for this format
 * @label: human readable label
 * 
 * Representation of a supported file format.
 */
typedef struct {
    BabelMode mode;
    gchar *name;
    gchar *ext;
    gchar *label;
} BabelFile;

GList *a_babel_file_list;
GList *a_babel_device_list;

void a_babel_foreach_file_with_mode (BabelMode mode, GFunc func, gpointer user_data);
void a_babel_foreach_file_read_any (GFunc func, gpointer user_data);

gboolean a_babel_convert( VikTrwLayer *vt, const char *babelargs, BabelStatusFunc cb, gpointer user_data, gpointer options );
gboolean a_babel_convert_from_filter( VikTrwLayer *vt, const char *babelargs, const char *file, const char *babelfilters, BabelStatusFunc cb, gpointer user_data, gpointer options );
gboolean a_babel_convert_from( VikTrwLayer *vt, const char *babelargs, const char *file, BabelStatusFunc cb, gpointer user_data, gpointer options );
gboolean a_babel_convert_from_shellcommand ( VikTrwLayer *vt, const char *input_cmd, const char *input_file_type, BabelStatusFunc cb, gpointer user_data, gpointer options );
gboolean a_babel_convert_from_url_filter ( VikTrwLayer *vt, const char *url, const char *input_type, const char *filter, BabelStatusFunc cb, gpointer user_data, DownloadMapOptions *options );
gboolean a_babel_convert_from_url ( VikTrwLayer *vt, const char *url, const char *input_type, BabelStatusFunc cb, gpointer user_data, DownloadMapOptions *options );
gboolean a_babel_convert_from_url_or_shell ( VikTrwLayer *vt, const char *input, const char *input_type, BabelStatusFunc cb, gpointer user_data, DownloadMapOptions *options );
gboolean a_babel_convert_to( VikTrwLayer *vt, VikTrack *track, const char *babelargs, const char *file, BabelStatusFunc cb, gpointer user_data );

void a_babel_init ();
void a_babel_post_init ();
void a_babel_uninit ();

gboolean a_babel_available ();

G_END_DECLS

#endif
