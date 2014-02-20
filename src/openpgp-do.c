/*
 * openpgp-do.c -- OpenPGP card Data Objects (DO) handling
 *
 * Copyright (C) 2010, 2011, 2012, 2013, 2014
 *               Free Software Initiative of Japan
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

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"

#include "sys.h"
#include "gnuk.h"
#include "openpgp.h"
#include "random.h"
#include "polarssl/config.h"
#include "polarssl/aes.h"

/* Handles possible unaligned access.  */
static uint32_t
fetch_four_bytes (const void *addr)
{
  const uint8_t *p = (const uint8_t *)addr;

  return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}


#define PASSWORD_ERRORS_MAX 3	/* >= errors, it will be locked */
static const uint8_t *pw_err_counter_p[3];

static int
gpg_pw_get_err_counter (uint8_t which)
{
  return flash_cnt123_get_value (pw_err_counter_p[which]);
}

int
gpg_pw_get_retry_counter (int who)
{
  if (who == 0x81 || who == 0x82)
    return PASSWORD_ERRORS_MAX - gpg_pw_get_err_counter (PW_ERR_PW1);
  else if (who == 0x83)
    return PASSWORD_ERRORS_MAX - gpg_pw_get_err_counter (PW_ERR_PW3);
  else
    return PASSWORD_ERRORS_MAX - gpg_pw_get_err_counter (PW_ERR_RC);
}

int
gpg_pw_locked (uint8_t which)
{
  if (gpg_pw_get_err_counter (which) >= PASSWORD_ERRORS_MAX)
    return 1;
  else
    return 0;
}

void
gpg_pw_reset_err_counter (uint8_t which)
{
  flash_cnt123_clear (&pw_err_counter_p[which]);
  if (pw_err_counter_p[which] != NULL)
    GPG_MEMORY_FAILURE ();
}

void
gpg_pw_increment_err_counter (uint8_t which)
{
  flash_cnt123_increment (which, &pw_err_counter_p[which]);
}


uint16_t data_objects_number_of_bytes;

/*
 * Compile time vars:
 *   Historical Bytes (template), Extended Capabilities,
 *   and Algorithm Attributes
 */

/* Historical Bytes (template) */
static const uint8_t historical_bytes[] __attribute__ ((aligned (1))) = {
  10,
  0x00,
  0x31, 0x84,			/* Full DF name, GET DATA, MF */
  0x73,
  0x80, 0x01, 0x80,		/* Full DF name */
				/* 1-byte */
				/* Command chaining, No extended Lc and Le */
  0x00, 0x90, 0x00		/* Status info (no life cycle management) */
};

/* Extended Capabilities */
static const uint8_t extended_capabilities[] __attribute__ ((aligned (1))) = {
  10,
  0x70,				/*
				 * No SM,
				 * GET CHALLENGE supported,
				 * Key import supported,
				 * PW status byte can be put,
				 * No private_use_DO,
				 * No algo change allowed
				 */
  0,		  /* Secure Messaging Algorithm: N/A (TDES=0, AES=1) */
  0x00, CHALLENGE_LEN, 		/* Max size of GET CHALLENGE */
#ifdef CERTDO_SUPPORT
  0x08, 0x00,	  /* max. length of cardholder certificate (2KiB) */
#else
  0x00, 0x00,
#endif
  /* Max. length of command APDU data */
  0x00, 0xff,
  /* Max. length of response APDU data */
  0x01, 0x00,
};

/* Algorithm Attributes */
static const uint8_t algorithm_attr_rsa[] __attribute__ ((aligned (1))) = {
  6,
  0x01, /* RSA */
  0x08, 0x00,	      /* Length modulus (in bit): 2048 */
  0x00, 0x20,	      /* Length exponent (in bit): 32  */
  0x00		      /* 0: p&q , 3: CRT with N (not yet supported) */
};

static const uint8_t algorithm_attr_p256r1[] __attribute__ ((aligned (1))) = {
  9,
  0x13, /* ECDSA */
  0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07 /* OID of NIST curve P-256 */
};

static const uint8_t algorithm_attr_p256k1[] __attribute__ ((aligned (1))) = {
  6,
  0x13, /* ECDSA */
  0x2b, 0x81, 0x04, 0x00, 0x0a /* OID of curve secp256k1 */
};

/*
 * Representation of PW1_LIFETIME:
 *    0: PW1_LIEFTIME_P == NULL : PW1 is valid for single PSO:CDS command
 *    1: PW1_LIEFTIME_P != NULL : PW1 is valid for several PSO:CDS commands
 *
 * The address in the variable PW1_LIEFTIME_P is used when filling zero
 * in flash memory
 */
static const uint8_t *pw1_lifetime_p;

static int
gpg_get_pw1_lifetime (void)
{
  if (pw1_lifetime_p == NULL)
    return 0;
  else
    return 1;
}


static uint32_t digital_signature_counter;

static const uint8_t *
gpg_write_digital_signature_counter (const uint8_t *p, uint32_t dsc)
{
  uint16_t hw0, hw1;

  if ((dsc >> 10) == 0)
    { /* no upper bits */
      hw1 = NR_COUNTER_DS_LSB | ((dsc & 0x0300) >> 8) | ((dsc & 0x00ff) << 8);
      flash_put_data_internal (p, hw1);
      return p+2;
    }
  else
    {
      hw0 = NR_COUNTER_DS | ((dsc & 0xfc0000) >> 18) | ((dsc & 0x03fc00) >> 2);
      hw1 = NR_COUNTER_DS_LSB;
      flash_put_data_internal (p, hw0);
      flash_put_data_internal (p+2, hw1);
      return p+4;
    }
}

static void
gpg_reset_digital_signature_counter (void)
{
  if (digital_signature_counter != 0)
    {
      flash_put_data (NR_COUNTER_DS);
      flash_put_data (NR_COUNTER_DS_LSB);
      digital_signature_counter = 0;
    }
}

void
gpg_increment_digital_signature_counter (void)
{
  uint16_t hw0, hw1;
  uint32_t dsc = (digital_signature_counter + 1) & 0x00ffffff;

  if ((dsc & 0x03ff) == 0)
    { /* carry occurs from l10 to h14 */
      hw0 = NR_COUNTER_DS | ((dsc & 0xfc0000) >> 18) | ((dsc & 0x03fc00) >> 2);
      hw1 = NR_COUNTER_DS_LSB;	/* zero */
      flash_put_data (hw0);
      flash_put_data (hw1);
    }
  else
    {
      hw1 = NR_COUNTER_DS_LSB | ((dsc & 0x0300) >> 8) | ((dsc & 0x00ff) << 8);
      flash_put_data (hw1);
    }

  digital_signature_counter = dsc;

  if (gpg_get_pw1_lifetime () == 0)
    ac_reset_pso_cds ();
}


#define SIZE_FINGER_PRINT 20
#define SIZE_KEYGEN_TIME 4	/* RFC4880 */

enum do_type {
  DO_FIXED,
  DO_VAR,
  DO_CMP_READ,
  DO_PROC_READ,
  DO_PROC_WRITE,
  DO_PROC_READWRITE,
};

struct do_table_entry {
  uint16_t tag;
  enum do_type do_type;
  uint8_t ac_read;
  uint8_t ac_write;
  const void *obj;
};

static uint8_t *res_p;

static void copy_do_1 (uint16_t tag, const uint8_t *do_data, int with_tag);
static const struct do_table_entry *get_do_entry (uint16_t tag);

#define GPG_DO_AID		0x004f
#define GPG_DO_NAME		0x005b
#define GPG_DO_LOGIN_DATA	0x005e
#define GPG_DO_CH_DATA		0x0065
#define GPG_DO_APP_DATA		0x006e
#define GPG_DO_DISCRETIONARY    0x0073
#define GPG_DO_SS_TEMP		0x007a
#define GPG_DO_DS_COUNT		0x0093
#define GPG_DO_EXTCAP		0x00c0
#define GPG_DO_ALG_SIG		0x00c1
#define GPG_DO_ALG_DEC		0x00c2
#define GPG_DO_ALG_AUT		0x00c3
#define GPG_DO_PW_STATUS	0x00c4
#define GPG_DO_FP_ALL		0x00c5
#define GPG_DO_CAFP_ALL		0x00c6
#define GPG_DO_FP_SIG		0x00c7
#define GPG_DO_FP_DEC		0x00c8
#define GPG_DO_FP_AUT		0x00c9
#define GPG_DO_CAFP_1		0x00ca
#define GPG_DO_CAFP_2		0x00cb
#define GPG_DO_CAFP_3		0x00cc
#define GPG_DO_KGTIME_ALL	0x00cd
#define GPG_DO_KGTIME_SIG	0x00ce
#define GPG_DO_KGTIME_DEC	0x00cf
#define GPG_DO_KGTIME_AUT	0x00d0
#define GPG_DO_RESETTING_CODE	0x00d3
#define GPG_DO_KEY_IMPORT	0x3fff
#define GPG_DO_LANGUAGE		0x5f2d
#define GPG_DO_SEX		0x5f35
#define GPG_DO_URL		0x5f50
#define GPG_DO_HIST_BYTES	0x5f52
#define GPG_DO_CH_CERTIFICATE	0x7f21

static const uint8_t *do_ptr[NR_DO__LAST__];

static uint8_t
do_tag_to_nr (uint16_t tag)
{
  switch (tag)
    {
    case GPG_DO_SEX:
      return NR_DO_SEX;
    case GPG_DO_FP_SIG:
      return NR_DO_FP_SIG;
    case GPG_DO_FP_DEC:
      return NR_DO_FP_DEC;
    case GPG_DO_FP_AUT:
      return NR_DO_FP_AUT;
    case GPG_DO_CAFP_1:
      return NR_DO_CAFP_1;
    case GPG_DO_CAFP_2:
      return NR_DO_CAFP_2;
    case GPG_DO_CAFP_3:
      return NR_DO_CAFP_3;
    case GPG_DO_KGTIME_SIG:
      return NR_DO_KGTIME_SIG;
    case GPG_DO_KGTIME_DEC:
      return NR_DO_KGTIME_DEC;
    case GPG_DO_KGTIME_AUT:
      return NR_DO_KGTIME_AUT;
    case GPG_DO_LOGIN_DATA:
      return NR_DO_LOGIN_DATA;
    case GPG_DO_URL:
      return NR_DO_URL;
    case GPG_DO_NAME:
      return NR_DO_NAME;
    case GPG_DO_LANGUAGE:
      return NR_DO_LANGUAGE;
    default:
      return NR_NONE;
    }
}

static void
copy_tag (uint16_t tag)
{
  if (tag < 0x0100)
    *res_p++ = (tag & 0xff);
  else
    {
      *res_p++ = (tag >> 8);
      *res_p++ = (tag & 0xff);
    }
}

static int
do_hist_bytes (uint16_t tag, int with_tag)
{
  /* XXX: For now, no life cycle management, just return template as is. */
  /* XXX: Supporing TERMINATE DF / ACTIVATE FILE, we need to fix here */
  copy_do_1 (tag, historical_bytes, with_tag);
  return 1;
}

#define SIZE_FP 20
#define SIZE_KGTIME 4

static int
do_fp_all (uint16_t tag, int with_tag)
{
  const uint8_t *data;

  if (with_tag)
    {
      copy_tag (tag);
      *res_p++ = SIZE_FP*3;
    }

  data = gpg_do_read_simple (NR_DO_FP_SIG);
  if (data)
    memcpy (res_p, data, SIZE_FP);
  else
    memset (res_p, 0, SIZE_FP);
  res_p += SIZE_FP;

  data = gpg_do_read_simple (NR_DO_FP_DEC);
  if (data)
    memcpy (res_p, data, SIZE_FP);
  else
    memset (res_p, 0, SIZE_FP);
  res_p += SIZE_FP;

  data = gpg_do_read_simple (NR_DO_FP_AUT);
  if (data)
    memcpy (res_p, data, SIZE_FP);
  else
    memset (res_p, 0, SIZE_FP);
  res_p += SIZE_FP;
  return 1;
}

static int
do_cafp_all (uint16_t tag, int with_tag)
{
  const uint8_t *data;

  if (with_tag)
    {
      copy_tag (tag);
      *res_p++ = SIZE_FP*3;
    }

  data = gpg_do_read_simple (NR_DO_CAFP_1);
  if (data)
    memcpy (res_p, data, SIZE_FP);
  else
    memset (res_p, 0, SIZE_FP);
  res_p += SIZE_FP;

  data = gpg_do_read_simple (NR_DO_CAFP_2);
  if (data)
    memcpy (res_p, data, SIZE_FP);
  else
    memset (res_p, 0, SIZE_FP);
  res_p += SIZE_FP;

  data = gpg_do_read_simple (NR_DO_CAFP_2);
  if (data)
    memcpy (res_p, data, SIZE_FP);
  else
    memset (res_p, 0, SIZE_FP);
  res_p += SIZE_FP;
  return 1;
}

static int
do_kgtime_all (uint16_t tag, int with_tag)
{
  const uint8_t *data;

  if (with_tag)
    {
      copy_tag (tag);
      *res_p++ = SIZE_KGTIME*3;
    }

  data = gpg_do_read_simple (NR_DO_KGTIME_SIG);
  if (data)
    memcpy (res_p, data, SIZE_KGTIME);
  else
    memset (res_p, 0, SIZE_KGTIME);
  res_p += SIZE_KGTIME;

  data = gpg_do_read_simple (NR_DO_KGTIME_DEC);
  if (data)
    memcpy (res_p, data, SIZE_KGTIME);
  else
    memset (res_p, 0, SIZE_KGTIME);
  res_p += SIZE_KGTIME;

  data = gpg_do_read_simple (NR_DO_KGTIME_AUT);
  if (data)
    memcpy (res_p, data, SIZE_KGTIME);
  else
    memset (res_p, 0, SIZE_KGTIME);
  res_p += SIZE_KGTIME;
  return 1;
}

const uint8_t openpgpcard_aid[] = {
  0xd2, 0x76,		    /* D: National, 276: DEU ISO 3166-1 */
  0x00, 0x01, 0x24,	    /* Registered Application Provider Identifier */
  0x01,			    /* Application: OpenPGPcard */
  0x02, 0x00,		    /* Version 2.0 */
  /* v. id */ /*   serial number   */
  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, /* To be overwritten */
};

static int
do_openpgpcard_aid (uint16_t tag, int with_tag)
{
  const volatile uint8_t *p = openpgpcard_aid;
  uint16_t vid = (p[8] << 8) | p[9];

  if (with_tag)
    {
      copy_tag (tag);
      *res_p++ = 16;
    }

  if (vid == 0xffff || vid == 0x0000)
    {
      const uint8_t *u = unique_device_id ();

      memcpy (res_p, openpgpcard_aid, 8);
      res_p += 8;

      /* vid == 0xfffe: serial number is random byte */
      *res_p++ = 0xff;
      *res_p++ = 0xfe;
      memcpy (res_p, u, 4);
      res_p += 4;
    }
  else
    {
      memcpy (res_p, openpgpcard_aid, 14);
      res_p += 14;
    }

  *res_p++ = 0;
  *res_p++ = 0;

  return 1;
}

static int
do_ds_count (uint16_t tag, int with_tag)
{
  if (with_tag)
    {
      copy_tag (tag);
      *res_p++ = 3;
    }

  *res_p++ = (digital_signature_counter >> 16) & 0xff;
  *res_p++ = (digital_signature_counter >> 8) & 0xff;
  *res_p++ = digital_signature_counter & 0xff;
  return 1;
}

static int
rw_pw_status (uint16_t tag, int with_tag,
	      const uint8_t *data, int len, int is_write)
{
  if (is_write)
    {
      (void)len;		/* Should be SIZE_PW_STATUS_BYTES */

      /* Only the first byte of DATA is checked */
      if (data[0] == 0)
	{
	  flash_bool_clear (&pw1_lifetime_p);
	  if (pw1_lifetime_p == NULL)
	    return 1;
	  else
	    return 0;
	}
      else
	{
	  pw1_lifetime_p = flash_bool_write (NR_BOOL_PW1_LIFETIME);
	  if (pw1_lifetime_p != NULL)
	    return 1;
	  else
	    return 0;
	}
    }
  else
    {
      if (with_tag)
	{
	  copy_tag (tag);
	  *res_p++ = SIZE_PW_STATUS_BYTES;
	}

      *res_p++ = gpg_get_pw1_lifetime ();
      *res_p++ = PW_LEN_MAX;
      *res_p++ = PW_LEN_MAX;
      *res_p++ = PW_LEN_MAX;
      *res_p++ = PASSWORD_ERRORS_MAX - gpg_pw_get_err_counter (PW_ERR_PW1);
      *res_p++ = PASSWORD_ERRORS_MAX - gpg_pw_get_err_counter (PW_ERR_RC);
      *res_p++ = PASSWORD_ERRORS_MAX - gpg_pw_get_err_counter (PW_ERR_PW3);
      return 1;
    }
}

static int
proc_resetting_code (const uint8_t *data, int len)
{
  const uint8_t *old_ks = keystring_md_pw3;
  uint8_t new_ks0[KEYSTRING_SIZE];
  uint8_t *new_ks = KS_GET_KEYSTRING (new_ks0);
  const uint8_t *newpw;
  int newpw_len;
  int r;
  uint8_t *salt = KS_GET_SALT (new_ks0);

  DEBUG_INFO ("Resetting Code!\r\n");

  newpw_len = len;
  newpw = data;
  new_ks0[0] = newpw_len;
  random_get_salt (salt);
  s2k (salt, SALT_SIZE, newpw, newpw_len, new_ks);
  r = gpg_change_keystring (admin_authorized, old_ks, BY_RESETCODE, new_ks);
  if (r <= -2)
    {
      DEBUG_INFO ("memory error.\r\n");
      return 0;
    }
  else if (r < 0)
    {
      DEBUG_INFO ("security error.\r\n");
      return 0;
    }
  else if (r == 0)
    {
      DEBUG_INFO ("error (no prvkey).\r\n");
      return 0;
    }
  else
    {
      DEBUG_INFO ("done.\r\n");
      gpg_do_write_simple (NR_DO_KEYSTRING_RC, new_ks0, KS_META_SIZE);
    }

  gpg_pw_reset_err_counter (PW_ERR_RC);
  return 1;
}

static void
encrypt (const uint8_t *key, const uint8_t *iv, uint8_t *data, int len)
{
  aes_context aes;
  uint8_t iv0[INITIAL_VECTOR_SIZE];
  unsigned int iv_offset;

  DEBUG_INFO ("ENC\r\n");
  DEBUG_BINARY (data, len);

  aes_setkey_enc (&aes, key, 128);
  memcpy (iv0, iv, INITIAL_VECTOR_SIZE);
  iv_offset = 0;
  aes_crypt_cfb128 (&aes, AES_ENCRYPT, len, &iv_offset, iv0, data, data);
}

/* Signing, Decryption, and Authentication */
struct key_data kd[3];

static void
decrypt (const uint8_t *key, const uint8_t *iv, uint8_t *data, int len)
{
  aes_context aes;
  uint8_t iv0[INITIAL_VECTOR_SIZE];
  unsigned int iv_offset;

  aes_setkey_enc (&aes, key, 128); /* This is setkey_enc, because of CFB.  */
  memcpy (iv0, iv, INITIAL_VECTOR_SIZE);
  iv_offset = 0;
  aes_crypt_cfb128 (&aes, AES_DECRYPT, len, &iv_offset, iv0, data, data);

  DEBUG_INFO ("DEC\r\n");
  DEBUG_BINARY (data, len);
}

static void
encrypt_dek (const uint8_t *key_string, uint8_t *dek)
{
  aes_context aes;

  aes_setkey_enc (&aes, key_string, 128);
  aes_crypt_ecb (&aes, AES_ENCRYPT, dek, dek);
}

static void
decrypt_dek (const uint8_t *key_string, uint8_t *dek)
{
  aes_context aes;

  aes_setkey_dec (&aes, key_string, 128);
  aes_crypt_ecb (&aes, AES_DECRYPT, dek, dek);
}

static uint8_t
get_do_ptr_nr_for_kk (enum kind_of_key kk)
{
  switch (kk)
    {
    case GPG_KEY_FOR_SIGNING:
      return NR_DO_PRVKEY_SIG;
    case GPG_KEY_FOR_DECRYPTION:
      return NR_DO_PRVKEY_DEC;
    case GPG_KEY_FOR_AUTHENTICATION:
      return NR_DO_PRVKEY_AUT;
    }
  return NR_DO_PRVKEY_SIG;
}

void
gpg_do_clear_prvkey (enum kind_of_key kk)
{
  memset ((void *)&kd[kk], 0, sizeof (struct key_data));
}


static int
compute_key_data_checksum (struct key_data_internal *kdi, int check_or_calc)
{
  unsigned int i;
  uint32_t d[4] = { 0, 0, 0, 0 };

  for (i = 0; i < KEY_CONTENT_LEN / sizeof (uint32_t); i++)
    d[i&3] ^= kdi->data[i];

  if (check_or_calc == 0)	/* store */
    {
      memcpy (kdi->checksum, d, DATA_ENCRYPTION_KEY_SIZE);
      return 0;
    }
  else				/* check */
    return memcmp (kdi->checksum, d, DATA_ENCRYPTION_KEY_SIZE) == 0;
}

/*
 * Return  1 on success,
 *         0 if none,
 *        -1 on error,
 */
int
gpg_do_load_prvkey (enum kind_of_key kk, int who, const uint8_t *keystring)
{
  uint8_t nr = get_do_ptr_nr_for_kk (kk);
  const uint8_t *do_data = do_ptr[nr - NR_DO__FIRST__];
  const uint8_t *key_addr;
  uint8_t dek[DATA_ENCRYPTION_KEY_SIZE];
  const uint8_t *iv;
  struct key_data_internal kdi;

  DEBUG_INFO ("Loading private key: ");
  DEBUG_BYTE (kk);

  if (do_data == NULL)
    return 0;

  key_addr = (const uint8_t *)fetch_four_bytes (&do_data[1]);
  memcpy (kdi.data, key_addr, KEY_CONTENT_LEN);
  iv = do_data+5;
  memcpy (kdi.checksum, iv + INITIAL_VECTOR_SIZE, DATA_ENCRYPTION_KEY_SIZE);

  memcpy (dek, do_data+5+16*(who+1), DATA_ENCRYPTION_KEY_SIZE);
  decrypt_dek (keystring, dek);

  decrypt (dek, iv, (uint8_t *)&kdi, sizeof (struct key_data_internal));
  memset (dek, 0, DATA_ENCRYPTION_KEY_SIZE);
  if (!compute_key_data_checksum (&kdi, 1))
    {
      DEBUG_INFO ("gpg_do_load_prvkey failed.\r\n");
      return -1;
    }

  memcpy (kd[kk].data, kdi.data, KEY_CONTENT_LEN);
  DEBUG_BINARY (&kd[kk], KEY_CONTENT_LEN);
  return 1;
}


static int8_t num_prv_keys;

static void
gpg_do_delete_prvkey (enum kind_of_key kk)
{
  uint8_t nr = get_do_ptr_nr_for_kk (kk);
  const uint8_t *do_data = do_ptr[nr - NR_DO__FIRST__];
  uint8_t *key_addr;

  if (do_data == NULL)
    return;

  key_addr = (uint8_t *)fetch_four_bytes (&do_data[1]);
  flash_key_release (key_addr);
  do_ptr[nr - NR_DO__FIRST__] = NULL;
  flash_do_release (do_data);

  if (admin_authorized == BY_ADMIN && kk == GPG_KEY_FOR_SIGNING)
    {			/* Recover admin keystring DO.  */
      const uint8_t *ks_pw3 = gpg_do_read_simple (NR_DO_KEYSTRING_PW3);

      if (ks_pw3 != NULL)
	{
	  uint8_t ks0[KEYSTRING_SIZE];

	  ks0[0] = ks_pw3[0] | PW_LEN_KEYSTRING_BIT;
	  memcpy (KS_GET_SALT (ks0), KS_GET_SALT (ks_pw3), SALT_SIZE);
	  memcpy (KS_GET_KEYSTRING (ks0), keystring_md_pw3, KEYSTRING_MD_SIZE);
	  gpg_do_write_simple (NR_DO_KEYSTRING_PW3, ks0, KEYSTRING_SIZE);
	}
    }

  if (--num_prv_keys == 0)
    {
      /* Delete PW1 and RC if any.  */
      gpg_do_write_simple (NR_DO_KEYSTRING_PW1, NULL, 0);
      gpg_do_write_simple (NR_DO_KEYSTRING_RC, NULL, 0);

      ac_reset_pso_cds ();
      ac_reset_other ();
      if (admin_authorized == BY_USER)
	ac_reset_admin ();
    }
}

static int
gpg_do_write_prvkey (enum kind_of_key kk, const uint8_t *key_data, int key_len,
		     const uint8_t *keystring_admin, const uint8_t *pubkey)
{
  uint8_t nr = get_do_ptr_nr_for_kk (kk);
  const uint8_t *p;
  int r;
  struct prvkey_data *pd;
  uint8_t *key_addr;
  const uint8_t *dek, *iv;
  struct key_data_internal kdi;
  uint8_t *pubkey_allocated_here = NULL;
  int pubkey_len = KEY_CONTENT_LEN;
  uint8_t ks[KEYSTRING_MD_SIZE];
  enum kind_of_key kk0;

  DEBUG_INFO ("Key import\r\n");
  DEBUG_SHORT (key_len);

  /* Delete it first, if any.  */
  gpg_do_delete_prvkey (kk);

#if defined(RSA_AUTH) && defined(RSA_SIG)
  if (key_len != KEY_CONTENT_LEN)
     return -1;
#elif defined(RSA_AUTH) && !defined(RSA_SIG)
  /* ECDSA with p256k1 for signature */
  if (kk != GPG_KEY_FOR_SIGNING && key_len != KEY_CONTENT_LEN)
    return -1;
  if (kk == GPG_KEY_FOR_SIGNING)
    {
      pubkey_len = key_len * 2;
      if (key_len != 32)
	return -1;
    }
#elif !defined(RSA_AUTH) && defined(RSA_SIG)
  /* ECDSA with p256r1 for authentication */
  if (kk != GPG_KEY_FOR_AUTHENTICATION && key_len != KEY_CONTENT_LEN)
    return -1;
  if (kk == GPG_KEY_FOR_AUTHENTICATION)
    {
      pubkey_len = key_len * 2;
      if (key_len != 32)
	return -1;
    }
#else
#error "not supported."
#endif

  pd = (struct prvkey_data *)malloc (sizeof (struct prvkey_data));
  if (pd == NULL)
    return -1;

  if (pubkey == NULL)
    {
#if defined(RSA_AUTH) && defined(RSA_SIG)
      pubkey_allocated_here = modulus_calc (key_data, key_len);
#elif defined(RSA_AUTH) && !defined(RSA_SIG)
      /* ECDSA with p256k1 for signature */
      if (kk == GPG_KEY_FOR_SIGNING)
	pubkey_allocated_here = ecdsa_compute_public_p256k1 (key_data);
      else
	pubkey_allocated_here = modulus_calc (key_data, key_len);
#elif !defined(RSA_AUTH) && defined(RSA_SIG)
      /* ECDSA with p256r1 for authentication */
      if (kk == GPG_KEY_FOR_AUTHENTICATION)
	pubkey_allocated_here = ecdsa_compute_public_p256r1 (key_data);
      else
	pubkey_allocated_here = modulus_calc (key_data, key_len);
#else
#error "not supported."
#endif
      if (pubkey_allocated_here == NULL)
	{
	  free (pd);
	  return -1;
	}
    }

  DEBUG_INFO ("Getting keystore address...\r\n");
  key_addr = flash_key_alloc ();
  if (key_addr == NULL)
    {
      if (pubkey_allocated_here)
	{
	  memset (pubkey_allocated_here, 0, pubkey_len);
	  free (pubkey_allocated_here);
	}
      free (pd);
      return -1;
    }

  num_prv_keys++;

  DEBUG_INFO ("key_addr: ");
  DEBUG_WORD ((uint32_t)key_addr);

#if defined(RSA_AUTH) && defined(RSA_SIG)
  memcpy (kdi.data, key_data, KEY_CONTENT_LEN);
#elif defined(RSA_AUTH) && !defined(RSA_SIG)
  /* ECDSA with p256k1 for signature */
  if (kk == GPG_KEY_FOR_SIGNING)
    {
      memcpy (kdi.data, key_data, key_len);
      memset ((uint8_t *)kdi.data + key_len, 0, KEY_CONTENT_LEN - key_len);
    }
  else
    memcpy (kdi.data, key_data, KEY_CONTENT_LEN);
#elif !defined(RSA_AUTH) && defined(RSA_SIG)
 /* ECDSA with p256r1 for authentication */
  if (kk == GPG_KEY_FOR_AUTHENTICATION)
    {
      memcpy (kdi.data, key_data, key_len);
      memset ((uint8_t *)kdi.data + key_len, 0, KEY_CONTENT_LEN - key_len);
    }
  else
    memcpy (kdi.data, key_data, KEY_CONTENT_LEN);
#else
#error "not supported."
#endif
  compute_key_data_checksum (&kdi, 0);

  dek = random_bytes_get (); /* 32-byte random bytes */
  iv = dek + DATA_ENCRYPTION_KEY_SIZE;
  memcpy (pd->dek_encrypted_1, dek, DATA_ENCRYPTION_KEY_SIZE);
  memcpy (pd->dek_encrypted_2, dek, DATA_ENCRYPTION_KEY_SIZE);
  memcpy (pd->dek_encrypted_3, dek, DATA_ENCRYPTION_KEY_SIZE);

  s2k (NULL, 0, (const uint8_t *)OPENPGP_CARD_INITIAL_PW1,
       strlen (OPENPGP_CARD_INITIAL_PW1), ks);

  /* Handle existing keys and keystring DOs.  */
  gpg_do_write_simple (NR_DO_KEYSTRING_PW1, NULL, 0);
  gpg_do_write_simple (NR_DO_KEYSTRING_RC, NULL, 0);
  for (kk0 = 0; kk0 <= GPG_KEY_FOR_AUTHENTICATION; kk0++)
    if (kk0 != kk)
      {
	gpg_do_chks_prvkey (kk0, admin_authorized, keystring_md_pw3,
			    BY_USER, ks);
	gpg_do_chks_prvkey (kk0, BY_RESETCODE, NULL, 0, NULL);
      }

  encrypt (dek, iv, (uint8_t *)&kdi, sizeof (struct key_data_internal));

  r = flash_key_write (key_addr, (const uint8_t *)kdi.data,
		       pubkey_allocated_here? pubkey_allocated_here: pubkey,
		       pubkey_len);
  if (pubkey_allocated_here)
    {
      memset (pubkey_allocated_here, 0, pubkey_len);
      free (pubkey_allocated_here);
    }

  if (r < 0)
    {
      random_bytes_free (dek);
      memset (pd, 0, sizeof (struct prvkey_data));
      free (pd);
      return r;
    }

  pd->key_addr = key_addr;
  memcpy (pd->iv, iv, INITIAL_VECTOR_SIZE);
  memcpy (pd->checksum_encrypted, kdi.checksum, DATA_ENCRYPTION_KEY_SIZE);

  encrypt_dek (ks, pd->dek_encrypted_1);

  memset (pd->dek_encrypted_2, 0, DATA_ENCRYPTION_KEY_SIZE);

  if (keystring_admin)
    encrypt_dek (keystring_admin, pd->dek_encrypted_3);
  else
    memset (pd->dek_encrypted_3, 0, DATA_ENCRYPTION_KEY_SIZE);

  p = flash_do_write (nr, (const uint8_t *)pd, sizeof (struct prvkey_data));
  do_ptr[nr - NR_DO__FIRST__] = p;

  random_bytes_free (dek);
  memset (pd, 0, sizeof (struct prvkey_data));
  free (pd);
  if (p == NULL)
    return -1;

  if (keystring_admin && kk == GPG_KEY_FOR_SIGNING)
    {
      const uint8_t *ks_admin = gpg_do_read_simple (NR_DO_KEYSTRING_PW3);
      uint8_t ks_info[KS_META_SIZE];

      if (ks_admin != NULL && (ks_admin[0] & PW_LEN_KEYSTRING_BIT))
	{
	  ks_info[0] = ks_admin[0] & PW_LEN_MASK;
	  memcpy (KS_GET_SALT (ks_info), KS_GET_SALT (ks_admin), SALT_SIZE);
	  gpg_do_write_simple (NR_DO_KEYSTRING_PW3, ks_info, KS_META_SIZE);
	}
      else
	{
	  DEBUG_INFO ("No admin keystring!\r\n");
	}
    }

  return 0;
}

int
gpg_do_chks_prvkey (enum kind_of_key kk,
		    int who_old, const uint8_t *old_ks,
		    int who_new, const uint8_t *new_ks)
{
  uint8_t nr = get_do_ptr_nr_for_kk (kk);
  const uint8_t *do_data = do_ptr[nr - NR_DO__FIRST__];
  uint8_t dek[DATA_ENCRYPTION_KEY_SIZE];
  struct prvkey_data *pd;
  const uint8_t *p;
  uint8_t *dek_p;
  int update_needed = 0;

  if (do_data == NULL)
    return 0;			/* No private key */

  pd = (struct prvkey_data *)malloc (sizeof (struct prvkey_data));
  if (pd == NULL)
    return -1;

  memcpy (pd, &do_data[1], sizeof (struct prvkey_data));

  dek_p = ((uint8_t *)pd) + 4 + INITIAL_VECTOR_SIZE
    + DATA_ENCRYPTION_KEY_SIZE * who_old;
  memcpy (dek, dek_p, DATA_ENCRYPTION_KEY_SIZE);
  if (who_new == 0)		/* Remove */
    {
      int i;

      for (i = 0; i < DATA_ENCRYPTION_KEY_SIZE; i++)
	if (dek_p[i] != 0)
	  {
	    update_needed = 1;
	    dek_p[i] = 0;
	  }
    }
  else
    {
      decrypt_dek (old_ks, dek);
      encrypt_dek (new_ks, dek);
      dek_p += DATA_ENCRYPTION_KEY_SIZE * (who_new - who_old);
      if (memcmp (dek_p, dek, DATA_ENCRYPTION_KEY_SIZE) != 0)
	{
	  memcpy (dek_p, dek, DATA_ENCRYPTION_KEY_SIZE);
	  update_needed = 1;
	}
    }

  if (update_needed)
    {
      flash_do_release (do_data);
      do_ptr[nr - NR_DO__FIRST__] = NULL;
      p = flash_do_write (nr, (const uint8_t *)pd, sizeof (struct prvkey_data));
      do_ptr[nr - NR_DO__FIRST__] = p;
    }

  memset (pd, 0, sizeof (struct prvkey_data));
  free (pd);
  if (update_needed && p == NULL)
    return -1;

  return 1;
}

/*
 * RSA:
 * 4d, xx, xx, xx:    Extended Header List
 *   b6 00 (SIG) / b8 00 (DEC) / a4 00 (AUT)
 *   7f48, xx: cardholder private key template
 *       91 xx: length of E
 *       92 xx xx: length of P
 *       93 xx xx: length of Q
 *   5f48, xx xx xx: cardholder private key
 * <E: 4-byte>, <P: 128-byte>, <Q: 128-byte>
 *
 * ECDSA:
 * 4d, xx:    Extended Header List
 *   a4 00 (AUT)
 *   7f48, xx: cardholder private key template
 *       91 xx: length of d
 *   5f48, xx : cardholder private key
 * <d>
 */
static int
proc_key_import (const uint8_t *data, int len)
{
  int r;
  enum kind_of_key kk;
  const uint8_t *keystring_admin;
  const uint8_t *p = data;

  if (admin_authorized == BY_ADMIN)
    keystring_admin = keystring_md_pw3;
  else
    keystring_admin = NULL;

  DEBUG_BINARY (data, len);

  if (*p++ != 0x4d)
    return 0;

  /* length field */
  if (*p == 0x82)
    p += 3;
  else if (*p == 0x81)
    p += 2;
  else
    p += 1;

  if (*p == 0xb6)
    {
      kk = GPG_KEY_FOR_SIGNING;
      ac_reset_pso_cds ();
      gpg_reset_digital_signature_counter ();
    }
  else
    {
      if (*p == 0xb8)
	kk = GPG_KEY_FOR_DECRYPTION;
      else				/* 0xa4 */
	kk = GPG_KEY_FOR_AUTHENTICATION;
      ac_reset_other ();
    }

#if defined(RSA_AUTH) && defined(RSA_SIG)
  if (len <= 22)
#elif defined(RSA_AUTH) && !defined(RSA_SIG)
  /* ECDSA with p256k1 for signature */
  if ((kk != GPG_KEY_FOR_SIGNING && len <= 22)
      || (kk == GPG_KEY_FOR_SIGNING && len <= 12))
#elif !defined(RSA_AUTH) && defined(RSA_SIG)
  /* ECDSA with p256r1 for authentication */
  if ((kk != GPG_KEY_FOR_AUTHENTICATION && len <= 22)
      || (kk == GPG_KEY_FOR_AUTHENTICATION && len <= 12))
#else
#error "not supported."
#endif
    {					    /* Deletion of the key */
      gpg_do_delete_prvkey (kk);
      return 1;
    }

#if defined(RSA_AUTH) && defined(RSA_SIG)
  r = gpg_do_write_prvkey (kk, &data[26], len - 26, keystring_admin, NULL);
#elif defined(RSA_AUTH) && !defined(RSA_SIG)
  /* ECDSA with p256k1 for signature */
  if (kk != GPG_KEY_FOR_SIGNING)
    {			   /* RSA */
      /* It should starts with 00 01 00 01 (E) */
      /* Skip E, 4-byte */
      r = gpg_do_write_prvkey (kk, &data[26], len - 26, keystring_admin, NULL);
    }
  else
    r = gpg_do_write_prvkey (kk, &data[12], len - 12, keystring_admin, NULL);
#elif !defined(RSA_AUTH) && defined(RSA_SIG)
  /* ECDSA with p256r1 for authentication */
  if (kk != GPG_KEY_FOR_AUTHENTICATION)
    {			   /* RSA */
      /* It should starts with 00 01 00 01 (E) */
      /* Skip E, 4-byte */
      r = gpg_do_write_prvkey (kk, &data[26], len - 26, keystring_admin, NULL);
    }
  else
    r = gpg_do_write_prvkey (kk, &data[12], len - 12, keystring_admin, NULL);
#else
#error "not supported."
#endif

  if (r < 0)
    return 0;
  else
    return 1;
}

static const uint16_t cmp_ch_data[] = {
  3,
  GPG_DO_NAME,
  GPG_DO_LANGUAGE,
  GPG_DO_SEX,
};

static const uint16_t cmp_app_data[] = {
  3,
  GPG_DO_AID,
  GPG_DO_HIST_BYTES,
  GPG_DO_DISCRETIONARY,
};

static const uint16_t cmp_discretionary[] = {
  8,
  GPG_DO_EXTCAP,
  GPG_DO_ALG_SIG, GPG_DO_ALG_DEC, GPG_DO_ALG_AUT,
  GPG_DO_PW_STATUS,
  GPG_DO_FP_ALL, GPG_DO_CAFP_ALL, GPG_DO_KGTIME_ALL
};

static const uint16_t cmp_ss_temp[] = { 1, GPG_DO_DS_COUNT };

static const struct do_table_entry
gpg_do_table[] = {
  /* Variables: Fixed size */
  { GPG_DO_SEX, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, &do_ptr[0] },
  { GPG_DO_FP_SIG, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, &do_ptr[1] },
  { GPG_DO_FP_DEC, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, &do_ptr[2] },
  { GPG_DO_FP_AUT, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, &do_ptr[3] },
  { GPG_DO_CAFP_1, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, &do_ptr[4] },
  { GPG_DO_CAFP_2, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, &do_ptr[5] },
  { GPG_DO_CAFP_3, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, &do_ptr[6] },
  { GPG_DO_KGTIME_SIG, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, &do_ptr[7] },
  { GPG_DO_KGTIME_DEC, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, &do_ptr[8] },
  { GPG_DO_KGTIME_AUT, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, &do_ptr[9] },
  /* Variables: Variable size */
  { GPG_DO_LOGIN_DATA, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, &do_ptr[10] },
  { GPG_DO_URL, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, &do_ptr[11] },
  { GPG_DO_NAME, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, &do_ptr[12] },
  { GPG_DO_LANGUAGE, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, &do_ptr[13] },
  /* Pseudo DO READ: calculated */
  { GPG_DO_HIST_BYTES, DO_PROC_READ, AC_ALWAYS, AC_NEVER, do_hist_bytes },
  { GPG_DO_FP_ALL, DO_PROC_READ, AC_ALWAYS, AC_NEVER, do_fp_all },
  { GPG_DO_CAFP_ALL, DO_PROC_READ, AC_ALWAYS, AC_NEVER, do_cafp_all },
  { GPG_DO_KGTIME_ALL, DO_PROC_READ, AC_ALWAYS, AC_NEVER, do_kgtime_all },
  /* Pseudo DO READ: calculated, not changeable by user */
  { GPG_DO_DS_COUNT, DO_PROC_READ, AC_ALWAYS, AC_NEVER, do_ds_count },
  { GPG_DO_AID, DO_PROC_READ, AC_ALWAYS, AC_NEVER, do_openpgpcard_aid },
  /* Pseudo DO READ/WRITE: calculated */
  { GPG_DO_PW_STATUS, DO_PROC_READWRITE, AC_ALWAYS, AC_ADMIN_AUTHORIZED,
    rw_pw_status },
  /* Fixed data */
  { GPG_DO_EXTCAP, DO_FIXED, AC_ALWAYS, AC_NEVER, extended_capabilities },
#ifdef RSA_SIG
  { GPG_DO_ALG_SIG, DO_FIXED, AC_ALWAYS, AC_NEVER, algorithm_attr_rsa },
#else
  { GPG_DO_ALG_SIG, DO_FIXED, AC_ALWAYS, AC_NEVER, algorithm_attr_p256k1 },
#endif
  { GPG_DO_ALG_DEC, DO_FIXED, AC_ALWAYS, AC_NEVER, algorithm_attr_rsa },
#ifdef RSA_AUTH
  { GPG_DO_ALG_AUT, DO_FIXED, AC_ALWAYS, AC_NEVER, algorithm_attr_rsa },
#else
  { GPG_DO_ALG_AUT, DO_FIXED, AC_ALWAYS, AC_NEVER, algorithm_attr_p256r1 },
#endif
  /* Compound data: Read access only */
  { GPG_DO_CH_DATA, DO_CMP_READ, AC_ALWAYS, AC_NEVER, cmp_ch_data },
  { GPG_DO_APP_DATA, DO_CMP_READ, AC_ALWAYS, AC_NEVER, cmp_app_data },
  { GPG_DO_DISCRETIONARY, DO_CMP_READ, AC_ALWAYS, AC_NEVER, cmp_discretionary },
  { GPG_DO_SS_TEMP, DO_CMP_READ, AC_ALWAYS, AC_NEVER, cmp_ss_temp },
  /* Simple data: write access only */
  { GPG_DO_RESETTING_CODE, DO_PROC_WRITE, AC_NEVER, AC_ADMIN_AUTHORIZED,
    proc_resetting_code },
  /* Compound data: Write access only */
  { GPG_DO_KEY_IMPORT, DO_PROC_WRITE, AC_NEVER, AC_ADMIN_AUTHORIZED,
    proc_key_import },
#if 0
  /* Card holder certificate is handled in special way, as its size is big */
  { GPG_DO_CH_CERTIFICATE, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, NULL },
#endif
};

#define NUM_DO_ENTRIES (int)(sizeof (gpg_do_table) \
			     / sizeof (struct do_table_entry))

/*
 * Reading data from Flash ROM, initialize DO_PTR, PW_ERR_COUNTERS, etc.
 */
void
gpg_data_scan (const uint8_t *p_start)
{
  const uint8_t *p;
  int i;
  const uint8_t *dsc_h14_p, *dsc_l10_p;
  int dsc_h14, dsc_l10;

  dsc_h14_p = dsc_l10_p = NULL;
  pw1_lifetime_p = NULL;
  pw_err_counter_p[PW_ERR_PW1] = NULL;
  pw_err_counter_p[PW_ERR_RC] = NULL;
  pw_err_counter_p[PW_ERR_PW3] = NULL;

  /* Traverse DO, counters, etc. in DATA pool */
  p = p_start;
  while (*p != NR_EMPTY)
    {
      uint8_t nr = *p++;
      uint8_t second_byte = *p;

      if (nr == 0x00 && second_byte == 0x00)
	p++;			/* Skip released word */
      else
	{
	  if (nr < 0x80)
	    {
	      /* It's Data Object */
	      do_ptr[nr - NR_DO__FIRST__] = p;
	      p += second_byte + 1; /* second_byte has length */

	      if (((uint32_t)p & 1))
		p++;
	    }
	  else if (nr >= 0x80 && nr <= 0xbf)
	    /* Encoded data of Digital Signature Counter: upper 14-bit */
	    {
	      dsc_h14_p = p - 1;
	      p++;
	    }
	  else if (nr >= 0xc0 && nr <= 0xc3)
	    /* Encoded data of Digital Signature Counter: lower 10-bit */
	    {
	      dsc_l10_p = p - 1;
	      p++;
	    }
	  else
	    switch (nr)
	      {
	      case NR_BOOL_PW1_LIFETIME:
		pw1_lifetime_p = p - 1;
		p++;
		continue;
	      case NR_COUNTER_123:
		p++;
		if (second_byte <= PW_ERR_PW3)
		  pw_err_counter_p[second_byte] = p;
		p += 2;
		break;
	      }
	}
    }

  flash_set_data_pool_last (p);

  num_prv_keys = 0;
  if (do_ptr[NR_DO_PRVKEY_SIG - NR_DO__FIRST__] != NULL)
    num_prv_keys++;
  if (do_ptr[NR_DO_PRVKEY_DEC - NR_DO__FIRST__] != NULL)
    num_prv_keys++;
  if (do_ptr[NR_DO_PRVKEY_AUT - NR_DO__FIRST__] != NULL)
    num_prv_keys++;

  data_objects_number_of_bytes = 0;
  for (i = NR_DO__FIRST__; i < NR_DO__LAST__; i++)
    if (do_ptr[i - NR_DO__FIRST__] != NULL)
      data_objects_number_of_bytes += *do_ptr[i - NR_DO__FIRST__];

  if (dsc_l10_p == NULL)
    dsc_l10 = 0;
  else
    dsc_l10 = ((*dsc_l10_p - 0xc0) << 8) | *(dsc_l10_p + 1);

  if (dsc_h14_p == NULL)
    dsc_h14 = 0;
  else
    {
      dsc_h14 = ((*dsc_h14_p - 0x80) << 8) | *(dsc_h14_p + 1);
      if (dsc_l10_p == NULL)
	DEBUG_INFO ("something wrong in DSC\r\n"); /* weird??? */
      else if (dsc_l10_p < dsc_h14_p)
	/* Possibly, power off during writing dsc_l10 */
	dsc_l10 = 0;
    }

  digital_signature_counter = (dsc_h14 << 10) | dsc_l10;
}

/*
 * Write all data to newly allocated Flash ROM page (from P_START),
 * updating PW1_LIFETIME_P, PW_ERR_COUNTER_P, and DO_PTR.
 * Called by flash_copying_gc.
 */
void
gpg_data_copy (const uint8_t *p_start)
{
  const uint8_t *p;
  int i;
  int v;

  p = gpg_write_digital_signature_counter (p_start, digital_signature_counter);

  if (pw1_lifetime_p != NULL)
    {
      flash_bool_write_internal (p, NR_BOOL_PW1_LIFETIME);
      pw1_lifetime_p = p;
      p += 2;
    }

  for (i = 0; i < 3; i++)
    if ((v = flash_cnt123_get_value (pw_err_counter_p[i])) != 0)
      {
	flash_cnt123_write_internal (p, i, v);
	pw_err_counter_p[i] = p + 2;
	p += 4;
      }

  data_objects_number_of_bytes = 0;
  for (i = NR_DO__FIRST__; i < NR_DO__LAST__; i++)
    if (do_ptr[i - NR_DO__FIRST__] != NULL)
      {
	const uint8_t *do_data = do_ptr[i - NR_DO__FIRST__];
	int len = do_data[0];

	flash_do_write_internal (p, i, &do_data[1], len);
	do_ptr[i - NR_DO__FIRST__] = p + 1;
	p += 2 + ((len + 1) & ~1);
	data_objects_number_of_bytes += len;
      }

  flash_set_data_pool_last (p);
}

static const struct do_table_entry *
get_do_entry (uint16_t tag)
{
  int i;

  for (i = 0; i < NUM_DO_ENTRIES; i++)
    if (gpg_do_table[i].tag == tag)
      return &gpg_do_table[i];

  return NULL;
}

static void
copy_do_1 (uint16_t tag, const uint8_t *do_data, int with_tag)
{
  int len;

  if (with_tag)
    {
      copy_tag (tag);

      if (do_data[0] >= 128)
	*res_p++ = 0x81;

      len = do_data[0] + 1;
    }
  else
    {
      len = do_data[0];
      do_data++;
    }

  memcpy (res_p, do_data, len);
  res_p += len;
}

static int
copy_do (const struct do_table_entry *do_p, int with_tag)
{
  if (do_p == NULL)
    return 0;

  if (!ac_check_status (do_p->ac_read))
    return -1;

  switch (do_p->do_type)
    {
    case DO_FIXED:
      {
	const uint8_t *do_data = (const uint8_t *)do_p->obj;
	if (do_data == NULL)
	  return 0;
	else
	  copy_do_1 (do_p->tag, do_data, with_tag);
	break;
      }
    case DO_VAR:
      {
	const uint8_t *do_data = *(const uint8_t **)do_p->obj;
	if (do_data == NULL)
	  return 0;
	else
	  copy_do_1 (do_p->tag, do_data, with_tag);
	break;
      }
    case DO_CMP_READ:
      {
	int i;
	const uint16_t *cmp_data = (const uint16_t *)do_p->obj;
	int num_components = cmp_data[0];
	uint8_t *len_p = NULL;

	if (with_tag)
	  {
	    copy_tag (do_p->tag);
	    *res_p++ = 0x81;	/* Assume it's less than 256 */
	    len_p = res_p;
	    *res_p++ = 0;	/* for now */
	  }

	for (i = 0; i < num_components; i++)
	  {
	    uint16_t tag0;
	    const struct do_table_entry *do0_p;

	    tag0 = cmp_data[i+1];
	    do0_p = get_do_entry (tag0);
	    if (copy_do (do0_p, 1) < 0)
	      return -1;
	  }

	if (len_p)
	  *len_p = res_p - len_p - 1;
	break;
      }
    case DO_PROC_READ:
      {
	int (*do_func)(uint16_t, int) = (int (*)(uint16_t, int))do_p->obj;

	return do_func (do_p->tag, with_tag);
      }
    case DO_PROC_READWRITE:
      {
	int (*rw_func)(uint16_t, int, const uint8_t *, int, int)
	  = (int (*)(uint16_t, int, const uint8_t *, int, int))do_p->obj;

	return rw_func (do_p->tag, with_tag, NULL, 0, 0);
      }
    case DO_PROC_WRITE:
      return -1;
    }

  return 1;
}

/*
 * Process GET_DATA request on Data Object specified by TAG
 *   Call write_res_adpu to fill data returned
 */
void
gpg_do_get_data (uint16_t tag, int with_tag)
{
#if defined(CERTDO_SUPPORT)
  if (tag == GPG_DO_CH_CERTIFICATE)
    {
      apdu.res_apdu_data = &ch_certificate_start;
      apdu.res_apdu_data_len = ((apdu.res_apdu_data[2] << 8) | apdu.res_apdu_data[3]);
      if (apdu.res_apdu_data_len == 0xffff)
	{
	  apdu.res_apdu_data_len = 0;
	  GPG_NO_RECORD ();
	}
      else
	/* Add length of (tag+len) */
	apdu.res_apdu_data_len += 4;
    }
  else
#endif
    {
      const struct do_table_entry *do_p = get_do_entry (tag);

      res_p = res_APDU;

      DEBUG_INFO ("   ");
      DEBUG_SHORT (tag);

      if (do_p)
	{
	  if (copy_do (do_p, with_tag) < 0)
	    /* Overwriting partially written result  */
	    GPG_SECURITY_FAILURE ();
	  else
	    {
	      res_APDU_size = res_p - res_APDU;
	      GPG_SUCCESS ();
	    }
	}
      else
	GPG_NO_RECORD ();
    }
}

void
gpg_do_put_data (uint16_t tag, const uint8_t *data, int len)
{
  const struct do_table_entry *do_p = get_do_entry (tag);

  DEBUG_INFO ("   ");
  DEBUG_SHORT (tag);

  if (do_p)
    {
      if (!ac_check_status (do_p->ac_write))
	{
	  GPG_SECURITY_FAILURE ();
	  return;
	}

      switch (do_p->do_type)
	{
	case DO_FIXED:
	case DO_CMP_READ:
	case DO_PROC_READ:
	  GPG_SECURITY_FAILURE ();
	  break;
	case DO_VAR:
	  {
	    const uint8_t **do_data_p = (const uint8_t **)do_p->obj;

	    if (*do_data_p)
	      flash_do_release (*do_data_p);

	    if (len == 0)
	      {
		/* make DO empty */
		*do_data_p = NULL;
		GPG_SUCCESS ();
	      }
	    else if (len > 255)
	      GPG_MEMORY_FAILURE ();
	    else
	      {
		uint8_t nr = do_tag_to_nr (tag);

		if (nr == NR_NONE)
		  GPG_MEMORY_FAILURE ();
		else
		  {
		    *do_data_p = NULL;
		    *do_data_p = flash_do_write (nr, data, len);
		    if (*do_data_p)
		      GPG_SUCCESS ();
		    else
		      GPG_MEMORY_FAILURE ();
		  }
	      }
	    break;
	  }
	case DO_PROC_READWRITE:
	  {
	    int (*rw_func)(uint16_t, int, const uint8_t *, int, int)
	      = (int (*)(uint16_t, int, const uint8_t *, int, int))do_p->obj;

	    if (rw_func (tag, 0, data, len, 1))
	      GPG_SUCCESS ();
	    else
	      GPG_ERROR ();
	    break;
	  }
	case DO_PROC_WRITE:
	  {
	    int (*proc_func)(const uint8_t *, int)
	      = (int (*)(const uint8_t *, int))do_p->obj;

	    if (proc_func (data, len))
	      GPG_SUCCESS ();
	    else
	      GPG_ERROR ();
	    break;
	  }
	}
    }
  else
    GPG_NO_RECORD ();
}

void
gpg_do_public_key (uint8_t kk_byte)
{
  const uint8_t *do_data;
  const uint8_t *key_addr;

  DEBUG_INFO ("Public key\r\n");
  DEBUG_BYTE (kk_byte);

  if (kk_byte == 0xb6)
    do_data = do_ptr[NR_DO_PRVKEY_SIG - NR_DO__FIRST__];
  else if (kk_byte == 0xb8)
    do_data = do_ptr[NR_DO_PRVKEY_DEC - NR_DO__FIRST__];
  else				/* 0xa4 */
    do_data = do_ptr[NR_DO_PRVKEY_AUT - NR_DO__FIRST__];

  if (do_data == NULL)
    {
      DEBUG_INFO ("none.\r\n");
      GPG_NO_RECORD ();
      return;
    }

  key_addr = (const uint8_t *)fetch_four_bytes (&do_data[1]);

  res_p = res_APDU;

  /* TAG */
  *res_p++ = 0x7f; *res_p++ = 0x49;

#if defined(RSA_AUTH) && defined(RSA_SIG)
  if (0)
#elif defined(RSA_AUTH) && !defined(RSA_SIG)
  /* ECDSA with p256k1 for signature */
  if (kk_byte == 0xb6)
#elif !defined(RSA_AUTH) && defined(RSA_SIG)
  /* ECDSA with p256r1 for authentication */
  if (kk_byte == 0xa4)
#else
#error "not supported."
#endif
    {				/* ECDSA */
      const uint8_t *algorithm_attr;
      int aa_len;

      if (kk_byte == 0xb6)
	algorithm_attr = algorithm_attr_p256k1;
      else
	algorithm_attr = algorithm_attr_p256r1;
      aa_len = algorithm_attr[0] - 1;

      /* LEN */
      *res_p++ = 2 + aa_len + 2 + 1 + 64;
      {
	/*TAG*/          /* LEN = AA_LEN */
	*res_p++ = 0x06; *res_p++ = aa_len;
	memcpy (res_p, algorithm_attr+2, aa_len);
	res_p += aa_len;

	/*TAG*/          /* LEN = 1+64 */
	*res_p++ = 0x86; *res_p++ = 0x41;
	*res_p++ = 0x04; 	/* No compression of EC point.  */
	/* 64-byte binary (big endian) */
	memcpy (res_p, key_addr + KEY_CONTENT_LEN, 64);
	res_p += 64;
      }
    }
  else
    {				/* RSA */
      /* LEN = 9+256 */
      *res_p++ = 0x82; *res_p++ = 0x01; *res_p++ = 0x09;

      {
	/*TAG*/          /* LEN = 256 */
	*res_p++ = 0x81; *res_p++ = 0x82; *res_p++ = 0x01; *res_p++ = 0x00;
	/* 256-byte binary (big endian) */
	memcpy (res_p, key_addr + KEY_CONTENT_LEN, KEY_CONTENT_LEN);
	res_p += 256;
      }
      {
	/*TAG*/          /* LEN= 3 */
	*res_p++ = 0x82; *res_p++ = 3;
	/* 3-byte E=0x10001 (big endian) */
	*res_p++ = 0x01; *res_p++ = 0x00; *res_p++ = 0x01;
      }
    }

  /* Success */
  res_APDU_size = res_p - res_APDU;
  GPG_SUCCESS ();

  DEBUG_INFO ("done.\r\n");
  return;
}

const uint8_t *
gpg_do_read_simple (uint8_t nr)
{
  const uint8_t *do_data;

  do_data = do_ptr[nr - NR_DO__FIRST__];
  if (do_data == NULL)
    return NULL;

  return do_data+1;
}

void
gpg_do_write_simple (uint8_t nr, const uint8_t *data, int size)
{
  const uint8_t **do_data_p;

  do_data_p = (const uint8_t **)&do_ptr[nr - NR_DO__FIRST__];
  if (*do_data_p)
    flash_do_release (*do_data_p);

  if (data != NULL)
    {
      *do_data_p = NULL;
      *do_data_p = flash_do_write (nr, data, size);
      if (*do_data_p == NULL)
	flash_warning ("DO WRITE ERROR");
    }
  else
    *do_data_p = NULL;
}

#ifdef KEYGEN_SUPPORT
void
gpg_do_keygen (uint8_t kk_byte)
{
  enum kind_of_key kk;
  const uint8_t *keystring_admin;
  uint8_t *p_q_modulus;
  const uint8_t *p_q;
  const uint8_t *modulus;
  int r;

  DEBUG_INFO ("Keygen\r\n");
  DEBUG_BYTE (kk_byte);

  if (kk_byte == 0xb6)
    kk = GPG_KEY_FOR_SIGNING;
  else if (kk_byte == 0xb8)
    kk = GPG_KEY_FOR_DECRYPTION;
  else				/* 0xa4 */
    kk = GPG_KEY_FOR_AUTHENTICATION;

  if (admin_authorized == BY_ADMIN)
    keystring_admin = keystring_md_pw3;
  else
    keystring_admin = NULL;

  p_q_modulus = rsa_genkey ();
  if (p_q_modulus == NULL)
    {
      GPG_MEMORY_FAILURE ();
      return;
    }

  p_q = p_q_modulus;
  modulus = p_q_modulus + KEY_CONTENT_LEN;

  r = gpg_do_write_prvkey (kk, p_q, KEY_CONTENT_LEN, keystring_admin, modulus);
  memset (p_q_modulus, 0, KEY_CONTENT_LEN*2);
  free (p_q_modulus);
  if (r < 0)
    {
      GPG_ERROR ();
      return;
    }

  DEBUG_INFO ("Calling gpg_do_public_key...\r\n");

  if (kk == GPG_KEY_FOR_SIGNING)
    {
      const uint8_t *pw = (const uint8_t *)OPENPGP_CARD_INITIAL_PW1;
      uint8_t keystring[KEYSTRING_MD_SIZE];

      /* GnuPG expects it's ready for signing. */
      /* Don't call ac_reset_pso_cds here, but load the private key */

      s2k (NULL, 0, pw, strlen (OPENPGP_CARD_INITIAL_PW1), keystring);
      gpg_do_load_prvkey (GPG_KEY_FOR_SIGNING, BY_USER, keystring);
    }
  else
    ac_reset_other ();

  gpg_do_public_key (kk_byte);
}
#endif
