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
#include <windef.h>
#define realpath(X,Y) _fullpath(Y,X,MAX_PATH)
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

/**
 * Just a wrapper around realpath, which itself is platform dependent
 */
char *file_realpath ( const char *path, char *real )
{
  return realpath ( path, real );
}

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif
/**
 * Always return the canonical filename in a newly allocated string
 */
char *file_realpath_dup ( const char *path )
{
	char real[MAXPATHLEN];

	g_return_val_if_fail(path != NULL, NULL);

	if (file_realpath(path, real))
		return g_strdup(real);

	return g_strdup(path);
}

/**
 * Permission granted to use this code after personal correspondance
 * Slightly reworked for better cross platform use, glibisms, function rename and a compacter format
 *
 * FROM http://www.codeguru.com/cpp/misc/misc/fileanddirectorynaming/article.php/c263
 */

// GetRelativeFilename(), by Rob Fisher.
// rfisher@iee.org
// http://come.to/robfisher

// The number of characters at the start of an absolute filename.  e.g. in DOS,
// absolute filenames start with "X:\" so this value should be 3, in UNIX they start
// with "\" so this value should be 1.
#ifdef WINDOWS
#define ABSOLUTE_NAME_START 3
#else
#define ABSOLUTE_NAME_START 1
#endif

// Given the absolute current directory and an absolute file name, returns a relative file name.
// For example, if the current directory is C:\foo\bar and the filename C:\foo\whee\text.txt is given,
// GetRelativeFilename will return ..\whee\text.txt.

const gchar *file_GetRelativeFilename ( gchar *currentDirectory, gchar *absoluteFilename )
{
  gint afMarker = 0, rfMarker = 0;
  gint cdLen = 0, afLen = 0;
  gint i = 0;
  gint levels = 0;
  static gchar relativeFilename[MAXPATHLEN+1];

  cdLen = strlen(currentDirectory);
  afLen = strlen(absoluteFilename);

  // make sure the names are not too long or too short
  if (cdLen > MAXPATHLEN || cdLen < ABSOLUTE_NAME_START+1 ||
      afLen > MAXPATHLEN || afLen < ABSOLUTE_NAME_START+1) {
    return NULL;
  }

  // Handle DOS names that are on different drives:
  if (currentDirectory[0] != absoluteFilename[0]) {
    // not on the same drive, so only absolute filename will do
    g_strlcpy(relativeFilename, absoluteFilename, MAXPATHLEN+1);
    return relativeFilename;
  }

  // they are on the same drive, find out how much of the current directory
  // is in the absolute filename
  i = ABSOLUTE_NAME_START;
  while (i < afLen && i < cdLen && currentDirectory[i] == absoluteFilename[i]) {
    i++;
  }

  if (i == cdLen && (absoluteFilename[i] == G_DIR_SEPARATOR || absoluteFilename[i-1] == G_DIR_SEPARATOR)) {
    // the whole current directory name is in the file name,
    // so we just trim off the current directory name to get the
    // current file name.
    if (absoluteFilename[i] == G_DIR_SEPARATOR) {
      // a directory name might have a trailing slash but a relative
      // file name should not have a leading one...
      i++;
    }

    g_strlcpy(relativeFilename, &absoluteFilename[i], MAXPATHLEN+1);
    return relativeFilename;
  }

  // The file is not in a child directory of the current directory, so we
  // need to step back the appropriate number of parent directories by
  // using "..\"s.  First find out how many levels deeper we are than the
  // common directory
  afMarker = i;
  levels = 1;

  // count the number of directory levels we have to go up to get to the
  // common directory
  while (i < cdLen) {
    i++;
    if (currentDirectory[i] == G_DIR_SEPARATOR) {
      // make sure it's not a trailing slash
      i++;
      if (currentDirectory[i] != '\0') {
	levels++;
      }
    }
  }

  // move the absolute filename marker back to the start of the directory name
  // that it has stopped in.
  while (afMarker > 0 && absoluteFilename[afMarker-1] != G_DIR_SEPARATOR) {
    afMarker--;
  }

  // check that the result will not be too long
  if (levels * 3 + afLen - afMarker > MAXPATHLEN) {
    return NULL;
  }

  // add the appropriate number of "..\"s.
  rfMarker = 0;
  for (i = 0; i < levels; i++) {
    relativeFilename[rfMarker++] = '.';
    relativeFilename[rfMarker++] = '.';
    relativeFilename[rfMarker++] = G_DIR_SEPARATOR;
  }

  // copy the rest of the filename into the result string
  strcpy(&relativeFilename[rfMarker], &absoluteFilename[afMarker]);

  return relativeFilename;
}
/* END http://www.codeguru.com/cpp/misc/misc/fileanddirectorynaming/article.php/c263 */
