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

#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <string.h>

#ifdef WINDOWS
#include <winsock.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#include "http.h"

int http_connect(const char *hostname, int port)
{
  int sock; 
  struct sockaddr_in server;
  struct hostent *host_addr;

  /* create a socket of type AF_INET, and SOCK_STREAM (TCP) */
  sock = socket(AF_INET, SOCK_STREAM, 0);

  /* get an IP from a domain name -- essential */
  host_addr = gethostbyname(hostname);
  if (host_addr == NULL)
    return(-1);

  server.sin_family = AF_INET;
  /* 110 is the standard POP port. Host TO Network order. */
  server.sin_port = htons(port);
  /* get the IP address.                                  */
  server.sin_addr = *((struct in_addr *) host_addr->h_addr);
  /* padding unused in sockaddr_in                        */
#ifndef WINDOWS
  bzero(&(server.sin_zero), 8);
#endif

  if ((connect(sock, (struct sockaddr *) &server, sizeof(struct sockaddr))) == -1)
    return(-2);

  return(sock);
}

int http_get_line(int sock, char *buf, int len)
{
  static char lilbuf;
  int size, count;

  count = 1;
  size = 1;
  lilbuf = 'a';
  while (size != 0 && lilbuf != '\n' && count < len)
  {
    size = recv(sock, &lilbuf, 1, 0);
    if (size == 0 && count == 1 )
      return 0;

    if (size > 0)
      *buf++ = lilbuf;
    count++;
  }
  *buf = '\0';

  return 1;
}

/* makes directory if neccessary */
int http_download_get_url ( const char *hostname, const char *uri, FILE *f, int already_redirected, int sendhostname )
{
  static char input_buffer[1024];
  int sock;
  int len;
  FILE *tmp_f;
  /* int hnlen = strlen ( hostname ); */

#ifdef WINDOWS
  WSADATA usadata;
  WSAStartup ( MAKEWORD(2,2), &usadata );
#endif

  sock = http_connect ( hostname, 80 );
  if (sock < 0)
  {
    return -1;
  }


  if ( sendhostname ) {
    send ( sock, "GET http://", 11, 0);
    send ( sock, hostname, strlen(hostname), 0 );
    send ( sock, uri, strlen ( uri ), 0 );
    send ( sock, " HTTP/1.0\r\n\r\n", 13, 0 );
  } else {
    send ( sock, "GET ", 4, 0 );
    send ( sock, uri, strlen ( uri ), 0 );
    send ( sock, "\r\n\r\n", 4, 0 );
  }

  /* next, skip through all headers EXCEPT content length.,
     that is, if it begins with "Content-Length: " (strncasecmp),
     atoi that line from +16 (+17 ?), read that many bytes directly 
     into file (IF we can open it, else return error) and we're done.
  */ 

  /* "HTTP/1.x 200 OK" check */
  if ( recv ( sock, input_buffer, 12, 0 ) < 12 || input_buffer[9] != '2' || input_buffer[10] != '0' || input_buffer[11] != '0' )
  {
    /* maybe it's a redirect */
    if ( ! already_redirected )
    do
    {
      if ( http_get_line ( sock, input_buffer, 1024 ) == 0 )
        break;

      /* Location: http://abc.def/bla */
      if ( strncmp(input_buffer, "Location: ", 10) == 0 && strlen(input_buffer) > 17 )
      {
        char *uri_start;

        int rv;
        uri_start = strchr(input_buffer+17,'/');

        if ( uri_start )
        {
          char *newhost = g_strndup ( input_buffer + 17, uri_start - input_buffer - 17 );
          char *newuri = strdup ( uri_start );
          close ( sock );

          rv = http_download_get_url ( newhost, newuri, f, 1, sendhostname );

          free ( newhost );
          free ( newuri );
          return rv;
        }
      }
    } while (input_buffer[0] != '\r' );

    /* Something went wrong */
    return 1;
  }

  do
  {
    if ( http_get_line ( sock, input_buffer, 1024 ) == 0 )
    {
      close ( sock );
      return -2;
    }
  } while (input_buffer[0] != '\r' );

  tmp_f = tmpfile();

  do {
    len = recv ( sock, input_buffer, 1024, 0 );
    if ( len > 0 )
      fwrite ( input_buffer, 1, len, tmp_f );
  } while ( len > 0 );

  rewind(tmp_f);

  while ( ! feof(tmp_f) )
  {
    len = fread ( input_buffer, 1, 1024, tmp_f );
    fwrite ( input_buffer, 1, len, f);
  }
  fclose ( tmp_f );

  close ( sock );
#ifdef WINDOWS
    WSACleanup(); /* they sure make winsock programming easy. */
#endif
  return 0;
}
