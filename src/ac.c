/*
 * ac.c -- Check access condition
 */

#include "config.h"
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

int
verify_pso_cds (const uint8_t *pw, int pw_len)
{
  int r;
  const uint8_t *pw_status_bytes = gpg_do_read_simple (NR_DO_PW_STATUS);
  uint8_t keystring[KEYSTRING_SIZE_PW1];
  uint8_t pwsb[SIZE_PW_STATUS_BYTES];

  if (pw_status_bytes == NULL
      || pw_status_bytes[PW_STATUS_PW1] == 0) /* locked */
    return 0;

  DEBUG_INFO ("verify_pso_cds\r\n");
  DEBUG_BYTE (pw_len);

  keystring[0] = pw_len;
  sha1 (pw, pw_len, keystring+1);
  memcpy (pwsb, pw_status_bytes, SIZE_PW_STATUS_BYTES);
  if ((r = gpg_do_load_prvkey (GPG_KEY_FOR_SIGNATURE, 1, keystring+1)) < 0)
    {
      pwsb[PW_STATUS_PW1]--;
      gpg_do_write_simple (NR_DO_PW_STATUS, pwsb, SIZE_PW_STATUS_BYTES);
      return r;
    }
  else if (pwsb[PW_STATUS_PW1] != 3)
    {
      pwsb[PW_STATUS_PW1] = 3;
      gpg_do_write_simple (NR_DO_PW_STATUS, pwsb, SIZE_PW_STATUS_BYTES);
    }

  auth_status |= AC_PSO_CDS_AUTHORIZED;
  return 1;
}

void
ac_reset_pso_cds (void)
{
  auth_status &= ~AC_PSO_CDS_AUTHORIZED;
}

int
verify_pso_other (const uint8_t *pw, int pw_len)
{
  int r;
  const uint8_t *pw_status_bytes = gpg_do_read_simple (NR_DO_PW_STATUS);
  uint8_t keystring[KEYSTRING_SIZE_PW1];
  uint8_t pwsb[SIZE_PW_STATUS_BYTES];

  if (pw_status_bytes == NULL
      || pw_status_bytes[PW_STATUS_PW1] == 0) /* locked */
    return 0;

  DEBUG_INFO ("verify_pso_other\r\n");

  keystring[0] = pw_len;
  sha1 (pw, pw_len, keystring+1);
  memcpy (pwsb, pw_status_bytes, SIZE_PW_STATUS_BYTES);
  if ((r = gpg_do_load_prvkey (GPG_KEY_FOR_DECRYPT, 1, keystring+1)) < 0)
    {
      pwsb[PW_STATUS_PW1]--;
      gpg_do_write_simple (NR_DO_PW_STATUS, pwsb, SIZE_PW_STATUS_BYTES);
      return r;
    }
  else if (pwsb[PW_STATUS_PW1] != 3)
    {
      pwsb[PW_STATUS_PW1] = 3;
      gpg_do_write_simple (NR_DO_PW_STATUS, pwsb, SIZE_PW_STATUS_BYTES);
    }

  auth_status |= AC_PSO_OTHER_AUTHORIZED;
  return 1;
}

/*
 * For keystring of PW3, we use SALT+ITER+MD format
 */

static uint32_t
decode_iterate_count (uint8_t x)
{
  return (16UL + ((x) & 15)) << (((x) >> 4) + 6);
}

static void
calc_md (int count, const uint8_t *salt, const uint8_t *pw, int pw_len,
	 uint8_t md[KEYSTRING_MD_SIZE])
{
  sha1_context sha1_ctx;

  sha1_starts (&sha1_ctx);

  while (count > pw_len + 8)
    {
      sha1_update (&sha1_ctx, salt, 8);
      sha1_update (&sha1_ctx, pw, pw_len);
      count -= pw_len + 8;
    }

  if (count < 8)
    sha1_update (&sha1_ctx, salt, count);
  else
    {
      sha1_update (&sha1_ctx, salt, 8);
      count -= 8;
      sha1_update (&sha1_ctx, pw, count);
    }

  sha1_finish (&sha1_ctx, md);
  memset (&sha1_ctx, 0, sizeof (sha1_ctx));
}

int
verify_admin_0 (const uint8_t *pw, int buf_len, int pw_len_known)
{
  const uint8_t *pw3_keystring;
  const uint8_t *pw_status_bytes = gpg_do_read_simple (NR_DO_PW_STATUS);
  int pw_len;

  if (pw_status_bytes == NULL
      || pw_status_bytes[PW_STATUS_PW3] == 0) /* locked */
    return 0;

  pw3_keystring = gpg_do_read_simple (NR_DO_KEYSTRING_PW3);
  if (pw3_keystring != NULL)
    {
      int count;
      uint8_t md[KEYSTRING_MD_SIZE];
      const uint8_t *salt;
      uint8_t pwsb[SIZE_PW_STATUS_BYTES];

      pw_len = pw3_keystring[0];
      if ((pw_len_known >= 0 && pw_len_known != pw_len) || pw_len < buf_len)
	goto failure;

      salt = &pw3_keystring[1];
      count = decode_iterate_count (pw3_keystring[1+8]);
      calc_md (count, salt, pw, pw_len, md);
      memcpy (pwsb, pw_status_bytes, SIZE_PW_STATUS_BYTES);

      if (memcmp (md, &pw3_keystring[1+8+1], KEYSTRING_MD_SIZE) != 0)
	{
	failure:
	  pwsb[PW_STATUS_PW3]--;
	  gpg_do_write_simple (NR_DO_PW_STATUS, pwsb, SIZE_PW_STATUS_BYTES);
	  return -1;
	}
      else if (pwsb[PW_STATUS_PW3] != 3)
	{		       /* OK, the user is now authenticated */
	  pwsb[PW_STATUS_PW3] = 3;
	  gpg_do_write_simple (NR_DO_PW_STATUS, pwsb, SIZE_PW_STATUS_BYTES);
	}
    }
  else
    /* For empty PW3, pass phrase should be "12345678" */
    {
      if ((pw_len_known >=0 && pw_len_known != 8)
	  || buf_len < 8 || strncmp ((const char *)pw, "12345678", 8) != 0)
	/* It is failure, but we don't try to lock for the case of empty PW3 */
	return -1;

      pw_len = 8;
    }

  return pw_len;
}

void
gpg_set_pw3 (const uint8_t *newpw, int newpw_len)
{
  uint8_t ks[KEYSTRING_SIZE_PW3];
  uint32_t random;

  ks[0] = newpw_len;
  random = get_random ();
  memcpy (&ks[1], &random, sizeof (random));
  random = get_random ();
  memcpy (&ks[5], &random, sizeof (random));
  ks[9] = 0x60;			/* 65536 iterations */

  calc_md (65536, &ks[1], newpw, newpw_len, &ks[10]);
  gpg_do_write_simple (NR_DO_KEYSTRING_PW3, ks, KEYSTRING_SIZE_PW3);
}

uint8_t keystring_md_pw3[KEYSTRING_MD_SIZE];

int
verify_admin (const uint8_t *pw, int pw_len)
{
  int r;

  r = verify_admin_0 (pw, pw_len, pw_len);
  if (r <= 0)
    return r;

  sha1 (pw, pw_len, keystring_md_pw3);
  auth_status |= AC_ADMIN_AUTHORIZED;
  return 1;
}
