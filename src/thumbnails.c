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

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "viking.h"
#include "thumbnails.h"
#include "icons/icons.h"

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

#undef MIN /* quit yer whining, gcc */
#undef MAX
#include <sys/param.h> /* for realpath() */
#ifndef MAX
/* We need MAX macro and some system does not offer it */
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#define HOME_DIR g_get_home_dir()

#ifdef WINDOWS
#define THUMB_DIR "\\THUMBNAILS\\" /* viking maps default viking\maps */
#define THUMB_SUB_DIR "normal\\"
#define realpath(X,Y) _fullpath(Y,X,MAX_PATH)

#else
#define THUMB_DIR "/.thumbnails/"
#define THUMB_SUB_DIR "normal/"
#endif

#define PIXMAP_THUMB_SIZE  128

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

static char *md5_hash(const char *message);
static char *pathdup(const char *path);
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
  return gdk_pixbuf_from_pixdata ( &thumbnails_pixbuf, FALSE, NULL );
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
		gdk_pixbuf_ref(src);
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
		gdk_pixbuf_unref ( image );
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

	path = pathdup(pathname);
	uri = g_strconcat("file://", path, NULL);
	md5 = md5_hash(uri);
	g_free(path);
		
	to = g_string_new(HOME_DIR);
	g_string_append(to, THUMB_DIR);
	g_string_append(to, THUMB_SUB_DIR);
	g_mkdir_with_parents(to->str, 0700);
	g_string_append(to, md5);
	name_len = to->len + 4; /* Truncate to this length when renaming */
#ifdef WINDOWS
	g_string_append_printf(to, ".png.Viking");
#else
	g_string_append_printf(to, ".png.Viking-%ld", (long) getpid());
#endif

	g_free(md5);

	old_mask = umask(0077);
	gdk_pixbuf_save(thumb, to->str, "png", NULL,
			"tEXt::Thumb::Image::Width", swidth,
			"tEXt::Thumb::Image::Height", sheight,
			"tEXt::Thumb::Size", ssize,
			"tEXt::Thumb::MTime", smtime,
			"tEXt::Thumb::URI", uri,
			"tEXt::Software", PROJECT,
			"tEXt::Software::Orientation", orientation ? orientation : "0",
			NULL);
	umask(old_mask);

	/* We create the file ###.png.ROX-Filer-PID and rename it to avoid
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

	path = pathdup(pathname);
	uri = g_strconcat("file://", path, NULL);
	md5 = md5_hash(uri);
	g_free(uri);
	
	thumb_path = g_strdup_printf("%s%s%s%s.png", HOME_DIR, THUMB_DIR, THUMB_SUB_DIR, md5);

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
		gdk_pixbuf_unref(thumb);
	thumb = NULL;
out:
	g_free(path);
	g_free(thumb_path);
	return thumb;
}

/* pathdup() stuff */

static char *pathdup(const char *path)
{
	char real[MAXPATHLEN];

	g_return_val_if_fail(path != NULL, NULL);

	if (realpath(path, real))
		return g_strdup(real);

	return g_strdup(path);
}

/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest. The original code was
 * written by Colin Plumb in 1993, and put in the public domain.
 * 
 * Modified to use glib datatypes. Put under GPL to simplify
 * licensing for ROX-Filer. Taken from Debian's dpkg package.
 *
 */

#define md5byte unsigned char

typedef struct _MD5Context MD5Context;

struct _MD5Context {
	guint32 buf[4];
	guint32 bytes[2];
	guint32 in[16];
};

static void MD5Init(MD5Context *ctx);
static void MD5Update(MD5Context *ctx, md5byte const *buf, unsigned len);
static char *MD5Final(MD5Context *ctx);
static void MD5Transform(guint32 buf[4], guint32 const in[16]);

#if G_BYTE_ORDER == G_BIG_ENDIAN
static void byteSwap(guint32 *buf, unsigned words)
{
	md5byte *p = (md5byte *)buf;

	do {
		*buf++ = (guint32)((unsigned)p[3] << 8 | p[2]) << 16 |
			((unsigned)p[1] << 8 | p[0]);
		p += 4;
	} while (--words);
}
#else
#define byteSwap(buf,words)
#endif

/*
 * Start MD5 accumulation. Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
static void MD5Init(MD5Context *ctx)
{
	ctx->buf[0] = 0x67452301;
	ctx->buf[1] = 0xefcdab89;
	ctx->buf[2] = 0x98badcfe;
	ctx->buf[3] = 0x10325476;

	ctx->bytes[0] = 0;
	ctx->bytes[1] = 0;
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
static void MD5Update(MD5Context *ctx, md5byte const *buf, unsigned len)
{
	guint32 t;

	/* Update byte count */

	t = ctx->bytes[0];
	if ((ctx->bytes[0] = t + len) < t)
		ctx->bytes[1]++;	/* Carry from low to high */

	t = 64 - (t & 0x3f);	/* Space available in ctx->in (at least 1) */
	if (t > len) {
		memcpy((md5byte *)ctx->in + 64 - t, buf, len);
		return;
	}
	/* First chunk is an odd size */
	memcpy((md5byte *)ctx->in + 64 - t, buf, t);
	byteSwap(ctx->in, 16);
	MD5Transform(ctx->buf, ctx->in);
	buf += t;
	len -= t;

	/* Process data in 64-byte chunks */
	while (len >= 64) {
		memcpy(ctx->in, buf, 64);
		byteSwap(ctx->in, 16);
		MD5Transform(ctx->buf, ctx->in);
		buf += 64;
		len -= 64;
	}

	/* Handle any remaining bytes of data. */
	memcpy(ctx->in, buf, len);
}

/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern 
 * 1 0* (64-bit count of bits processed, MSB-first)
 * Returns the newly allocated string of the hash.
 */
static char *MD5Final(MD5Context *ctx)
{
	char *retval;
	int i;
	int count = ctx->bytes[0] & 0x3f;	/* Number of bytes in ctx->in */
	md5byte *p = (md5byte *)ctx->in + count;
	guint8	*bytes;

	/* Set the first char of padding to 0x80.  There is always room. */
	*p++ = 0x80;

	/* Bytes of padding needed to make 56 bytes (-8..55) */
	count = 56 - 1 - count;

	if (count < 0) {	/* Padding forces an extra block */
		memset(p, 0, count + 8);
		byteSwap(ctx->in, 16);
		MD5Transform(ctx->buf, ctx->in);
		p = (md5byte *)ctx->in;
		count = 56;
	}
	memset(p, 0, count);
	byteSwap(ctx->in, 14);

	/* Append length in bits and transform */
	ctx->in[14] = ctx->bytes[0] << 3;
	ctx->in[15] = ctx->bytes[1] << 3 | ctx->bytes[0] >> 29;
	MD5Transform(ctx->buf, ctx->in);

	byteSwap(ctx->buf, 4);

	retval = g_malloc(33);
	bytes = (guint8 *) ctx->buf;
	for (i = 0; i < 16; i++)
		sprintf(retval + (i * 2), "%02x", bytes[i]);
	retval[32] = '\0';
	
	return retval;
}

# ifndef ASM_MD5

/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f,w,x,y,z,in,s) \
	 (w += f(x,y,z) + in, w = (w<<s | w>>(32-s)) + x)

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
static void MD5Transform(guint32 buf[4], guint32 const in[16])
{
	register guint32 a, b, c, d;

	a = buf[0];
	b = buf[1];
	c = buf[2];
	d = buf[3];

	MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
	MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
	MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
	MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
	MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
	MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
	MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
	MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
	MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
	MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
	MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
	MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
	MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
	MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
	MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
	MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

	MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
	MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
	MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
	MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
	MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
	MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
	MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
	MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
	MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
	MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
	MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
	MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
	MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
	MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
	MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
	MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

	MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
	MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
	MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
	MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
	MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
	MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
	MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
	MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
	MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
	MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
	MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
	MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
	MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
	MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
	MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
	MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

	MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
	MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
	MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
	MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
	MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
	MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
	MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
	MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
	MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
	MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
	MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
	MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
	MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
	MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
	MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
	MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

	buf[0] += a;
	buf[1] += b;
	buf[2] += c;
	buf[3] += d;
}

# endif /* ASM_MD5 */

static char *md5_hash(const char *message)
{
	MD5Context ctx;

	MD5Init(&ctx);
	MD5Update(&ctx, (md5byte *) message, strlen(message));
	return MD5Final(&ctx);
}
