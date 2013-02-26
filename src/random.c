/*
 * random.c -- get random bytes
 *
 * Copyright (C) 2010, 2011, 2012, 2013 Free Software Initiative of Japan
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
#include "neug.h"

#define RANDOM_BYTES_LENGTH 32
static uint32_t random_word[RANDOM_BYTES_LENGTH/sizeof (uint32_t)];

void
random_init (void)
{
  int i;

  neug_init (random_word, RANDOM_BYTES_LENGTH/sizeof (uint32_t));

  for (i = 0; i < NEUG_PRE_LOOP; i++)
    (void)neug_get (NEUG_KICK_FILLING);
}

void
random_fini (void)
{
  neug_fini ();
}

/*
 * Return pointer to random 32-byte
 */
const uint8_t *
random_bytes_get (void)
{
  neug_wait_full ();
  return (const uint8_t *)random_word;
}

/*
 * Free pointer to random 32-byte
 */
void
random_bytes_free (const uint8_t *p)
{
  (void)p;
  memset (random_word, 0, RANDOM_BYTES_LENGTH);
  neug_flush ();
}

/*
 * Return 4-byte salt
 */
uint32_t
get_salt (void)
{
  return neug_get (NEUG_KICK_FILLING);
}


#ifdef KEYGEN_SUPPORT
/*
 * Random byte iterator
 */
uint8_t
random_byte (void *arg)
{
  uint8_t *index_p = (uint8_t *)arg;
  uint8_t index = *index_p;
  uint8_t *p = ((uint8_t *)random_word) + index;
  uint8_t v;

  neug_wait_full ();

  v = *p;

  if (++index >= RANDOM_BYTES_LENGTH)
    {
      index = 0;
      neug_flush ();
    }

  *index_p = index;

  return v;
}
#endif
