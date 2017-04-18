/*                                                    -*- coding: utf-8 -*-
 * usbip-server.c - USB Device Emulation by USBIP Protocol
 *
 * Copyright (C) 2017  Free Software Initiative of Japan
 * Author: NIIBE Yutaka <gniibe@fsij.org>
 *
 * This file is a part of Gnuk, a GnuPG USB Token implementation.
 *
 * Gnuk is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Gnuk is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define USBIP_PORT 3240

#define CMD_REQ_LIST   0x01008005
#define CMD_REQ_ATTACH 0x01008003
#define CMD_URB        0x00000001
#define CMD_DETACH     0x00000002

struct usbip_msg_head {
  uint32_t cmd;
  uint32_t seq;
};

static char *
list_devices (size_t *len_p)
{
  *len_p = 0;
  return NULL;
}

static char *
attach_device (char busid[32], size_t *len_p)
{
  *len_p = 0;
  return NULL;
}

static int
handle_urb (int fd)
{
  return 0;
}



void
run_server (void)
{
  int sock;
  struct sockaddr_in v4addr;
  const int one = 1;

  if ((sock = socket (PF_INET, SOCK_STREAM, 0)) < 0)
    {
      perror ("socket");
      exit (1);
    }

  if (setsockopt (sock, SOL_SOCKET, SO_REUSEADDR,
		  (const char*)&one, sizeof (int)) < 0)
    perror ("WARN: setsockopt");

  memset (&v4addr, 0, sizeof (v4addr));
  v4addr.sin_family = AF_INET;
  v4addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
  v4addr.sin_port = htons (USBIP_PORT);

  if (bind (sock, (const struct sockaddr *)&v4addr, sizeof (v4addr)) < 0)
    {
      perror ("bind");
      exit (1);
    }

  /* We only accept a connection from a single client.  */
  if (listen (sock, 1) < 0)
    {
      perror ("listen");
      exit (1);
    }

  for (;;)
    {
      int fd;
      int attached = 0;

      /* We don't care who is connecting.  */
      if ((fd = accept (sock, NULL, NULL)) < 0)
        {
          perror ("accept");
          exit (1);
        }

      for (;;)
        {
	  struct usbip_msg_head msg;

	  if (recv (fd, (char *)&msg, sizeof (msg), 0) != sizeof (msg))
	    {
	      perror ("msg recv");
	      break;
	    }

	  msg.cmd = ntohl (msg.cmd);
	  msg.seq = ntohl (msg.seq);

	  if (msg.cmd == CMD_REQ_LIST)
	    {
	      char *device_list;
	      size_t device_list_size;

	      if (attached)
		{
		  fprintf (stderr, "REQ list while attached\n");
		  break;
		}

	      device_list = list_devices (&device_list_size);

	      if (send (fd, device_list, device_list_size, 0) != device_list_size)
		{
		  perror ("list send");
		  break;
		}

	      free (device_list);
	    }
	  else if (msg.cmd == CMD_REQ_ATTACH)
	    {
	      char busid[32];
	      char *attach;
	      size_t attach_size;

	      if (attached)
		{
		  fprintf (stderr, "REQ attach while attached\n");
		  break;
		}
	      
	      if (recv (fd, busid, 32, 0) != 32)
		{
		  perror ("attach recv");
		  break;
		}

	      attach = attach_device (busid, &attach_size);
	      if (send (fd, attach, attach_size, 0) != attach_size)
		{
		  perror ("list send");
		  break;
		}

	      free (attach);
	      attached = 1;
	    }
	  else if (msg.cmd == CMD_URB)
	    {
	      if (!attached)
		{
		  fprintf (stderr, "URB while attached\n");
		  break;
		}

	      if (handle_urb (fd) < 0)
		{
		  fprintf (stderr, "URB handling failed\n");
		  break;
		}
	    }
	  else if(msg.cmd == CMD_DETACH)
	    {
	      if (!attached)
		{
		  fprintf (stderr, "DETACH while attached\n");
		  break;
		}

	      /* send reply??? */
	      break;
	    }
	  else
	    {
	      fprintf (stderr, "Unknown command %x, disconnecting.\n", msg.cmd);
	      break;
	    }
	}

       close (fd);
    }
}
