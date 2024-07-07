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

/*
 * Large (and important) sections of this file were adapted from
 * ROX-Filer source code, Copyright (C) 2003, the ROX-Filer team,
 * originally licensed under the GPL v2 or greater (as above).
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>
#include "viking.h"
#include "thumbnails.h"
#include "icons/icons.h"
#include "md5_hash.h"

#ifdef __CYGWIN__
#ifdef __CYGWIN_USE_BIG_TYPES__
#define ST_SIZE_FMT "%lld"
#else
#define ST_SIZE_FMT "%ld"
#endif
#else
/* FIXME -- on some systems this may need to me "lld", see ROX-Filer code */
#define ST_SIZE_FMT "%ld"
#endif

static gchar* thumb_dir = NULL;

#ifdef WINDOWS
static void set_thumb_dir ()
{
  thumb_dir = g_strconcat ( g_get_home_dir(), "\\THUMBNAILS\\normal\\", NULL ) ;
}
#else
static void set_thumb_dir ()
{
  const gchar *xdg_cache_home = g_getenv("XDG_CACHE_HOME");
  if ( xdg_cache_home && g_strcmp0 (xdg_cache_home, "") != 0 )
    thumb_dir = g_strconcat ( xdg_cache_home, "/thumbnails/normal/", NULL );
  else
    thumb_dir = g_strconcat ( g_get_home_dir(), "/.cache/thumbnails/normal/", NULL );
}
#endif

#define PIXMAP_THUMB_SIZE  128

static GdkPixbuf *save_thumbnail(const char *pathname, GdkPixbuf *full);
static GdkPixbuf *child_create_thumbnail(const gchar *path);

gboolean a_thumbnails_exists ( const gchar *filename )
{
  GdkPixbuf *pixbuf = a_thumbnails_get(filename);
  if ( pixbuf )
  {
    g_object_unref ( G_OBJECT ( pixbuf ) );
    return TRUE;
  }
  return FALSE;
}

GdkPixbuf *a_thumbnails_get_default ()
{
  return ui_get_icon ( "thumbnails", 128 );
}

/* filename must be absolute. you could have a function to make sure it exists and absolutize it */

void a_thumbnails_create(const gchar *filename)
{
  GdkPixbuf *pixbuf = a_thumbnails_get(filename);

  if ( ! pixbuf )
    pixbuf = child_create_thumbnail(filename);

  if ( pixbuf )
    g_object_unref (  G_OBJECT ( pixbuf ) );
}

GdkPixbuf *a_thumbnails_scale_pixbuf(GdkPixbuf *src, int max_w, int max_h)
{
	int	w, h;

	w = gdk_pixbuf_get_width(src);
	h = gdk_pixbuf_get_height(src);

	if (w <= max_w && h <= max_h)
	{
		g_object_ref ( G_OBJECT ( src ) );
		return src;
	}
	else
	{
		float scale_x = ((float) w) / max_w;
		float scale_y = ((float) h) / max_h;
		float scale = MAX(scale_x, scale_y);
		int dest_w = w / scale;
		int dest_h = h / scale;

		return gdk_pixbuf_scale_simple(src,
						MAX(dest_w, 1),
						MAX(dest_h, 1),
						GDK_INTERP_BILINEAR);
	}
}

static GdkPixbuf *child_create_thumbnail(const gchar *path)
{
	GdkPixbuf *image, *tmpbuf;

	image = gdk_pixbuf_new_from_file(path, NULL);
	if (!image)
		return NULL;

	tmpbuf = gdk_pixbuf_apply_embedded_orientation(image);
	g_object_unref(G_OBJECT(image));
	image = tmpbuf;

	if (image)
	{
		GdkPixbuf *thumb = save_thumbnail(path, image);
		g_object_unref ( G_OBJECT ( image ) );
		return thumb;
	}

	return NULL;
}

static GdkPixbuf *save_thumbnail(const char *pathname, GdkPixbuf *full)
{
	struct stat info;
	gchar *path;
	int original_width, original_height;
	const gchar* orientation;
	GString *to;
	char *md5, *swidth, *sheight, *ssize, *smtime, *uri;
	mode_t old_mask;
	int name_len;
	GdkPixbuf *thumb;

	if (stat(pathname, &info) != 0)
		return NULL;

	thumb = a_thumbnails_scale_pixbuf(full, PIXMAP_THUMB_SIZE, PIXMAP_THUMB_SIZE);

	orientation = gdk_pixbuf_get_option (full, "orientation");

	original_width = gdk_pixbuf_get_width(full);
	original_height = gdk_pixbuf_get_height(full);


	swidth = g_strdup_printf("%d", original_width);
	sheight = g_strdup_printf("%d", original_height);
	ssize = g_strdup_printf(ST_SIZE_FMT, info.st_size);
	smtime = g_strdup_printf("%ld", (long) info.st_mtime);

	path = file_realpath_dup(pathname);
	uri = g_strconcat("file://", path, NULL);
	md5 = md5_hash(uri);
	g_free(path);

	to = g_string_new ( thumb_dir );
	if ( g_mkdir_with_parents(to->str, 0700) != 0 )
		g_warning ("%s: Failed to mkdir %s", __FUNCTION__, to->str );
	g_string_append(to, md5);
	name_len = to->len + 4; /* Truncate to this length when renaming */
#ifdef WINDOWS
	g_string_append_printf(to, ".png.Viking");
#else
	g_string_append_printf(to, ".png.Viking-%ld", (long) getpid());
#endif

	g_free(md5);

	// Thumb::URI must be in ISO-8859-1 encoding otherwise gdk_pixbuf_save() will fail
	// - e.g. if characters such as 'ě' are encountered
	// Also see http://en.wikipedia.org/wiki/ISO/IEC_8859-1
	// ATM GLIB Manual doesn't specify in which version this function became available
	//  find out that it's fairly recent so may break builds without this test
#if GLIB_CHECK_VERSION(2,40,0)
	char *thumb_uri = g_str_to_ascii ( uri, NULL );
#else
	char *thumb_uri = g_strdup ( uri );
#endif
	old_mask = umask(0077);
	GError *error = NULL;
	gdk_pixbuf_save(thumb, to->str, "png", &error,
	                "tEXt::Thumb::Image::Width", swidth,
	                "tEXt::Thumb::Image::Height", sheight,
	                "tEXt::Thumb::Size", ssize,
	                "tEXt::Thumb::MTime", smtime,
	                "tEXt::Thumb::URI", thumb_uri,
	                "tEXt::Software", PROJECT,
	                "tEXt::Software::Orientation", orientation ? orientation : "0",
	                NULL);
	umask(old_mask);
	g_free(thumb_uri);

	if (error) {
		g_warning ( "%s::%s", __FUNCTION__, error->message );
		g_error_free ( error );
		g_object_unref ( G_OBJECT(thumb) );
		thumb = NULL; /* return NULL */
	}
	else
	/* We create the file ###.png.Viking-PID and rename it to avoid
	 * a race condition if two programs create the same thumb at
	 * once.
	 */
	{
		gchar *final;

		final = g_strndup(to->str, name_len);
		if (rename(to->str, final))
		{
			g_warning("Failed to rename '%s' to '%s': %s",
				  to->str, final, g_strerror(errno));
			g_object_unref ( G_OBJECT(thumb) );
			thumb = NULL; /* return NULL */
		}

		g_free(final);
	}

	g_string_free(to, TRUE);
	g_free(swidth);
	g_free(sheight);
	g_free(ssize);
	g_free(smtime);
	g_free(uri);

	return thumb;
}


GdkPixbuf *a_thumbnails_get(const gchar *pathname)
{
	GdkPixbuf *thumb = NULL;
	char *thumb_path, *md5, *uri, *path;
	const char *ssize, *smtime;
	struct stat info;

	path = file_realpath_dup(pathname);
	uri = g_strconcat("file://", path, NULL);
	md5 = md5_hash(uri);
	g_free(uri);

	thumb_path = g_strdup_printf("%s%s.png", thumb_dir, md5);

	g_free(md5);

	thumb = gdk_pixbuf_new_from_file(thumb_path, NULL);
	if (!thumb)
		goto err;

	/* Note that these don't need freeing... */
	ssize = gdk_pixbuf_get_option(thumb, "tEXt::Thumb::Size");
	if (!ssize)
		goto err;

	smtime = gdk_pixbuf_get_option(thumb, "tEXt::Thumb::MTime");
	if (!smtime)
		goto err;

	if (stat(path, &info) != 0)
		goto err;

	if (info.st_mtime != atol(smtime) || info.st_size != atol(ssize))
		goto err;

	goto out;
err:
	if (thumb)
		g_object_unref ( G_OBJECT ( thumb ) );
	thumb = NULL;
out:
	g_free(path);
	g_free(thumb_path);
	return thumb;
}

/*
 * Startup and finish routines
 */

void a_thumbnails_init ()
{
  set_thumb_dir ();
}

void a_thumbnails_uninit ()
{
  g_free ( thumb_dir );
}
