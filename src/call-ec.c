/*
 * call-ec.c - interface between Gnuk and Elliptic curve over GF(prime)
 *
 * Copyright (C) 2013 Free Software Initiative of Japan
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

#include "field-group-select.h"

/* We are little endian.  */

#define ECDSA_BYTE_SIZE 32

int
FUNC(ecdsa_sign) (const uint8_t *hash, uint8_t *output,
		  const uint8_t *key_data)
{
  int i;
  bn256 r[1], s[1], z[1], d[1];
  uint8_t *p;

  p = (uint8_t *)d;
  for (i = 0; i < ECDSA_BYTE_SIZE; i++)
    p[ECDSA_BYTE_SIZE - i - 1] = key_data[i];

  p = (uint8_t *)z;
  for (i = 0; i < ECDSA_BYTE_SIZE; i++)
    p[ECDSA_BYTE_SIZE - i - 1] = hash[i];

  FUNC(ecdsa) (r, s, z, d);
  p = (uint8_t *)r;
  for (i = 0; i < ECDSA_BYTE_SIZE; i++)
    *output++ = p[ECDSA_BYTE_SIZE - i - 1];
  p = (uint8_t *)s;
  for (i = 0; i < ECDSA_BYTE_SIZE; i++)
    *output++ = p[ECDSA_BYTE_SIZE - i - 1];
  return 0;
}

uint8_t *
FUNC(ecdsa_compute_public) (const uint8_t *key_data)
{
  uint8_t *p0, *p, *p1;
  ac q[1];
  bn256 k[1];
  int i;

  p0 = (uint8_t *)malloc (ECDSA_BYTE_SIZE * 2);
  if (p0 == NULL)
    return NULL;

  p = (uint8_t *)k;
  for (i = 0; i < ECDSA_BYTE_SIZE; i++)
    p[ECDSA_BYTE_SIZE - i - 1] = key_data[i];
  if (FUNC(compute_kG) (q, k) < 0)
    {
      free (p0);
      return NULL;
    }

  p = p0;
  p1 = (uint8_t *)q->x;
  for (i = 0; i < ECDSA_BYTE_SIZE; i++)
    *p++ = p1[ECDSA_BYTE_SIZE - i - 1];
  p1 = (uint8_t *)q->y;
  for (i = 0; i < ECDSA_BYTE_SIZE; i++)
    *p++ = p1[ECDSA_BYTE_SIZE - i - 1];

  return p0;
}
