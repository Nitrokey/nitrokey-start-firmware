/*
 * mod25638.c -- modulo arithmetic of 2^256-38 for 2^255-19 field
 *
 * Copyright (C) 2014 Free Software Initiative of Japan
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
 * The field is \Z/(2^255-19)
 *
 * We use radix-32.  During computation, it's not reduced to 2^255-19,
 * but it is represented in 256-bit (it is redundant representation),
 * that is, 2^256-38.
 *
 * The idea is, to keep 2^256-38 until it will be converted to affine
 * coordinates.
 */

#include <stdint.h>
#include <string.h>

#include "bn.h"
#include "mod25638.h"

#ifndef BN256_C_IMPLEMENTATION
#define ASM_IMPLEMENTATION 1
#endif

#if ASM_IMPLEMENTATION
#include "muladd_256.h"
#define ADDWORD_256(d_,w_,c_)                    \
 asm ( "ldmia  %[d], { r4, r5, r6, r7 } \n\t"    \
       "adds   r4, r4, %[w]             \n\t"    \
       "adcs   r5, r5, #0               \n\t"    \
       "adcs   r6, r6, #0               \n\t"    \
       "adcs   r7, r7, #0               \n\t"    \
       "stmia  %[d]!, { r4, r5, r6, r7 }\n\t"    \
       "ldmia  %[d], { r4, r5, r6, r7 } \n\t"    \
       "adcs   r4, r4, #0               \n\t"    \
       "adcs   r5, r5, #0               \n\t"    \
       "adcs   r6, r6, #0               \n\t"    \
       "adcs   r7, r7, #0               \n\t"    \
       "stmia  %[d]!, { r4, r5, r6, r7 }\n\t"    \
       "mov    %[c], #0                 \n\t"    \
       "adc    %[c], %[c], #0"                   \
       : [d] "=&r" (d_), [c] "=&r" (c_)          \
       : "[d]" (d_), [w] "r" (w_)                \
       : "r4", "r5", "r6", "r7", "memory", "cc" )
#endif

/*
256      224      192      160      128       96       64       32        0
2^256
  1 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
2^256 - 32
  0 ffffffff ffffffff ffffffff ffffffff ffffffff ffffffff ffffffff ffffffe0
2^256 - 32 - 4
  0 ffffffff ffffffff ffffffff ffffffff ffffffff ffffffff ffffffff ffffffdc
2^256 - 32 - 4 - 2
  0 ffffffff ffffffff ffffffff ffffffff ffffffff ffffffff ffffffff ffffffda
*/
const bn256 n25638[1] = {
  {{0xffffffda, 0xffffffff, 0xffffffff, 0xffffffff,
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff }} };


/*
 * Implementation Note.
 *
 * It's not always modulo n25638.  The representation is redundant
 * during computation.  For example, when we add the number - 1 and 1,
 * it won't overflow to 2^256, and the result is represented within
 * 256-bit.
 */

/**
 * @brief  X = (A + B) mod 2^256-38
 */
void
mod25638_add (bn256 *X, const bn256 *A, const bn256 *B)
{
  uint32_t carry;
  bn256 tmp[1];

  carry = bn256_add (X, A, B);
  if (carry)
    bn256_sub (X, X, n25638);
  else
    bn256_sub (tmp, X, n25638);
}

/**
 * @brief  X = (A - B) mod 2^256-38
 */
void
mod25638_sub (bn256 *X, const bn256 *A, const bn256 *B)
{
  uint32_t borrow;
  bn256 tmp[1];

  borrow = bn256_sub (X, A, B);
  if (borrow)
    bn256_add (X, X, n25638);
  else
    bn256_add (tmp, X, n25638);
}


/**
 * @brief  X = (A * B) mod 2^256-38
 */
void
mod25638_mul (bn256 *X, const bn256 *A, const bn256 *B)
{
  uint32_t word[BN256_WORDS*2];
  const uint32_t *s;
  uint32_t *d;
  uint32_t w;
  uint32_t c, c0;

#if ASM_IMPLEMENTATION
  memset (word, 0, sizeof (uint32_t)*BN256_WORDS);

  s = A->word;  d = &word[0];  w = B->word[0];  MULADD_256 (s, d, w, c);
  s = A->word;  d = &word[1];  w = B->word[1];  MULADD_256 (s, d, w, c);
  s = A->word;  d = &word[2];  w = B->word[2];  MULADD_256 (s, d, w, c);
  s = A->word;  d = &word[3];  w = B->word[3];  MULADD_256 (s, d, w, c);
  s = A->word;  d = &word[4];  w = B->word[4];  MULADD_256 (s, d, w, c);
  s = A->word;  d = &word[5];  w = B->word[5];  MULADD_256 (s, d, w, c);
  s = A->word;  d = &word[6];  w = B->word[6];  MULADD_256 (s, d, w, c);
  s = A->word;  d = &word[7];  w = B->word[7];  MULADD_256 (s, d, w, c);
  s = &word[8]; d = &word[0];  w = 38;          MULADD_256 (s, d, w, c);
  c0 = word[8] * 38;
  s = word;
  ADDWORD_256 (s, c0, c);
  word[0] += c * 38;
  memcpy (X, word, sizeof (bn256));
#else
  (void)c;  (void)c0;
  bn256_mul ((bn512 *)word, A, B);

  s = &word[8]; d = &word[0];  w = 38;
  {
    int i;
    uint32_t r0, r1;

    r0 = r1 = 0;
    for (i = 0; i < BN256_WORDS; i++)
      {
	uint64_t uv;
	uint32_t u, v;
	uint32_t carry;

	r0 += d[i];
	r1 += (r0 < d[i]);
	carry = (r1 < (r0 < d[i]));

	uv = ((uint64_t)s[i])*w;
	v = uv;
	u = (uv >> 32);
	r0 += v;
	r1 += (r0 < v);
	carry += (r1 < (r0 < v));
	r1 += u;
	carry += (r1 < u);

	d[i] = r0;
	r0 = r1;
	r1 = carry;
      }
    d[i] = r0;

    r0 = word[8] * 38;
    d = word;
    for (i = 0; i < BN256_WORDS; i++)
      {
	uint32_t carry;

	r0 += d[i];
	carry = (r0 < d[i]);
	d[i] = r0;
	r0 = carry;
      }
    word[0] += r0 * 38;
  }

  memcpy (X, word, sizeof (bn256));
#endif
}

/**
 * @brief  X = A * A mod 2^256-38
 */
void
mod25638_sqr (bn256 *X, const bn256 *A)
{
  /* This could be improved a bit, see bn256_sqr.  */
  mod25638_mul (X, A, A);
}


/**
 * @brief  X = (A << shift) mod 2^256-38
 * @note   shift < 32
 */
void
mod25638_shift (bn256 *X, const bn256 *A, int shift)
{
  uint32_t carry;
  bn256 tmp[1];

  carry = bn256_shift (X, A, shift);
  if (shift < 0)
    return;

  memset (tmp, 0, sizeof (bn256));
  tmp->word[0] = (carry << 1);
  /* tmp->word[1] = (carry >> 31);  always zero.  */
  tmp->word[0] = tmp->word[0] + (carry << 2);
  tmp->word[1] = (tmp->word[0] < (carry << 2)) + (carry >> 30);
  tmp->word[0] = tmp->word[0] + (carry << 5);
  tmp->word[1] = tmp->word[1] + (tmp->word[0] < (carry << 5)) + (carry >> 27);

  mod25638_add (X, X, tmp);
}
