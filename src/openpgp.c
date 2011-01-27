/*
 * openpgp.c -- OpenPGP card protocol support
 *
 * Copyright (C) 2010, 2011 Free Software Initiative of Japan
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
#include "hal.h"
#include "gnuk.h"
#include "openpgp.h"
#include "polarssl/config.h"
#include "polarssl/sha1.h"

#define INS_NOP	        			0x00
#define INS_VERIFY        			0x20
#define INS_CHANGE_REFERENCE_DATA		0x24
#define INS_PSO		  			0x2a
#define INS_RESET_RETRY_COUNTER			0x2c
#define INS_PGP_GENERATE_ASYMMETRIC_KEY_PAIR	0x47
#define INS_INTERNAL_AUTHENTICATE		0x88
#define INS_SELECT_FILE				0xa4
#define INS_READ_BINARY				0xb0
#define INS_GET_DATA				0xca
#define INS_UPDATE_BINARY			0xd6
#define INS_PUT_DATA				0xda
#define INS_PUT_DATA_ODD			0xdb	/* For key import */

static const uint8_t
select_file_TOP_result[] __attribute__ ((aligned (1))) = {
  0x00, 0x00,	     /* unused */
  0x00, 0x00,	     /* number of bytes in this directory: to be filled */
  0x3f, 0x00,	     /* field of selected file: MF, 3f00 */
  0x38,			/* it's DF */
  0xff,			/* unused */
  0xff,	0x44, 0x44,	/* access conditions */
  0x01,			/* status of the selected file (OK, unblocked) */
  0x05,			/* number of bytes of data follow */
    0x03,			/* Features: unused */
    0x01,			/* number of subdirectories (OpenPGP) */
    0x01,			/* number of elementary files (SerialNo) */
    0x00,			/* number of secret codes */
    0x00,			/* Unused */
  0x00, 0x00		/* PIN status: OK, PIN blocked?: No */
};

void
write_res_apdu (const uint8_t *p, int len, uint8_t sw1, uint8_t sw2)
{
  res_APDU_size = 2 + len;
  if (len)
    memcpy (res_APDU, p, len);
  res_APDU[len] = sw1;
  res_APDU[len+1] = sw2;
}

#define FILE_NONE	0
#define FILE_DF_OPENPGP	1
#define FILE_MF		2
#define FILE_EF_DIR	3
#define FILE_EF_SERIAL	4
#define FILE_EF_CH_CERTIFICATE  5
#define FILE_EF_RANDOM		6

static uint8_t file_selection;

static void
gpg_init (void)
{
  const uint8_t *flash_data_start;

  file_selection = FILE_NONE;
  flash_data_start = flash_init ();
  gpg_data_scan (flash_data_start);
}

static void
gpg_fini (void)
{
  ac_fini ();
}

#if defined(PINPAD_SUPPORT)
/* 
 * Invoke the thread PIN_MAIN, and let user input PIN string.
 * Return length of the string.
 * The string itself is in PIN_INPUT_BUFFER.
 */
static int
get_pinpad_input (int msg_code)
{
  Thread *t;

  t = chThdCreateFromHeap (NULL, THD_WA_SIZE (128),
			   NORMALPRIO, pin_main, (void *)msg_code);
  if (t == NULL)
    return -1;
  else
    {
      chThdWait (t);
      return pin_input_len;
    }
}
#endif

static void
cmd_verify (void)
{
  int len;
  uint8_t p2 = cmd_APDU[3];
  int r;
  int data_start = 5;
  const uint8_t *pw;

  DEBUG_INFO (" - VERIFY\r\n");
  DEBUG_BYTE (p2);

#if defined(PINPAD_SUPPORT)
  if (cmd_APDU_size == 4)
    /* Verification with pinpad */
    {
      len = get_pinpad_input (PIN_INPUT_CURRENT);
      if (len < 0)
	{
	  GPG_ERROR ();
	  return;
	}

      pw = pin_input_buffer;
    }
  else
#endif
    {
      len = cmd_APDU[4];
      if (len == 0)			/* extended length */
	{
	  len = (cmd_APDU[5]<<8) | cmd_APDU[6];
	  data_start = 7;
	}

      pw = &cmd_APDU[data_start];
    }


  if (p2 == 0x81)
    r = verify_pso_cds (pw, len);
  else if (p2 == 0x82)
    r = verify_other (pw, len);
  else
    r = verify_admin (pw, len);

  if (r < 0)
    {
      DEBUG_INFO ("failed\r\n");
      GPG_SECURITY_FAILURE ();
    }
  else if (r == 0)
    {
      DEBUG_INFO ("blocked\r\n");
      GPG_SECURITY_AUTH_BLOCKED ();
    }
  else
    {
      DEBUG_INFO ("good\r\n");
      GPG_SUCCESS ();
    }
}

int
gpg_change_keystring (int who_old, const uint8_t *old_ks,
		      int who_new, const uint8_t *new_ks)
{
  int r;
  int prv_keys_exist = 0;

  r = gpg_do_load_prvkey (GPG_KEY_FOR_SIGNING, who_old, old_ks);
  if (r < 0)
    return r;

  if (r > 0)
    prv_keys_exist++;

  r = gpg_do_chks_prvkey (GPG_KEY_FOR_SIGNING, who_old, old_ks,
			  who_new, new_ks);
  if (r < 0)
    return -2;

  r = gpg_do_load_prvkey (GPG_KEY_FOR_DECRYPTION, who_old, old_ks);
  if (r < 0)
    return r;

  if (r > 0)
    prv_keys_exist++;

  r = gpg_do_chks_prvkey (GPG_KEY_FOR_DECRYPTION, who_old, old_ks,
			  who_new, new_ks);
  if (r < 0)
    return -2;

  r = gpg_do_load_prvkey (GPG_KEY_FOR_AUTHENTICATION, who_old, old_ks);
  if (r < 0)
    return r;

  if (r > 0)
    prv_keys_exist++;

  r = gpg_do_chks_prvkey (GPG_KEY_FOR_AUTHENTICATION, who_old, old_ks,
			  who_new, new_ks);
  if (r < 0)
    return -2;

  if (prv_keys_exist)
    return 1;
  else
    return 0;
}

static void
cmd_change_password (void)
{
  uint8_t old_ks[KEYSTRING_MD_SIZE];
  uint8_t new_ks0[KEYSTRING_MD_SIZE+1];
  uint8_t *new_ks = &new_ks0[1];
  uint8_t p2 = cmd_APDU[3];
  int len;
  const uint8_t *pw;
  const uint8_t *newpw;
  int pw_len, newpw_len;
  int who = p2 - 0x80;
  int r;

  DEBUG_INFO ("Change PW\r\n");
  DEBUG_BYTE (who);

#if defined(PINPAD_SUPPORT)
  if (cmd_APDU_size == 4)
    /* Modification with pinpad */
    {
      pw_len = get_pinpad_input (PIN_INPUT_CURRENT);
      if (pw_len < 0)
	{
	  GPG_ERROR ();
	  return;
	}

      pw = &cmd_APDU[5];
      memcpy (&cmd_APDU[5], pin_input_buffer, pw_len);
      newpw = pw + pw_len;

      newpw_len = get_pinpad_input (PIN_INPUT_NEW);
      if (newpw_len < 0)
	{
	  GPG_ERROR ();
	  return;
	}

      memcpy (&cmd_APDU[5]+pw_len, pin_input_buffer, newpw_len);

      len = get_pinpad_input (PIN_INPUT_CONFIRM);
      if (len < 0)
	{
	  GPG_ERROR ();
	  return;
	}

      if (len != newpw_len || memcmp (newpw, pin_input_buffer, len) != 0)
	{
	  GPG_SECURITY_FAILURE ();
	  return;
	}

      len = cmd_APDU[4] = pw_len + newpw_len;
    }
  else
#endif
    {
      len = cmd_APDU[4];
      pw = &cmd_APDU[5];
      if (len == 0)			/* extended length */
	{
	  len = (cmd_APDU[5]<<8) | cmd_APDU[6];
	  pw += 2;
	}
    }

  if (who == BY_USER)			/* PW1 */
    {
      const uint8_t *pk = gpg_do_read_simple (NR_DO_KEYSTRING_PW1);

      if (pk == NULL)
	{
	  if (len < (int)strlen (OPENPGP_CARD_INITIAL_PW1))
	    {
	      DEBUG_INFO ("permission denied.\r\n");
	      GPG_SECURITY_FAILURE ();
	      return;
	    }

	  pw_len = strlen (OPENPGP_CARD_INITIAL_PW1);
	  newpw = pw + pw_len;
	  newpw_len = len - pw_len;
	}
      else
	{
	  pw_len = pk[0];
	  newpw = pw + pw_len;
	  newpw_len = len - pw_len;
	}
    }
  else				/* PW3 (0x83) */
    {
      pw_len = verify_admin_0 (pw, len, -1);

      if (pw_len < 0)
	{
	  DEBUG_INFO ("permission denied.\r\n");
	  GPG_SECURITY_FAILURE ();
	  return;
	}
      else if (pw_len == 0)
	{
	  DEBUG_INFO ("blocked.\r\n");
	  GPG_SECURITY_AUTH_BLOCKED ();
	  return;
	}
      else
	{
	  newpw = pw + pw_len;
	  newpw_len = len - pw_len;
	  gpg_set_pw3 (newpw, newpw_len);
	}
    }

  sha1 (pw, pw_len, old_ks);
  sha1 (newpw, newpw_len, new_ks);
  new_ks0[0] = newpw_len;

  r = gpg_change_keystring (who, old_ks, who, new_ks);
  if (r < -2)
    {
      DEBUG_INFO ("memory error.\r\n");
      GPG_MEMORY_FAILURE ();
    }
  else if (r < 0)
    {
      DEBUG_INFO ("security error.\r\n");
      GPG_SECURITY_FAILURE ();
    }
  else if (r == 0 && who == BY_USER)	/* no prvkey */
    {
      gpg_do_write_simple (NR_DO_KEYSTRING_PW1, new_ks0, KEYSTRING_SIZE_PW1);
      ac_reset_pso_cds ();
      ac_reset_other ();
      gpg_reset_pw_err_counter (PW_ERR_PW1);
      DEBUG_INFO ("Changed DO_KEYSTRING_PW1.\r\n");
      GPG_SUCCESS ();
    }
  else if (r > 0 && who == BY_USER)
    {
      gpg_do_write_simple (NR_DO_KEYSTRING_PW1, new_ks0, 1);
      ac_reset_pso_cds ();
      ac_reset_other ();
      gpg_reset_pw_err_counter (PW_ERR_PW1);
      DEBUG_INFO ("Changed length of DO_KEYSTRING_PW1.\r\n");
      GPG_SUCCESS ();
    }
  else				/* r >= 0 && who == BY_ADMIN */
    {
      DEBUG_INFO ("done.\r\n");
      ac_reset_admin ();
      gpg_reset_pw_err_counter (PW_ERR_PW3);
      GPG_SUCCESS ();
    }
}

static void
cmd_reset_user_password (void)
{
  uint8_t p1 = cmd_APDU[2];
  int len;
  const uint8_t *pw;
  const uint8_t *newpw;
  int pw_len, newpw_len;
  int r;
  uint8_t new_ks0[KEYSTRING_MD_SIZE+1];
  uint8_t *new_ks = &new_ks0[1];

  DEBUG_INFO ("Reset PW1\r\n");
  DEBUG_BYTE (p1);

#if defined(PINPAD_SUPPORT)
  if (cmd_APDU_size == 4)
    /* Modification with pinpad */
    {
      pw_len = get_pinpad_input (PIN_INPUT_CURRENT);
      if (pw_len < 0)
	{
	  GPG_ERROR ();
	  return;
	}

      pw = &cmd_APDU[5];
      memcpy (&cmd_APDU[5], pin_input_buffer, pw_len);
      newpw = pw + pw_len;

      newpw_len = get_pinpad_input (PIN_INPUT_NEW);
      if (newpw_len < 0)
	{
	  GPG_ERROR ();
	  return;
	}

      memcpy (&cmd_APDU[5]+pw_len, pin_input_buffer, newpw_len);

      len = get_pinpad_input (PIN_INPUT_CONFIRM);
      if (len < 0)
	{
	  GPG_ERROR ();
	  return;
	}

      if (len != newpw_len || memcmp (newpw, pin_input_buffer, len) != 0)
	{
	  GPG_SECURITY_FAILURE ();
	  return;
	}

      len = cmd_APDU[4] = pw_len + newpw_len;
    }
  else
#endif
    {
      len = cmd_APDU[4];
      pw = &cmd_APDU[5];
      if (len == 0)			/* extended length */
	{
	  len = (cmd_APDU[5]<<8) | cmd_APDU[6];
	  pw += 2;
	}
    }

  if (p1 == 0x00)		/* by User with Reseting Code */
    {
      const uint8_t *ks_rc = gpg_do_read_simple (NR_DO_KEYSTRING_RC);
      uint8_t old_ks[KEYSTRING_MD_SIZE];

      if (gpg_passwd_locked (PW_ERR_RC))
	{
	  DEBUG_INFO ("blocked.\r\n");
	  GPG_SECURITY_AUTH_BLOCKED ();
	  return;
	}

      if (ks_rc == NULL)
	{
	  DEBUG_INFO ("security error.\r\n");
	  GPG_SECURITY_FAILURE ();
	  return;
	}

      pw_len = ks_rc[0];
      newpw = pw + pw_len;
      newpw_len = len - pw_len;
      sha1 (pw, pw_len, old_ks);
      sha1 (newpw, newpw_len, new_ks);
      new_ks0[0] = newpw_len;
      r = gpg_change_keystring (BY_RESETCODE, old_ks, BY_USER, new_ks);
      if (r < -2)
	{
	  DEBUG_INFO ("memory error.\r\n");
	  GPG_MEMORY_FAILURE ();
	}
      else if (r < 0)
	{
	sec_fail:
	  DEBUG_INFO ("failed.\r\n");
	  gpg_increment_pw_err_counter (PW_ERR_RC);
	  GPG_SECURITY_FAILURE ();
	}
      else if (r == 0)
	{
	  if (memcmp (ks_rc+1, old_ks, KEYSTRING_MD_SIZE) != 0)
	    goto sec_fail;
	  DEBUG_INFO ("done (no prvkey).\r\n");
	  gpg_do_write_simple (NR_DO_KEYSTRING_PW1, new_ks0,
			       KEYSTRING_SIZE_PW1);
	  ac_reset_pso_cds ();
	  ac_reset_other ();
	  gpg_reset_pw_err_counter (PW_ERR_RC);
	  gpg_reset_pw_err_counter (PW_ERR_PW1);
	  GPG_SUCCESS ();
	}
      else
	{
	  DEBUG_INFO ("done.\r\n");
	  ac_reset_pso_cds ();
	  ac_reset_other ();
	  gpg_reset_pw_err_counter (PW_ERR_RC);
	  gpg_reset_pw_err_counter (PW_ERR_PW1);
	  GPG_SUCCESS ();
	}
    }
  else				/* by Admin (p1 == 0x02) */
    {
      const uint8_t *old_ks = keystring_md_pw3;

      if (!ac_check_status (AC_ADMIN_AUTHORIZED))
	{
	  DEBUG_INFO ("permission denied.\r\n");
	  GPG_SECURITY_FAILURE ();
	  return;
	}

      newpw_len = len;
      newpw = pw;
      sha1 (newpw, newpw_len, new_ks);
      new_ks0[0] = newpw_len;
      r = gpg_change_keystring (BY_ADMIN, old_ks, BY_USER, new_ks);
      if (r < -2)
	{
	  DEBUG_INFO ("memory error.\r\n");
	  GPG_MEMORY_FAILURE ();
	}
      else if (r < 0)
	{
	  DEBUG_INFO ("security error.\r\n");
	  GPG_SECURITY_FAILURE ();
	}
      else if (r == 0)
	{
	  DEBUG_INFO ("done (no privkey).\r\n");
	  gpg_do_write_simple (NR_DO_KEYSTRING_PW1, new_ks0,
			       KEYSTRING_SIZE_PW1);
	  ac_reset_pso_cds ();
	  ac_reset_other ();
	  gpg_reset_pw_err_counter (PW_ERR_PW1);
	  GPG_SUCCESS ();
	}
      else
	{
	  DEBUG_INFO ("done.\r\n");
	  ac_reset_pso_cds ();
	  ac_reset_other ();
	  gpg_reset_pw_err_counter (PW_ERR_PW1);
	  GPG_SUCCESS ();
	}
    }
}

static void
cmd_put_data (void)
{
  uint8_t *data;
  uint16_t tag;
  int len;

  DEBUG_INFO (" - PUT DATA\r\n");

  if (file_selection != FILE_DF_OPENPGP)
    GPG_NO_RECORD();

  tag = ((cmd_APDU[2]<<8) | cmd_APDU[3]);
  len = cmd_APDU_size - 5;
  data = &cmd_APDU[5];
  if (len >= 256)
    /* extended Lc */
    {
      data += 2;
      len -= 2;
    }

  gpg_do_put_data (tag, data, len);
}

static void
cmd_pgp_gakp (void)
{
  DEBUG_INFO (" - Generate Asymmetric Key Pair\r\n");
  DEBUG_BYTE (cmd_APDU[2]);

  if (cmd_APDU[2] == 0x81)
    /* Get public key */
    {
      if (cmd_APDU[4] == 0)
	gpg_do_public_key (cmd_APDU[7]);
      else
	gpg_do_public_key (cmd_APDU[5]);
    }
  else
    {					/* Generate key pair */
      if (!ac_check_status (AC_ADMIN_AUTHORIZED))
	GPG_SECURITY_FAILURE ();

      /* XXX: Not yet supported */
      GPG_ERROR ();
    }
}

static void
cmd_read_binary (void)
{
  DEBUG_INFO (" - Read binary\r\n");

  if (file_selection == FILE_EF_SERIAL)
    {
      if (cmd_APDU[3] >= 6)
	GPG_BAD_P0_P1 ();
      else
	{
	  gpg_do_get_data (0x004f, 1); /* AID */
	  res_APDU[0] = 0x5a;
	}
    }
  else
    GPG_NO_RECORD();
}

static void
cmd_select_file (void)
{
  if (cmd_APDU[2] == 4)	/* Selection by DF name: it must be OpenPGP card */
    {
      DEBUG_INFO (" - select DF by name\r\n");

      /*
       * P2 == 0, LC=6, name = D2 76 00 01 24 01
       */

      file_selection = FILE_DF_OPENPGP;
      if (cmd_APDU[3] == 0x0c)	/* No FCI */
	GPG_SUCCESS ();
      else
	{
	  gpg_do_get_data (0x004f, 1); /* AID */
	  memmove (res_APDU+2, res_APDU, res_APDU_size);
	  res_APDU[0] = 0x6f;
	  res_APDU[1] = 0x12;
	  res_APDU[2] = 0x84;	/* overwrite: DF name */
	  res_APDU_size += 2;
	}
    }
  else if (cmd_APDU[4] == 2
	   && cmd_APDU[5] == 0x2f && cmd_APDU[6] == 0x02)
    {
      DEBUG_INFO (" - select 0x2f02 EF\r\n");
      /*
       * MF.EF-GDO -- Serial number of the card and name of the owner
       */
      GPG_SUCCESS ();
      file_selection = FILE_EF_SERIAL;
    }
  else if (cmd_APDU[4] == 2
	   && cmd_APDU[5] == 0x3f && cmd_APDU[6] == 0x00)
    {
      DEBUG_INFO (" - select ROOT MF\r\n");
      if (cmd_APDU[3] == 0x0c)
	{
	  GPG_SUCCESS ();
	}
      else
	{
	  write_res_apdu (select_file_TOP_result,
			  sizeof (select_file_TOP_result), 0x90, 0x00);
	  res_APDU[2] = (data_objects_number_of_bytes & 0xff);
	  res_APDU[3] = (data_objects_number_of_bytes >> 8);
	}

      file_selection = FILE_MF;
      ac_fini ();		/* Reset authentication */
    }
  else
    {
      DEBUG_INFO (" - select ?? \r\n");

      file_selection = FILE_NONE;
      GPG_NO_FILE ();
    }
}

static void
cmd_get_data (void)
{
  uint16_t tag = ((cmd_APDU[2]<<8) | cmd_APDU[3]);

  DEBUG_INFO (" - Get Data\r\n");

  if (file_selection != FILE_DF_OPENPGP)
    GPG_NO_RECORD ();

  gpg_do_get_data (tag, 0);
}

static void
cmd_pso (void)
{
  int len = cmd_APDU[4];
  int data_start = 5;
  int r;

  if (len == 0)
    {
      len = (cmd_APDU[5]<<8) | cmd_APDU[6];
      data_start = 7;
    }

  DEBUG_INFO (" - PSO: ");
  DEBUG_WORD ((uint32_t)&r);

  if (cmd_APDU[2] == 0x9e && cmd_APDU[3] == 0x9a)
    {
      if (!ac_check_status (AC_PSO_CDS_AUTHORIZED))
	{
	  DEBUG_INFO ("security error.");
	  GPG_SECURITY_FAILURE ();
	  return;
	}

      if (cmd_APDU_size != 7 + 34 + 2  /* MD5 */
	  /* Header (with Extended Lc)=7, size of digestInfo, and Le=2-byte */
	  && cmd_APDU_size != 7 + 35 + 2  /* SHA1 / RIPEMD-160 */
	  && cmd_APDU_size != 7 + 47 + 2  /* SHA224 */
	  && cmd_APDU_size != 7 + 51 + 2  /* SHA256 */
	  && cmd_APDU_size != 7 + 67 + 2  /* SHA384 */
	  && cmd_APDU_size != 7 + 83 + 2) /* SHA512 */
	{
	  DEBUG_INFO (" wrong length: ");
	  DEBUG_SHORT (cmd_APDU_size);
	  GPG_ERROR ();
	}
      else
	{
	  DEBUG_SHORT (len);  /* Should be cmd_APDU_size - 8 [- 1] */

	  r = rsa_sign (&cmd_APDU[data_start], res_APDU, len,
			&kd[GPG_KEY_FOR_SIGNING]);
	  if (r < 0)
	    {
	      ac_reset_pso_cds ();
	      GPG_ERROR ();
	    }
	  else
	    {			/* Success */
	      if (gpg_get_pw1_lifetime () == 0)
		ac_reset_pso_cds ();

	      gpg_increment_digital_signature_counter ();
	    }
	}
    }
  else if (cmd_APDU[2] == 0x80 && cmd_APDU[3] == 0x86)
    {
      DEBUG_SHORT (len);

      if (gpg_passwd_locked (PW_ERR_PW1)
	  || !ac_check_status (AC_OTHER_AUTHORIZED))
	{
	  DEBUG_INFO ("security error.");
	  GPG_SECURITY_FAILURE ();
	  return;
	}

      /* Skip padding 0x00 */
      data_start++;
      len--;
      r = rsa_decrypt (&cmd_APDU[data_start], res_APDU, len,
		       &kd[GPG_KEY_FOR_DECRYPTION]);
      if (r < 0)
	GPG_ERROR ();
    }
  else
    {				/* XXX: not yet supported */
      DEBUG_INFO (" - ??");
      DEBUG_BYTE (cmd_APDU[2]);
      DEBUG_INFO (" - ??");
      DEBUG_BYTE (cmd_APDU[3]);
      GPG_ERROR ();
    }

  DEBUG_INFO ("PSO done.\r\n");
}

static void
cmd_internal_authenticate (void)
{
  int len = cmd_APDU[4];
  int data_start = 5;
  int r;

  if (len == 0)
    {
      len = (cmd_APDU[5]<<8) | cmd_APDU[6];
      data_start = 7;
    }

  DEBUG_INFO (" - INTERNAL AUTHENTICATE\r\n");

  if (cmd_APDU[2] == 0x00 && cmd_APDU[3] == 0x00)
    {
      DEBUG_SHORT (len);

      if (gpg_passwd_locked (PW_ERR_PW1)
	  || !ac_check_status (AC_OTHER_AUTHORIZED))
	{
	  DEBUG_INFO ("security error.");
	  GPG_SECURITY_FAILURE ();
	  return;
	}

      r = rsa_sign (&cmd_APDU[data_start], res_APDU, len,
		    &kd[GPG_KEY_FOR_AUTHENTICATION]);
      if (r < 0)
	GPG_ERROR ();
    }
  else
    {
      DEBUG_INFO (" - ??");
      DEBUG_BYTE (cmd_APDU[2]);
      DEBUG_INFO (" - ??");
      DEBUG_BYTE (cmd_APDU[3]);
      GPG_ERROR ();
    }

  DEBUG_INFO ("INTERNAL AUTHENTICATE done.\r\n");
}


static void
cmd_update_binary (void)
{
  int len = cmd_APDU[4];
  int data_start = 5;
  uint16_t offset;
  int r;

  if (len == 0)
    {
      len = (cmd_APDU[5]<<8) | cmd_APDU[6];
      data_start = 7;
    }

  DEBUG_INFO (" - UPDATE BINARY\r\n");

  if (gpg_passwd_locked (PW_ERR_PW3) || !ac_check_status (AC_ADMIN_AUTHORIZED))
    {
      DEBUG_INFO ("security error.");
      GPG_SECURITY_FAILURE ();
      return;
    }

  if ((cmd_APDU[2] & 0x80))
    if ((cmd_APDU[2] & 0x7f) <= 0x01)
      {
	file_selection = FILE_EF_CH_CERTIFICATE + (cmd_APDU[2] & 0x7f);
	r = flash_erase_binary (file_selection - FILE_EF_CH_CERTIFICATE);
	if (r < 0)
	  {
	    DEBUG_INFO ("memory error.\r\n");
	    GPG_MEMORY_FAILURE ();
	    return;
	  }

	offset = 0;
      }
    else
      {
	GPG_NO_FILE ();
	return;
      }
  else
    {
      if (file_selection != FILE_EF_CH_CERTIFICATE
	  && file_selection != FILE_EF_RANDOM)
	{
	  GPG_COMMAND_NOT_ALLOWED ();
	  return;
	}

      offset = (cmd_APDU[2] << 8) | cmd_APDU[3];
    }

  DEBUG_SHORT (len);
  DEBUG_SHORT (offset);

  r = flash_write_binary (file_selection - FILE_EF_CH_CERTIFICATE,
			  &cmd_APDU[data_start], len, offset);
  if (r < 0)
    {
      DEBUG_INFO ("memory error.\r\n");
      GPG_MEMORY_FAILURE ();
      return;
    }

  DEBUG_INFO ("UPDATE BINARY done.\r\n");
}


struct command
{
  uint8_t command;
  void (*cmd_handler) (void);
};

const struct command cmds[] = {
  { INS_VERIFY, cmd_verify },
  { INS_CHANGE_REFERENCE_DATA, cmd_change_password },
  { INS_PSO, cmd_pso },
  { INS_RESET_RETRY_COUNTER, cmd_reset_user_password },
  { INS_PGP_GENERATE_ASYMMETRIC_KEY_PAIR, cmd_pgp_gakp },
  { INS_INTERNAL_AUTHENTICATE, cmd_internal_authenticate },
  { INS_SELECT_FILE, cmd_select_file },
  { INS_READ_BINARY, cmd_read_binary },
  { INS_GET_DATA, cmd_get_data },
  { INS_UPDATE_BINARY, cmd_update_binary }, /* Not in OpenPGP card protocol */
  { INS_PUT_DATA, cmd_put_data },
  { INS_PUT_DATA_ODD, cmd_put_data },
};
#define NUM_CMDS ((int)(sizeof (cmds) / sizeof (struct command)))

static void
process_command_apdu (void)
{
  int i;
  uint8_t cmd = cmd_APDU[1];

  for (i = 0; i < NUM_CMDS; i++)
    if (cmds[i].command == cmd)
      break;

  res_APDU_pointer = NULL; /* default */

  if (i < NUM_CMDS)
    cmds[i].cmd_handler ();
  else
    {
      DEBUG_INFO (" - ??");
      DEBUG_BYTE (cmd);
      GPG_NO_INS ();
    }
}

msg_t
GPGthread (void *arg)
{
  Thread *icc_thread = (Thread *)arg;

  gpg_init ();

  chEvtClear (ALL_EVENTS);

  while (!chThdShouldTerminate ())
    {
      eventmask_t m;

      m = chEvtWaitOne (ALL_EVENTS);

      DEBUG_INFO ("GPG!: ");
      DEBUG_WORD ((uint32_t)&m);

      if (icc_data_size != 0)
	{
	  process_command_apdu ();
	  chEvtSignal (icc_thread, EV_EXEC_FINISHED);
	}
    }

  gpg_fini ();
  return 0;
}
