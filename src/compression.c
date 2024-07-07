/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2008, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2007, Quy Tonthat <qtonthat@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LIBZ
#include <zlib.h>
#endif

#ifdef HAVE_BZLIB_H
#include <bzlib.h>
#endif

#ifdef HAVE_ZIP_H
#include <zip.h>
#endif

#ifdef HAVE_LZMA_H
#include <lzma.h>
#endif

#include "compression.h"
#include "util.h"
#include <string.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

#ifdef HAVE_ZIP_H
/**
 * figure_out_answer:
 *
 * Maintain an 'it worked' status between multiple file loads within a compressed file
 * A compressed file could clearly have GPS & non GPS related files.
 * So simplify the various load types into either 'it worked', 'worked with issues', or 'generally all failed'.
 * However if just 1 file report that particular status regardless.
 */
VikLoadType_t figure_out_answer ( VikLoadType_t current_ans, VikLoadType_t last_ans, gint ii, gint entries )
{
	// Only singular entry - return this status
	VikLoadType_t ans = current_ans;
	if ( ii == 0 && entries == 1 )
		return ans;

	// Something previously had worked, but this one didn't
	if ( last_ans >= LOAD_TYPE_OTHER_FAILURE_NON_FATAL ) {
		if ( ans < LOAD_TYPE_OTHER_FAILURE_NON_FATAL )
			ans = LOAD_TYPE_OTHER_FAILURE_NON_FATAL;
	}
	else {
		// If the final entry - any failures convert to the general failure
		if ( ii == entries-1 ) {
			if ( ans < LOAD_TYPE_OTHER_FAILURE_NON_FATAL )
				ans = LOAD_TYPE_READ_FAILURE;
		}
	}
	return ans;
}
#endif

/**
 * NB is typically called from file.c and circularly calls back into file.c
 * ATM this works OK!
 *
 */
VikLoadType_t uncompress_load_zip_file ( const gchar *filename,
                                         VikAggregateLayer *top,
                                         VikViewport *vp,
                                         VikTrwLayer *vtl,
                                         gboolean new_layer,
                                         gboolean external,
                                         const gchar *dirpath )
{
	VikLoadType_t ans = LOAD_TYPE_READ_FAILURE;
#ifdef HAVE_ZIP_H
// Older libzip compatibility:
#ifndef zip_t
typedef struct zip zip_t;
typedef struct zip_file zip_file_t;
#endif
#ifndef ZIP_RDONLY
#define ZIP_RDONLY 0
#endif

#ifdef WINDOWS
	GError *err = NULL;
	char *zip_filename = g_locale_from_utf8 ( filename, -1, NULL, NULL, &err );
	if ( err ) {
		g_warning ( "%s: UTF8 issues for '%s' %s", __FUNCTION__, filename, err->message );
		g_error_free ( err );
	}
#else
	char *zip_filename = g_strdup ( filename );
#endif
	int zans = ZIP_ER_OK;
	zip_t *archive = zip_open ( zip_filename, ZIP_RDONLY, &zans );
	if ( !archive ) {
		g_warning ( "%s: Unable to open archive: '%s' Error code %d", __FUNCTION__, zip_filename, zans );
		goto cleanup;
	}

	int entries = zip_get_num_entries ( archive, ZIP_FL_UNCHANGED );
	g_debug ( "%s: zip file %s entries %d", __FUNCTION__, zip_filename, entries );
	if ( entries == 0 )
		ans = LOAD_TYPE_OTHER_FAILURE_NON_FATAL;

	struct zip_stat zs;
	for ( int ii = 0; ii < entries; ii++ ) {
		if ( zip_stat_index( archive, ii, 0, &zs ) == 0) {
			zip_file_t *zf = zip_fopen_index ( archive, ii, 0 );
			if ( zf ) {
				char *buffer = g_malloc(zs.size);
				int len = zip_fread ( zf, buffer, zs.size );
				if ( len == zs.size ) {
					VikLoadType_t current_ans = LOAD_TYPE_READ_FAILURE;
#ifdef HAVE_FMEMOPEN
					FILE *ff = fmemopen ( buffer, zs.size, "r" );
					if ( ff ) {
						current_ans = a_file_load_stream ( ff, zs.name, top, vp, vtl, new_layer, external, dirpath, zs.name );
						(void)fclose ( ff );
					}
					else {
						g_warning ( "%s: Unable to load stream: %d in '%s'", __FUNCTION__, ii, zip_filename );
					}
#else
					// For example, Windows doesn't have fmemopen()
					// Fallback to extracting contents to temporary files and then reread back in
					// Not so efficient but should be reliable enough
					gchar *tmp_name = util_write_tmp_file_from_bytes ( buffer, zs.size );
					current_ans = a_file_load ( top, vp, vtl, tmp_name, new_layer, external, zs.name );
					(void)util_remove ( tmp_name );
#endif
					ans = figure_out_answer ( current_ans, ans, ii, entries );
				}
				else {
					g_warning ( "%s: Unable to read index: %d in '%s', got %d, wanted %ld", __FUNCTION__, ii, zip_filename, len, zs.size );
				}
			}
			else {
				g_warning ( "%s: Unable to open index: %d in '%s'", __FUNCTION__, ii, zip_filename );
			}
		}
		else {
			g_warning ( "%s: Unable to stat index: %d in '%s'", __FUNCTION__, ii, zip_filename );
		}
	}
	zip_discard ( archive );

 cleanup:
	g_free ( zip_filename );
#endif
	return ans;
}

#ifdef HAVE_LIBZ
/* return size of unzip data or 0 if failed */
static guint uncompress_data(void *uncompressed_buffer, guint uncompressed_size, void *compressed_data, guint compressed_size)
{
	z_stream stream;
	int err;

	stream.next_in = compressed_data;
	stream.avail_in = compressed_size;
	stream.next_out = uncompressed_buffer;
	stream.avail_out = uncompressed_size;
	stream.zalloc = (alloc_func)0;
	stream.zfree = (free_func)0;
	stream.opaque = (voidpf)0;

	/* negative windowBits to inflateInit2 means "no header" */
	if ((err = inflateInit2(&stream, -MAX_WBITS)) != Z_OK) {
		g_warning("%s(): inflateInit2 failed", __PRETTY_FUNCTION__);
		return 0;
	}

	err = inflate(&stream, Z_FINISH);
	if ((err != Z_OK) && (err != Z_STREAM_END) && stream.msg) {
		g_warning("%s() inflate failed err=%d \"%s\"", __PRETTY_FUNCTION__, err, stream.msg == NULL ? "unknown" : stream.msg);
		inflateEnd(&stream);
		return 0;
	}

	inflateEnd(&stream);
	return(stream.total_out);
}
#endif

/**
 * unzip_file:
 * @zip_file:   pointer to start of compressed data
 * @unzip_size: the size of the compressed data block
 *
 * Returns a pointer to uncompressed data (maybe NULL)
 *  data should be freed once used
 */
void *unzip_file(gchar *zip_file, gulong *unzip_size)
{
	void *unzip_data = NULL;
#ifndef HAVE_LIBZ
	goto end;
#else
	gchar *zip_data;
	// See http://en.wikipedia.org/wiki/Zip_(file_format)
	struct _lfh {
		guint32 sig;
		guint16 extract_version;
		guint16 flags;
		guint16 comp_method;
		guint16 time;
		guint16 date;
		guint32 crc_32;
		guint32 compressed_size;
		guint32 uncompressed_size;
		guint16 filename_len;
		guint16 extra_field_len;
	}  __attribute__ ((gcc_struct,__packed__)) *local_file_header = NULL;

	if ( sizeof(struct _lfh) != 30 ) {
		g_critical ("Incorrect internal zip header size, should be 30 but is %zd", sizeof(struct _lfh) );
		goto end;
	}

	local_file_header = (struct _lfh *) zip_file;
	if (GUINT32_FROM_LE(local_file_header->sig) != 0x04034b50) {
		g_warning("%s(): wrong format (%d)", __PRETTY_FUNCTION__, GUINT32_FROM_LE(local_file_header->sig));
		goto end;
	}

	zip_data = zip_file + sizeof(struct _lfh)
		+ GUINT16_FROM_LE(local_file_header->filename_len)
		+ GUINT16_FROM_LE(local_file_header->extra_field_len);
	gulong uncompressed_size = GUINT32_FROM_LE(local_file_header->uncompressed_size);
	unzip_data = g_malloc(uncompressed_size);

	// Protection against malloc failures
	// ATM not normally been checking malloc failures in Viking but sometimes using zip files can be quite large
	//  (e.g. when using DEMs) so more potential for failure.
	if ( !unzip_data )
		goto end;

	g_debug ("%s: method %d: from size %d to %ld", __FUNCTION__, GUINT16_FROM_LE(local_file_header->comp_method), GUINT32_FROM_LE(local_file_header->compressed_size), uncompressed_size);

	if ( GUINT16_FROM_LE(local_file_header->comp_method) == 0 &&
		(uncompressed_size == GUINT32_FROM_LE(local_file_header->compressed_size)) ) {
		// Stored only - no need to 'uncompress'
		// Thus just copy
		memcpy ( unzip_data, zip_data, uncompressed_size );
		*unzip_size = uncompressed_size;
		goto end;
	}

	if (!(*unzip_size = uncompress_data(unzip_data, uncompressed_size, zip_data, GUINT32_FROM_LE(local_file_header->compressed_size)))) {
		g_free(unzip_data);
		unzip_data = NULL;
		goto end;
	}

#endif
end:
	return(unzip_data);
}

/**
 * ungzip_file:
 * @gzip_file:  pointer to start of compressed data
 * @zipped_size: the size of the compressed data block
 * @unzip_size:  the size of the uncompressed data block
 *
 * Returns a pointer to uncompressed data (maybe NULL)
 *  data should be freed once used
 *
 */
static void *ungzip_file ( gchar *gzip_file, gsize zipped_size, gulong *unzip_size )
{
	g_autoptr(GInputStream) inputGz = g_memory_input_stream_new_from_data ( gzip_file, zipped_size, NULL );
	if ( !inputGz )
		return NULL;

	GError *error = NULL;
	g_autoptr(GBytes) bytes;
	g_autoptr(GZlibDecompressor) decompressor = g_zlib_decompressor_new ( G_ZLIB_COMPRESSOR_FORMAT_GZIP );
	g_autoptr(GInputStream) streamData = g_converter_input_stream_new ( inputGz, G_CONVERTER(decompressor) );
	// Need to read up to the unzipped size - which is unknown, so the following doesn't properly work:
	//bytes = g_input_stream_read_bytes ( streamData, zipped_size, NULL, &error );

	// Thus instead use this 'splice' technique - which completely reads from input to output
	g_autoptr(GOutputStream) memory_output = g_memory_output_stream_new_resizable ();
	g_output_stream_splice ( memory_output, streamData, 0, NULL, &error );
	(void)g_output_stream_close ( memory_output, NULL, NULL );
	bytes = g_memory_output_stream_steal_as_bytes ( G_MEMORY_OUTPUT_STREAM(memory_output) );

	*unzip_size = g_bytes_get_size ( bytes );
	if ( *unzip_size > 0 )
		return g_memdup ( g_bytes_get_data(bytes, NULL), *unzip_size );

	return NULL;
}

/**
 * uncompress_bzip2:
 * @name: The name of the file to attempt to decompress
 *
 * Returns: The name of the uncompressed file (in a temporary location) or NULL
 *   free the returned name after use.
 *
 * Also see: http://www.bzip.org/1.0.5/bzip2-manual-1.0.5.html
 */
gchar* uncompress_bzip2 ( const gchar *name )
{
#ifdef HAVE_BZLIB_H
	FILE *ff = g_fopen ( name, "rb" );
	if ( !ff )
		return NULL;

	int     bzerror;
	BZFILE* bf = BZ2_bzReadOpen ( &bzerror, ff, 0, 0, NULL, 0 ); // This should take care of the bz2 file header
	if ( bzerror != BZ_OK ) {
		BZ2_bzReadClose ( &bzerror, bf );
		// handle error
		g_warning ( "%s: BZ ReadOpen error on %s", __FUNCTION__, name );
		return NULL;
	}

	GFileIOStream *gios;
	GError *error = NULL;
	GFile *gf = g_file_new_tmp ( "vik-bz2-tmp.XXXXXX", &gios, &error );
	gchar *tmpname = g_file_get_path (gf);

	GOutputStream *gos = g_io_stream_get_output_stream ( G_IO_STREAM(gios) );

	// Process in arbitary sized chunks
	char buf[4096];
	bzerror = BZ_OK;
	int nBuf = 0;
	// Now process the actual compression data
	while ( bzerror == BZ_OK ) {
		nBuf = BZ2_bzRead ( &bzerror, bf, buf, 4096 );
		if ( bzerror == BZ_OK || bzerror == BZ_STREAM_END) {
			// do something with buf[0 .. nBuf-1]
			if ( g_output_stream_write ( gos, buf, nBuf, NULL, &error ) < 0 ) {
				g_critical ( "Couldn't write bz2 tmp %s file due to %s", tmpname, error->message );
				g_error_free (error);
				BZ2_bzReadClose ( &bzerror, bf );
				goto end;
			}
		}
	}
	if ( bzerror != BZ_STREAM_END ) {
		// handle error...
		g_warning ( "%s: BZ error :( %d. read %d", __FUNCTION__, bzerror, nBuf );
	}
	BZ2_bzReadClose ( &bzerror, bf );
	g_output_stream_close ( gos, NULL, &error );

 end:
	g_object_unref ( gios );
	fclose ( ff );

	return tmpname;
#else
	return NULL;
#endif
}

VikLoadType_t uncompress_load_bzip_file ( const gchar *filename,
                                          VikAggregateLayer *top,
                                          VikViewport *vp,
                                          VikTrwLayer *vtl,
                                          gboolean new_layer,
                                          gboolean external )
{
	gchar *tmp_name = uncompress_bzip2 ( filename );
	VikLoadType_t ans = a_file_load ( top, vp, vtl, tmp_name, new_layer, external, filename );
	(void)util_remove ( tmp_name );
	return ans;
}

/**
 * uncompress_xz:
 * @name: The name of the file to attempt to decompress
 *
 * Returns: The name of the uncompressed file (in a temporary location) or NULL
 *   free the returned name after use.
 *
 */
gchar* uncompress_xz ( const gchar *name )
{
#ifdef HAVE_LZMA_H

	FILE *ff = g_fopen ( name, "rb" );
	if ( !ff )
		return NULL;

	unsigned char bufi[4096];
	// Seems to me to be sensible to have a bigger buffer for decompressed data!
	unsigned char bufo[4096*16];

	GFileIOStream *gios;
	GError *error = NULL;
	GFile *gf = g_file_new_tmp ( "vik-xz-tmp.XXXXXX", &gios, &error );
	gchar *tmpname = g_file_get_path ( gf );

	GOutputStream *gos = g_io_stream_get_output_stream ( G_IO_STREAM(gios) );

	lzma_stream lstrm = LZMA_STREAM_INIT;

	lzma_ret rv = lzma_auto_decoder ( &lstrm, UINT64_MAX, 0 );

	if ( rv != LZMA_OK )
		goto err;

	gboolean eof = FALSE;
	size_t total = 0;

	lstrm.next_out = bufo;
	lstrm.avail_out = sizeof(bufo);
	while ( rv == LZMA_OK ) {

		// Get next block of data from file
		if ( lstrm.avail_in == 0 ) {
			lstrm.next_in = bufi;
			lstrm.avail_in = fread ( bufi, 1, sizeof(bufi), ff );
			if ( lstrm.avail_in == 0 ) {
				eof = TRUE;
				goto end;
			}
		}

		// Keep processing with lzma (in decompressor mode)
		rv = lzma_code ( &lstrm, LZMA_RUN );

		// If successful - write decompressed stream out to the file
		if ( rv == LZMA_OK || rv == LZMA_STREAM_END ) {

			if ( g_output_stream_write ( gos, bufo, lstrm.total_out - total, NULL, &error ) < 0 ) {
				g_critical ( "Couldn't write xz tmp %s file due to %s", tmpname, error->message );
				g_error_free ( error );
				goto end;
			}
			// Reset bufo and point the stream back to it
			lstrm.next_out = bufo;
			lstrm.avail_out = sizeof(bufo);
			total = lstrm.total_out;
		}
		if ( rv == LZMA_STREAM_END ) {
			lzma_end ( &lstrm );
			goto end;
		} else {
			if ( rv != LZMA_OK ) {
				lzma_end ( &lstrm );
				goto err;
			}
		}
		if ( eof ) {
			g_warning ( "%s: End of file found instead of stream end!", __FUNCTION__ );
			goto end;
		}
	}

 err:
	g_warning ("%s: %u", __FUNCTION__, rv);

 end:
	g_output_stream_close ( gos, NULL, &error );
	g_object_unref ( gios );
	fclose ( ff );

	return tmpname;
#else
	return NULL;
#endif
}

VikLoadType_t uncompress_load_xz_file ( const gchar *filename,
                                        VikAggregateLayer *top,
                                        VikViewport *vp,
                                        VikTrwLayer *vtl,
                                        gboolean new_layer,
                                        gboolean external )
{
	gchar *tmp_name = uncompress_xz ( filename );
	VikLoadType_t ans = a_file_load ( top, vp, vtl, tmp_name, new_layer, external, filename );
	(void)util_remove ( tmp_name );
	return ans;
}

VikLoadType_t uncompress_load_gz_file ( const gchar *filename,
                                        VikAggregateLayer *top,
                                        VikViewport *vp,
                                        VikTrwLayer *vtl,
                                        gboolean new_layer,
                                        gboolean external )
{
	VikLoadType_t ans = LOAD_TYPE_READ_FAILURE;
	GMappedFile *mf;
	GError *error = NULL;
	if ( (mf = g_mapped_file_new(filename, FALSE, &error)) == NULL ) {
		g_critical ( "Couldn't map file %s: %s", filename, error->message );
		g_error_free ( error );
		return LOAD_TYPE_READ_FAILURE;
	}

	gsize file_size = g_mapped_file_get_length ( mf );
	gchar *contents = g_mapped_file_get_contents ( mf );
	void *unzip_mem = NULL;
	gulong ucsize;

	if ( (unzip_mem = ungzip_file(contents, file_size, &ucsize)) == NULL ) {
		g_mapped_file_unref ( mf );
		return LOAD_TYPE_READ_FAILURE;
    }

	GFileIOStream *gios;
	GFile *gf = g_file_new_tmp ( "vik-gz-tmp.XXXXXX", &gios, &error );
	gchar *tmp_name = g_file_get_path ( gf );

	GOutputStream *gos = g_io_stream_get_output_stream ( G_IO_STREAM(gios) );

	if ( g_output_stream_write ( gos, unzip_mem, ucsize, NULL, &error ) < 0 ) {
		g_critical ( "Couldn't write gz tmp %s file due to %s", tmp_name, error->message );
		g_error_free ( error );
		goto end;
	}
	g_free ( unzip_mem );

	ans = a_file_load ( top, vp, vtl, tmp_name, new_layer, external, filename );
	(void)util_remove ( tmp_name );

 end:
	g_mapped_file_unref ( mf );
	g_output_stream_close ( gos, NULL, &error );
	g_object_unref ( gios );

	return ans;
}
