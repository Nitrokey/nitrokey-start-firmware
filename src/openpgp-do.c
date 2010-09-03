/*
 * openpgp-do.c -- OpenPGP card Data Objects (DO) handling
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
#include "openpgp.h"

#include "polarssl/config.h"
#include "polarssl/aes.h"
#include "polarssl/sha1.h"

/*
 * Compile time vars:
 *   AID, Historical Bytes (template), Extended Capabilities,
 *   and Algorithm Attributes
 */

/* AID */
static const uint8_t const aid[] __attribute__ ((aligned (1))) = {
  16,
  0xd2, 0x76, 0x00, 0x01, 0x24, 0x01,
  0x02, 0x00,			/* Version 2.0 */
  0xf5, 0x17,			/* Manufacturer (FSIJ) */
  0x00, 0x00, 0x00, 0x01,	/* Serial */
  0x00, 0x00
};

/* Historical Bytes (template) */
static const uint8_t const historical_bytes[] __attribute__ ((aligned (1))) = {
  10,
  0x00,
  0x31, 0x80,			/* Full DF name */
  0x73,
  0x80, 0x01, 0x40,		/* Full DF name */
				/* 1-byte */
				/* No command chaining */
				/* Extended Lc and Le */
  0x00, 0x90, 0x00		/* Status info (no life cycle management) */
};

/* Extended Capabilities */
static const uint8_t const extended_capabilities[] __attribute__ ((aligned (1))) = {
  10,
  0x30,				/*
				 * No SM, No get challenge,
				 * Key import supported,
				 * PW status byte can be put,
				 * No private_use_DO,
				 * No algo change allowed
				 */
  0,		  /* Secure Messaging Algorithm: N/A (TDES=0, AES=1) */
  0x00, 0x00,	  /* Max get challenge */
  0x00, 0x00,	  /* max. length of cardholder certificate */
  (MAX_CMD_APDU_SIZE>>8), (MAX_CMD_APDU_SIZE&0xff), /* Max. length of command data */
  (MAX_RES_APDU_SIZE>>8), (MAX_RES_APDU_SIZE&0xff), /* Max. length of response data */
};

/* Algorithm Attributes */
static const uint8_t const algorithm_attr[] __attribute__ ((aligned (1))) = {
  6,
  0x01, /* RSA */
  0x08, 0x00,	      /* Length modulus (in bit): 2048 */
  0x00, 0x20,	      /* Length exponent (in bit): 32  */
  0x00		      /* 0: p&q , 3: CRT with N (not yet supported) */
};

const uint8_t const pw_status_bytes_template[] =
{
  1,				/* PW1 valid for several PSO:CDS commands */
  127, 127, 127,		/* max length of PW1, RC, and PW3 */
  3, 0, 3			/* Error counter of PW1, RC, and PW3 */
};

#define SIZE_DIGITAL_SIGNATURE_COUNTER 3
/* 3-byte binary (big endian) */

#define SIZE_FINGER_PRINT 20
#define SIZE_KEYGEN_TIME 4	/* RFC4880 */

enum do_type {
  DO_FIXED,
  DO_VAR,
  DO_CN_READ,
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
static int with_tag;

static void copy_do_1 (uint16_t tag, const uint8_t *do_data);
static struct do_table_entry *get_do_entry (uint16_t tag);

#define GPG_DO_AID		0x004f
#define GPG_DO_NAME		0x005b
#define GPG_DO_LOGIN_DATA	0x005e
#define GPG_DO_CH_DATA		0x0065
#define GPG_DO_APP_DATA		0x006e
/* XXX: 0x0073 ??? */
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
do_hist_bytes (uint16_t tag)
{
  /* XXX: For now, no life cycle management, just return template as is. */
  /* XXX: Supporing TERMINATE DF / ACTIVATE FILE, we need to fix here */
  copy_do_1 (tag, historical_bytes);
  return 0;
}

#define SIZE_FP 20
#define SIZE_KGTIME 4

static int
do_fp_all (uint16_t tag)
{
  struct do_table_entry *do_p;
  const uint8_t *do_data;

  if (with_tag)
    {
      copy_tag (tag);
      *res_p++ = SIZE_FP*3;
    }

  do_p = get_do_entry (GPG_DO_FP_SIG);
  do_data = (const uint8_t *)do_p->obj;
  if (do_data)
    memcpy (res_p, &do_data[1], SIZE_FP);
  else
    memset (res_p, 0, SIZE_FP);
  res_p += SIZE_FP;

  do_p = get_do_entry (GPG_DO_FP_DEC);
  do_data = (const uint8_t *)do_p->obj;
  if (do_data)
    memcpy (res_p, &do_data[1], SIZE_FP);
  else
    memset (res_p, 0, SIZE_FP);
  res_p += SIZE_FP;

  do_p = get_do_entry (GPG_DO_FP_AUT);
  do_data = (const uint8_t *)do_p->obj;
  if (do_data)
    memcpy (res_p, &do_data[1], SIZE_FP);
  else
    memset (res_p, 0, SIZE_FP);
  res_p += SIZE_FP;

  return 0;
}

static int
do_cafp_all (uint16_t tag)
{
  struct do_table_entry *do_p;
  const uint8_t *do_data;

  if (with_tag)
    {
      copy_tag (tag);
      *res_p++ = SIZE_FP*3;
    }

  do_p = get_do_entry (GPG_DO_CAFP_1);
  do_data = (const uint8_t *)do_p->obj;
  if (do_data)
    memcpy (res_p, &do_data[1], SIZE_FP);
  else
    memset (res_p, 0, SIZE_FP);
  res_p += SIZE_FP;

  do_p = get_do_entry (GPG_DO_CAFP_2);
  do_data = (const uint8_t *)do_p->obj;
  if (do_data)
    memcpy (res_p, &do_data[1], SIZE_FP);
  else
    memset (res_p, 0, SIZE_FP);
  res_p += SIZE_FP;

  do_p = get_do_entry (GPG_DO_CAFP_3);
  do_data = (const uint8_t *)do_p->obj;
  if (do_data)
    memcpy (res_p, &do_data[1], SIZE_FP);
  else
    memset (res_p, 0, SIZE_FP);
  res_p += SIZE_FP;

  return 0;
}

static int
do_kgtime_all (uint16_t tag)
{
  struct do_table_entry *do_p;
  const uint8_t *do_data;

  if (with_tag)
    {
      copy_tag (tag);
      *res_p++ = SIZE_KGTIME*3;
    }

  do_p = get_do_entry (GPG_DO_KGTIME_SIG);
  do_data = (const uint8_t *)do_p->obj;
  if (do_data)
    memcpy (res_p, &do_data[1], SIZE_KGTIME);
  else
    memset (res_p, 0, SIZE_KGTIME);
  res_p += SIZE_KGTIME;

  do_p = get_do_entry (GPG_DO_KGTIME_DEC);
  do_data = (const uint8_t *)do_p->obj;
  if (do_data)
    memcpy (res_p, &do_data[1], SIZE_KGTIME);
  else
    memset (res_p, 0, SIZE_KGTIME);
  res_p += SIZE_KGTIME;

  do_p = get_do_entry (GPG_DO_KGTIME_AUT);
  do_data = (const uint8_t *)do_p->obj;
  if (do_data)
    memcpy (res_p, &do_data[1], SIZE_KGTIME);
  else
    memset (res_p, 0, SIZE_KGTIME);
  res_p += SIZE_KGTIME;
  return 0;
}

static int
rw_pw_status (uint16_t tag, const uint8_t *data, int len, int is_write)
{
  struct do_table_entry *do_p;

  if (is_write)
    {
      const uint8_t *do_data;
      uint8_t pwsb[SIZE_PW_STATUS_BYTES];

      (void)len;
      do_p = get_do_entry (GNUK_DO_PW_STATUS);
      do_data = (const uint8_t *)do_p->obj;
      if (do_data)
	{
	  memcpy (pwsb, &do_data[1], SIZE_PW_STATUS_BYTES);
	  flash_do_release (do_p->obj);
	}
      else
	memcpy (pwsb, pw_status_bytes_template, SIZE_PW_STATUS_BYTES);

      pwsb[0] = data[0];
      do_p->obj = flash_do_write (tag, pwsb, SIZE_PW_STATUS_BYTES);
      if (do_p->obj)
	GPG_SUCCESS ();
      else
	GPG_MEMORY_FAILURE();

      return 0;
    }
  else
    {
      const uint8_t *do_data;

      if (with_tag)
	{
	  copy_tag (tag);
	  *res_p++ = SIZE_PW_STATUS_BYTES;
	}

      do_p = get_do_entry (GNUK_DO_PW_STATUS);
      do_data = (const uint8_t *)do_p->obj;
      if (do_data)
	{
	  memcpy (res_p, &do_data[1], SIZE_PW_STATUS_BYTES);
	  res_p += SIZE_PW_STATUS_BYTES;
	}
      else
	return -1;
    }

  return 0;
}

static aes_context aes;
static uint8_t iv[16];
static int iv_offset;

static void
proc_resetting_code (const uint8_t *data, int len)
{
  const uint8_t *old_ks = keystring_md_pw3;
  uint8_t new_ks0[KEYSTRING_MD_SIZE+1];
  uint8_t *new_ks = &new_ks0[1];
  const uint8_t *newpw;
  int newpw_len;
  int r;
  uint8_t pwsb[SIZE_PW_STATUS_BYTES];
  struct do_table_entry *do_p;
  const uint8_t *do_data;

  newpw_len = len;
  newpw = data;
  sha1 (newpw, newpw_len, new_ks);
  new_ks0[0] = newpw_len;
  r = gpg_change_keystring (3, old_ks, 2, new_ks);
  if (r < -2)
    {
      GPG_MEMORY_FAILURE ();
      return;
    }
  else if (r < 0)
    {
      GPG_SECURITY_FAILURE ();
      return;
    }
  else if (r == 0)
    gpg_do_write_simple (GNUK_DO_KEYSTRING_RC, new_ks0, KEYSTRING_SIZE_RC);
  else
    GPG_SUCCESS ();

  /* Reset RC counter in GNUK_DO_PW_STATUS */
  do_p = get_do_entry (GNUK_DO_PW_STATUS);
  do_data = (const uint8_t *)do_p->obj;
  if (do_data)
    {
      memcpy (pwsb, &do_data[1], SIZE_PW_STATUS_BYTES);
      pwsb[PW_STATUS_RC] = 3;
      flash_do_release (do_data);
    }
  else
    {
      memcpy (pwsb, pw_status_bytes_template, SIZE_PW_STATUS_BYTES);
      pwsb[5] = 3;
    }

  gpg_do_write_simple (GNUK_DO_PW_STATUS, pwsb, SIZE_PW_STATUS_BYTES);
}

static void
encrypt (const uint8_t *key_str, uint8_t *data, int len)
{
  aes_setkey_enc (&aes, key_str, 128);
  memset (iv, 0, 16);
  iv_offset = 0;
  aes_crypt_cfb128 (&aes, AES_ENCRYPT, len, &iv_offset, iv, data, data);

  DEBUG_INFO ("ENC\r\n");
  DEBUG_BINARY (data, KEYSTORE_LEN);
}

struct key_data kd;

static void
decrypt (const uint8_t *key_str, uint8_t *data, int len)
{
  aes_setkey_enc (&aes, key_str, 128);
  memset (iv, 0, 16);
  iv_offset = 0;
  aes_crypt_cfb128 (&aes, AES_DECRYPT, len, &iv_offset, iv, data, data);
  DEBUG_INFO ("DEC\r\n");
  DEBUG_BINARY (data, KEYSTORE_LEN);
}

static uint16_t
get_tag_for_kk (enum kind_of_key kk)
{
  switch (kk)
    {
    case GPG_KEY_FOR_SIGNATURE:
      return GNUK_DO_PRVKEY_SIG;
    case GPG_KEY_FOR_DECRYPT:
      return GNUK_DO_PRVKEY_DEC;
    case GPG_KEY_FOR_AUTHENTICATION:
      return GNUK_DO_PRVKEY_AUT;
    }
  return GNUK_DO_PRVKEY_SIG;
}

/*
 * Return  1 on success,
 *         0 if none,
 *        -1 on error,
 */
int
gpg_do_load_prvkey (enum kind_of_key kk, int who, const uint8_t *keystring)
{
  uint16_t tag = get_tag_for_kk (kk);
  struct do_table_entry *do_p = get_do_entry (tag);
  uint8_t *key_addr;
  uint8_t dek[DATA_ENCRYPTION_KEY_SIZE];

  if (do_p->obj == NULL)
    return 0;

  key_addr = *(uint8_t **)&((uint8_t *)do_p->obj)[1];
  memcpy (kd.data, key_addr, KEY_CONTENT_LEN);
  memcpy (((uint8_t *)&kd.check), ((uint8_t *)do_p->obj)+5, ADDITIONAL_DATA_SIZE);
  memcpy (dek, ((uint8_t *)do_p->obj)+5+16*who, DATA_ENCRYPTION_KEY_SIZE);

  decrypt (keystring, dek, DATA_ENCRYPTION_KEY_SIZE);
  decrypt (dek, (uint8_t *)&kd, sizeof (kd));
  if (memcmp (kd.magic, GNUK_MAGIC, KEY_MAGIC_LEN) != 0)
    return -1;
  /* XXX: more sanity check */
  return 1;
}

static uint32_t
calc_check32 (const uint8_t *p, int len)
{
  uint32_t check = 0;
  uint32_t *data = (uint32_t *)p;
  int i;

  for (i = 0; i < len/4; i++)
    check += data[i];

  return check;
}

int
gpg_do_write_prvkey (enum kind_of_key kk, const uint8_t *key_data, int key_len,
		     const uint8_t *keystring)
{
  const uint8_t *p;
  int r;
  struct do_table_entry *do_p;
  const uint8_t *modulus;
  struct key_data *kd;
  struct prvkey_data *pd;
  uint8_t *key_addr;
  uint8_t *dek;
  uint16_t tag = get_tag_for_kk (kk);
  const uint8_t *ks_pw1 = gpg_do_read_simple (GNUK_DO_KEYSTRING_PW1);
  const uint8_t *ks_rc = gpg_do_read_simple (GNUK_DO_KEYSTRING_RC);

#if 0
  assert (key_len == KEY_CONTENT_LEN);
#endif

  DEBUG_SHORT (key_len);

  kd = (struct key_data *)malloc (sizeof (struct key_data));
  if (kd == NULL)
    return -1;

  pd = (struct prvkey_data *)malloc (sizeof (struct prvkey_data));
  if (pd == NULL)
    {
      free (kd);
      return -1;
    }

  modulus = modulus_calc (key_data, key_len);
  if (modulus == NULL)
    {
      free (kd);
      free (pd);
      return -1;
    }

  key_addr = flash_key_alloc (kk);
  if (key_addr == NULL)
    {
      free (kd);
      free (pd);
      modulus_free (modulus);
      return -1;
    }

  memcpy (kd->data, key_data, KEY_CONTENT_LEN);
  kd->check = calc_check32 (key_data, KEY_CONTENT_LEN);
  kd->random = get_random ();
  memcpy (kd->magic, GNUK_MAGIC, KEY_MAGIC_LEN);

  DEBUG_INFO ("enc...");

  dek = get_data_encryption_key (); /* 16-byte random bytes */
  encrypt (dek, (uint8_t *)kd, sizeof (kd));

  DEBUG_INFO ("done\r\n");

  r = flash_key_write (key_addr, kd->data, modulus);
  modulus_free (modulus);

  if (r < 0)
    {
      dek_free (dek);
      free (pd);
      free (kd);
      return r;
    }

  pd->key_addr = key_addr;
  memcpy (pd->crm_encrypted, (uint8_t *)&kd->check, ADDITIONAL_DATA_SIZE);

  reset_pso_cds ();
  if (ks_pw1)
    {
      memcpy (pd->dek_encrypted_1, dek, DATA_ENCRYPTION_KEY_SIZE);
      encrypt (ks_pw1+1, pd->dek_encrypted_1, DATA_ENCRYPTION_KEY_SIZE);
      /* Only its length */
      gpg_do_write_simple (GNUK_DO_KEYSTRING_PW1, ks_pw1, 1);
    }
  else
    {
      uint8_t ks123_pw1[KEYSTRING_SIZE_PW1];

      ks123_pw1[0] = 6;
      sha1 ((uint8_t *)"123456", 6, ks123_pw1+1);
      memcpy (pd->dek_encrypted_1, dek, DATA_ENCRYPTION_KEY_SIZE);
      encrypt (ks123_pw1+1, pd->dek_encrypted_1, DATA_ENCRYPTION_KEY_SIZE);
      /* Only but its length */
      gpg_do_write_simple (GNUK_DO_KEYSTRING_PW1, ks123_pw1, 1);
    }

  if (ks_rc)
    {
      memcpy (pd->dek_encrypted_2, dek, DATA_ENCRYPTION_KEY_SIZE);
      encrypt (ks_rc+1, pd->dek_encrypted_2, DATA_ENCRYPTION_KEY_SIZE);
      /* Only its length */
      gpg_do_write_simple (GNUK_DO_KEYSTRING_RC, ks_rc, 1);
    }
  else
    memset (pd->dek_encrypted_2, 0, DATA_ENCRYPTION_KEY_SIZE);

  memcpy (pd->dek_encrypted_3, dek, DATA_ENCRYPTION_KEY_SIZE);
  encrypt (keystring, pd->dek_encrypted_3, DATA_ENCRYPTION_KEY_SIZE);

  p = flash_do_write (tag, (const uint8_t *)pd, sizeof (struct prvkey_data));
  do_p = get_do_entry (tag);
  do_p->obj = p;

  dek_free (dek);
  free (kd);
  free (pd);
  if (p == NULL)
    return -1;

  return 0;
}

int
gpg_do_chks_prvkey (enum kind_of_key kk,
		    int who_old, const uint8_t *old_ks,
		    int who_new, const uint8_t *new_ks)
{
  uint16_t tag = get_tag_for_kk (kk);
  struct do_table_entry *do_p = get_do_entry (tag);
  uint8_t dek[DATA_ENCRYPTION_KEY_SIZE];
  struct prvkey_data *pd;
  const uint8_t *p;
  uint8_t *dek_p;

  if (do_p->obj == NULL)
    return 0;			/* No private key */

  pd = (struct prvkey_data *)malloc (sizeof (struct prvkey_data));
  if (pd == NULL)
    return -1;

  memcpy (pd, &((uint8_t *)do_p->obj)[1], sizeof (struct prvkey_data));
  dek_p = ((uint8_t *)pd) + 4 + ADDITIONAL_DATA_SIZE + DATA_ENCRYPTION_KEY_SIZE * (who_old - 1);
  memcpy (dek, dek_p, DATA_ENCRYPTION_KEY_SIZE);
  decrypt (old_ks, dek, DATA_ENCRYPTION_KEY_SIZE);
  encrypt (new_ks, dek, DATA_ENCRYPTION_KEY_SIZE);
  dek_p += DATA_ENCRYPTION_KEY_SIZE * (who_new - who_old);
  memcpy (dek_p, dek, DATA_ENCRYPTION_KEY_SIZE);

  p = flash_do_write (tag, (const uint8_t *)pd, sizeof (struct prvkey_data));
  do_p->obj = p;

  free (pd);
  if (p == NULL)
    return -1;

  return 1;
}

/*
 * 4d, xx, xx:    Extended Header List
 *   b6 00 (SIG) / b8 00 (DEC) / a4 00 (AUT)
 *   7f48, xx: cardholder private key template
 *       91 xx
 *       92 xx
 *       93 xx
 *   5f48, xx: cardholder private key
 */
static void
proc_key_import (const uint8_t *data, int len)
{
  int r;
  enum kind_of_key kk;

  DEBUG_BINARY (data, len);

  if (data[4] == 0xb6)
    kk = GPG_KEY_FOR_SIGNATURE;
  else if (data[4] == 0xb8)
    kk = GPG_KEY_FOR_DECRYPT;
  else				/* 0xa4 */
    kk = GPG_KEY_FOR_AUTHENTICATION;

  if (len <= 22)
    {					    /* Deletion of the key */
      uint16_t tag = get_tag_for_kk (kk);
      struct do_table_entry *do_p = get_do_entry (tag);

      if (do_p->obj)
	{
	  uint8_t *key_addr = *(uint8_t **)&((uint8_t *)do_p->obj)[1];

	  flash_do_release (do_p->obj);
	  flash_key_release (key_addr);
	}

      do_p->obj = NULL;
      GPG_SUCCESS ();
      return;
    }

  /* It should starts with 00 01 00 01 (E) */
  /* Skip E, 4-byte */
  r = gpg_do_write_prvkey (kk, &data[26], len - 26, keystring_md_pw3);
  if (r < 0)
    GPG_MEMORY_FAILURE();
  else
    GPG_SUCCESS ();
}

static const uint16_t const cn_ch_data[] = {
  3,
  GPG_DO_NAME,
  GPG_DO_LANGUAGE,
  GPG_DO_SEX,
};

static const uint16_t const cn_app_data[] = {
  10,
  GPG_DO_AID,
  GPG_DO_HIST_BYTES,
  /* XXX Discretionary data objects 0x0073 ??? */
  GPG_DO_EXTCAP,
  GPG_DO_ALG_SIG, GPG_DO_ALG_DEC, GPG_DO_ALG_AUT,
  GPG_DO_PW_STATUS,
  GPG_DO_FP_ALL, GPG_DO_CAFP_ALL, GPG_DO_KGTIME_ALL
};

static const uint16_t const cn_ss_temp[] = { 1, GPG_DO_DS_COUNT };

static struct do_table_entry
gpg_do_table[] = {
  /* Pseudo DO (private): not directly user accessible */
  { GNUK_DO_PRVKEY_SIG, DO_VAR, AC_NEVER, AC_NEVER, NULL },
  { GNUK_DO_PRVKEY_DEC, DO_VAR, AC_NEVER, AC_NEVER, NULL },
  { GNUK_DO_PRVKEY_AUT, DO_VAR, AC_NEVER, AC_NEVER, NULL },
  { GNUK_DO_KEYSTRING_PW1, DO_VAR, AC_NEVER, AC_NEVER, NULL },
  { GNUK_DO_KEYSTRING_PW3, DO_VAR, AC_NEVER, AC_NEVER, NULL },
  { GNUK_DO_KEYSTRING_RC, DO_VAR, AC_NEVER, AC_NEVER, NULL },
  { GNUK_DO_PW_STATUS, DO_VAR, AC_NEVER, AC_NEVER, NULL },
  /* Pseudo DO READ: calculated */
  { GPG_DO_HIST_BYTES, DO_PROC_READ, AC_ALWAYS, AC_NEVER, do_hist_bytes },
  { GPG_DO_FP_ALL, DO_PROC_READ, AC_ALWAYS, AC_NEVER, do_fp_all },
  { GPG_DO_CAFP_ALL, DO_PROC_READ, AC_ALWAYS, AC_NEVER, do_cafp_all },
  { GPG_DO_KGTIME_ALL, DO_PROC_READ, AC_ALWAYS, AC_NEVER, do_kgtime_all },
  /* Pseudo DO READ/WRITE: calculated */
  { GPG_DO_PW_STATUS, DO_PROC_READWRITE, AC_ALWAYS, AC_ADMIN_AUTHORIZED,
    rw_pw_status },
  /* Fixed data */
  { GPG_DO_AID, DO_FIXED, AC_ALWAYS, AC_NEVER, aid },
  { GPG_DO_EXTCAP, DO_FIXED, AC_ALWAYS, AC_NEVER, extended_capabilities },
  { GPG_DO_ALG_SIG, DO_FIXED, AC_ALWAYS, AC_NEVER, algorithm_attr },
  { GPG_DO_ALG_DEC, DO_FIXED, AC_ALWAYS, AC_NEVER, algorithm_attr },
  { GPG_DO_ALG_AUT, DO_FIXED, AC_ALWAYS, AC_NEVER, algorithm_attr },
  /* Variable(s): Fixed size, not changeable by user */
  { GPG_DO_DS_COUNT, DO_VAR, AC_ALWAYS, AC_NEVER, NULL },
  /* Variables: Fixed size */
  { GPG_DO_SEX, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, NULL },
  { GPG_DO_FP_SIG, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, NULL },
  { GPG_DO_FP_DEC, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, NULL },
  { GPG_DO_FP_AUT, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, NULL },
  { GPG_DO_CAFP_1, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, NULL },
  { GPG_DO_CAFP_2, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, NULL },
  { GPG_DO_CAFP_3, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, NULL },
  { GPG_DO_KGTIME_SIG, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, NULL },
  { GPG_DO_KGTIME_DEC, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, NULL },
  { GPG_DO_KGTIME_AUT, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, NULL },
  /* Variables: Variable size */
  { GPG_DO_LOGIN_DATA, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, NULL },
  { GPG_DO_URL, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, NULL },
  { GPG_DO_NAME, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, NULL },
  { GPG_DO_LANGUAGE, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, NULL },
  { GPG_DO_CH_CERTIFICATE, DO_VAR, AC_ALWAYS, AC_ADMIN_AUTHORIZED, NULL },
  /* Compound data: Read access only */
  { GPG_DO_CH_DATA, DO_CN_READ, AC_ALWAYS, AC_NEVER, cn_ch_data },
  { GPG_DO_APP_DATA, DO_CN_READ, AC_ALWAYS, AC_NEVER, cn_app_data },
  { GPG_DO_SS_TEMP, DO_CN_READ, AC_ALWAYS, AC_NEVER, cn_ss_temp },
  /* Simple data: write access only */
  { GPG_DO_RESETTING_CODE, DO_PROC_WRITE, AC_NEVER, AC_ADMIN_AUTHORIZED,
    proc_resetting_code },
  /* Compound data: Write access only*/
  { GPG_DO_KEY_IMPORT, DO_PROC_WRITE, AC_NEVER, AC_ADMIN_AUTHORIZED,
    proc_key_import },
};

#define NUM_DO_ENTRIES (int)(sizeof (gpg_do_table) / sizeof (struct do_table_entry))

extern const uint8_t const do_5e[];
extern const uint8_t const do_5b[];
extern const uint8_t const do_5f2d[];
extern const uint8_t const do_5f35[];
extern const uint8_t const do_5f50[];

/*
 * Initialize GPG_DO_TABLE reading from Flash ROM
 */
int
gpg_do_table_init (void)
{
  struct do_table_entry *do_p;

  do_p = get_do_entry (GPG_DO_LOGIN_DATA);
  do_p->obj = do_5e;
  do_p = get_do_entry (GNUK_DO_PW_STATUS);
  do_p->obj = pw_status_bytes_template;
  do_p = get_do_entry (GPG_DO_NAME);
  do_p->obj = do_5b;
  do_p = get_do_entry (GPG_DO_LANGUAGE);
  do_p->obj = do_5f2d;
  do_p = get_do_entry (GPG_DO_SEX);
  do_p->obj = do_5f35;
  do_p = get_do_entry (GPG_DO_URL);
  do_p->obj = do_5f50;

  return 0;
}

static struct do_table_entry *
get_do_entry (uint16_t tag)
{
  int i;

  for (i = 0; i < NUM_DO_ENTRIES; i++)
    if (gpg_do_table[i].tag == tag)
      return &gpg_do_table[i];

  return NULL;
}

static void
copy_do_1 (uint16_t tag, const uint8_t *do_data)
{
  int len;

  if (with_tag)
    {
      copy_tag (tag);

      if (do_data[0] < 127)
	len = do_data[0] + 1;
      else if (do_data[1] == 0x81)
	len = do_data[1] + 2;
      else				/* 0x82 */
	len = ((do_data[1] << 8) | do_data[2]) + 3;
    }
  else
    {
      if (do_data[0] < 127)
	{
	  len = do_data[0];
	  do_data++;
	}
      else if (do_data[1] == 0x81)
	{
	  len = do_data[1];
	  do_data += 2;
	}
      else				/* 0x82 */
	{
	  len = ((do_data[1] << 8) | do_data[2]);
	  do_data += 3;
	}
    }

  memcpy (res_p, do_data, len);
  res_p += len;
}

static int
copy_do (struct do_table_entry *do_p)
{
  if (do_p == NULL)
    return 0;

  if (!ac_check_status (do_p->ac_read))
    return -1;

  switch (do_p->do_type)
    {
    case DO_FIXED:
    case DO_VAR:
      {
	const uint8_t *do_data = (const uint8_t *)do_p->obj;
	if (do_data == NULL)
	  return 0;
	else
	  copy_do_1 (do_p->tag, do_data);
	break;
      }
    case DO_CN_READ:
      {
	int i;
	const uint16_t *cn_data = (const uint16_t *)do_p->obj;
	int num_components = cn_data[0];
	uint8_t *len_p;

	copy_tag (do_p->tag);
	*res_p++ = 0x81;
	len_p = res_p;
	*res_p++ = 0;		/* for now */
	with_tag = 1;

	for (i = 0; i < num_components; i++)
	  {
	    uint16_t tag0;
	    struct do_table_entry *do0_p;

	    tag0 = cn_data[i+1];
	    do0_p = get_do_entry (tag0);
	    if (copy_do (do0_p) < 0)
	      return -1;
	  }

	*len_p = (res_p - len_p);
	break;
      }
    case DO_PROC_READ:
      {
	int (*do_func)(uint16_t) = (int (*)(uint16_t))do_p->obj;

	return do_func (do_p->tag);
      }
    case DO_PROC_READWRITE:
      {
	int (*rw_func)(uint16_t, uint8_t *, int, int)
	  = (int (*)(uint16_t, uint8_t *, int, int))do_p->obj;

	return rw_func (do_p->tag, NULL, 0, 0);
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
gpg_do_get_data (uint16_t tag)
{
  struct do_table_entry *do_p = get_do_entry (tag);

  res_p = res_APDU;
  with_tag = 0;

  DEBUG_INFO ("   ");
  DEBUG_SHORT (tag);

  if (do_p)
    {
      if (copy_do (do_p) < 0)
	/* Overwriting partially written result  */
	GPG_SECURITY_FAILURE ();
      else
	{
	  *res_p++ = 0x90;
	  *res_p++ = 0x00;
	  res_APDU_size = res_p - res_APDU;
	}
    }
  else
    GPG_NO_RECORD();
}

void
gpg_do_put_data (uint16_t tag, const uint8_t *data, int len)
{
  struct do_table_entry *do_p = get_do_entry (tag);

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
	case DO_CN_READ:
	case DO_PROC_READ:
	  GPG_SECURITY_FAILURE ();
	  break;
	case DO_VAR:
	  {
	    const uint8_t *do_data = (const uint8_t *)do_p->obj;

	    if (do_data)
	      flash_do_release (do_data);

	    if (len == 0)
	      /* make DO empty */
	      do_p->obj = NULL;
	    else
	      {
		do_p->obj = flash_do_write (tag, data, len);
		if (do_p->obj)
		  GPG_SUCCESS ();
		else
		  GPG_MEMORY_FAILURE();
	      }
	    break;
	  }
	case DO_PROC_READWRITE:
	  {
	    int (*rw_func)(uint16_t, const uint8_t *, int, int)
	      = (int (*)(uint16_t, const uint8_t *, int, int))do_p->obj;

	    rw_func (tag, data, len, 1);
	    break;
	  }
	case DO_PROC_WRITE:
	  {
	    void (*proc_func)(const uint8_t *, int)
	      = (void (*)(const uint8_t *, int))do_p->obj;

	    proc_func (data, len);
	    break;
	  }
	}
    }
  else
    GPG_NO_RECORD();
}

void
gpg_do_public_key (uint8_t kk_byte)
{
  struct do_table_entry *do_p;
  uint8_t *key_addr;

  if (kk_byte == 0xa4)
    do_p = get_do_entry (GNUK_DO_PRVKEY_AUT);
  else if (kk_byte == 0xb8)
    do_p = get_do_entry (GNUK_DO_PRVKEY_DEC);
  else				/* 0xb6 */
    do_p = get_do_entry (GNUK_DO_PRVKEY_SIG);
  if (do_p->obj == NULL)
    {
      GPG_NO_RECORD();
      return;
    }

  key_addr = *(uint8_t **)&((uint8_t *)do_p->obj)[1];

  res_p = res_APDU;

  /* TAG */
  *res_p++ = 0x7f; *res_p++ = 0x49;
  /* LEN = 9+256 */
  *res_p++ = 0x82; *res_p++ = 0x01; *res_p++ = 0x09;

  {
    /*TAG*/          /*LEN = 256 */
    *res_p++ = 0x81; *res_p++ = 0x82; *res_p++ = 0x01; *res_p++ = 0x00;
    /* 256-byte binary (big endian) */
    memcpy (res_p, key_addr + KEY_CONTENT_LEN, KEY_CONTENT_LEN);
    res_p += 256;
  }
  {
    /*TAG*/          /*LEN= 3 */
    *res_p++ = 0x82; *res_p++ = 3;
    /* 3-byte E=0x10001 (big endian) */
    *res_p++ = 0x01; *res_p++ = 0x00; *res_p++ = 0x01;

    /* Success */
    *res_p++ = 0x90; *res_p++ = 0x00;
    res_APDU_size = res_p - res_APDU;
  }

  return;
}

const uint8_t *
gpg_do_read_simple (uint16_t tag)
{
  struct do_table_entry *do_p;
  const uint8_t *do_data;

  do_p = get_do_entry (tag);
  do_data = (const uint8_t *)do_p->obj;
  if (do_data == NULL)
    return NULL;

  return do_data+1;
}

void
gpg_do_write_simple (uint16_t tag, const uint8_t *data, int size)
{
  struct do_table_entry *do_p;
  const uint8_t *do_data;

  do_p = get_do_entry (tag);
  do_data = (const uint8_t *)do_p->obj;
  if (do_data)
    flash_do_release (do_p->obj);

  do_p->obj = flash_do_write (tag, data, size);
  if (do_p->obj)
    GPG_SUCCESS ();
  else
    GPG_MEMORY_FAILURE();
}

void
gpg_do_increment_digital_signature_counter (void)
{
  struct do_table_entry *do_p;
  const uint8_t *do_data;
  uint32_t count;
  uint8_t count_data[SIZE_DIGITAL_SIGNATURE_COUNTER];

  do_p = get_do_entry (GPG_DO_DS_COUNT);
  do_data = (const uint8_t *)do_p->obj;
  if (do_data == NULL)		/* No object means count 0 */
    count = 0; 
  else
    count = (do_data[1]<<16) | (do_data[2]<<8) | do_data[3];

  count++;
  count_data[0] = (count >> 16) & 0xff;
  count_data[1] = (count >> 8) & 0xff;
  count_data[2] = count & 0xff;

  do_p->obj = flash_do_write (GPG_DO_DS_COUNT, count_data,
			      SIZE_DIGITAL_SIGNATURE_COUNTER);
}
