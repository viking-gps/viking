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

#ifndef __VIKING_VIKING_H
#define __VIKING_VIKING_H

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#include <time.h>

#ifdef WINDOWS
#include <io.h>
#include <winsock.h>
#endif

#include "config.h"

#include "globals.h"
#include "coords.h"
#include "vikcoord.h"
#include "vik_compat.h"
#include "download.h"
#include "vikwaypoint.h"
#include "viktrack.h"
#include "vikviewport.h"
#include "viktreeview.h"
#include "viklayer.h"
#include "viklayer_defaults.h"
#include "vikaggregatelayer.h"
#include "viklayerspanel.h"
#include "vikcoordlayer.h"
#include "vikgeoreflayer.h"
#include "vikstatus.h"
#include "vikfileentry.h"
#include "viktrwlayer.h"
#include "vikgpslayer.h"
#ifdef HAVE_LIBMAPNIK
#include "vikmapniklayer.h"
#endif
#include "clipboard.h"
#include "dialog.h"
#include "file.h"
#include "fileutils.h"
#include "vikwindow.h"
#include "gpspoint.h"
#include "gpsmapper.h"
#include "settings.h"
#include "util.h"

#endif
