#include "ch.h"
#include "gnuk.h"
#include "polarssl/config.h"
#include "polarssl/rsa.h"

static rsa_context ctx;

int
rsa_sign (const uint8_t *raw_message, uint8_t *output, int msg_len)
{
  mpi P1, Q1, H;
  int r;

  mpi_init( &P1, &Q1, &H, NULL );
  rsa_init( &ctx, RSA_PKCS_V15, 0 );

  ctx.len = 2048 / 8;
  mpi_read_string ( &ctx.E, 16, "10001" );
  mpi_read_binary ( &ctx.P, &kd.data[0], ctx.len / 2 );
  mpi_read_binary ( &ctx.Q, &kd.data[128], ctx.len / 2 );
  mpi_mul_mpi ( &ctx.N, &ctx.P, &ctx.Q );
  mpi_sub_int ( &P1, &ctx.P, 1 );
  mpi_sub_int ( &Q1, &ctx.Q, 1 );
  mpi_mul_mpi ( &H, &P1, &Q1 );
  mpi_inv_mod ( &ctx.D , &ctx.E, &H  );
  mpi_mod_mpi ( &ctx.DP, &ctx.D, &P1);
  mpi_mod_mpi ( &ctx.DQ, &ctx.D, &Q1);
  mpi_inv_mod ( &ctx.QP, &ctx.Q, &ctx.P);
  mpi_free (&P1, &Q1, &H, NULL);

  DEBUG_INFO ("RSA...");

  if ((r = rsa_check_privkey ( &ctx )) == 0)
    DEBUG_INFO ("ok...");
  else
    {
      DEBUG_INFO ("failed.\r\n");
      DEBUG_BYTE (r);
      rsa_free (&ctx);
      return r;
    }

  r = rsa_pkcs1_sign ( &ctx, RSA_PRIVATE, SIG_RSA_RAW,
		       msg_len, raw_message, output );
  rsa_free (&ctx);
  DEBUG_INFO ("done.\r\n");
  if (r < 0)
    {
      DEBUG_SHORT (r);
      return r;
    }
  else
    return 0;
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
