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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

#include <metatile.h>

int main ( int argc, char *argv[] )
{
    const int tile_max = METATILE_MAX_SIZE;
    char err_msg[PATH_MAX];
    char id[PATH_MAX];
    char *buf;
    int len;
    int compressed;

    // Could extend to get values from the command-line.
    int x = 4051;
    int y = 2753;
    int z = 13;
    //char dir[] = "/var/lib/mod_tile/default";
    char dir[] = "metatile_example";
    // Example defaults to a metatile that was pre generated using mod_tile
    // Equates to 'metatile_example/13/0/0/250/220/0.meta'
    //  which is Brownsea Island, Dorset, UK (50.69N, 1.96W)

    buf = malloc(tile_max);
    if (!buf) {
        return 1;
    }

    err_msg[0] = 0;

    if ( argc > 1 )
      len = metatile_read(argv[1], x, y, z, buf, tile_max, &compressed, err_msg);
    else
      len = metatile_read(dir, x, y, z, buf, tile_max, &compressed, err_msg);

    if (len > 0) {
        // Do something with buf
        // Just dump to a file
        FILE *fp;
        if (compressed)
          fp = fopen( "tilefrommeta.gz" , "w" );
        else
          fp = fopen( "tilefrommeta.png" , "w" );

        if ( fp ) {
          fwrite(buf, 1 , len, fp);
          fclose(fp);
        }
        else
          fprintf(stderr, "Failed to open file because: %s\n", strerror(errno));

        free(buf);
        return 0;
    }
    else
        fprintf(stderr, "FAILED: %s\n", err_msg);

    free(buf);
    return 3;
}
