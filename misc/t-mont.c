/*
 * t-eddsa.c - testing EdDSA
 * Copyright (C) 2014 Free Software Initiative of Japan
 * Author: NIIBE Yutaka <gniibe@fsij.org>
 *
 * Run following commands.  The file t-ed25519.inp is available in GNU
 * libgcrypt source code under 'tests' directory.

  gcc -Wall -c -DBN256_C_IMPLEMENTATION ecc-mont.c
  gcc -Wall -c -DBN256_NO_RANDOM -DBN256_C_IMPLEMENTATION bn.c
  gcc -Wall -c mod.c
  gcc -Wall -c -DBN256_C_IMPLEMENTATION mod25638.c
  gcc -Wall -c t-mont.c
  gcc -Wall -c debug-bn.c
  gcc -o t-mont t-mont.o ecc-mont.o bn.o mod.o mod25638.o debug-bn.o


 *
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "bn.h"

const uint8_t k[32] = {
  0x30, 0x01, 0x33, 0xE7, 0xDC, 0x52, 0xAD, 0x9F,
  0x89, 0xFE, 0xC0, 0x59, 0x4A, 0x6D, 0x65, 0xE5,
  0xF8, 0x7A, 0xD6, 0xA9, 0xA4, 0x89, 0x00, 0xB1,
  0x93, 0x7E, 0xD3, 0x6F, 0x09, 0x1E, 0xB7, 0x76,
};

int
main (int argc, char *argv[])
{
  int all_good = 1;
  int r;
  bn256 *pk;
  bn256 a[1];
  uint8_t out[32];

  extern void ecdh_decrypt_curve25519 (const uint8_t *input,
				       uint8_t *output,
				       const bn256 *k);
  extern uint8_t *ecdh_compute_public_25519 (const uint8_t*k);
  extern void print_le_bn256 (const bn256 *X);

  while (1)
    {
#if 0
      hash[0] &= 248;
      hash[31] &= 127;
      hash[31] |= 64;
      memcpy (a, hash, sizeof (bn256)); /* Lower half of hash */
#endif

      pk = ecdh_compute_public_25519 (k);
      print_le_bn256 (pk);
      return 0;

#if 0
      if (memcmp (pk, pk_calculated, sizeof (bn256)) != 0)
	{
	  printf ("ERR PK: %d\n", test_no);
	  print_be_bn256 (sk);
	  print_be_bn256 (pk);
	  print_be_bn256 (pk_calculated);
	  all_good = 0;
	  continue;
	}

      ecdh_decrypt_25519 (msg, out, a);
      if (memcmp (sig, R, sizeof (bn256)) != 0
	  || memcmp (((const uint8_t *)sig)+32, S, sizeof (bn256)) != 0)
	{
	  printf ("ERR SIG: %d\n", test_no);
	  print_le_bn256 (R);
	  print_le_bn256 (S);
	  print_le_bn256 ((const bn256 *)sig);
	  print_le_bn256 ((const bn256 *)(((const uint8_t *)sig)+32));
	  all_good = 0;
	  continue;
	}

      printf ("%d\n", test_no);
#endif
    }
  return all_good == 1?0:1;
}
