/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#ifndef _VIKING_TRWLAYER_EXPORT_H
#define _VIKING_TRWLAYER_EXPORT_H

#include "viktrwlayer.h"

G_BEGIN_DECLS

void vik_trw_layer_export ( VikLayer *vl, const gchar *title, const gchar* default_name, VikTrack* trk, VikFileType_t file_type );

void vik_trw_layer_export_external_gpx ( VikTrwLayer *vtl, const gchar* external_program );

void vik_trw_layer_export_gpsbabel ( VikLayer *vl, const gchar *title, const gchar* default_name, const gchar* gpx_file_name );

void vik_trw_layer_export_init ();

G_END_DECLS

#endif
