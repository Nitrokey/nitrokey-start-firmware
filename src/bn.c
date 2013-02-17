/*
 * bn.c -- 256-bit (and 512-bit) bignum calculation
 *
 * Copyright (C) 2011 Free Software Initiative of Japan
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

#include <stdint.h>
#include "bn.h"

uint32_t
bn256_add (bn256 *X, const bn256 *A, const bn256 *B)
{
  int i;
  uint32_t carry = 0;
  uint32_t *px;
  const uint32_t *pa, *pb;

  px = X->words;
  pa = A->words;
  pb = B->words;

  for (i = 0; i < BN256_WORDS; i++)
    {
      *px = *pa + carry;
      carry = (*px < carry);
      *px += *pb;
      carry += (*px < *pb);
      px++;
      pa++;
      pb++;
    }

  return carry;
}

uint32_t
bn256_sub (bn256 *X, const bn256 *A, const bn256 *B)
{
  int i;
  uint32_t borrow = 0;
  uint32_t *px;
  const uint32_t *pa, *pb;

  px = X->words;
  pa = A->words;
  pb = B->words;

  for (i = 0; i < BN256_WORDS; i++)
    {
      uint32_t borrow0 = (*pa < borrow);

      *px = *pa - borrow;
      borrow = (*px < *pb) + borrow0;
      *px -= *pb;
      px++;
      pa++;
      pb++;
    }

  return borrow;
}

uint32_t
bn256_add_uint (bn256 *X, const bn256 *A, uint32_t w)
{
  int i;
  uint32_t carry = 0;
  uint32_t *px;
  const uint32_t *pa;

  px = X->words;
  pa = A->words;

  for (i = 0; i < BN256_WORDS; i++)
    {
      *px = *pa + carry;
      carry = (*px < carry);
      if (i == 0)
	{
	  *px += w;
	  carry += (*px < w);
	}

      px++;
      pa++;
    }

  return carry;
}

uint32_t
bn256_sub_uint (bn256 *X, const bn256 *A, uint32_t w)
{
  int i;
  uint32_t borrow = 0;
  uint32_t *px;
  const uint32_t *pa;

  px = X->words;
  pa = A->words;

  for (i = 0; i < BN256_WORDS; i++)
    {
      uint32_t borrow0 = (*pa < borrow);

      *px = *pa - borrow;
      if (i == 0)
	{
	  borrow = (*px < w) + borrow0;
	  *px -= w;
	}
      else
	borrow = borrow0;

      px++;
      pa++;
    }

  return borrow;
}

void
bn256_mul (bn512 *X, const bn256 *A, const bn256 *B)
{
  int i, j, k;
  int i_beg, i_end;
  uint32_t r0, r1, r2;

  r0 = r1 = r2 = 0;
  for (k = 0; k <= (BN256_WORDS - 1)*2; k++)
    {
      if (k < BN256_WORDS)
	{
	  i_beg = 0;
	  i_end = k;
	}
      else
	{
	  i_beg = k - BN256_WORDS + 1;
	  i_end = BN256_WORDS - 1;
	}

      for (i = i_beg; i <= i_end; i++)
	{
	  uint64_t uv;
	  uint32_t u, v;
	  uint32_t carry;

	  j = k - i;

	  uv = ((uint64_t )A->words[i])*((uint64_t )B->words[j]);
	  v = uv;
	  u = (uv >> 32);
	  r0 += v;
	  carry = (r0 < v);
	  r1 += carry;
	  carry = (r1 < carry);
	  r1 += u;
	  carry += (r1 < u);
	  r2 += carry;
	}

      X->words[k] = r0;
      r0 = r1;
      r1 = r2;
      r2 = 0;
    }

  X->words[k] = r0;
}

void
bn256_sqr (bn512 *X, const bn256 *A)
{
  int i, j, k;
  int i_beg, i_end;
  uint32_t r0, r1, r2;

  r0 = r1 = r2 = 0;
  for (k = 0; k <= (BN256_WORDS - 1)*2; k++)
    {
      if (k < BN256_WORDS)
	{
	  i_beg = 0;
	  i_end = k/2;
	}
      else
	{
	  i_beg = k - BN256_WORDS + 1;
	  i_end = k/2;
	}

      for (i = i_beg; i <= i_end; i++)
	{
	  uint64_t uv;
	  uint32_t u, v;
	  uint32_t carry;

	  j = k - i;

	  uv = ((uint64_t )A->words[i])*((uint64_t )A->words[j]);
	  if (i < j)
	    {
	      if ((uv >> 63) != 0)
		r2++;
	      uv <<= 1;
	    }
	  v = uv;
	  u = (uv >> 32);
	  r0 += v;
	  carry = (r0 < v);
	  r1 += carry;
	  carry = (r1 < carry);
	  r1 += u;
	  carry += (r1 < u);
	  r2 += carry;
	}

      X->words[k] = r0;
      r0 = r1;
      r1 = r2;
      r2 = 0;
    }

  X->words[k] = r0;
}

uint32_t
bn256_shift (bn256 *X, const bn256 *A, int shift)
{
  int i;
  uint32_t carry = 0, next_carry;

  if (shift > 0)
    {
      for (i = 0; i < BN256_WORDS; i++)
	{
	  next_carry = A->words[i] >> (32 - shift);
	  X->words[i] = (A->words[i] << shift) | carry;
	  carry = next_carry;
	}
    }
  else
    {
      shift = -shift;

      for (i = BN256_WORDS - 1; i >= 0; i--)
	{
	  next_carry = A->words[i] & ((1 << shift) - 1);
	  X->words[i] = (A->words[i] >> shift) | (carry << (32 - shift));
	  carry = next_carry;
	}
    }

  return carry;
}

int
bn256_is_zero (const bn256 *X)
{
  int i;

  for (i = 0; i < BN256_WORDS; i++)
    if (X->words[i] != 0)
      return 0;

  return 1;
}

int
bn256_is_even (const bn256 *X)
{
  return !(X->words[0] & 1);
}

int
bn256_is_ge (const bn256 *A, const bn256 *B)
{
  int i;

  for (i = BN256_WORDS - 1; i >= 0; i--)
    if (A->words[i] > B->words[i])
      return 1;
    else if (A->words[i] < B->words[i])
      return 0;

  return 1;
}

void
bn256_random (bn256 *X)
{
  const uint8_t *rand = random_bytes_get ();

  X->words[7] = ((uint32_t *)rand)[7];
  X->words[6] = ((uint32_t *)rand)[6];
  X->words[5] = ((uint32_t *)rand)[5];
  X->words[4] = ((uint32_t *)rand)[4];
  X->words[3] = ((uint32_t *)rand)[3];
  X->words[2] = ((uint32_t *)rand)[2];
  X->words[1] = ((uint32_t *)rand)[1];
  X->words[0] = ((uint32_t *)rand)[0];

  random_bytes_free (rand);
}
