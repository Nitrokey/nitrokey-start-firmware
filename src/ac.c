/*
 * ac.c -- Check access condition
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

void
ac_reset_pso_cds (void)
{
  auth_status &= ~AC_PSO_CDS_AUTHORIZED;
}

uint8_t pw1_keystring[KEYSTRING_SIZE_PW1];

void
ac_reset_pso_other (void)
{
  memset (pw1_keystring, 0, KEYSTRING_SIZE_PW1);
  auth_status &= ~AC_PSO_OTHER_AUTHORIZED;
}

/*
 * Verify for "Perform Security Operation : Compute Digital Signature"
 */
int
verify_pso_cds (const uint8_t *pw, int pw_len)
{
  int r;
  uint8_t keystring[KEYSTRING_SIZE_PW1];

  if (gpg_passwd_locked (PW_ERR_PW1))
    return 0;

  DEBUG_INFO ("verify_pso_cds\r\n");
  DEBUG_BYTE (pw_len);

  keystring[0] = pw_len;
  sha1 (pw, pw_len, keystring+1);
  if ((r = gpg_do_load_prvkey (GPG_KEY_FOR_SIGNING, BY_USER, keystring+1)) < 0)
    {
      gpg_increment_pw_err_counter (PW_ERR_PW1);
      return r;
    }
  else
    gpg_reset_pw_err_counter (PW_ERR_PW1);

  auth_status |= AC_PSO_CDS_AUTHORIZED;
  return 1;
}

int
verify_pso_other (const uint8_t *pw, int pw_len)
{
  const uint8_t *ks_pw1;

  DEBUG_INFO ("verify_pso_other\r\n");

  if (gpg_passwd_locked (PW_ERR_PW1))
    return 0;

  /*
   * We check only the length of password string here.
   * Real check is defered to decrypt/authenticate routines.
   */
  ks_pw1 = gpg_do_read_simple (NR_DO_KEYSTRING_PW1);
  if ((ks_pw1 == NULL && pw_len == strlen (OPENPGP_CARD_INITIAL_PW1))
      || (ks_pw1 != NULL && pw_len == ks_pw1[0]))
    {				/* No problem */
      /*
       * We don't call gpg_reset_pw_err_counters here, because
       * password may be wrong.
       */
      pw1_keystring[0] = pw_len;
      sha1 (pw, pw_len, pw1_keystring+1);
      auth_status |= AC_PSO_OTHER_AUTHORIZED;
      return 1;
    }
  else
    {
      gpg_increment_pw_err_counter (PW_ERR_PW1);
      return 0;
    }
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
  int pw_len;

  if (gpg_passwd_locked (PW_ERR_PW3))
    return 0;

  pw3_keystring = gpg_do_read_simple (NR_DO_KEYSTRING_PW3);
  if (pw3_keystring != NULL)
    {
      int count;
      uint8_t md[KEYSTRING_MD_SIZE];
      const uint8_t *salt;

      pw_len = pw3_keystring[0];
      if ((pw_len_known >= 0 && pw_len_known != pw_len) || pw_len < buf_len)
	goto failure;

      salt = &pw3_keystring[1];
      count = decode_iterate_count (pw3_keystring[1+8]);
      calc_md (count, salt, pw, pw_len, md);

      if (memcmp (md, &pw3_keystring[1+8+1], KEYSTRING_MD_SIZE) != 0)
	{
	failure:
	  gpg_increment_pw_err_counter (PW_ERR_PW3);
	  return -1;
	}
      else
	/* OK, the user is now authenticated */
	gpg_reset_pw_err_counter (PW_ERR_PW3);
    }
  else
    /* For empty PW3, pass phrase should be OPENPGP_CARD_INITIAL_PW3 */
    {
      if ((pw_len_known >=0 && pw_len_known != strlen (OPENPGP_CARD_INITIAL_PW3))
	  || buf_len < (int)strlen (OPENPGP_CARD_INITIAL_PW3)
	  || strncmp ((const char *)pw, OPENPGP_CARD_INITIAL_PW3,
		      strlen (OPENPGP_CARD_INITIAL_PW3)) != 0)
	/* It is failure, but we don't try to lock for the case of empty PW3 */
	return -1;

      pw_len = strlen (OPENPGP_CARD_INITIAL_PW3);
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
