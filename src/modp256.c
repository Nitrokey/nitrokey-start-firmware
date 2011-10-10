/*
 * modp256.c -- modulo P256 arithmetic
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

/*
 * p256 =  2^256 - 2^224 + 2^192 + 2^96 - 1
 */
#include <stdint.h>
#include <string.h>

#include "bn.h"
#include "modp256.h"

/*
256      224      192      160      128       96       64       32        0
2^256
  1 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
2^256 - 2^224
  0 ffffffff 00000000 00000000 00000000 00000000 00000000 00000000 00000000
2^256 - 2^224 + 2^192
  0 ffffffff 00000001 00000000 00000000 00000000 00000000 00000000 00000000
2^256 - 2^224 + 2^192 + 2^96
  0 ffffffff 00000001 00000000 00000000 00000001 00000000 00000000 00000000
2^256 - 2^224 + 2^192 + 2^96 - 1
  0 ffffffff 00000001 00000000 00000000 00000000 ffffffff ffffffff ffffffff
*/
bn256 p256 = { {0xffffffff, 0xffffffff, 0xffffffff, 0x00000000,
		0x00000000, 0x00000000, 0x00000001, 0xffffffff} };

/**
 * @brief  X = (A + B) mod p256
 */
void
modp256_add (bn256 *X, const bn256 *A, const bn256 *B)
{
  int carry;

  carry = bn256_add (X, A, B);
  if (carry)
    bn256_sub (X, X, P256);
}

/**
 * @brief  X = (A - B) mod p256
 */
void
modp256_sub (bn256 *X, bn256 *A, bn256 *B)
{
  int borrow;

  borrow = bn256_sub (X, A, B);
  if (borrow)
    bn256_add (X, X, P256);
}

/**
 * @brief  X = A mod p256
 */
void
modp256_reduce (bn256 *X, bn512 *A)
{
  bn256 tmp[1];

#define S1 X
#define S2 tmp
#define S3 tmp
#define S4 tmp
#define S5 tmp
#define S6 tmp
#define S7 tmp
#define S8 tmp
#define S9 tmp

  S1->words[7] = A->words[7];
  S1->words[6] = A->words[6];
  S1->words[5] = A->words[5];
  S1->words[4] = A->words[4];
  S1->words[3] = A->words[3];
  S1->words[2] = A->words[2];
  S1->words[1] = A->words[1];
  S1->words[0] = A->words[0];
  /* X = S1 */

  S2->words[7] = A->words[15];
  S2->words[6] = A->words[14];
  S2->words[5] = A->words[13];
  S2->words[4] = A->words[12];
  S2->words[3] = A->words[11];
  S2->words[2] = S2->words[1] = S2->words[0] = 0;
  /* X += 2 * S2 */
  modp256_add (X, X, S2);
  modp256_add (X, X, S2);

  S3->words[7] = 0;
  S3->words[6] = A->words[15];
  S3->words[5] = A->words[14];
  S3->words[4] = A->words[13];
  S3->words[3] = A->words[12];
  S3->words[2] = S3->words[1] = S3->words[0] = 0;
  /* X += 2 * S3 */
  modp256_add (X, X, S3);
  modp256_add (X, X, S3);

  S4->words[7] = A->words[15];
  S4->words[6] = A->words[14];
  S4->words[5] = S4->words[4] = S4->words[3] = 0;
  S4->words[2] = A->words[10];
  S4->words[1] = A->words[9];
  S4->words[0] = A->words[8];
  /* X += S4 */
  modp256_add (X, X, S4);

  S5->words[7] = A->words[8];
  S5->words[6] = A->words[13];
  S5->words[5] = A->words[15];
  S5->words[4] = A->words[14];
  S5->words[3] = A->words[13];
  S5->words[2] = A->words[11];
  S5->words[1] = A->words[10];
  S5->words[0] = A->words[9];
  /* X += S5 */
  modp256_add (X, X, S5);

  S6->words[7] = A->words[10];
  S6->words[6] = A->words[8];
  S6->words[5] = S6->words[4] = S6->words[3] = 0;
  S6->words[2] = A->words[13];
  S6->words[1] = A->words[12];
  S6->words[0] = A->words[11];
  /* X -= S6 */
  modp256_sub (X, X, S6);

  S7->words[7] = A->words[11];
  S7->words[6] = A->words[9];
  S7->words[5] = S7->words[4] = 0;
  S7->words[3] = A->words[15];
  S7->words[2] = A->words[14];
  S7->words[1] = A->words[13];
  S7->words[0] = A->words[12];
  /* X -= S7 */
  modp256_sub (X, X, S7);

  S8->words[7] = A->words[12];
  S8->words[6] = 0;
  S8->words[5] = A->words[10];
  S8->words[4] = A->words[9];
  S8->words[3] = A->words[8];
  S8->words[2] = A->words[15];
  S8->words[1] = A->words[14];
  S8->words[0] = A->words[13];
  /* X -= S8 */
  modp256_sub (X, X, S8);

  S9->words[7] = A->words[13];
  S9->words[6] = 0;
  S9->words[5] = A->words[11];
  S9->words[4] = A->words[10];
  S9->words[3] = A->words[9];
  S9->words[2] = 0;
  S9->words[1] = A->words[15];
  S9->words[0] = A->words[14];
  /* X -= S9 */
  modp256_sub (X, X, S9);

  if (bn256_is_ge (X, P256))
    bn256_sub (X, X, P256);
}

/**
 * @brief  X = (A * B) mod p256
 */
void
modp256_mul (bn256 *X, bn256 *A, bn256 *B)
{
  bn512 AB[1];

  bn256_mul (AB, A, B);
  modp256_reduce (X, AB);
}

/**
 * @brief  X = A * A mod p256
 */
void
modp256_sqr (bn256 *X, bn256 *A)
{
  bn512 AA[1];

  bn256_sqr (AA, A);
  modp256_reduce (X, AA);
}

/**
 * @brief  C = (1 / a)  mod p256
 */
void
modp256_inv (bn256 *C, const bn256 *a)
{
  bn256 u[1], v[1];
  bn256 A[1] = { { { 1, 0, 0, 0, 0, 0, 0, 0 } } };

  memset (C, 0, sizeof (bn256));
  memcpy (u, a, sizeof (bn256));
  memcpy (v, P256, sizeof (bn256));

  while (!bn256_is_zero (u))
    {
      while (bn256_is_even (u))
	{
	  bn256_shift (u, u, -1);
	  if (bn256_is_even (A))
	    bn256_shift (A, A, -1);
	  else
	    {
	      int carry = bn256_add (A, A, P256);

	      bn256_shift (A, A, -1);
	      if (carry)
		A->words[7] |= 0x80000000;
	    }
	}

      while (bn256_is_even (v))
	{
	  bn256_shift (v, v, -1);
	  if (bn256_is_even (C))
	    bn256_shift (C, C, -1);
	  else
	    {
	      int carry = bn256_add (C, C, P256);

	      bn256_shift (C, C, -1);
	      if (carry)
		C->words[7] |= 0x80000000;
	    }
	}

      if (bn256_is_ge (u, v))
	{
	  bn256_sub (u, u, v);
	  modp256_sub (A, A, C);
	}
      else
	{
	  bn256_sub (v, v, u);
	  modp256_sub (C, C, A);
	}
    }
}
