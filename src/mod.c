/*
 * mod.c -- modulo arithmetic
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
#include <string.h>
#include "bn.h"

/**
 * @brief X = A mod B (using MU=(1<<(256)+MU_lower)) (Barret reduction)
 * 
 */
void
mod_reduce (bn256 *X, const bn512 *A, const bn256 *B, const bn256 *MU_lower)
{
  bn256 q[1];
  bn512 q_big[1], tmp[1];
  uint32_t carry;
#define borrow carry
  uint32_t borrow_next;

  memset (q, 0, sizeof (bn256));
  q->words[0] = A->words[15];
  bn256_mul (tmp, q, MU_lower);
  tmp->words[8] += A->words[15];
  carry = (tmp->words[8] < A->words[15]);
  tmp->words[9] += carry;

  q->words[7] = A->words[14];
  q->words[6] = A->words[13];
  q->words[5] = A->words[12];
  q->words[4] = A->words[11];
  q->words[3] = A->words[10];
  q->words[2] = A->words[9];
  q->words[1] = A->words[8];
  q->words[0] = A->words[7];
  bn256_mul (q_big, q, MU_lower);
  bn256_add ((bn256 *)&q_big->words[8], (bn256 *)&q_big->words[8], q);

  q->words[0] = q_big->words[9] + tmp->words[1];
  carry = (q->words[0] < tmp->words[1]);

  q->words[1] = q_big->words[10] + carry;
  carry = (q->words[1] < carry);
  q->words[1] += tmp->words[2];
  carry += (q->words[1] < tmp->words[2]);

  q->words[2] = q_big->words[11] + carry;
  carry = (q->words[2] < carry);
  q->words[2] += tmp->words[3];
  carry += (q->words[2] < tmp->words[3]);

  q->words[3] = q_big->words[12] + carry;
  carry = (q->words[3] < carry);
  q->words[3] += tmp->words[4];
  carry += (q->words[3] < tmp->words[4]);

  q->words[4] = q_big->words[13] + carry;
  carry = (q->words[4] < carry);
  q->words[4] += tmp->words[5];
  carry += (q->words[4] < tmp->words[5]);

  q->words[5] = q_big->words[14] + carry;
  carry = (q->words[5] < carry);
  q->words[5] += tmp->words[6];
  carry += (q->words[5] < tmp->words[6]);

  q->words[6] = q_big->words[15] + carry;
  carry = (q->words[6] < carry);
  q->words[6] += tmp->words[7];
  carry += (q->words[6] < tmp->words[7]);

  q->words[7] = carry;
  q->words[7] += tmp->words[8];
  carry = (q->words[7] < tmp->words[8]);

  memset (q_big, 0, sizeof (bn512));
  q_big->words[8] = A->words[8];
  q_big->words[7] = A->words[7];
  q_big->words[6] = A->words[6];
  q_big->words[5] = A->words[5];
  q_big->words[4] = A->words[4];
  q_big->words[3] = A->words[3];
  q_big->words[2] = A->words[2];
  q_big->words[1] = A->words[1];
  q_big->words[0] = A->words[0];

  bn256_mul (tmp, q, B);
  if (carry)
    tmp->words[8] += B->words[0];
  tmp->words[15] = tmp->words[14] = tmp->words[13] = tmp->words[12]
    = tmp->words[11] = tmp->words[10] = tmp->words[9] = 0;

  borrow = bn256_sub (X, (bn256 *)&q_big->words[0], (bn256 *)&tmp->words[0]);
  borrow_next = (q_big->words[8] < borrow);
  q_big->words[8] -= borrow;
  borrow_next += (q_big->words[8] < tmp->words[8]);
  q_big->words[8] -= tmp->words[8];

  carry = q_big->words[8];
  while (carry)
    {
      borrow_next = bn256_sub (X, X, B);
      carry -= borrow_next;
    }

  if (bn256_is_ge (X, B))
    bn256_sub (X, X, B);
}

/**
 * @brief C = X^(-1) mod N
 * 
 */
void
mod_inv (bn256 *C, const bn256 *X, const bn256 *N)
{
  bn256 u[1], v[1];
  bn256 A[1] = { { { 1, 0, 0, 0, 0, 0, 0, 0 } } };

  memset (C, 0, sizeof (bn256));
  memcpy (u, X, sizeof (bn256));
  memcpy (v, N, sizeof (bn256));

  while (!bn256_is_zero (u))
    {
      while (bn256_is_even (u))
	{
	  bn256_shift (u, u, -1);
	  if (bn256_is_even (A))
	    bn256_shift (A, A, -1);
	  else
	    {
	      int carry = bn256_add (A, A, N);

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
	      int carry = bn256_add (C, C, N);

	      bn256_shift (C, C, -1);
	      if (carry)
		C->words[7] |= 0x80000000;
	    }
	}

      if (bn256_is_ge (u, v))
	{
	  int borrow;

	  bn256_sub (u, u, v);
	  borrow = bn256_sub (A, A, C);
	  if (borrow)
	    bn256_add (A, A, N);
	}
      else
	{
	  int borrow;

	  bn256_sub (v, v, u);
	  borrow = bn256_sub (C, C, A);
	  if (borrow)
	    bn256_add (C, C, N);
	}
    }
}
