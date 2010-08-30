#include "ch.h"
#include "gnuk.h"
#include "polarssl/config.h"
#include "polarssl/rsa.h"

static unsigned char output[256];
static rsa_context ctx;

unsigned char *
rsa_sign (unsigned char *raw_message)
{
  mpi P1, Q1, H;
  int len;
  int r;

  len = (cmd_APDU[5]<<8) | cmd_APDU[6];
  put_byte (len);
  /* cmd_APDU_size - 6 */

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

  put_string ("RSA...");
  {
    int r;
    if ((r = rsa_check_privkey ( &ctx )) == 0)
      put_string ("ok...");
    else
      {
	put_string ("failed.\r\n");
	put_byte (r);
	rsa_free (&ctx);
	return output;
      }
  }

  r = rsa_pkcs1_sign ( &ctx, RSA_PRIVATE, SIG_RSA_RAW,
		       len, raw_message, output );
  put_short (r);
  put_string ("done.\r\n");
  rsa_free (&ctx);
  return output;
}
