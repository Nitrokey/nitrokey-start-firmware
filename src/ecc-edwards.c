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
  bn256 c[1], d[1], e[1], tmp[1];
  bn256 minus_B_x[1];
  uint32_t borrow;

  /* Compute: C = X1 * X2 */
  borrow = bn256_sub (minus_B_x, n25638, B->x);
  if (borrow)
    bn256_add (minus_B_x, minus_B_x, n25638); /* carry ignored */
  else
    bn256_add (c, minus_B_x, n25638); /* dummy calculation */
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

  /* tmp = D_1 - E : F */
  mod25638_sub (tmp, d, e);

  /* D_2 = D_1 + E : G */
  mod25638_add (d, d, e);

  /* X3_final = Z1 * tmp * ((X1 + Y1) * (X2 + Y2) - C_1) */
  mod25638_add (X->x, A->x, A->y);
  if (minus)
    mod25638_sub (e, B->y, B->x);
  else
    mod25638_add (e, B->x, B->y);
  mod25638_mul (e, X->x, e);
  mod25638_sub (e, e, c);
  mod25638_mul (e, tmp, e);
  mod25638_mul (X->x, A->z, e);

  /* Y3_final = Z1 * D_2 * C_1 */
  mod25638_mul (c, d, c);
  mod25638_mul (X->y, A->z, c);

  /* Z3_final = tmp * D_2 */
  mod25638_mul (X->z, tmp, d);

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


static const ac precomputed_KG[15] = {
  { {{{ 0x8f25d51a, 0xc9562d60, 0x9525a7b2, 0x692cc760, 
        0xfdd6dc5c, 0xc0a4e231, 0xcd6e53fe, 0x216936d3 }}},
    {{{ 0x66666658, 0x66666666, 0x66666666, 0x66666666, 
        0x66666666, 0x66666666, 0x66666666, 0x66666666 }}} },
  { {{{ 0xf4eda202, 0x3e0b6b8f, 0xd51a35eb, 0x0078db7e, 
        0xb4a08a96, 0xd44b60cf, 0xbf2df9d5, 0x6222bd88 }}},
    {{{ 0x82e45313, 0x8f1efa57, 0xba902b06, 0x5410b608, 
        0x261b7c4f, 0xdd6bdaed, 0xea4ed025, 0x0325bb42 }}} },
  { {{{ 0x61ccfba2, 0x1a700667, 0xff3a78c4, 0x2cdd6232, 
        0x3b1950ab, 0xb87d9bf2, 0x9c294ffd, 0x0eba91a7 }}},
    {{{ 0xfe515e46, 0xe5e5bf1d, 0x670d959b, 0x5ab5d1f8, 
        0xc32c93a1, 0x85970ede, 0xabea7f2d, 0x1830473e }}} },
  { {{{ 0x60b7e824, 0xfc8047ae, 0xc2e723e5, 0x98e685c9, 
        0xe14e29a0, 0x952d3984, 0x3c45f32c, 0x4c27afff }}},
    {{{ 0x4bf5a66b, 0x5bbabd11, 0x51a4c49e, 0x90d0be1e, 
        0x26c29c3a, 0x95f11eb6, 0x526dc87d, 0x5f2c99e6 }}} },
  { {{{ 0x680c969a, 0xfbe2fd29, 0x31ecbce6, 0xb0e6ec08, 
        0x8cc36053, 0x8ab3c1be, 0x2b88e48f, 0x6e64e555 }}},
    {{{ 0x7bafd09b, 0x25352a64, 0x9ec55210, 0x36391158, 
        0x39b85145, 0x6a9dfc93, 0xa4cb58be, 0x383c510f }}} },
  { {{{ 0x43abca05, 0x8bf30e63, 0x9bf8a641, 0x53807053, 
        0xe38f5e86, 0xc8180dc3, 0xd81f344b, 0x6df2bc1d }}},
    {{{ 0xdfbe3a34, 0x89f3f6d9, 0x9f94e1a1, 0xe95d4c5d, 
        0xef9249a1, 0x8981530e, 0x37a68758, 0x6062ddf1 }}} },
  { {{{ 0x1b9d5a63, 0x527dc68c, 0x6a0970ea, 0x73f332e1, 
        0x7b071f21, 0xd8499b7c, 0x7225f3c0, 0x31ed9d6f }}},
    {{{ 0x54363667, 0xe6719240, 0xad112811, 0x7b853293, 
        0x493bb73e, 0xb0071c13, 0xfdaa932e, 0x3d4728fd }}} },
  { {{{ 0xc7dad28d, 0xdb7ad644, 0xb81d7d26, 0x7a9ddee1, 
        0x1c7e177d, 0x2d8d0437, 0x38185e7c, 0x1bc7af1e }}},
    {{{ 0x00314833, 0xcaf2f659, 0x631b270f, 0x1d027e12, 
        0x795dc049, 0x7a5eef87, 0x55661f2f, 0x61d909d8 }}} },
  { {{{ 0x07b06838, 0x85ccfca3, 0x654c7f10, 0xfafab365, 
        0xdb6f53a5, 0x46564c74, 0x7ad5e203, 0x02c61c29 }}},
    {{{ 0x04f259bc, 0x84c06375, 0x671c602f, 0x8663fd76, 
        0xdcbffaf3, 0x91902dd2, 0xe5a933bd, 0x42da0c66 }}} },
  { {{{ 0x66f4ca27, 0x1492ecc2, 0xd0630657, 0xeb06154d, 
        0x774f5869, 0xf0c78bc5, 0xa064ed8e, 0x71663cb3 }}},
    {{{ 0x0ada2dc6, 0x2770fe0d, 0xfa27f864, 0xa5305ff6, 
        0xf2da6c0d, 0x47785e62, 0x1c0066d3, 0x5d1f56fd }}} },
  { {{{ 0x4cf46f3f, 0x270efdd8, 0xbc2b5cc9, 0x23e7a4c0, 
        0x319f0229, 0x96d7e9d6, 0x0b5ee0f4, 0x3cee130e }}},
    {{{ 0x3df2ed09, 0xa4c39176, 0x87d4ae97, 0x18f65dd0, 
        0x671d1f47, 0xa063cff2, 0x93f82791, 0x3f237545 }}} },
  { {{{ 0x23adf1d1, 0x969364dd, 0xf77f7041, 0xa289a9f5, 
        0x1b8db034, 0x491519ae, 0x876d2358, 0x76814f15 }}},
    {{{ 0xeab523fb, 0x8d54accf, 0xeb2f424e, 0x68db630f, 
        0x8bcfa837, 0x6ea4f5ab, 0xd6b22a96, 0x0dbd9ebe }}} },
  { {{{ 0xcfa942b4, 0x178a8301, 0xc6c47647, 0x0b950483, 
        0x62c911fc, 0x84760cb8, 0xfa37b9d9, 0x6dc27cfc }}},
    {{{ 0x04b33e58, 0x488f8cbb, 0xcc2791bc, 0x1922b7f9, 
        0xb5092e83, 0x1c54d972, 0x0beaa14d, 0x7208c6f1 }}} },
  { {{{ 0x6e7a8746, 0x8a0a5680, 0x6b11ddc0, 0xdf47ddd6, 
        0xead8d910, 0x038fb07c, 0x8fc12e00, 0x30d3a844 }}},
    {{{ 0xf9a28906, 0x03dcad34, 0xa751ed85, 0x5de79c82, 
        0x320c9352, 0xaae15b9a, 0x6d02b8ca, 0x3ab1d43a }}} },
  { {{{ 0xb5be5ff0, 0x386b100d, 0x8076ac32, 0x7194cabd, 
        0x35c9f27a, 0x429fde2a, 0xab011849, 0x647cefbc }}},
    {{{ 0x923d583f, 0xdb13db59, 0xe00a6e58, 0x084a91b7, 
        0x3c2ed620, 0x178bc945, 0x90c7e779, 0x25183a99 }}} },
};

static const ac precomputed_2E_KG[15] = {
  { {{{ 0x6abc0cbb, 0x931797a4, 0x72de6f2d, 0x2c081c10, 
        0x6832800f, 0xddabd427, 0x136158c5, 0x4d1e116d }}},
    {{{ 0x10c9b91a, 0xf44e1efb, 0x5e8a4b84, 0x43e84b7b, 
        0xb5008f8c, 0x5cc51354, 0x9d4e35b6, 0x6d415be4 }}} },
  { {{{ 0x3b5775d0, 0x56145ceb, 0xb84fc950, 0xf4a31eb8, 
        0x20a9f5ab, 0xda829415, 0x599b1c96, 0x51f4ff8c }}},
    {{{ 0xd7863ac1, 0x7f8406b0, 0x07d4bd1b, 0xb12e8078, 
        0x3852eeb4, 0xf6f99aee, 0xd46e41f3, 0x35ac9588 }}} },
  { {{{ 0xb1e3cbcb, 0x23246812, 0xb51b86b7, 0x4626ba92, 
        0x7c5a82a9, 0xbad60319, 0xb0a4d421, 0x1a3d5fa5 }}},
    {{{ 0xa7f87bc6, 0xd984822e, 0xd61f19a1, 0xc9878318, 
        0xd28f1441, 0x033d3655, 0xa0ee984e, 0x1d5faa15 }}} },
  { {{{ 0x52014031, 0x285b9456, 0xee52aa8a, 0x8d050ad8, 
        0x2eaab5cd, 0x87b7aa38, 0x04fb2bf7, 0x543d84cb }}},
    {{{ 0xde59ef20, 0x6e932ba4, 0x9a42ec2e, 0x46f42dd4, 
        0x182b2758, 0x693d838f, 0xb63ed49e, 0x0358fdc5 }}} },
  { {{{ 0xb32f56f0, 0x5682666f, 0x38fd4625, 0xe8be4ba2, 
        0x649501d6, 0xf78cf0e0, 0xbcce6bab, 0x2567450f }}},
    {{{ 0x538b58d7, 0x6581f96e, 0xc25cfdf1, 0x5eb54f3e, 
        0xf899e2cf, 0x84f772f7, 0x4768df26, 0x7c9ca5b5 }}} },
  { {{{ 0xa955bba4, 0x35d5face, 0xd1bf9787, 0x13e50d77, 
        0xeff0ee33, 0xc9b67e36, 0x40ef61cd, 0x32e253be }}},
    {{{ 0xcc8e3fd4, 0x03fa09fa, 0x57f17d59, 0x04d1daed, 
        0xe17b4361, 0x6f356428, 0x5d8a9401, 0x2d9acf86 }}} },
  { {{{ 0xe9edd690, 0x06ea63bb, 0xb2d7d5f4, 0x1532536e, 
        0x8e9f5c47, 0xd55c86c3, 0x0077fe28, 0x35e81b03 }}},
    {{{ 0xdeb7e5f8, 0xf8f5f35a, 0x9386c94f, 0xf1384675, 
        0xbbf27ae1, 0x89c2f5c7, 0x1bcb5d12, 0x6e2810d6 }}} },
  { {{{ 0xcb05dc3f, 0x23c83c41, 0x99382c04, 0xf95568e3, 
        0xbfc732d3, 0x5d1bd4fa, 0x4210dcde, 0x75d942c0 }}},
    {{{ 0x4e35ab2d, 0x9765c487, 0x47a42467, 0xf38e3fad, 
        0x771731cb, 0x8fd7e2c5, 0x56cdc13c, 0x696cc148 }}} },
  { {{{ 0x1c3eae7b, 0x3fde9e2a, 0x54665d04, 0xf3bea8c8, 
        0x0d7e1fdf, 0x11c026d7, 0xe0744e05, 0x7d17c4ff }}},
    {{{ 0x2479704e, 0x71530a4d, 0x70a41133, 0x316b2419, 
        0x3008bf60, 0xe6b151ed, 0x91f4ba0f, 0x3d8acfb9 }}} },
  { {{{ 0xfba75a58, 0xb6859189, 0xefad9ec8, 0xb459a9b1, 
        0x6c18aaad, 0x250970a4, 0x6c9ed604, 0x48540861 }}},
    {{{ 0xdbe02f54, 0x51c52acc, 0x5c87096e, 0xa1ffff9e, 
        0x5cca82cd, 0xd7b2b928, 0xa7047f4e, 0x00b7a530 }}} },
  { {{{ 0xd7a206ad, 0x2fa8b877, 0xc26be66b, 0xeb0ec8e5, 
        0x6c43abcf, 0x812d4d32, 0x21b49825, 0x15b95ece }}},
    {{{ 0x6401bfc5, 0xc5c588ca, 0x26caa2eb, 0x6c2f708f, 
        0x7b620be3, 0x5575925c, 0x285eb272, 0x1b8e9998 }}} },
  { {{{ 0xc8c0b25a, 0x3c58fb25, 0xf61dd811, 0x45e69804, 
        0x4f4f3099, 0xe0506d88, 0xf6cb4804, 0x46e5ad62 }}},
    {{{ 0x2b3d2d89, 0x3ab72c24, 0x83a0d441, 0xc3547b12, 
        0x6b6b242d, 0x4bc9ef39, 0x8c21d8f4, 0x35dd429a }}} },
  { {{{ 0x40d5666c, 0xda6a76e6, 0x02bbbfdd, 0xfe5fb226, 
        0xec0db634, 0x335c47d6, 0x27a83af1, 0x0b3a68e0 }}},
    {{{ 0xeb1ed0fe, 0x958eda03, 0x58cb1b6c, 0x98ad7ed0, 
        0xe6e0887c, 0xc9a8a932, 0x8d10575b, 0x182ae50d }}} },
  { {{{ 0xc80926b5, 0xe85218d5, 0x663244da, 0x4e3ca84c, 
        0x8c93a82e, 0xd8dd2b95, 0x694d17e6, 0x3be6208c }}},
    {{{ 0xa3cd685d, 0xbb2f7abd, 0x9636e5cc, 0x43ddbc27, 
        0x8398acfd, 0x22cd89da, 0xa454d187, 0x663fede3 }}} },
  { {{{ 0xdd12463a, 0x77e83188, 0x10321a86, 0xc29101f7, 
        0x80c4fcac, 0x739f0dfb, 0x3c0a1c4e, 0x6bb76555 }}},
    {{{ 0x4f7a95fd, 0xb6b30c93, 0x28f71108, 0x3070bbe4, 
        0xee80b79c, 0x1ce155df, 0xe9bdf1b1, 0x181b445b }}} },
};

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
  return 0;
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

#ifdef PRINT_OUT_TABLE_AS_C
  fputs ("  { {{{ ", stdout);
  for (i = 0; i < 4; i++)
    printf ("0x%08x, ", X->x->word[i]);
  fputs ("\n        ", stdout);
  for (; i < 7; i++)
    printf ("0x%08x, ", X->x->word[i]);
  printf ("0x%08x }}},\n", X->x->word[i]);
  fputs ("    {{{ ", stdout);
  for (i = 0; i < 4; i++)
    printf ("0x%08x, ", X->y->word[i]);
  fputs ("\n        ", stdout);
  for (; i < 7; i++)
    printf ("0x%08x, ", X->y->word[i]);
  printf ("0x%08x }}} },\n", X->y->word[i]);
#else
  puts ("--");
  for (i = 7; i >= 0; i--)
    printf ("%08x", X->x->word[i]);
  puts ("");
  for (i = 7; i >= 0; i--)
    printf ("%08x", X->y->word[i]);
  puts ("");
  puts ("--");
#endif
}

#if 0
static void
print_point_ptc (const ptc *X)
{
  int i;

  puts ("---");
  for (i = 7; i >= 0; i--)
    printf ("%08x", X->x->word[i]);
  puts ("");
  for (i = 7; i >= 0; i--)
    printf ("%08x", X->y->word[i]);
  puts ("");
  for (i = 7; i >= 0; i--)
    printf ("%08x", X->z->word[i]);
  puts ("");
  puts ("---");
}
#endif

static const uint8_t *str = (const uint8_t *)
  "abcdefghijklmnopqrstuvwxyz0123456789";

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
  ac a0001[1], a0010[1], a0100[1], a1000[1];
  ac x[1];
  ptc a[1];
  int i;

  memcpy (a, G, sizeof (ptc));
  ptc_to_ac_25519 (a0001, a);

  for (i = 0; i < 64; i++)
    ed_double_25638 (a, a);
  ptc_to_ac_25519 (a0010, a);

  for (i = 0; i < 64; i++)
    ed_double_25638 (a, a);
  ptc_to_ac_25519 (a0100, a);

  for (i = 0; i < 64; i++)
    ed_double_25638 (a, a);
  ptc_to_ac_25519 (a1000, a);

  for (i = 1; i < 16; i++)
    {
      /* A := Identity Element  */
      memset (a, 0, sizeof (ptc));
      a->y->word[0] = 1;
      a->z->word[0] = 1;

      if ((i & 1))
	ed_add_25638 (a, a, a0001, 0);
      if ((i & 2))
	ed_add_25638 (a, a, a0010, 0);
      if ((i & 4))
	ed_add_25638 (a, a, a0100, 0);
      if ((i & 8))
	ed_add_25638 (a, a, a1000, 0);

      ptc_to_ac_25519 (x, a);
      print_point (x);
    }

  fputs ("\n", stdout);

  memcpy (a, a0001, sizeof (ac));
  memset (a->z, 0, sizeof (bn256));
  a->z->word[0] = 1;
  for (i = 0; i < 32; i++)
    ed_double_25638 (a, a);
  ptc_to_ac_25519 (a0001, a);

  memcpy (a, a0010, sizeof (ac));
  memset (a->z, 0, sizeof (bn256));
  a->z->word[0] = 1;
  for (i = 0; i < 32; i++)
    ed_double_25638 (a, a);
  ptc_to_ac_25519 (a0010, a);

  memcpy (a, a0100, sizeof (ac));
  memset (a->z, 0, sizeof (bn256));
  a->z->word[0] = 1;
  for (i = 0; i < 32; i++)
    ed_double_25638 (a, a);
  ptc_to_ac_25519 (a0100, a);

  memcpy (a, a1000, sizeof (ac));
  memset (a->z, 0, sizeof (bn256));
  a->z->word[0] = 1;
  for (i = 0; i < 32; i++)
    ed_double_25638 (a, a);
  ptc_to_ac_25519 (a1000, a);

  for (i = 1; i < 16; i++)
    {
      /* A := Identity Element  */
      memset (a, 0, sizeof (ptc));
      a->y->word[0] = 1;
      a->z->word[0] = 1;

      if ((i & 1))
	ed_add_25638 (a, a, a0001, 0);
      if ((i & 2))
	ed_add_25638 (a, a, a0010, 0);
      if ((i & 4))
	ed_add_25638 (a, a, a0100, 0);
      if ((i & 8))
	ed_add_25638 (a, a, a1000, 0);

      ptc_to_ac_25519 (x, a);
      print_point (x);
    }

  return 0;
}
#endif
