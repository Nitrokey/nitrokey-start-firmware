/*
 * ac.c -- Check access condition
 */

#include "ch.h"
#include "gnuk.h"

#include "polarssl/config.h"
#include "polarssl/sha1.h"

static uint8_t auth_status = AC_NONE_AUTHORIZED;

int
ac_check_status (uint8_t ac_flag)
{
  if (ac_flag == AC_ALWAYS)
    return 1;
  else if (ac_flag == AC_NEVER)
    return 0;
  else
    return (ac_flag & auth_status)? 1 : 0;
}

/*
 * XXX: should be implemented
 *
 * string --S2K(SHA1,SALT,ITERATE)--> key
 * load params from flash with key (key,enc_params --decrypt--> params)
 * check magic in params
 */

uint8_t keystring_pw1[KEYSTRING_LEN] = {
  0x62, 0x10, 0x27, 0x44, 0x34, 0x05, 0x2f, 0x20,
  0xfc, 0xb8, 0x3e, 0x1d, 0x0f, 0x49, 0x22, 0x04,
  0xfc, 0xb1, 0x18, 0x84
};

int
verify_pso_cds (uint8_t *pw, int pw_len)
{
  int r;

  sha1 (pw, pw_len, keystring_pw1);
  if ((r = gpg_load_key (GPG_KEY_FOR_SIGNATURE)) < 0)
    return r;

  auth_status |= AC_PSO_CDS_AUTHORIZED;
  return 0;
}

int
verify_pso_other (uint8_t *pw, int pw_len)
{
  auth_status |= AC_PSO_OTHER_AUTHORIZED;
  return 0;
}

int
verify_pso_admin (uint8_t *pw, int pw_len)
{
#if 0
  compute_hash;
  if (cmp_hash (pw3_hash, hash) == 0)
    good;
  else
    return -1;
#endif
  auth_status |= AC_ADMIN_AUTHORIZED;
  return 0;
}
