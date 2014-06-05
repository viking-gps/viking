/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (c) 2014, Rob Norris <rw_norris@hotmail.com>
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
#ifndef _VIKING_TRWLAYER_WPWIN_H
#define _VIKING_TRWLAYER_WPWIN_H

#include <glib.h>
#include <gtk/gtk.h>
#include "viktrwlayer.h"

G_BEGIN_DECLS

/* Specify if a new waypoint or not */
/* If a new waypoint then it uses the default_name for the suggested name allowing the user to change it.
    The name to use is returned
   When an existing waypoint the name is shown but is not allowed to be changed and NULL is returned
 */
gchar *a_dialog_waypoint ( GtkWindow *parent, gchar *default_name, VikTrwLayer *vtl, VikWaypoint *wp, VikCoordMode coord_mode, gboolean is_new, gboolean *updated );

G_END_DECLS

#endif
