/*
 * flash.c -- Data Objects (DO) and GPG Key handling on Flash ROM
 *
 * Copyright (C) 2010 Free Software Initiative of Japan
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

/*
 * Writing to Flash ROM is NOT YET IMPLEMENTED, just API only
 */

#include "ch.h"
#include "gnuk.h"

static uint8_t do_pool[256];
static uint8_t *last_p = do_pool;

/*
 * TLV (Tag, Length, and Value)
 */
uint8_t *
flash_do_write (uint16_t tag, uint8_t *data, int len)
{
  uint8_t *p = last_p;

  if (last_p - do_pool + len + 2 + 3 > 1024)
    return NULL;

  *last_p++ = (tag >> 8);
  *last_p++ = (tag & 0xff);
  if (len < 128)
    *last_p++ = len;
  else if (len < 256)
    {
      *last_p++ = 0x81;
      *last_p++ = len;
    }
  else
    {
      *last_p++ = 0x82;
      *last_p++ = (len >> 8);
      *last_p++ = (len & 0xff);
    }
  memcpy (last_p, data, len);
  last_p += len;

  return p + 2;
}

static uint8_t k1[KEY_CONTENT_LEN];
#if 0
static uint8_t k2[KEY_CONTENT_LEN];
static uint8_t k3[KEY_CONTENT_LEN];
#endif

uint8_t *
flash_key_alloc (enum kind_of_key kk)
{
  switch (kk)
    {
    case GPG_KEY_FOR_SIGNATURE:
      return k1;
#if 0
    case GPG_KEY_FOR_DECRYPT:
      return k2;
    case GPG_KEY_FOR_AUTHENTICATION:
      return k3;
#else
    default:
      return k1;
#endif
    }
}

int
flash_key_write (uint8_t *key_addr, uint8_t *key_data)
{
  memcpy (key_addr, key_data, KEY_CONTENT_LEN);
  return 0;
}
