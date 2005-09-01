/* gtkcellrendererprogress.h
 * Taken from Gaim 0.77.
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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

#ifndef __GTK_CELL_RENDERER_PROGRESS_H__
#define __GTK_CELL_RENDERER_PROGRESS_H__

#include <gtk/gtkcellrenderer.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_CELL_RENDERER_PROGRESS         (gtk_cell_renderer_progress_get_type())
#define GTK_CELL_RENDERER_PROGRESS(obj)         (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_CELL_RENDERER_PROGRESS, GtkCellRendererProgress))
#define GTK_CELL_RENDERER_PROGRESS_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CELL_RENDERER_PROGRESS, GtkCellRendererProgressClass))
#define GTK_IS_CELL_PROGRESS_PROGRESS(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CELL_RENDERER_PROGRESS))
#define GTK_IS_CELL_PROGRESS_PROGRESS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CELL_RENDERER_PROGRESS))
#define GTK_CELL_RENDERER_PROGRESS_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CELL_RENDERER_PROGRESS, GtkCellRendererProgressClass))

typedef struct _GtkCellRendererProgress GtkCellRendererProgress;
typedef struct _GtkCellRendererProgressClass GtkCellRendererProgressClass;

	struct _GtkCellRendererProgress {
		GtkCellRenderer parent;

		gdouble progress;
		gchar *text;
		gboolean text_set;
	};

	struct _GtkCellRendererProgressClass {
		GtkCellRendererClass parent_class;
	};

	GType            gtk_cell_renderer_progress_get_type     (void);
	GtkCellRenderer  *gtk_cell_renderer_progress_new          (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_CELL_RENDERER_PROGRESS_H__ */
