/*
 * modp256r1.c -- modulo arithmetic for p256r1
 *
 * Copyright (C) 2011, 2013, 2014 Free Software Initiative of Japan
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
#include "modp256r1.h"

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
const bn256 p256r1 = { {0xffffffff, 0xffffffff, 0xffffffff, 0x00000000,
			0x00000000, 0x00000000, 0x00000001, 0xffffffff} };

/*
 * Implementation Note.
 *
 * It's not always modulo p256r1.  The representation is redundant
 * during computation.  For example, when we add the prime - 1 and 1,
 * it won't overflow to 2^256, and the result is represented within
 * 256-bit.
 *
 * It is guaranteed that modp256r1_reduce reduces to modulo p256r1.
 */

/**
 * @brief  X = (A + B) mod p256r1
 */
void
modp256r1_add (bn256 *X, const bn256 *A, const bn256 *B)
{
  uint32_t carry;
  bn256 tmp[1];

  carry = bn256_add (X, A, B);
  if (carry)
    bn256_sub (X, X, P256R1);
  else
    bn256_sub (tmp, X, P256R1);
}

/**
 * @brief  X = (A - B) mod p256r1
 */
void
modp256r1_sub (bn256 *X, const bn256 *A, const bn256 *B)
{
  uint32_t borrow;
  bn256 tmp[1];

  borrow = bn256_sub (X, A, B);
  if (borrow)
    bn256_add (X, X, P256R1);
  else
    bn256_add (tmp, X, P256R1);
}

/**
 * @brief  X = A mod p256r1
 */
void
modp256r1_reduce (bn256 *X, const bn512 *A)
{
  bn256 tmp[1];
  uint32_t borrow;

#define S1 X
#define S2 tmp
#define S3 tmp
#define S4 tmp
#define S5 tmp
#define S6 tmp
#define S7 tmp
#define S8 tmp
#define S9 tmp

  S1->word[7] = A->word[7];
  S1->word[6] = A->word[6];
  S1->word[5] = A->word[5];
  S1->word[4] = A->word[4];
  S1->word[3] = A->word[3];
  S1->word[2] = A->word[2];
  S1->word[1] = A->word[1];
  S1->word[0] = A->word[0];
  /* X = S1 */

  S2->word[7] = A->word[15];
  S2->word[6] = A->word[14];
  S2->word[5] = A->word[13];
  S2->word[4] = A->word[12];
  S2->word[3] = A->word[11];
  S2->word[2] = S2->word[1] = S2->word[0] = 0;
  /* X += 2 * S2 */
  modp256r1_add (X, X, S2);
  modp256r1_add (X, X, S2);

  S3->word[7] = 0;
  S3->word[6] = A->word[15];
  S3->word[5] = A->word[14];
  S3->word[4] = A->word[13];
  S3->word[3] = A->word[12];
  S3->word[2] = S3->word[1] = S3->word[0] = 0;
  /* X += 2 * S3 */
  modp256r1_add (X, X, S3);
  modp256r1_add (X, X, S3);

  S4->word[7] = A->word[15];
  S4->word[6] = A->word[14];
  S4->word[5] = S4->word[4] = S4->word[3] = 0;
  S4->word[2] = A->word[10];
  S4->word[1] = A->word[9];
  S4->word[0] = A->word[8];
  /* X += S4 */
  modp256r1_add (X, X, S4);

  S5->word[7] = A->word[8];
  S5->word[6] = A->word[13];
  S5->word[5] = A->word[15];
  S5->word[4] = A->word[14];
  S5->word[3] = A->word[13];
  S5->word[2] = A->word[11];
  S5->word[1] = A->word[10];
  S5->word[0] = A->word[9];
  /* X += S5 */
  modp256r1_add (X, X, S5);

  S6->word[7] = A->word[10];
  S6->word[6] = A->word[8];
  S6->word[5] = S6->word[4] = S6->word[3] = 0;
  S6->word[2] = A->word[13];
  S6->word[1] = A->word[12];
  S6->word[0] = A->word[11];
  /* X -= S6 */
  modp256r1_sub (X, X, S6);

  S7->word[7] = A->word[11];
  S7->word[6] = A->word[9];
  S7->word[5] = S7->word[4] = 0;
  S7->word[3] = A->word[15];
  S7->word[2] = A->word[14];
  S7->word[1] = A->word[13];
  S7->word[0] = A->word[12];
  /* X -= S7 */
  modp256r1_sub (X, X, S7);

  S8->word[7] = A->word[12];
  S8->word[6] = 0;
  S8->word[5] = A->word[10];
  S8->word[4] = A->word[9];
  S8->word[3] = A->word[8];
  S8->word[2] = A->word[15];
  S8->word[1] = A->word[14];
  S8->word[0] = A->word[13];
  /* X -= S8 */
  modp256r1_sub (X, X, S8);

  S9->word[7] = A->word[13];
  S9->word[6] = 0;
  S9->word[5] = A->word[11];
  S9->word[4] = A->word[10];
  S9->word[3] = A->word[9];
  S9->word[2] = 0;
  S9->word[1] = A->word[15];
  S9->word[0] = A->word[14];
  /* X -= S9 */
  modp256r1_sub (X, X, S9);

  borrow = bn256_sub (tmp, X, P256R1);
  if (borrow)
    memcpy (tmp, X, sizeof (bn256));
  else
    memcpy (X, tmp, sizeof (bn256));

#undef S1
#undef S2
#undef S3
#undef S4
#undef S5
#undef S6
#undef S7
#undef S8
#undef S9
}

/**
 * @brief  X = (A * B) mod p256r1
 */
void
modp256r1_mul (bn256 *X, const bn256 *A, const bn256 *B)
{
  bn512 AB[1];

  bn256_mul (AB, A, B);
  modp256r1_reduce (X, AB);
}

/**
 * @brief  X = A * A mod p256r1
 */
void
modp256r1_sqr (bn256 *X, const bn256 *A)
{
  bn512 AA[1];

  bn256_sqr (AA, A);
  modp256r1_reduce (X, AA);
}

/**
 * @brief  C = (1 / a)  mod p256r1
 *
 * Return -1 on error.
 * Return 0 on success.
 */
#define MAX_N_BITS 256

int
modp256r1_inv (bn256 *C, const bn256 *a)
{
  bn256 u[1], v[1], tmp[1];
  bn256 A[1] = { { { 1, 0, 0, 0, 0, 0, 0, 0 } } };
  uint32_t carry;
  int n = MAX_N_BITS * 3;

  if (bn256_is_zero (a))
    return -1;

  memset (C, 0, sizeof (bn256));
  memcpy (u, a, sizeof (bn256));
  memcpy (v, P256R1, sizeof (bn256));

  while (n--)
    {
      int c = (bn256_is_even (u) << 1) + bn256_is_even (v);

      switch (c)
	{
	case 3:
	  bn256_shift (u, u, -1);
	  if (bn256_is_even (A))
	    {
	      bn256_add (tmp, A, P256R1);
	      carry = 0;
	    }
	  else
	    carry = bn256_add (A, A, P256R1);

	  bn256_shift (A, A, -1);
	  A->word[7] |= carry * 0x80000000;

	  bn256_shift (v, v, -1);
	  if (bn256_is_even (C))
	    {
	      bn256_add (tmp, C, P256R1);
	      carry = 0;
	    }
	  else
	    carry = bn256_add (C, C, P256R1);

	  bn256_shift (C, C, -1);
	  C->word[7] |= carry * 0x80000000;

	  if (bn256_is_ge (tmp, tmp))
	    {
	      bn256_sub (tmp, tmp, tmp);
	      modp256r1_sub (tmp, tmp, tmp);
	    }
	  else
	    {
	      bn256_sub (tmp, tmp, tmp);
	      modp256r1_sub (tmp, tmp, A);
	    }
	  break;

	case 1:
	  bn256_shift (tmp, tmp, -1);
	  if (bn256_is_even (tmp))
	    {
	      bn256_add (tmp, tmp, P256R1);
	      carry = 0;
	    }
	  else
	    carry = bn256_add (tmp, tmp, P256R1);

	  bn256_shift (tmp, tmp, -1);
	  tmp->word[7] |= carry * 0x80000000;

	  bn256_shift (v, v, -1);
	  if (bn256_is_even (C))
	    {
	      bn256_add (tmp, C, P256R1);
	      carry = 0;
	    }
	  else
	    carry = bn256_add (C, C, P256R1);

	  bn256_shift (C, C, -1);
	  C->word[7] |= carry * 0x80000000;

	  if (bn256_is_ge (tmp, tmp))
	    {
	      bn256_sub (tmp, tmp, tmp);
	      modp256r1_sub (tmp, tmp, tmp);
	    }
	  else
	    {
	      bn256_sub (tmp, tmp, tmp);
	      modp256r1_sub (tmp, tmp, A);
	    }
	  break;

	case 2:
	  bn256_shift (u, u, -1);
	  if (bn256_is_even (A))
	    {
	      bn256_add (tmp, A, P256R1);
	      carry = 0;
	    }
	  else
	    carry = bn256_add (A, A, P256R1);

	  bn256_shift (A, A, -1);
	  A->word[7] |= carry * 0x80000000;

	  bn256_shift (tmp, tmp, -1);
	  if (bn256_is_even (tmp))
	    {
	      bn256_add (tmp, tmp, P256R1);
	      carry = 0;
	    }
	  else
	    carry = bn256_add (tmp, tmp, P256R1);

	  bn256_shift (tmp, tmp, -1);
	  tmp->word[7] |= carry * 0x80000000;

	  if (bn256_is_ge (tmp, tmp))
	    {
	      bn256_sub (tmp, tmp, tmp);
	      modp256r1_sub (tmp, tmp, tmp);
	    }
	  else
	    {
	      bn256_sub (tmp, tmp, tmp);
	      modp256r1_sub (tmp, tmp, A);
	    }
	  break;

	case 0:
	  bn256_shift (tmp, tmp, -1);
	  if (bn256_is_even (tmp))
	    {
	      bn256_add (tmp, tmp, P256R1);
	      carry = 0;
	    }
	  else
	    carry = bn256_add (tmp, tmp, P256R1);

	  bn256_shift (tmp, tmp, -1);
	  tmp->word[7] |= carry * 0x80000000;

	  bn256_shift (tmp, tmp, -1);
	  if (bn256_is_even (tmp))
	    {
	      bn256_add (tmp, tmp, P256R1);
	      carry = 0;
	    }
	  else
	    carry = bn256_add (tmp, tmp, P256R1);

	  bn256_shift (tmp, tmp, -1);
	  tmp->word[7] |= carry * 0x80000000;

	  if (bn256_is_ge (u, v))
	    {
	      bn256_sub (u, u, v);
	      modp256r1_sub (A, A, C);
	    }
	  else
	    {
	      bn256_sub (v, v, u);
	      modp256r1_sub (C, C, A);
	    }
	  break;
	}
    }

  return 0;
}

/**
 * @brief  X = (A << shift) mod p256r1
 * @note   shift < 32
 */
void
modp256r1_shift (bn256 *X, const bn256 *A, int shift)
{
  uint32_t carry;
#define borrow carry
  bn256 tmp[1];

  carry = bn256_shift (X, A, shift);
  if (shift < 0)
    return;

  memset (tmp, 0, sizeof (bn256));
  tmp->word[7] = carry;
  tmp->word[0] = carry;
  modp256r1_add (X, X, tmp);

  tmp->word[7] = 0;
  tmp->word[0] = 0;
  tmp->word[6] = carry;
  tmp->word[3] = carry;
  modp256r1_sub (X, X, tmp);

  borrow = bn256_sub (tmp, X, P256R1);
  if (borrow)
    memcpy (tmp, X, sizeof (bn256));
  else
    memcpy (X, tmp, sizeof (bn256));
#undef borrow
}
