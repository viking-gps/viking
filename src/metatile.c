/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2014, Rob Norris <rw_norris@hotmail.com>
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
 * Mostly imported from https://github.com/openstreetmap/mod_tile/
 *  Release 0.4
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

#include "metatile.h"
/**
 * metatile.h
 */
#define META_MAGIC "META"
#define META_MAGIC_COMPRESSED "METZ"

struct entry {
    int offset;
    int size;
};

struct meta_layout {
    char magic[4]; // META_MAGIC or META_MAGIC_COMPRESSED
    int count; // METATILE ^ 2
    int x, y, z; // lowest x,y of this metatile, plus z
    struct entry index[]; // count entries
    // Followed by the tile data
    // The index offsets are measured from the start of the file
};

// Use this to enable meta-tiles which will render NxN tiles at once
// Note: This should be a power of 2 (2, 4, 8, 16 ...)
#define METATILE (8)

/**
 * xyz_to_meta:
 * Based on function from mod_tile/src/store_file_utils.c
 *
 * Returns the path to the meta-tile and the offset within the meta-tile
 */
int xyz_to_meta(char *path, size_t len, const char *dir, int x, int y, int z)
{
    unsigned char i, hash[5], offset, mask;

    // Each meta tile winds up in its own file, with several in each leaf directory
    // the .meta tile name is based on the sub-tile at (0,0)
    mask = METATILE - 1;
    offset = (x & mask) * METATILE + (y & mask);
    x &= ~mask;
    y &= ~mask;

    for (i=0; i<5; i++) {
        hash[i] = ((x & 0x0f) << 4) | (y & 0x0f);
        x >>= 4;
        y >>= 4;
    }

    snprintf(path, len, "%s/%d/%u/%u/%u/%u/%u.meta", dir, z, hash[4], hash[3], hash[2], hash[1], hash[0]);
    return offset;
}

/**
 * metatile_read:
 * From function in mod_tile/src/store_file.c
 * Slightly reworked to use simplified xyz_to_meta() above
 *
 * Reads into buf upto size specified by sz
 *
 * Returns whether the file is in a compressed format (possibly only gzip)
 *
 * Error messages returned in log_msg
 */
int metatile_read(const char *dir, int x, int y, int z, char *buf, size_t sz, int * compressed, char * log_msg)
{
    char path[PATH_MAX];
    int meta_offset, fd;
    unsigned int pos;
    unsigned int header_len = sizeof(struct meta_layout) + METATILE*METATILE*sizeof(struct entry);
    struct meta_layout *meta = (struct meta_layout *)malloc(header_len);
    size_t file_offset, tile_size;

    meta_offset = xyz_to_meta(path, sizeof(path), dir, x, y, z);

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        snprintf(log_msg,PATH_MAX - 1, "Could not open metatile %s. Reason: %s\n", path, strerror(errno));
        free(meta);
        return -1;
    }

    pos = 0;
    while (pos < header_len) {
        size_t len = header_len - pos;
        int got = read(fd, ((unsigned char *) meta) + pos, len);
        if (got < 0) {
            snprintf(log_msg,PATH_MAX - 1, "Failed to read complete header for metatile %s Reason: %s\n", path, strerror(errno));
            close(fd);
            free(meta);
            return -2;
        } else if (got > 0) {
            pos += got;
        } else {
            break;
        }
    }
    if (pos < header_len) {
        snprintf(log_msg,PATH_MAX - 1, "Meta file %s too small to contain header\n", path);
        close(fd);
        free(meta);
        return -3;
    }
    if (memcmp(meta->magic, META_MAGIC, strlen(META_MAGIC))) {
        if (memcmp(meta->magic, META_MAGIC_COMPRESSED, strlen(META_MAGIC_COMPRESSED))) {
            snprintf(log_msg,PATH_MAX - 1, "Meta file %s header magic mismatch\n", path);
            close(fd);
            free(meta);
            return -4;
        } else {
            *compressed = 1;
        }
    } else *compressed = 0;

    // Currently this code only works with fixed metatile sizes (due to xyz_to_meta above)
    if (meta->count != (METATILE * METATILE)) {
        snprintf(log_msg, PATH_MAX - 1, "Meta file %s header bad count %d != %d\n", path, meta->count, METATILE * METATILE);
        free(meta);
        close(fd);
        return -5;
    }

    file_offset = meta->index[meta_offset].offset;
    tile_size   = meta->index[meta_offset].size;

    free(meta);

    if (tile_size > sz) {
        snprintf(log_msg, PATH_MAX - 1, "Truncating tile %zd to fit buffer of %zd\n", tile_size, sz);
        tile_size = sz;
        close(fd);
        return -6;
    }

    if (lseek(fd, file_offset, SEEK_SET) < 0) {
        snprintf(log_msg, PATH_MAX - 1, "Meta file %s seek error: %s\n", path, strerror(errno));
        close(fd);
        return -7;
    }

    pos = 0;
    while (pos < tile_size) {
        size_t len = tile_size - pos;
        int got = read(fd, buf + pos, len);
        if (got < 0) {
            snprintf(log_msg, PATH_MAX - 1, "Failed to read data from file %s. Reason: %s\n", path, strerror(errno));
            close(fd);
            return -8;
        } else if (got > 0) {
            pos += got;
        } else {
            break;
        }
    }
    close(fd);
    return pos;
}
