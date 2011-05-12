/*
 * random.c -- get random bytes
 *
 * Copyright (C) 2010, 2011 Free Software Initiative of Japan
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

#include "config.h"
#include "ch.h"
#include "gnuk.h"

/*
 * Return pointer to random 16-byte
 */
const uint8_t *
random_bytes_get (void)
{
  uint32_t addr, addr0;

  addr = (uint32_t)&random_bits_start + ((hardclock () << 4) & 0x3f0);
  addr0 = addr;

  while (1)
    {
      if (*(uint32_t *)addr != 0 && *(uint32_t *)addr != 0xffffffff)
	break;

      addr += 16;
      if (addr >= ((uint32_t)&random_bits_start) + 1024)
	addr = ((uint32_t)&random_bits_start);

      if (addr == addr0)
	fatal (FATAL_RANDOM);
    }

  return (const uint8_t *)addr;
}

/*
 * Free pointer to random 16-byte
 */
void
random_bytes_free (const uint8_t *p)
{
  int i;
  uint32_t addr = (uint32_t)p;

  for (i = 0; i < 8; i++)
    flash_clear_halfword (addr+i*2);
}

/*
 * Return 4-byte salt
 */
uint32_t
get_salt (void)
{
  const uint8_t *u = unique_device_id (); /* 12-byte unique id */
  uint32_t r = 0;
  int i;

  for (i = 0; i < 4; i++)
    {
      r <<= 8;
      r |= u[hardclock () % 12];
    }

  return r;
}
