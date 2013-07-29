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
 * Code that is independent of any other Viking specific types
 * Otherwise see file.c
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "fileutils.h"

#ifdef WINDOWS
#define FILE_SEP '\\'
#else
#define FILE_SEP '/'
#endif

const gchar *a_file_basename ( const gchar *filename )
{
  const gchar *t = filename + strlen(filename) - 1;
  while ( --t > filename )
    if ( *(t-1) == FILE_SEP )
      break;
  if ( t >= filename )
    return t;
  return filename;
}
