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

#include <gtk/gtk.h>
#include <string.h>
#include "mapcache.h"

#include "config.h"

typedef struct _List {
  struct _List *next;
  gchar *key;
} List;

/* a circular linked list, a pointer to the tail, and the tail points to the head */
/* this is so we can free the last */
static List *queue_tail = NULL;
static int queue_count = 0;

#define VIK_CONFIG_MAPCACHE_MAX_SIZE 50331648 /* 48 meggerbytes */
static guint32 queue_size = 0;


static GHashTable *cache = NULL;

#define HASHKEY_FORMAT_STRING "%d-%d-%d-%d-%d-%d-%.3f-%.3f"
#define HASHKEY_FORMAT_STRING_NOSHRINK "%d-%d-%d-%d-%d-%d-"

void a_mapcache_init ()
{
  cache = g_hash_table_new_full ( g_str_hash, g_str_equal, g_free, g_object_unref );
}

/* returns key from head, adds on newtailkey to tail. */
static gchar *list_shift_add_entry ( gchar *newtailkey )
{
  gchar *oldheadkey = queue_tail->next->key;
  queue_tail->next->key = newtailkey;
  queue_tail = queue_tail->next;
  return oldheadkey;
}

static gchar *list_shift ()
{
  gchar *oldheadkey = queue_tail->next->key;
  List *oldhead = queue_tail->next;
  queue_tail->next = queue_tail->next->next;
  g_free ( oldhead );
  return oldheadkey;
}

/* adds key to tail */
static void list_add_entry ( gchar *key )
{
  List *newlist = g_malloc ( sizeof ( List ) );
  newlist->key = key;
  if ( queue_tail ) {
    newlist->next = queue_tail->next;
    queue_tail->next = newlist;
    queue_tail = newlist;
  } else {
    newlist->next = newlist;
    queue_tail = newlist;
  }
}

void a_mapcache_add ( GdkPixbuf *pixbuf, gint x, gint y, gint z, guint8 type, guint zoom, guint8 alpha, gdouble xshrinkfactor, gdouble yshrinkfactor )
{
  gchar *key = g_strdup_printf ( HASHKEY_FORMAT_STRING, x, y, z, type, zoom, alpha, xshrinkfactor, yshrinkfactor );
  static int tmp = 0;

  /* FIXME: what if already exists? problem! */
  g_hash_table_insert ( cache, key, pixbuf );

  queue_size += gdk_pixbuf_get_rowstride(pixbuf) * gdk_pixbuf_get_height(pixbuf);
  queue_size += 100;
  queue_count++;

  if ( queue_size > VIK_CONFIG_MAPCACHE_SIZE ) {
    gchar *oldkey = list_shift_add_entry ( key );
    GdkPixbuf *oldbuf = g_hash_table_lookup ( cache, oldkey );
    queue_size -= gdk_pixbuf_get_rowstride(oldbuf) * gdk_pixbuf_get_height(oldbuf);
    queue_size -= 100;
    queue_count --;
    g_hash_table_remove ( cache, oldkey );
    while ( queue_size > VIK_CONFIG_MAPCACHE_SIZE ) {
      oldkey = list_shift ();
      oldbuf = g_hash_table_lookup ( cache, oldkey );
      queue_size -= gdk_pixbuf_get_rowstride(oldbuf) * gdk_pixbuf_get_height(oldbuf);
      queue_size -= 100;
      queue_count --;
      g_hash_table_remove ( cache, oldkey );
    }

    /* chop off 'start' etc */
  } else {
    list_add_entry ( key );
    /* business as usual */
  }

  if ( (++tmp == 100 ))  { g_print("DEBUG: queue count=%d %ld\n", queue_count, queue_size ); tmp=0; }
}

GdkPixbuf *a_mapcache_get ( gint x, gint y, gint z, guint8 type, guint zoom, guint8 alpha, gdouble xshrinkfactor, gdouble yshrinkfactor )
{
  static char key[48];
  g_snprintf ( key, sizeof(key), HASHKEY_FORMAT_STRING, x, y, z, type, zoom, alpha, xshrinkfactor, yshrinkfactor );
  return g_hash_table_lookup ( cache, key );
}

void a_mapcache_remove_all_shrinkfactors ( guint16 x, guint16 y, guint16 z, guint8 type, guint zoom, guint8 alpha )
{
  char key[40];
  List *loop = queue_tail;
  List *tmp;
  gint len;

  if ( queue_tail == NULL )
    return;

  g_snprintf ( key, sizeof(key), HASHKEY_FORMAT_STRING_NOSHRINK, x, y, z, type, zoom, alpha );
  len = strlen(key);

  /* TODO: check logic here */
  do {
    tmp = loop->next;
    if ( strncmp(tmp->key, key, len) == 0 )
    {
      g_hash_table_remove ( cache, tmp->key );
      if ( tmp == loop )
        loop = queue_tail = NULL;
      else
        loop->next = tmp->next;
      g_free ( tmp );
      tmp = NULL;
    }
    else
      loop = tmp;

  } while ( loop && (loop != queue_tail || tmp == NULL) );

  /* loop thru list, looking for the one, compare first whatever chars */

  g_hash_table_remove ( cache, key );
}

void a_mapcache_uninit ()
{
  g_hash_table_destroy ( cache );
  /* free list */
  cache = NULL;
}


