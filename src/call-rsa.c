/*
 * call-rsa.c -- Glue code between RSA computation and OpenPGP card protocol
 *
 * Copyright (C) 2010 Free Software Initiative of Japan
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

#include <stdlib.h>
#include "config.h"
#include "ch.h"
#include "gnuk.h"
#include "polarssl/config.h"
#include "polarssl/rsa.h"

#define RSA_SIGNATURE_LENGTH 256 /* 256 byte == 2048-bit */

static rsa_context rsa_ctx;

int
rsa_sign (const uint8_t *raw_message, uint8_t *output, int msg_len,
	  struct key_data *kd)
{
  mpi P1, Q1, H;
  int r;

  mpi_init (&P1, &Q1, &H, NULL);
  rsa_init (&rsa_ctx, RSA_PKCS_V15, 0);

  rsa_ctx.len = 2048 / 8;
  mpi_read_string (&rsa_ctx.E, 16, "10001");
  mpi_read_binary (&rsa_ctx.P, &kd->data[0], rsa_ctx.len / 2);
  mpi_read_binary (&rsa_ctx.Q, &kd->data[128], rsa_ctx.len / 2);
  mpi_mul_mpi (&rsa_ctx.N, &rsa_ctx.P, &rsa_ctx.Q);
  mpi_sub_int (&P1, &rsa_ctx.P, 1);
  mpi_sub_int (&Q1, &rsa_ctx.Q, 1);
  mpi_mul_mpi (&H, &P1, &Q1);
  mpi_inv_mod (&rsa_ctx.D , &rsa_ctx.E, &H);
  mpi_mod_mpi (&rsa_ctx.DP, &rsa_ctx.D, &P1);
  mpi_mod_mpi (&rsa_ctx.DQ, &rsa_ctx.D, &Q1);
  mpi_inv_mod (&rsa_ctx.QP, &rsa_ctx.Q, &rsa_ctx.P);
  mpi_free (&P1, &Q1, &H, NULL);

  DEBUG_INFO ("RSA sign...");

  if ((r = rsa_check_privkey (&rsa_ctx)) == 0)
    DEBUG_INFO ("ok...");
  else
    {
      DEBUG_INFO ("failed.\r\n");
      DEBUG_SHORT (r);
      rsa_free (&rsa_ctx);
      return r;
    }

  r = rsa_pkcs1_sign (&rsa_ctx, RSA_PRIVATE, SIG_RSA_RAW,
		      msg_len, raw_message, output);
  rsa_free (&rsa_ctx);
  if (r < 0)
    {
      DEBUG_INFO ("fail:");
      DEBUG_SHORT (r);
      return r;
    }
  else
    {
      res_APDU[RSA_SIGNATURE_LENGTH] =  0x90;
      res_APDU[RSA_SIGNATURE_LENGTH+1] =  0x00;
      res_APDU_size = RSA_SIGNATURE_LENGTH + 2;
      DEBUG_INFO ("done.\r\n");
      return 0;
    }
}

const uint8_t *
modulus_calc (const uint8_t *p, int len)
{
  mpi P, Q, N;
  uint8_t *modulus;

  (void)len;			/* 2048-bit assumed */
  modulus = malloc (2048 / 8);
  if (modulus == NULL)
    return NULL;

  mpi_init (&P, &Q, &N, NULL);
  mpi_read_binary (&P, p, 2048 / 8 / 2);
  mpi_read_binary (&Q, p + 128, 2048 / 8 / 2);
  mpi_mul_mpi (&N, &P, &Q);

  mpi_write_binary (&N, modulus, 2048 / 8);
  mpi_free (&P, &Q, &N, NULL);
  return modulus;
}

void
modulus_free (const uint8_t *p)
{
  free ((void *)p);
}

int
rsa_decrypt (const uint8_t *input, uint8_t *output, int msg_len,
	     struct key_data *kd)
{
  mpi P1, Q1, H;
  int r;
  int output_len;

  DEBUG_INFO ("RSA decrypt:");
  DEBUG_WORD ((uint32_t)&output_len);

  mpi_init (&P1, &Q1, &H, NULL);
  rsa_init (&rsa_ctx, RSA_PKCS_V15, 0);

  rsa_ctx.len = msg_len;
  DEBUG_WORD (msg_len);

  mpi_read_string (&rsa_ctx.E, 16, "10001");
  mpi_read_binary (&rsa_ctx.P, &kd->data[0], 2048 / 8 / 2);
  mpi_read_binary (&rsa_ctx.Q, &kd->data[128], 2048 / 8 / 2);
  mpi_mul_mpi (&rsa_ctx.N, &rsa_ctx.P, &rsa_ctx.Q);
  mpi_sub_int (&P1, &rsa_ctx.P, 1);
  mpi_sub_int (&Q1, &rsa_ctx.Q, 1);
  mpi_mul_mpi (&H, &P1, &Q1);
  mpi_inv_mod (&rsa_ctx.D , &rsa_ctx.E, &H);
  mpi_mod_mpi (&rsa_ctx.DP, &rsa_ctx.D, &P1);
  mpi_mod_mpi (&rsa_ctx.DQ, &rsa_ctx.D, &Q1);
  mpi_inv_mod (&rsa_ctx.QP, &rsa_ctx.Q, &rsa_ctx.P);
  mpi_free (&P1, &Q1, &H, NULL);

  DEBUG_INFO ("RSA decrypt ...");
#if 0
  /* This consume some memory */
  if ((r = rsa_check_privkey (&rsa_ctx)) == 0)
    DEBUG_INFO ("ok...");
  else
    {
      DEBUG_INFO ("failed.\r\n");
      DEBUG_SHORT (r);
      rsa_free (&rsa_ctx);
      return r;
    }
#endif

  r = rsa_pkcs1_decrypt (&rsa_ctx, RSA_PRIVATE, &output_len,
			 input, output, MAX_RES_APDU_SIZE - 2);
  rsa_free (&rsa_ctx);
  if (r < 0)
    {
      DEBUG_INFO ("fail:");
      DEBUG_SHORT (r);
      return r;
    }
  else
    {
      res_APDU[output_len] = 0x90;
      res_APDU[output_len+1] = 0x00;
      res_APDU_size = output_len + 2;
      DEBUG_INFO ("done.\r\n");
      return 0;
    }
}
