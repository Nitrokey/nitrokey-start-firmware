/*
 * random.c -- get random bytes
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

#include "config.h"
#include "ch.h"
#include "gnuk.h"

extern void *_binary_random_bits_start;

const uint8_t *
random_bytes_get (void)
{
  uint32_t addr, addr0;

  addr = (uint32_t)&_binary_random_bits_start + ((hardclock () << 5) & 0x3e0);
  addr0 = addr; 

  while (1)
    {
      if (*(uint32_t *)addr != 0)
	break;

      addr += 32;
      if (addr >= ((uint32_t)&_binary_random_bits_start) + 1024)
	addr = ((uint32_t)&_binary_random_bits_start);

      if (addr == addr0)
	fatal (FATAL_RANDOM);
    }

  return (const uint8_t *)addr;
}

void
random_bytes_free (const uint8_t *p)
{
  int i;
  uint32_t addr = (uint32_t)p;

  for (i = 0; i < 16; i++)
    flash_clear_halfword (addr+i*2);
}

uint32_t
get_random (void)
{
  const uint32_t *p = (const uint32_t *)random_bytes_get ();
  uint32_t r = *p;

  random_bytes_free ((const uint8_t *)p);
  return r;
}
