/*
 * ac.c -- Check access condition
 */

#include "ch.h"
#include "gnuk.h"

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

int
verify_pso_cds (uint8_t *pw, int pw_len)
{
#if 0
  compute_hash;
  if (cmp_hash (pw1_hash, hash) == 0)
    good;
  else
    return -1;
#endif
  auth_status |= AC_PSO_CDS_AUTHORIZED;
  return 0;
}

int
verify_pso_other (uint8_t *pw, int pw_len)
{
#if 0
  compute_hash;
  if (cmp_hash (pw1_hash, hash) == 0)
    good;
  else
    return -1;
#endif
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
