/*
 * ec_p256.c - Elliptic curve over GF(p256)
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

/*
 * References:
 *
 * [1] Suite B Implementer's Guide to FIPS 186-3 (ECDSA), February 3, 2010.
 *
 * [2] Michael Brown, Darrel Hankerson, Julio López, and Alfred Menezes,
 *     Software Implementation of the NIST Elliptic Curves Over Prime Fields,
 *     Proceedings of the 2001 Conference on Topics in Cryptology: The
 *     Cryptographer's Track at RSA
 *     Pages 250-265, Springer-Verlag London, UK, 2001
 *     ISBN:3-540-41898-9
 */

#include <stdint.h>
#include <string.h>
#include "bn.h"
#include "modp256.h"
#include "jpc-ac.h"
#include "mod.h"
#include "ec_p256.h"

#if TEST
/*
 * Generator of Elliptic curve over GF(p256)
 */
const bn256 Gx[1] = {
  {{  0xd898c296, 0xf4a13945, 0x2deb33a0, 0x77037d81,
      0x63a440f2, 0xf8bce6e5, 0xe12c4247, 0x6b17d1f2  }}
};

const bn256 Gy[1] = {
  {{  0x37bf51f5, 0xcbb64068, 0x6b315ece, 0x2bce3357,
      0x7c0f9e16, 0x8ee7eb4a, 0xfe1a7f9b, 0x4fe342e2  }}
};
#endif


/*
 * a = -3 mod p256
 */
static const bn256 coefficient_a[1] = {
  {{  0xfffffffc, 0xffffffff, 0xffffffff, 0x00000000,
      0x00000000, 0x00000000, 0x00000001, 0xffffffff }}
};

static const bn256 coefficient_b[1] = {
  {{ 0x27d2604b, 0x3bce3c3e, 0xcc53b0f6, 0x651d06b0,
     0x769886bc, 0xb3ebbd55, 0xaa3a93e7, 0x5ac635d8 }}
};


/*
 * w = 4
 * m = 256
 * d = 64
 * e = 32
 */

static const ac precomputed_KG[15] = {
  {
    {{{ 0xd898c296, 0xf4a13945, 0x2deb33a0, 0x77037d81,
	0x63a440f2, 0xf8bce6e5, 0xe12c4247, 0x6b17d1f2 }}},
    {{{ 0x37bf51f5, 0xcbb64068, 0x6b315ece, 0x2bce3357,
	0x7c0f9e16, 0x8ee7eb4a, 0xfe1a7f9b, 0x4fe342e2 }}}
  }, {
    {{{ 0x8e14db63, 0x90e75cb4, 0xad651f7e, 0x29493baa,
	0x326e25de, 0x8492592e, 0x2811aaa5, 0x0fa822bc }}},
    {{{ 0x5f462ee7, 0xe4112454, 0x50fe82f5, 0x34b1a650,
	0xb3df188b, 0x6f4ad4bc, 0xf5dba80d, 0xbff44ae8 }}}
  }, {
    {{{ 0x097992af, 0x93391ce2, 0x0d35f1fa, 0xe96c98fd,
	0x95e02789, 0xb257c0de, 0x89d6726f, 0x300a4bbc }}},
    {{{ 0xc08127a0, 0xaa54a291, 0xa9d806a5, 0x5bb1eead,
	0xff1e3c6f, 0x7f1ddb25, 0xd09b4644, 0x72aac7e0 }}}
  }, {
    {{{ 0xd789bd85, 0x57c84fc9, 0xc297eac3, 0xfc35ff7d,
	0x88c6766e, 0xfb982fd5, 0xeedb5e67, 0x447d739b }}},
    {{{ 0x72e25b32, 0x0c7e33c9, 0xa7fae500, 0x3d349b95,
	0x3a4aaff7, 0xe12e9d95, 0x834131ee, 0x2d4825ab }}}
  }, {
    {{{ 0x2a1d367f, 0x13949c93, 0x1a0a11b7, 0xef7fbd2b,
	0xb91dfc60, 0xddc6068b, 0x8a9c72ff, 0xef951932 }}},
    {{{ 0x7376d8a8, 0x196035a7, 0x95ca1740, 0x23183b08,
	0x022c219c, 0xc1ee9807, 0x7dbb2c9b, 0x611e9fc3 }}}
  }, {
    {{{ 0x0b57f4bc, 0xcae2b192, 0xc6c9bc36, 0x2936df5e,
	0xe11238bf, 0x7dea6482, 0x7b51f5d8, 0x55066379 }}},
    {{{ 0x348a964c, 0x44ffe216, 0xdbdefbe1, 0x9fb3d576,
	0x8d9d50e5, 0x0afa4001, 0x8aecb851, 0x15716484 }}}
  }, {
    {{{ 0xfc5cde01, 0xe48ecaff, 0x0d715f26, 0x7ccd84e7,
	0xf43e4391, 0xa2e8f483, 0xb21141ea, 0xeb5d7745 }}},
    {{{ 0x731a3479, 0xcac917e2, 0x2844b645, 0x85f22cfe,
	0x58006cee, 0x0990e6a1, 0xdbecc17b, 0xeafd72eb }}}
  }, {
    {{{ 0x313728be, 0x6cf20ffb, 0xa3c6b94a, 0x96439591,
	0x44315fc5, 0x2736ff83, 0xa7849276, 0xa6d39677 }}},
    {{{ 0xc357f5f4, 0xf2bab833, 0x2284059b, 0x824a920c,
	0x2d27ecdf, 0x66b8babd, 0x9b0b8816, 0x674f8474 }}}
  }, {
    {{{ 0x677c8a3e, 0x2df48c04, 0x0203a56b, 0x74e02f08,
	0xb8c7fedb, 0x31855f7d, 0x72c9ddad, 0x4e769e76 }}},
    {{{ 0xb824bbb0, 0xa4c36165, 0x3b9122a5, 0xfb9ae16f,
	0x06947281, 0x1ec00572, 0xde830663, 0x42b99082 }}}
  }, {
    {{{ 0xdda868b9, 0x6ef95150, 0x9c0ce131, 0xd1f89e79,
	0x08a1c478, 0x7fdc1ca0, 0x1c6ce04d, 0x78878ef6 }}},
    {{{ 0x1fe0d976, 0x9c62b912, 0xbde08d4f, 0x6ace570e,
	0x12309def, 0xde53142c, 0x7b72c321, 0xb6cb3f5d }}}
  }, {
    {{{ 0xc31a3573, 0x7f991ed2, 0xd54fb496, 0x5b82dd5b,
	0x812ffcae, 0x595c5220, 0x716b1287, 0x0c88bc4d }}},
    {{{ 0x5f48aca8, 0x3a57bf63, 0xdf2564f3, 0x7c8181f4,
	0x9c04e6aa, 0x18d1b5b3, 0xf3901dc6, 0xdd5ddea3 }}}
  }, {
    {{{ 0x3e72ad0c, 0xe96a79fb, 0x42ba792f, 0x43a0a28c,
	0x083e49f3, 0xefe0a423, 0x6b317466, 0x68f344af }}},
    {{{ 0x3fb24d4a, 0xcdfe17db, 0x71f5c626, 0x668bfc22,
	0x24d67ff3, 0x604ed93c, 0xf8540a20, 0x31b9c405 }}}
  }, {
    {{{ 0xa2582e7f, 0xd36b4789, 0x4ec39c28, 0xd1a1014,
	0xedbad7a0, 0x663c62c3, 0x6f461db9, 0x4052bf4b }}},
    {{{ 0x188d25eb, 0x235a27c3, 0x99bfcc5b, 0xe724f339,
	0x71d70cc8, 0x862be6bd, 0x90b0fc61, 0xfecf4d51 }}}
  }, {
    {{{ 0xa1d4cfac, 0x74346c10, 0x8526a7a4, 0xafdf5cc0,
	0xf62bff7a, 0x123202a8, 0xc802e41a, 0x1eddbae2 }}},
    {{{ 0xd603f844, 0x8fa0af2d, 0x4c701917, 0x36e06b7e,
	0x73db33a0, 0x0c45f452, 0x560ebcfc, 0x43104d86 }}}
  }, {
    {{{ 0x0d1d78e5, 0x9615b511, 0x25c4744b, 0x66b0de32,
	0x6aaf363a, 0x0a4a46fb, 0x84f7a21c, 0xb48e26b4 }}},
    {{{ 0x21a01b2d, 0x06ebb0f6, 0x8b7b0f98, 0xc004e404,
	0xfed6f668, 0x64131bcd, 0x4d4d3dab, 0xfac01540 }}}
  }
};

static const ac precomputed_2E_KG[15] = {
  {
    {{{ 0x185a5943, 0x3a5a9e22, 0x5c65dfb6, 0x1ab91936,
	0x262c71da, 0x21656b32, 0xaf22af89, 0x7fe36b40 }}},
    {{{ 0x699ca101, 0xd50d152c, 0x7b8af212, 0x74b3d586,
	0x07dca6f1, 0x9f09f404, 0x25b63624, 0xe697d458 }}}
  }, {
    {{{ 0x7512218e, 0xa84aa939, 0x74ca0141, 0xe9a521b0,
	0x18a2e902, 0x57880b3a, 0x12a677a6, 0x4a5b5066 }}},
    {{{ 0x4c4f3840, 0x0beada7a, 0x19e26d9d, 0x626db154,
	0xe1627d40, 0xc42604fb, 0xeac089f1, 0xeb13461c }}}
  }, {
    {{{ 0x27a43281, 0xf9faed09, 0x4103ecbc, 0x5e52c414,
	0xa815c857, 0xc342967a, 0x1c6a220a, 0x0781b829 }}},
    {{{ 0xeac55f80, 0x5a8343ce, 0xe54a05e3, 0x88f80eee,
	0x12916434, 0x97b2a14f, 0xf0151593, 0x690cde8d }}}
  }, {
    {{{ 0xf7f82f2a, 0xaee9c75d, 0x4afdf43a, 0x9e4c3587,
	0x37371326, 0xf5622df4, 0x6ec73617, 0x8a535f56 }}},
    {{{ 0x223094b7, 0xc5f9a0ac, 0x4c8c7669, 0xcde53386,
	0x085a92bf, 0x37e02819, 0x68b08bd7, 0x0455c084 }}}
  }, {
    {{{ 0x9477b5d9, 0x0c0a6e2c, 0x876dc444, 0xf9a4bf62,
	0xb6cdc279, 0x5050a949, 0xb77f8276, 0x06bada7a }}},
    {{{ 0xea48dac9, 0xc8b4aed1, 0x7ea1070f, 0xdebd8a4b,
	0x1366eb70, 0x427d4910, 0x0e6cb18a, 0x5b476dfd }}}
  }, {
    {{{ 0x278c340a, 0x7c5c3e44, 0x12d66f3b, 0x4d546068,
	0xae23c5d8, 0x29a751b1, 0x8a2ec908, 0x3e29864e }}},
    {{{ 0x26dbb850, 0x142d2a66, 0x765bd780, 0xad1744c4,
	0xe322d1ed, 0x1f150e68, 0x3dc31e7e, 0x239b90ea }}}
  }, {
    {{{ 0x7a53322a, 0x78c41652, 0x09776f8e, 0x305dde67,
	0xf8862ed4, 0xdbcab759, 0x49f72ff7, 0x820f4dd9 }}},
    {{{ 0x2b5debd4, 0x6cc544a6, 0x7b4e8cc4, 0x75be5d93,
	0x215c14d3, 0x1b481b1b, 0x783a05ec, 0x140406ec }}}
  }, {
    {{{ 0xe895df07, 0x6a703f10, 0x01876bd8, 0xfd75f3fa,
	0x0ce08ffe, 0xeb5b06e7, 0x2783dfee, 0x68f6b854 }}},
    {{{ 0x78712655, 0x90c76f8a, 0xf310bf7f, 0xcf5293d2,
	0xfda45028, 0xfbc8044d, 0x92e40ce6, 0xcbe1feba }}}
  }, {
    {{{ 0x4396e4c1, 0xe998ceea, 0x6acea274, 0xfc82ef0b,
	0x2250e927, 0x230f729f, 0x2f420109, 0xd0b2f94d }}},
    {{{ 0xb38d4966, 0x4305addd, 0x624c3b45, 0x10b838f8,
	0x58954e7a, 0x7db26366, 0x8b0719e5, 0x97145982 }}}
  }, {
    {{{ 0x23369fc9, 0x4bd6b726, 0x53d0b876, 0x57f2929e,
	0xf2340687, 0xc2d5cba4, 0x4a866aba, 0x96161000 }}},
    {{{ 0x2e407a5e, 0x49997bcd, 0x92ddcb24, 0x69ab197d,
	0x8fe5131c, 0x2cf1f243, 0xcee75e44, 0x7acb9fad }}}
  }, {
    {{{ 0x23d2d4c0, 0x254e8394, 0x7aea685b, 0xf57f0c91,
	0x6f75aaea, 0xa60d880f, 0xa333bf5b, 0x24eb9acc }}},
    {{{ 0x1cda5dea, 0xe3de4ccb, 0xc51a6b4f, 0xfeef9341,
	0x8bac4c4d, 0x743125f8, 0xacd079cc, 0x69f891c5 }}}
  }, {
    {{{ 0x702476b5, 0xeee44b35, 0xe45c2258, 0x7ed031a0,
	0xbd6f8514, 0xb422d1e7, 0x5972a107, 0xe51f547c }}},
    {{{ 0xc9cf343d, 0xa25bcd6f, 0x097c184e, 0x8ca922ee,
	0xa9fe9a06, 0xa62f98b3, 0x25bb1387, 0x1c309a2b }}}
  }, {
    {{{ 0x1967c459, 0x9295dbeb, 0x3472c98e, 0xb0014883,
	0x08011828, 0xc5049777, 0xa2c4e503, 0x20b87b8a }}},
    {{{ 0xe057c277, 0x3063175d, 0x8fe582dd, 0x1bd53933,
	0x5f69a044, 0x0d11adef, 0x919776be, 0xf5c6fa49 }}}
  }, {
    {{{ 0x0fd59e11, 0x8c944e76, 0x102fad5f, 0x3876cba1,
	0xd83faa56, 0xa454c3fa, 0x332010b9, 0x1ed7d1b9 }}},
    {{{ 0x0024b889, 0xa1011a27, 0xac0cd344, 0x05e4d0dc,
	0xeb6a2a24, 0x52b520f0, 0x3217257a, 0x3a2b03f0 }}}
  }, {
    {{{ 0xdf1d043d, 0xf20fc2af, 0xb58d5a62, 0xf330240d,
	0xa0058c3b, 0xfc7d229c, 0xc78dd9f6, 0x15fee545 }}},
    {{{ 0x5bc98cda, 0x501e8288, 0xd046ac04, 0x41ef80e5,
	0x461210fb, 0x557d9f49, 0xb8753f81, 0x4ab5b6b2 }}}
  }
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
compute_kG (ac *X, const bn256 *K)
{
  int i;
  int q_is_infinite = 1;
  jpc Q[1];

  for (i = 31; i >= 0; i--)
    {
      int k_i, k_i_e;

      if (!q_is_infinite)
	jpc_double (Q, Q);

      k_i = (((K->word[6] >> i) & 1) << 3)
	| (((K->word[4] >> i) & 1) << 2)
	| (((K->word[2] >> i) & 1) << 1)
	| ((K->word[0] >> i) & 1);
      k_i_e = (((K->word[7] >> i) & 1) << 3)
	| (((K->word[5] >> i) & 1) << 2)
	| (((K->word[3] >> i) & 1) << 1)
	| ((K->word[1] >> i) & 1);

      if (k_i)
	{
	  if (q_is_infinite)
	    {
	      memcpy (Q->x, (&precomputed_KG[k_i - 1])->x, sizeof (bn256));
	      memcpy (Q->y, (&precomputed_KG[k_i - 1])->y, sizeof (bn256));
	      Q->z->word[0] = 1;
	      Q->z->word[1] = Q->z->word[2] = Q->z->word[3]
		= Q->z->word[4] = Q->z->word[5] = Q->z->word[6]
		= Q->z->word[7] = 0;
	      q_is_infinite = 0;
	    }
	  else
	    jpc_add_ac (Q, Q, &precomputed_KG[k_i - 1]);
	}
      if (k_i_e)
	{
	  if (q_is_infinite)
	    {
	      memcpy (Q->x, (&precomputed_2E_KG[k_i_e - 1])->x, sizeof (bn256));
	      memcpy (Q->y, (&precomputed_2E_KG[k_i_e - 1])->y, sizeof (bn256));
	      memset (Q->z, 0, sizeof (bn256));
	      Q->z->word[0] = 1;
	      q_is_infinite = 0;
	    }
	  else
	    jpc_add_ac (Q, Q, &precomputed_2E_KG[k_i_e - 1]);
	}
    }

  return jpc_to_ac (X, Q);
}


#define NAF_K_SIGN(k)	(k&8)
#define NAF_K_INDEX(k)	((k&7)-1)

static void
naf4_257_set (naf4_257 *NAF_K, int i, int ki)
{
  if (ki != 0)
    {
      if (ki > 0)
	ki = (ki+1)/2;
      else
	ki = (1-ki)/2 | 8;
    }

  if (i == 256)
    NAF_K->last_nibble = ki;
  else
    {
      NAF_K->word[i/8] &= ~(0x0f << ((i & 0x07)*4));
      NAF_K->word[i/8] |= (ki << ((i & 0x07)*4));
    }
}

static int
naf4_257_get (const naf4_257 *NAF_K, int i)
{
  int ki;

  if (i == 256)
    ki = NAF_K->last_nibble;
  else
    {
      ki = NAF_K->word[i/8] >> ((i & 0x07)*4);
      ki &= 0x0f;
    }

  return ki;
}


/*
 * convert 256-bit bignum into non-adjacent form (NAF)
 */
void
compute_naf4_257 (naf4_257 *NAF_K, const bn256 *K)
{
  int i = 0;
  bn256 K_tmp[1];
  uint32_t carry = 0;

  memcpy (K_tmp, K, sizeof (bn256));
  memset (NAF_K, 0, sizeof (naf4_257));

  while (!bn256_is_zero (K_tmp))
    {
      if (bn256_is_even (K_tmp))
	naf4_257_set (NAF_K, i, 0);
      else
	{
	  int ki = (K_tmp->word[0]) & 0x0f;

	  if ((ki & 0x08))
	    {
	      carry = bn256_add_uint (K_tmp, K_tmp, 16 - ki);
	      ki = ki - 16;
	    }
	  else
	    K_tmp->word[0] &= 0xfffffff0;

	  naf4_257_set (NAF_K, i, ki);
	}

      bn256_shift (K_tmp, K_tmp, -1);
      if (carry)
	{
	  K_tmp->word[7] |= 0x80000000;
	  carry = 0;
	}
      i++;
    }
}

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

  /* Elliptic curve: y^2 = x^3 + a*x + b */
  modp256_sqr (s, P->x);
  modp256_mul (s, s, P->x);

  modp256_mul (t, coefficient_a, P->x);
  modp256_add (s, s, t);
  modp256_add (s, s, coefficient_b);

  modp256_sqr (t, P->y);
  if (bn256_cmp (s, t) == 0)
    return 0;
  else
    return -1;
}

/**
 * @brief	X  = k * P
 *
 * @param NAF_K	NAF representation of k
 * @param P	P in affine coordiate
 *
 * Return -1 on error.
 * Return 0 on success.
 *
 * For the curve (cofactor is 1 and n is prime), possible error cases are:
 *
 *     P is not on the curve.
 *     P = G, k = n
 *     Something wrong in the code.
 *
 * Mathmatically, k=1 and P=O is another possible case, but O cannot be
 * represented by affine coordinate.
 */
int
compute_kP (ac *X, const naf4_257 *NAF_K, const ac *P)
{
  int i;
  int q_is_infinite = 1;
  jpc Q[1];
  ac P3[1], P5[1], P7[1];
  const ac *p_Pi[4];

  if (point_is_on_the_curve (P) < 0)
    return -1;

  p_Pi[0] = P;
  p_Pi[1] = P3;
  p_Pi[2] = P5;
  p_Pi[3] = P7;

  {
    jpc Q1[1];

    memcpy (Q->x, P->x, sizeof (bn256));
    memcpy (Q->y, P->y, sizeof (bn256));
    memset (Q->z, 0, sizeof (bn256));
    Q->z->word[0] = 1;

    jpc_double (Q, Q);
    jpc_add_ac (Q1, Q, P);
    if (jpc_to_ac (P3, Q1) < 0)	/* Never occurs, except coding errors.  */
      return -1;
    jpc_double (Q, Q);
    jpc_add_ac (Q1, Q, P);
    if (jpc_to_ac (P5, Q1) < 0)	/* Never occurs, except coding errors.  */
      return -1;

    memcpy (Q->x, P3->x, sizeof (bn256));
    memcpy (Q->y, P3->y, sizeof (bn256));
    memset (Q->z, 0, sizeof (bn256));
    Q->z->word[0] = 1;
    jpc_double (Q, Q);
    jpc_add_ac (Q1, Q, P);
    if (jpc_to_ac (P7, Q1) < 0)	/* Never occurs, except coding errors.  */
      return -1;
  }

  for (i = 256; i >= 0; i--)
    {
      int k_i;

      if (!q_is_infinite)
	jpc_double (Q, Q);

      k_i = naf4_257_get (NAF_K, i);
      if (k_i)
	{
	  if (q_is_infinite)
	    {
	      memcpy (Q->x, p_Pi[NAF_K_INDEX(k_i)]->x, sizeof (bn256));
	      if (NAF_K_SIGN (k_i))
		bn256_sub (Q->y, P256, p_Pi[NAF_K_INDEX(k_i)]->y);
	      else
		memcpy (Q->y, p_Pi[NAF_K_INDEX(k_i)]->y, sizeof (bn256));
	      memset (Q->z, 0, sizeof (bn256));
	      Q->z->word[0] = 1;
	      q_is_infinite = 0;
	    }
	  else
	    jpc_add_ac_signed (Q, Q, p_Pi[NAF_K_INDEX(k_i)], NAF_K_SIGN (k_i));
	}
    }

  return jpc_to_ac (X, Q);
}


/*
 * N: order of G
 */
static const bn256 N[1] = {
  {{ 0xfc632551, 0xf3b9cac2, 0xa7179e84, 0xbce6faad,
     0xffffffff, 0xffffffff, 0x00000000, 0xffffffff }}
};

/*
 * MU = 2^512 / N
 * MU = ( (1 << 256) | MU_lower )
 */
static const bn256 MU_lower[1] = {
  {{ 0xeedf9bfe, 0x012ffd85, 0xdf1a6c21, 0x43190552,
     0xffffffff, 0xfffffffe, 0xffffffff, 0x00000000 }}
};


/**
 * @brief Compute signature (r,s) of hash string z with secret key d
 */
void
ecdsa (bn256 *r, bn256 *s, const bn256 *z, const bn256 *d)
{
  bn256 k[1];
  ac KG[1];
  bn512 tmp[1];
  bn256 k_inv[1];
  uint32_t carry;
#define borrow carry
#define tmp_k k_inv

  do
    {
      do
	{
	  bn256_random (k);
	  if (bn256_sub (tmp_k, k, N) == 0)	/* > N, it's too big.  */
	    continue;
	  if (bn256_add_uint (tmp_k, tmp_k, 2)) /* > N - 2, still big.  */
	    continue;
	  bn256_add_uint (k, k, 1);
	  compute_kG (KG, k);
	  borrow = bn256_sub (r, KG->x, N);
	  if (borrow)
	    memcpy (r, KG->x, sizeof (bn256));
	  else
	    memcpy (KG->x, r, sizeof (bn256));
	}
      while (bn256_is_zero (r));

      mod_inv (k_inv, k, N);
      bn256_mul (tmp, r, d);
      mod_reduce (s, tmp, N, MU_lower);
      carry = bn256_add (s, s, z);
      if (carry)
	bn256_sub (s, s, N);
      else
	bn256_sub (tmp, s, N);
      bn256_mul (tmp, s, k_inv);
      mod_reduce (s, tmp, N, MU_lower);
    }
  while (bn256_is_zero (s));
}
