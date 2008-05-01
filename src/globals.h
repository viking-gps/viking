/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
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

#ifndef __VIKING_GLOBALS_H
#define __VIKING_GLOBALS_H

#include <glib.h>

#define PROJECT "Viking"
#define VIKING_VERSION PACKAGE_VERSION
#define VIKING_VERSION_NAME "This Name For Rent"
#define VIKING_URL "http://viking.sf.net/"

#define ALTI_TO_MPP 1.4017295
#define MPP_TO_ALTI 0.7134044

#define VIK_DEFAULT_ALTITUDE 0.0

#define VIK_GTK_WINDOW_FROM_WIDGET(x) GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(x)))
#define VIK_GTK_WINDOW_FROM_LAYER(x) VIK_GTK_WINDOW_FROM_WIDGET(VIK_LAYER(x)->vt)

#define DEG2RAD 0.017453293
#define RAD2DEG 57.2957795

/* mercator projection, latitude conversion (degrees) */
#define MERCLAT(x) (RAD2DEG * log(tan((0.25 * M_PI) + (0.5 * DEG2RAD * (x)))))
#define DEMERCLAT(x) (RAD2DEG * atan(sinh(DEG2RAD * (x))))

/* Some command line options */
extern gboolean vik_debug;
extern gboolean vik_verbose;
extern gboolean vik_version;

#endif
