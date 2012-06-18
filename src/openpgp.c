/*
 * openpgp.c -- OpenPGP card protocol support
 *
 * Copyright (C) 2010, 2011, 2012 Free Software Initiative of Japan
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
#include "sys.h"
#include "openpgp.h"
#include "sha256.h"

#define CLS(a) a.cmd_apdu_head[0]
#define INS(a) a.cmd_apdu_head[1]
#define P1(a) a.cmd_apdu_head[2]
#define P2(a) a.cmd_apdu_head[3]

#define INS_VERIFY        			0x20
#define INS_CHANGE_REFERENCE_DATA		0x24
#define INS_PSO		  			0x2a
#define INS_RESET_RETRY_COUNTER			0x2c
#define INS_PGP_GENERATE_ASYMMETRIC_KEY_PAIR	0x47
#define INS_EXTERNAL_AUTHENTICATE		0x82
#define INS_GET_CHALLENGE			0x84
#define INS_INTERNAL_AUTHENTICATE		0x88
#define INS_SELECT_FILE				0xa4
#define INS_READ_BINARY				0xb0
#define INS_GET_DATA				0xca
#define INS_WRITE_BINARY			0xd0
#define INS_UPDATE_BINARY			0xd6
#define INS_PUT_DATA				0xda
#define INS_PUT_DATA_ODD			0xdb	/* For key import */

#define CHALLENGE_LEN	32
static const uint8_t *challenge; /* Random bytes */

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
set_res_sw (uint8_t sw1, uint8_t sw2)
{
  apdu.sw = (sw1 << 8) | sw2;
}

#define FILE_NONE	0
#define FILE_DF_OPENPGP	1
#define FILE_MF		2
#define FILE_EF_DIR	3
#define FILE_EF_SERIAL_NO	4
#define FILE_EF_UPDATE_KEY_0	5
#define FILE_EF_UPDATE_KEY_1	6
#define FILE_EF_UPDATE_KEY_2	7
#define FILE_EF_UPDATE_KEY_3	8
#define FILE_EF_CH_CERTIFICATE	9

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
 * Let user input PIN string.
 * Return length of the string.
 * The string itself is in PIN_INPUT_BUFFER.
 */
static int
get_pinpad_input (int msg_code)
{
  int r;

  led_blink (LED_START_COMMAND);
  r = pinpad_getline (msg_code, MS2ST (8000));
  led_blink (LED_FINISH_COMMAND);
  return r;
}
#endif

static void
cmd_verify (void)
{
  int len;
  uint8_t p2 = P2 (apdu);
  int r;
  const uint8_t *pw;

  DEBUG_INFO (" - VERIFY\r\n");
  DEBUG_BYTE (p2);

  len = apdu.cmd_apdu_data_len;
  pw = apdu.cmd_apdu_data;

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
  uint8_t p1 = P1 (apdu);	/* 0: change (old+new), 1: exchange (new) */
  uint8_t p2 = P2 (apdu);
  int len;
  const uint8_t *pw;
  const uint8_t *newpw;
  int pw_len, newpw_len;
  int who = p2 - 0x80;
  int who_old;
  int r;

  DEBUG_INFO ("Change PW\r\n");
  DEBUG_BYTE (who);

  len = apdu.cmd_apdu_data_len;
  pw = apdu.cmd_apdu_data;

  if (p1 != 0)
    {
      GPG_FUNCTION_NOT_SUPPORTED();
      return;
    }

  if (who == BY_USER)			/* PW1 */
    {
      const uint8_t *ks_pw1 = gpg_do_read_simple (NR_DO_KEYSTRING_PW1);

      pw_len = verify_user_0 (AC_PSO_CDS_AUTHORIZED, pw, len, -1, ks_pw1);
      who_old = who;

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
	  who_old = admin_authorized;
	}
    }

  sha256 (pw, pw_len, old_ks);
  sha256 (newpw, newpw_len, new_ks);
  new_ks0[0] = newpw_len;

  r = gpg_change_keystring (who_old, old_ks, who, new_ks);
  if (r <= -2)
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
      DEBUG_INFO ("Changed DO_KEYSTRING_PW1.\r\n");
      GPG_SUCCESS ();
    }
  else if (r > 0 && who == BY_USER)
    {
      gpg_do_write_simple (NR_DO_KEYSTRING_PW1, new_ks0, 1);
      ac_reset_pso_cds ();
      ac_reset_other ();
      DEBUG_INFO ("Changed length of DO_KEYSTRING_PW1.\r\n");
      GPG_SUCCESS ();
    }
  else				/* r >= 0 && who == BY_ADMIN */
    {
      DEBUG_INFO ("done.\r\n");
      ac_reset_admin ();
      GPG_SUCCESS ();
    }
}


#define RESETCODE_S2K_MAGIC "\xffRESET\r\n"

void
resetcode_s2k (const unsigned char *input, unsigned int ilen,
	       unsigned char output[32])
{
  sha256_context ctx;

  sha256_start (&ctx);
  sha256_update (&ctx, input, ilen);
  sha256_update (&ctx, (unsigned char *)RESETCODE_S2K_MAGIC,
		 sizeof (RESETCODE_S2K_MAGIC));
  sha256_finish (&ctx, output);
}


static void
cmd_reset_user_password (void)
{
  uint8_t p1 = P1 (apdu);
  int len;
  const uint8_t *pw;
  const uint8_t *newpw;
  int pw_len, newpw_len;
  int r;
  uint8_t new_ks0[KEYSTRING_MD_SIZE+1];
  uint8_t *new_ks = &new_ks0[1];

  DEBUG_INFO ("Reset PW1\r\n");
  DEBUG_BYTE (p1);

  len = apdu.cmd_apdu_data_len;
  pw = apdu.cmd_apdu_data;

  if (p1 == 0x00)		/* by User with Reseting Code */
    {
      const uint8_t *ks_rc = gpg_do_read_simple (NR_DO_KEYSTRING_RC);
      uint8_t old_ks[KEYSTRING_MD_SIZE];

      if (gpg_pw_locked (PW_ERR_RC))
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
      resetcode_s2k (pw, pw_len, old_ks);
      sha256 (newpw, newpw_len, new_ks);
      new_ks0[0] = newpw_len;
      r = gpg_change_keystring (BY_RESETCODE, old_ks, BY_USER, new_ks);
      if (r <= -2)
	{
	  DEBUG_INFO ("memory error.\r\n");
	  GPG_MEMORY_FAILURE ();
	}
      else if (r < 0)
	{
	sec_fail:
	  DEBUG_INFO ("failed.\r\n");
	  gpg_pw_increment_err_counter (PW_ERR_RC);
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
	  gpg_pw_reset_err_counter (PW_ERR_RC);
	  gpg_pw_reset_err_counter (PW_ERR_PW1);
	  GPG_SUCCESS ();
	}
      else
	{
	  DEBUG_INFO ("done.\r\n");
	  ac_reset_pso_cds ();
	  ac_reset_other ();
	  gpg_pw_reset_err_counter (PW_ERR_RC);
	  gpg_pw_reset_err_counter (PW_ERR_PW1);
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
      sha256 (newpw, newpw_len, new_ks);
      new_ks0[0] = newpw_len;
      r = gpg_change_keystring (admin_authorized, old_ks, BY_USER, new_ks);
      if (r <= -2)
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
	  gpg_pw_reset_err_counter (PW_ERR_PW1);
	  GPG_SUCCESS ();
	}
      else
	{
	  DEBUG_INFO ("done.\r\n");
	  ac_reset_pso_cds ();
	  ac_reset_other ();
	  gpg_pw_reset_err_counter (PW_ERR_PW1);
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

  tag = ((P1 (apdu)<<8) | P2 (apdu));
  len = apdu.cmd_apdu_data_len;
  data = apdu.cmd_apdu_data;
  gpg_do_put_data (tag, data, len);
}

static void
cmd_pgp_gakp (void)
{
  DEBUG_INFO (" - Generate Asymmetric Key Pair\r\n");
  DEBUG_BYTE (P1 (apdu));

  if (P1 (apdu) == 0x81)
    /* Get public key */
    gpg_do_public_key (apdu.cmd_apdu_data[0]);
  else
    {
      if (!ac_check_status (AC_ADMIN_AUTHORIZED))
	GPG_SECURITY_FAILURE ();
#ifdef KEYGEN_SUPPORT
      /* Generate key pair */
      gpg_do_keygen (apdu.cmd_apdu_data[0]);
#else
      GPG_FUNCTION_NOT_SUPPORTED ();
#endif
    }
}

const uint8_t *
gpg_get_firmware_update_key (uint8_t keyno)
{
  extern uint8_t _updatekey_store;
  const uint8_t *p;

  p = &_updatekey_store + keyno * KEY_CONTENT_LEN;
  return p;
}

#ifdef CERTDO_SUPPORT
#define FILEID_CH_CERTIFICATE_IS_VALID 1
#else
#define FILEID_CH_CERTIFICATE_IS_VALID 0
#endif

static void
cmd_read_binary (void)
{
  int is_short_EF = (P1 (apdu) & 0x80) != 0;
  uint8_t file_id;
  const uint8_t *p;
  uint16_t offset;

  DEBUG_INFO (" - Read binary\r\n");

  if (is_short_EF)
    file_id = (P1 (apdu) & 0x1f);
  else
    file_id = file_selection - FILE_EF_SERIAL_NO + FILEID_SERIAL_NO;

  if ((!FILEID_CH_CERTIFICATE_IS_VALID && file_id == FILEID_CH_CERTIFICATE)
      || file_id > FILEID_CH_CERTIFICATE)
    {
      GPG_NO_FILE ();
      return;
    }

  if (is_short_EF)
    {
      file_selection = file_id - FILEID_SERIAL_NO + FILE_EF_SERIAL_NO;
      offset = P2 (apdu);
    }
  else
    offset = (P1 (apdu) << 8) | P2 (apdu);

  if (file_id == FILEID_SERIAL_NO)
    {
      if (offset != 0)
	GPG_BAD_P1_P2 ();
      else
	{
	  gpg_do_get_data (0x004f, 1); /* Get AID... */
	  res_APDU[0] = 0x5a; /* ... and overwrite the first byte of data. */
	}
      return;
    }

  if (file_id >= FILEID_UPDATE_KEY_0 && file_id <= FILEID_UPDATE_KEY_3)
    {
      if (offset != 0)
	GPG_MEMORY_FAILURE ();
      else
	{
	  p = gpg_get_firmware_update_key (file_id - FILEID_UPDATE_KEY_0);
	  res_APDU_size = KEY_CONTENT_LEN;
	  memcpy (res_APDU, p, KEY_CONTENT_LEN);
	  GPG_SUCCESS ();
	}
    }
#if defined(CERTDO_SUPPORT)
  else /* file_id == FILEID_CH_CERTIFICATE */
    {
      uint16_t len = 256;

      p = &ch_certificate_start;
      if (offset >= FLASH_CH_CERTIFICATE_SIZE)
	GPG_MEMORY_FAILURE ();
      else
	{
	  if (offset + len >= FLASH_CH_CERTIFICATE_SIZE)
	    len = FLASH_CH_CERTIFICATE_SIZE - offset;

	  res_APDU_size = len;
	  memcpy (res_APDU, p + offset, len);
	  GPG_SUCCESS ();
	}
    }
#endif
}

static void
cmd_select_file (void)
{
  if (P1 (apdu) == 4)	/* Selection by DF name */
    {
      DEBUG_INFO (" - select DF by name\r\n");

      /* name = D2 76 00 01 24 01 */
      if (apdu.cmd_apdu_data_len != 6
	  || memcmp (openpgpcard_aid, apdu.cmd_apdu_data, 6) != 0)
	{
	  DEBUG_SHORT (apdu.cmd_apdu_data_len);
	  DEBUG_BINARY (apdu.cmd_apdu_data, apdu.cmd_apdu_data_len);

	  GPG_NO_FILE ();
	  return;
	}

      file_selection = FILE_DF_OPENPGP;
      if ((P2 (apdu) & 0x0c) == 0x0c)	/* No FCI */
	GPG_SUCCESS ();
      else
	{
	  gpg_do_get_data (0x004f, 1); /* AID */
	  memmove (res_APDU+2, res_APDU, res_APDU_size);
	  res_APDU[0] = 0x6f;
	  res_APDU[1] = 0x12;
	  res_APDU[2] = 0x84;	/* overwrite: DF name */
	  res_APDU_size += 2;
	  GPG_SUCCESS ();
	}
    }
  else if (apdu.cmd_apdu_data_len == 2
	   && apdu.cmd_apdu_data[0] == 0x2f && apdu.cmd_apdu_data[1] == 0x02)
    {
      DEBUG_INFO (" - select 0x2f02 EF\r\n");
      /*
       * MF.EF-GDO -- Serial number of the card and name of the owner
       */
      GPG_SUCCESS ();
      file_selection = FILE_EF_SERIAL_NO;
    }
  else if (apdu.cmd_apdu_data_len == 2
	   && apdu.cmd_apdu_data[0] == 0x3f && apdu.cmd_apdu_data[1] == 0x00)
    {
      DEBUG_INFO (" - select ROOT MF\r\n");
      if (P2 (apdu) == 0x0c)
	{
	  GPG_SUCCESS ();
	}
      else
	{
	  int len = sizeof (select_file_TOP_result);

	  res_APDU_size = len;
	  memcpy (res_APDU, select_file_TOP_result, len);
	  res_APDU[2] = (data_objects_number_of_bytes & 0xff);
	  res_APDU[3] = (data_objects_number_of_bytes >> 8);
	  GPG_SUCCESS ();
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
  uint16_t tag = ((P1 (apdu)<<8) | P2 (apdu));

  DEBUG_INFO (" - Get Data\r\n");

  if (file_selection != FILE_DF_OPENPGP)
    GPG_NO_RECORD ();

  gpg_do_get_data (tag, 0);
}

static void
cmd_pso (void)
{
  int len = apdu.cmd_apdu_data_len;
  int r;

  DEBUG_INFO (" - PSO: ");
  DEBUG_WORD ((uint32_t)&r);
  DEBUG_BINARY (apdu.cmd_apdu_data, apdu.cmd_apdu_data_len);

  if (P1 (apdu) == 0x9e && P2 (apdu) == 0x9a)
    {
      if (!ac_check_status (AC_PSO_CDS_AUTHORIZED))
	{
	  DEBUG_INFO ("security error.");
	  GPG_SECURITY_FAILURE ();
	  return;
	}

      /* Check size of digestInfo */
      if (len != 34		/* MD5 */
	  && len != 35		/* SHA1 / RIPEMD-160 */
	  && len != 47		/* SHA224 */
	  && len != 51		/* SHA256 */
	  && len != 67		/* SHA384 */
	  && len != 83)		/* SHA512 */
	{
	  DEBUG_INFO (" wrong length: ");
	  DEBUG_SHORT (len);
	  GPG_ERROR ();
	}
      else
	{
	  DEBUG_SHORT (len);
	  DEBUG_BINARY (&kd[GPG_KEY_FOR_SIGNING], KEY_CONTENT_LEN);

	  r = rsa_sign (apdu.cmd_apdu_data, res_APDU, len,
			&kd[GPG_KEY_FOR_SIGNING]);
	  if (r < 0)
	    {
	      ac_reset_pso_cds ();
	      GPG_ERROR ();
	    }
	  else
	    /* Success */
	    gpg_increment_digital_signature_counter ();
	}
    }
  else if (P1 (apdu) == 0x80 && P2 (apdu) == 0x86)
    {
      DEBUG_SHORT (len);
      DEBUG_BINARY (&kd[GPG_KEY_FOR_DECRYPTION], KEY_CONTENT_LEN);

      if (!ac_check_status (AC_OTHER_AUTHORIZED))
	{
	  DEBUG_INFO ("security error.");
	  GPG_SECURITY_FAILURE ();
	  return;
	}

      /* Skip padding 0x00 */
      len--;
      r = rsa_decrypt (apdu.cmd_apdu_data+1, res_APDU, len,
		       &kd[GPG_KEY_FOR_DECRYPTION]);
      if (r < 0)
	GPG_ERROR ();
    }
  else
    {
      DEBUG_INFO (" - ??");
      DEBUG_BYTE (P1 (apdu));
      DEBUG_INFO (" - ??");
      DEBUG_BYTE (P2 (apdu));
      GPG_ERROR ();
    }

  DEBUG_INFO ("PSO done.\r\n");
}


#define MAX_DIGEST_INFO_LEN 102 /* 40% */
static void
cmd_internal_authenticate (void)
{
  int len = apdu.cmd_apdu_data_len;
  int r;

  DEBUG_INFO (" - INTERNAL AUTHENTICATE\r\n");

  if (P1 (apdu) == 0x00 && P2 (apdu) == 0x00)
    {
      DEBUG_SHORT (len);

      if (!ac_check_status (AC_OTHER_AUTHORIZED))
	{
	  DEBUG_INFO ("security error.");
	  GPG_SECURITY_FAILURE ();
	  return;
	}

      if (len > MAX_DIGEST_INFO_LEN)
	{
	  DEBUG_INFO ("input is too long.");
	  GPG_CONDITION_NOT_SATISFIED ();
	  return;
	}

      r = rsa_sign (apdu.cmd_apdu_data, res_APDU, len,
		    &kd[GPG_KEY_FOR_AUTHENTICATION]);
      if (r < 0)
	GPG_ERROR ();
    }
  else
    {
      DEBUG_INFO (" - ??");
      DEBUG_BYTE (P1 (apdu));
      DEBUG_INFO (" - ??");
      DEBUG_BYTE (P2 (apdu));
      GPG_ERROR ();
    }

  DEBUG_INFO ("INTERNAL AUTHENTICATE done.\r\n");
}

#define MBD_OPRATION_WRITE  0
#define MBD_OPRATION_UPDATE 1

static void
modify_binary (uint8_t op, uint8_t p1, uint8_t p2, int len)
{
  uint8_t file_id;
  uint16_t offset;
  int is_short_EF = (p1 & 0x80) != 0;
  int r;

  if (!ac_check_status (AC_ADMIN_AUTHORIZED))
    {
      DEBUG_INFO ("security error.");
      GPG_SECURITY_FAILURE ();
      return;
    }

  if (is_short_EF)
    file_id = (p1 & 0x1f);
  else
    file_id = file_selection - FILE_EF_SERIAL_NO + FILEID_SERIAL_NO;

  if (!FILEID_CH_CERTIFICATE_IS_VALID && file_id == FILEID_CH_CERTIFICATE)
    {
      GPG_NO_FILE ();
      return;
    }

  if (op == MBD_OPRATION_UPDATE && file_id != FILEID_CH_CERTIFICATE)
    {
      GPG_CONDITION_NOT_SATISFIED ();
      return;
    }

  if (file_id > FILEID_CH_CERTIFICATE)
    {
      GPG_NO_FILE ();
      return;
    }

  if (is_short_EF)
    {
      file_selection = file_id - FILEID_SERIAL_NO + FILE_EF_SERIAL_NO;
      offset = p2;

      if (op == MBD_OPRATION_UPDATE)
	{
	  r = flash_erase_binary (file_id);
	  if (r < 0)
	    {
	      DEBUG_INFO ("memory error.\r\n");
	      GPG_MEMORY_FAILURE ();
	      return;
	    }
	}
    }
  else
    offset = (p1 << 8) | p2;

  DEBUG_SHORT (len);
  DEBUG_SHORT (offset);

  r = flash_write_binary (file_id, apdu.cmd_apdu_data, len, offset);
  if (r < 0)
    {
      DEBUG_INFO ("memory error.\r\n");
      GPG_MEMORY_FAILURE ();
      return;
    }

  GPG_SUCCESS ();
}


#if defined(CERTDO_SUPPORT)
static void
cmd_update_binary (void)
{
  int len = apdu.cmd_apdu_data_len;

  DEBUG_INFO (" - UPDATE BINARY\r\n");
  modify_binary (MBD_OPRATION_UPDATE, P1 (apdu), P2 (apdu), len);
  DEBUG_INFO ("UPDATE BINARY done.\r\n");
}
#endif


static void
cmd_write_binary (void)
{
  int len = apdu.cmd_apdu_data_len;
  int i;
  const uint8_t *p;

  DEBUG_INFO (" - WRITE BINARY\r\n");
  modify_binary (MBD_OPRATION_WRITE, P1 (apdu), P2 (apdu), len);

  for (i = 0; i < 4; i++)
    {
      p = gpg_get_firmware_update_key (i);
      if (p[0] != 0x00 || p[1] != 0x00) /* still valid */
	break;
    }

  if (i == 4)			/* all update keys are removed */
    {
      p = gpg_get_firmware_update_key (0);
      flash_erase_page ((uint32_t)p);
    }

  DEBUG_INFO ("WRITE BINARY done.\r\n");
}


static void
cmd_external_authenticate (void)
{
  const uint8_t *pubkey;
  const uint8_t *signature = apdu.cmd_apdu_data;
  int len = apdu.cmd_apdu_data_len;
  uint8_t keyno = P2 (apdu);
  int r;

  DEBUG_INFO (" - EXTERNAL AUTHENTICATE\r\n");

  if (keyno > 4)
    {
      GPG_CONDITION_NOT_SATISFIED ();
      return;
    }

  pubkey = gpg_get_firmware_update_key (keyno);
  if (len != 256
      || (pubkey[0] == 0xff && pubkey[1] == 0xff) /* not registered */
      || (pubkey[0] == 0x00 && pubkey[1] == 0x00) /* removed */)
    {
      GPG_CONDITION_NOT_SATISFIED ();
      return;
    }

  r = rsa_verify (pubkey, challenge, signature);
  random_bytes_free (challenge);
  challenge = NULL;

  if (r < 0)
    {
      GPG_SECURITY_FAILURE ();
      return;
    }

  chThdTerminate (chThdSelf ());
  set_res_sw (0xff, 0xff);
  DEBUG_INFO ("EXTERNAL AUTHENTICATE done.\r\n");
}

static void
cmd_get_challenge (void)
{
  DEBUG_INFO (" - GET CHALLENGE\r\n");

  if (challenge)
    random_bytes_free (challenge);

  challenge = random_bytes_get ();
  memcpy (res_APDU, challenge, CHALLENGE_LEN);
  res_APDU_size = CHALLENGE_LEN;
  GPG_SUCCESS ();
  DEBUG_INFO ("GET CHALLENGE done.\r\n");
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
  { INS_EXTERNAL_AUTHENTICATE,	            /* Not in OpenPGP card protocol */
    cmd_external_authenticate },
  { INS_GET_CHALLENGE, cmd_get_challenge }, /* Not in OpenPGP card protocol */
  { INS_INTERNAL_AUTHENTICATE, cmd_internal_authenticate },
  { INS_SELECT_FILE, cmd_select_file },
  { INS_READ_BINARY, cmd_read_binary },
  { INS_GET_DATA, cmd_get_data },
  { INS_WRITE_BINARY, cmd_write_binary},    /* Not in OpenPGP card protocol */
#if defined(CERTDO_SUPPORT)
  { INS_UPDATE_BINARY, cmd_update_binary }, /* Not in OpenPGP card protocol */
#endif
  { INS_PUT_DATA, cmd_put_data },
  { INS_PUT_DATA_ODD, cmd_put_data },
};
#define NUM_CMDS ((int)(sizeof (cmds) / sizeof (struct command)))

static void
process_command_apdu (void)
{
  int i;
  uint8_t cmd = INS (apdu);

  for (i = 0; i < NUM_CMDS; i++)
    if (cmds[i].command == cmd)
      break;

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
      eventmask_t m = chEvtWaitOne (ALL_EVENTS);
#if defined(PINPAD_SUPPORT)
      int len, pw_len, newpw_len;
#endif

      DEBUG_INFO ("GPG!: ");

      if (m == EV_VERIFY_CMD_AVAILABLE)
	{
#if defined(PINPAD_SUPPORT)
	  if (INS (apdu) != INS_VERIFY)
	    {
	      GPG_CONDITION_NOT_SATISFIED ();
	      goto done;
	    }

	  pw_len = get_pinpad_input (PIN_INPUT_CURRENT);
	  if (pw_len < 0)
	    {
	      GPG_ERROR ();
	      goto done;
	    }
	  memcpy (apdu.cmd_apdu_data, pin_input_buffer, pw_len);
	  apdu.cmd_apdu_data_len = pw_len;
#else
	  GPG_ERROR ();
	  goto done;
#endif
	}
      else if (m == EV_MODIFY_CMD_AVAILABLE)
	{
#if defined(PINPAD_SUPPORT)
	  uint8_t bConfirmPIN = apdu.cmd_apdu_data[5];
	  uint8_t *p = apdu.cmd_apdu_data;

	  if (INS (apdu) != INS_CHANGE_REFERENCE_DATA
	      && INS (apdu) != INS_RESET_RETRY_COUNTER
	      && INS (apdu) != INS_PUT_DATA)
	    {
	      GPG_CONDITION_NOT_SATISFIED ();
	      goto done;
	    }

	  if ((bConfirmPIN & 2))	/* Require old PIN */
	    {
	      pw_len = get_pinpad_input (PIN_INPUT_CURRENT);
	      if (pw_len < 0)
		{
		  GPG_ERROR ();
		  goto done;
		}
	      memcpy (p, pin_input_buffer, pw_len);
	      p += pw_len;
	    }
	  else
	    pw_len = 0;

	  newpw_len = get_pinpad_input (PIN_INPUT_NEW);
	  if (newpw_len < 0)
	    {
	      GPG_ERROR ();
	      goto done;
	    }
	  memcpy (p, pin_input_buffer, newpw_len);

	  if ((bConfirmPIN & 1))	/* New PIN twice */
	    {
	      len = get_pinpad_input (PIN_INPUT_CONFIRM);
	      if (len < 0)
		{
		  GPG_ERROR ();
		  goto done;
		}

	      if (len != newpw_len || memcmp (p, pin_input_buffer, len) != 0)
		{
		  GPG_SECURITY_FAILURE ();
		  goto done;
		}
	    }

	  apdu.cmd_apdu_data_len = pw_len + newpw_len;
#else
	  GPG_ERROR ();
	  goto done;
#endif
	}
      else if (m == EV_NOP)
	continue;

      led_blink (LED_START_COMMAND);
      process_command_apdu ();
      led_blink (LED_FINISH_COMMAND);
    done:
      chEvtSignal (icc_thread, EV_EXEC_FINISHED);
    }

  gpg_fini ();
  return 0;
}
