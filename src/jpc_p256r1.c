/*
 * jpc_p256r1.c -- arithmetic on Jacobian projective coordinates for p256r1.
 *
 * Copyright (C) 2011, 2013 Free Software Initiative of Japan
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
#include "modp256r1.h"
#include "jpc-ac_p256r1.h"

/**
 * @brief	X = 2 * A
 *
 * @param X	Destination JPC
 * @param A	JPC
 */
void
jpc_double_p256r1 (jpc *X, const jpc *A)
{
  bn256 a[1], b[1], c[1], tmp0[1];
  bn256 *d;

  if (bn256_is_zero (A->z))		/* A is infinite */
    return;

  d = X->x;
  modp256r1_sqr (a, A->y);
  memcpy (b, a, sizeof (bn256));
  modp256r1_mul (a, a, A->x);
  modp256r1_shift (a, a, 2);

  modp256r1_sqr (b, b);
  modp256r1_shift (b, b, 3);

  modp256r1_sqr (tmp0, A->z);
  modp256r1_sub (c, A->x, tmp0);
  modp256r1_add (tmp0, tmp0, A->x);
  modp256r1_mul (tmp0, tmp0, c);
  modp256r1_shift (c, tmp0, 1);
  modp256r1_add (c, c, tmp0);

  modp256r1_sqr (d, c);
  modp256r1_shift (tmp0, a, 1);
  modp256r1_sub (d, d, tmp0);

  modp256r1_mul (X->z, A->y, A->z);
  modp256r1_shift (X->z, X->z, 1);

  modp256r1_sub (tmp0, a, d);
  modp256r1_mul (tmp0, c, tmp0);
  modp256r1_sub (X->y, tmp0, b);
}

/**
 * @brief	X = A + B
 *
 * @param X	Destination JPC
 * @param A	JPC
 * @param B	AC
 * @param MINUS if 1 subtraction, addition otherwise.
 */
void
jpc_add_ac_signed_p256r1 (jpc *X, const jpc *A, const ac *B, int minus)
{
  bn256 a[1], b[1], c[1], d[1], tmp[1];
#define minus_B_y c
#define c_sqr a
#define c_cube b
#define x1_c_sqr c
#define x1_c_sqr_2 c
#define c_cube_plus_x1_c_sqr_2 c
#define x1_c_sqr_copy a
#define y3_tmp c
#define y1_c_cube a

  if (bn256_is_zero (A->z))		/* A is infinite */
    {
      memcpy (X->x, B->x, sizeof (bn256));
      if (minus)
	{
	  memcpy (tmp, B->y, sizeof (bn256));
	  bn256_sub (X->y, P256, B->y);
	}
      else
	{
	  memcpy (X->y, B->y, sizeof (bn256));
	  bn256_sub (tmp, P256, B->y);
	}
      memset (X->z, 0, sizeof (bn256));
      X->z->word[0] = 1;
      return;
    }

  modp256r1_sqr (a, A->z);
  memcpy (b, a, sizeof (bn256));
  modp256r1_mul (a, a, B->x);

  modp256r1_mul (b, b, A->z);
  if (minus)
    {
      bn256_sub (minus_B_y, P256, B->y);
      modp256r1_mul (b, b, minus_B_y);
    }
  else
    {
      bn256_sub (tmp, P256, B->y);
      modp256r1_mul (b, b, B->y);
    }

  if (bn256_cmp (A->x, a) == 0 && bn256_cmp (A->y, b) == 0)
    {
      jpc_double (X, A);
      return;
    }

  modp256r1_sub (c, a, A->x);
  modp256r1_sub (d, b, A->y);

  modp256r1_mul (X->z, A->z, c);

  modp256r1_sqr (c_sqr, c);
  modp256r1_mul (c_cube, c_sqr, c);

  modp256r1_mul (x1_c_sqr, A->x, c_sqr);

  modp256r1_sqr (X->x, d);
  memcpy (x1_c_sqr_copy, x1_c_sqr, sizeof (bn256));
  modp256r1_shift (x1_c_sqr_2, x1_c_sqr, 1);
  modp256r1_add (c_cube_plus_x1_c_sqr_2, x1_c_sqr_2, c_cube);
  modp256r1_sub (X->x, X->x, c_cube_plus_x1_c_sqr_2);

  modp256r1_sub (y3_tmp, x1_c_sqr_copy, X->x);
  modp256r1_mul (y3_tmp, y3_tmp, d);
  modp256r1_mul (y1_c_cube, A->y, c_cube);
  modp256r1_sub (X->y, y3_tmp, y1_c_cube);
}

/**
 * @brief	X = A + B
 *
 * @param X	Destination JPC
 * @param A	JPC
 * @param B	AC
 */
void
jpc_add_ac_p256r1 (jpc *X, const jpc *A, const ac *B)
{
  jpc_add_ac_signed_p256r1 (X, A, B, 0);
}

/**
 * @brief	X = convert A
 *
 * @param X	Destination AC
 * @param A	JPC
 *
 * Return -1 on error (infinite).
 * Return 0 on success.
 */
int
jpc_to_ac_p256r1 (ac *X, const jpc *A)
{
  bn256 z_inv[1], z_inv_sqr[1];

  if (modp256r1_inv (z_inv, A->z) < 0)
    return -1;

  modp256r1_sqr (z_inv_sqr, z_inv);
  modp256r1_mul (z_inv, z_inv, z_inv_sqr);

  modp256r1_mul (X->x, A->x, z_inv_sqr);
  modp256r1_mul (X->y, A->y, z_inv);
  return 0;
}
