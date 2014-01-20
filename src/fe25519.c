/*
 * fe25519.c -- 2^255-19 field element computation
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
 * but it is represented in 256-bit (it is redundant representation).
 */

#include <stdint.h>
#include <string.h>
#include "fe25519.h"

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
       : [d] "=&r" (d_), [c] "=&r" (c_)            \
       : "[d]" (d_), [w] "r" (w_)                \
       : "r4", "r5", "r6", "r7", "memory", "cc" )

#define MULADD_256(s_,d_,w_,c_)   do {           \
 asm ( "ldmia  %[s]!, { r8, r9, r10 } \n\t"      \
       "ldmia  %[d], { r5, r6, r7 }   \n\t"      \
       "umull  r4, r8, %[w], r8       \n\t"      \
       "adds   r5, r5, r4             \n\t"      \
       "adcs   r6, r6, r8             \n\t"      \
       "umull  r4, r8, %[w], r9       \n\t"      \
       "adc    %[c], r8, #0           \n\t"      \
       "adds   r6, r6, r4             \n\t"      \
       "adcs   r7, r7, %[c]           \n\t"      \
       "umull  r4, r8, %[w], r10      \n\t"      \
       "adc    %[c], r8, #0           \n\t"      \
       "adds   r7, r7, r4             \n\t"      \
       "stmia  %[d]!, { r5, r6, r7 }  \n\t"      \
       "ldmia  %[s]!, { r8, r9, r10 } \n\t"      \
       "ldmia  %[d], { r5, r6, r7 }   \n\t"      \
       "adcs   r5, r5, %[c]           \n\t"      \
       "umull  r4, r8, %[w], r8       \n\t"      \
       "adc    %[c], r8, #0           \n\t"      \
       "adds   r5, r5, r4             \n\t"      \
       "adcs   r6, r6, %[c]           \n\t"      \
       "umull  r4, r8, %[w], r9       \n\t"      \
       "adc    %[c], r8, #0           \n\t"      \
       "adds   r6, r6, r4             \n\t"      \
       "adcs   r7, r7, %[c]           \n\t"      \
       "umull  r4, r8, %[w], r10      \n\t"      \
       "adc    %[c], r8, #0           \n\t"      \
       "adds   r7, r7, r4             \n\t"      \
       "stmia  %[d]!, { r5, r6, r7 }  \n\t"      \
       "ldmia  %[s]!, { r8, r9 }      \n\t"      \
       "ldmia  %[d], { r5, r6 }       \n\t"      \
       "adcs   r5, r5, %[c]           \n\t"      \
       "umull  r4, r8, %[w], r8       \n\t"      \
       "adc    %[c], r8, #0           \n\t"      \
       "adds   r5, r5, r4             \n\t"      \
       "adcs   r6, r6, %[c]           \n\t"      \
       "umull  r4, r8, %[w], r9       \n\t"      \
       "adc    %[c], r8, #0           \n\t"      \
       "adds   r6, r6, r4             \n\t"      \
       "adc    %[c], %[c], #0         \n\t"      \
       "stmia  %[d]!, { r5, r6 }"                \
       : [s] "=&r" (s_), [d] "=&r" (d_), [c] "=&r" (c_)     \
       : "[s]" (s_), "[d]" (d_), [w] "r" (w_)            \
       : "r4", "r5", "r6", "r7", "r8", "r9", "r10",      \
         "memory", "cc" );                               \
  *d_ = c_;                                              \
} while (0)

static void mul_hlp (const uint32_t *s, uint32_t *d, uint32_t w)
{
  uint32_t c;

  MULADD_256 (s, d, w, c);
}


void fe_mul (fe25519 *X, const fe25519 *A, const fe25519 *B)
{
  uint32_t word[FE25519_WORDS*2];
  const uint32_t *s;
  uint32_t *d;
  uint32_t w;
  uint32_t c, c0;

  memset (word, 0, sizeof (uint32_t)*FE25519_WORDS);

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
  memcpy (X->word, word, sizeof X->word);
}
