/*                                                    -*- coding: utf-8 -*-
 * ecc-edwards.c - Elliptic curve computation for
 *                 the twisted Edwards curve: -x^2 + y^2 = 1 + d*x^2*y^2
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

#include <stdint.h>
#include <string.h>

#include "bn.h"
#include "mod.h"
#include "mod25638.h"

/*
 * Identity element: (0,1)
 * Negation: -(x,y) = (-x,y)
 *
 * d: -0x2DFC9311D490018C7338BF8688861767FF8FF5B2BEBE27548A14B235ECA6874A
 * order:
 *     0x1000000000000000000000000000000014DEF9DEA2F79CD65812631A5CF5D3ED
 * Gx: 0x216936D3CD6E53FEC0A4E231FDD6DC5C692CC7609525A7B2C9562D608F25D51A
 * Gy: 0x6666666666666666666666666666666666666666666666666666666666666658
 */

/* d + 2^255 - 19 */
static const bn256 coefficient_d[1] = {
  {{ 0x135978a3, 0x75eb4dca, 0x4141d8ab, 0x00700a4d,
     0x7779e898, 0x8cc74079, 0x2b6ffe73, 0x52036cee }} };


/**
 * @brief	Projective Twisted Coordinates
 */
typedef struct
{
  bn256 x[1];
  bn256 y[1];
  bn256 z[1];
} ptc;

#include "affine.h"


/**
 * @brief  X = 2 * A
 *
 * Compute (X3 : Y3 : Z3) = 2 * (X1 : Y1 : Z1)
 */
static void
ed_double_25638 (ptc *X, const ptc *A)
{
  uint32_t borrow;
  bn256 b[1], d[1], e[1];

  /* Compute: B = (X1 + Y1)^2 */
  mod25638_add (b, A->x, A->y);
  mod25638_sqr (b, b);

  /* Compute: C = X1^2        : E      */
  mod25638_sqr (e, A->x);

  /* Compute: D = Y1^2             */
  mod25638_sqr (d, A->y);

  /* E = aC; where a = -1 */
  /* Compute: E - D = -(C+D)  : Y3_tmp */
  mod25638_add (X->y, e, d);
  /* Negation: it can result borrow, as it is in redundant representation. */
  borrow = bn256_sub (X->y, n25638, X->y);
  if (borrow)
    bn256_add (X->y, X->y, n25638); /* carry ignored */
  else
    bn256_add (X->x, X->y, n25638); /* dummy calculation */

  /* Compute: F = E + D = D - C; where a = -1 : E */
  mod25638_sub (e, d, e);

  /* Compute: H = Z1^2        : D     */
  mod25638_sqr (d, A->z);

  /* Compute: J = F - 2*H     : D     */
  mod25638_add (d, d, d);
  mod25638_sub (d, e, d);

  /* Compute: X3 = (B-C-D)*J = (B+Y3_tmp)*J  */
  mod25638_add (X->x, b, X->y);
  mod25638_mul (X->x, X->x, d);

  /* Compute: Y3 = F*(E-D) = F*Y3_tmp            */
  mod25638_mul (X->y, X->y, e);

  /* Z3 = F*J             */
  mod25638_mul (X->z, e, d);
}


/**
 * @brief	X = A +/- B
 *
 * @param X	Destination PTC
 * @param A	PTC
 * @param B	AC
 * @param MINUS if 1 subtraction, addition otherwise.
 *
 * Compute: (X3 : Y3 : Z3) = (X1 : Y1 : Z1) + (X2 : Y2 : 1)
 */
static void
ed_add_25638 (ptc *X, const ptc *A, const ac *B, int minus)
{
  bn256 c[1], d[1], e[1];
#define minus_B_x X->x
  uint32_t borrow;

  /* Compute: C = X1 * X2 */
  borrow = bn256_sub (minus_B_x, n25638, B->x);
  if (borrow)
    bn256_add (minus_B_x, minus_B_x, n25638); /* carry ignored */
  else
    bn256_add (X->z, minus_B_x, n25638); /* dummy calculation */
  if (minus)
    mod25638_mul (c, A->x, minus_B_x);
  else
    mod25638_mul (c, A->x, B->x);
#undef minus_B_x

  /* Compute: D = Y1 * Y2 */
  mod25638_mul (d, A->y, B->y);

  /* Compute: E = d * C * D */
  mod25638_mul (e, c, d);
  mod25638_mul (e, coefficient_d, e);

  /* Compute: C_1 = C + D */
  mod25638_add (c, c, d);

  /* Compute: D_1 = Z1^2 : B */
  mod25638_sqr (d, A->z);

  /* Z3_tmp = D_1 - E : F */
  mod25638_sub (X->z, d, e);

  /* D_2 = D_1 + E : G */
  mod25638_add (d, d, e);

  /* X3_final = Z1 * Z3_tmp * ((X1 + Y1) * (X2 + Y2) - C_1) */
  mod25638_add (X->x, A->x, A->y);
  if (minus)
    mod25638_sub (e, B->y, B->x);
  else
    mod25638_add (e, B->x, B->y);
  mod25638_mul (e, X->x, e);
  mod25638_sub (e, e, c);
  mod25638_mul (e, X->z, e);
  mod25638_mul (X->x, A->z, e);

  /* Y3_final = Z1 * D_2 * C_1 */
  mod25638_mul (c, d, c);
  mod25638_mul (X->y, A->z, c);

  /* Z3_final = Z3_tmp * D_2 */
  mod25638_mul (X->z, X->z, d);

  /* A = Z1 */
  /* B = A^2 */
  /* C = X1 * X2 */
  /* D = Y1 * Y2 */
  /* E = d * C * D */
  /* F = B - E */
  /* G = B + E */
  /* X3 = A * F * ((X1 + Y1) * (X2 + Y2) - C - D) */
  /* Y3 = A * G * (D - aC); where a = -1 */
  /* Z3 = F * G */
}


static const bn256 p25519[1] = {
  {{ 0xffffffed, 0xffffffff, 0xffffffff, 0xffffffff,
     0xffffffff, 0xffffffff, 0xffffffff, 0x7fffffff }} };

/**
 * @brief	X = convert A
 *
 * @param X	Destination AC
 * @param A	PTC
 *
 * (X1:Y1:Z1) represents the affine point (x=X1/Z1, y=Y1/Z1) 
 */
static void
ptc_to_ac_25519 (ac *X, const ptc *A)
{
  uint32_t borrow;
  bn256 z[1], z_inv[1];

  /*
   * A->z may be bigger than p25519, or two times bigger than p25519.
   * We try to subtract p25519 twice.
   */
  borrow = bn256_sub (z_inv, A->z, p25519);
  if (borrow)
    memcpy (z_inv, A->z, sizeof (bn256));
  else
    memcpy (z, A->z, sizeof (bn256)); /* dumy copy */
  borrow = bn256_sub (z, z_inv, p25519);
  if (borrow)
    memcpy (z, z_inv, sizeof (bn256));
  else
    memcpy (z_inv, z, sizeof (bn256)); /* dumy copy */

  mod_inv (z_inv, z, p25519);

  mod25638_mul (X->x, A->x, z_inv);
  borrow = bn256_sub (z, X->x, p25519);
  if (borrow)
    memcpy (z, X->x, sizeof (bn256)); /* dumy copy */
  else
    memcpy (X->x, z, sizeof (bn256));

  mod25638_mul (X->y, A->y, z_inv);
  borrow = bn256_sub (z, X->y, p25519);
  if (borrow)
    memcpy (z, X->y, sizeof (bn256)); /* dumy copy */
  else
    memcpy (X->y, z, sizeof (bn256));
}


/**
 * @brief	X  = k * G
 *
 * @param K	scalar k
 *
 * Return -1 on error.
 * Return 0 on success.
 */
int
compute_kG_25519 (ac *X, const bn256 *K)
{
}


void
eddsa_25519 (bn256 *r, bn256 *s, const bn256 *z, const bn256 *d)
{
}


#if 0
/**
 * check if P is on the curve.
 *
 * Return -1 on error.
 * Return 0 on success.
 */
static int
point_is_on_the_curve (const ac *P)
{
  bn256 s[1], t[1];

  /* Twisted Edwards curve: a*x^2 + y^2 = 1 + d*x^2*y^2 */
}

int
compute_kP_25519 (ac *X, const bn256 *K, const ac *P);
#endif

#ifdef PRINT_OUT_TABLE
static const ptc G[1] = {{
  {{{ 0x8f25d51a, 0xc9562d60, 0x9525a7b2, 0x692cc760,
      0xfdd6dc5c, 0xc0a4e231, 0xcd6e53fe, 0x216936d3 }}},
  {{{ 0x66666658, 0x66666666, 0x66666666, 0x66666666,
      0x66666666, 0x66666666, 0x66666666, 0x66666666 }}},
  {{{ 1, 0, 0, 0, 0, 0, 0, 0 }}},
}};

#include <stdio.h>

static void
print_point (const ac *X)
{
  int i;

  for (i = 0; i < 8; i++)
    printf ("%08x\n", X->x->word[i]);
  puts ("");
  for (i = 0; i < 8; i++)
    printf ("%08x\n", X->y->word[i]);
}


static const uint8_t *str = "abcdefghijklmnopqrstuvwxyz0123456789";

const uint8_t *
random_bytes_get (void)
{
  return (const uint8_t *)str;
}

/*
 * Free pointer to random 32-byte
 */
void
random_bytes_free (const uint8_t *p)
{
  (void)p;
}


int
main (int argc, char *argv[])
{
  ac x[1];
  ptc a[1];
  int i;

  ed_double_25638 (a, G);
  ptc_to_ac_25519 (x, a);
  print_point (x);

  ed_add_25638 (a, G, G, 1);
  ptc_to_ac_25519 (x, a);
  print_point (x);

  ed_add_25638 (a, G, G, 0);
  ptc_to_ac_25519 (x, a);
  print_point (x);

  for (i = 0; i < 64 - 1; i++)
    ed_double_25638 (a, a);

  ptc_to_ac_25519 (x, a);
  print_point (x);
  return 0;
}
#endif
