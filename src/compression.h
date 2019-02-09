/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2013, Rob Norris <rw_norris@hotmail.com>
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

#ifndef _VIKING_COMPRESSION_H
#define _VIKING_COMPRESSION_H

#include <glib.h>

#include "vikaggregatelayer.h"
#include "vikviewport.h"
#include "file.h"

G_BEGIN_DECLS

void *unzip_file(gchar *zip_file, gulong *unzip_size);

gchar* uncompress_bzip2 ( const gchar *name );

VikLoadType_t uncompress_load_zip_file ( const gchar *filename,
                                         VikAggregateLayer *top,
                                         VikViewport *vp,
                                         VikTrwLayer *vtl,
                                         gboolean new_layer,
                                         gboolean external,
                                         const gchar *dirpath );
                                          VikAggregateLayer *top,
                                          VikViewport *vp,
                                          VikTrwLayer *vtl,
                                          gboolean new_layer,
                                          gboolean external,
                                          const gchar *dirpath );
G_END_DECLS

#endif

