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

#include "polarssl/config.h"
#include "polarssl/aes.h"

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



#define SIZE_PW_STATUS_BYTES 7
#if 0
{
  1,				/* PW1 valid for several PSO:CDS commands */
  127, 127, 127,		/* max length of PW1, RC, and PW3 */
  3, 0, 3			/* Error counter of PW1, RC, and PW3 */
};
#endif

#define SIZE_DIGITAL_SIGNATURE_COUNTER 3
#if 0
{
  0, 0, 0			/* 3-byte binary */
};
#endif

#define SIZE_FINGER_PRINT 20
#define SIZE_KEYGEN_TIME 4	/* RFC4880 */

/* Runtime vars: PSO */

struct key_store {
  uint8_t p[128];
  uint8_t q[128];
};

static struct key_store key_sig, key_dec, key_aut;

#define HASH_SIZE 20
static uint8_t pw3_hash[HASH_SIZE];

enum do_type {
  DO_FIXED,
  DO_VAR,
  DO_CN_READ,
  DO_PROC_READ,
  DO_PROC_WRITE,
  DO_PROC_READWRITE,
  DO_HASH,
  DO_KEYPTR
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

#define GNUK_DO_KEYPTR_SIG 0xff01
#define GNUK_DO_KEYPTR_DEC 0xff02
#define GNUK_DO_KEYPTR_AUT 0xff03
#define GNUK_DO_HASH_PW3   0xff04
#define GNUK_DO_HASH_RC    0xff05
#define GNUK_DO_PW_STATUS  0xff06

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

static void
do_hist_bytes (uint16_t tag)
{
  /* XXX: For now, no life cycle management, just return template as is. */
  /* XXX: Supporing TERMINATE DF / ACTIVATE FILE, we need to fix here */
  copy_do_1 (tag, historical_bytes);
}

#define SIZE_FP 20
#define SIZE_KGTIME 4

static void
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
}

static void
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
}

static void
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
}

static void
rw_pw_status (uint16_t tag, uint8_t *data, int len, int is_write)
{
  struct do_table_entry *do_p;

  if (is_write)
    {
      const uint8_t *do_data;

      do_p = get_do_entry (GNUK_DO_PW_STATUS);
      if (do_p)
	{
	  do_data = (const uint8_t *)do_p->obj;
	  if (do_data)
	    memcpy (data, &do_data[2], SIZE_PW_STATUS_BYTES - 1);

	  do_p->obj = flash_do_write (tag, data, len);
	  if (do_p->obj)
	    write_res_apdu (NULL, 0, 0x90, 0x00); /* success */
	  else
	    write_res_apdu (NULL, 0, 0x65, 0x81); /* memory failure */
	}
      else
	/* No record */
	write_res_apdu (NULL, 0, 0x6a, 0x88);
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
	/* No record */
	write_res_apdu (NULL, 0, 0x6a, 0x88);
    }
}

static void
proc_resetting_code (uint16_t tag, uint8_t *data, int len)
{
  /* Not implementad yet */
  /*
   * calculate hash, write it to GNUK_DO_HASH_RC.
   */
  /* Then, we need to reset RC counter in GNUK_DO_PW_STATUS */
}

static uint8_t iv[16];

static aes_context aes;
static int iv_offset;

static void
encrypt (uint8_t *key_str, uint8_t *data)
{
  aes_setkey_enc (&aes, key_str, 128);
  memset (iv, 0, 16);
  iv_offset = 0;
  aes_crypt_cfb128 (&aes, AES_ENCRYPT, KEYSTORE_LEN, &iv_offset, iv, data, data);

  {
    int i, len = KEYSTORE_LEN;

    put_string ("ENC\r\n");
    for (i = 0; i < len; i++)
      {
	put_byte_with_no_nl (data[i]);
	if ((i & 0x0f) == 0x0f)
	  _write ("\r\n", 2);
      }
    _write ("\r\n", 2);
  }
}

struct key_data kd;

static void
decrypt (uint8_t *key_str, uint8_t *data)
{
  aes_setkey_enc (&aes, key_str, 128);
  memset (iv, 0, 16);
  iv_offset = 0;
  aes_crypt_cfb128 (&aes, AES_DECRYPT, KEYSTORE_LEN, &iv_offset, iv, data, data);
  {
    int i, len = KEYSTORE_LEN;

    put_string ("DEC\r\n");
    for (i = 0; i < len; i++)
      {
	put_byte_with_no_nl (data[i]);
	if ((i & 0x0f) == 0x0f)
	  _write ("\r\n", 2);
      }
    _write ("\r\n", 2);
  }
}

int
gpg_load_key (enum kind_of_key kk)
{
  struct do_table_entry *do_p;

  switch (kk)
    {
    case GPG_KEY_FOR_SIGNATURE:
      do_p = get_do_entry (GNUK_DO_KEYPTR_SIG);
      break;
    case GPG_KEY_FOR_DECRYPT:
      do_p = get_do_entry (GNUK_DO_KEYPTR_DEC);
      break;
    case GPG_KEY_FOR_AUTHENTICATION:
      do_p = get_do_entry (GNUK_DO_KEYPTR_AUT);
      break;
    }
  if (do_p == NULL)
    return -1;

  kd.key_addr = *(uint8_t **)&((uint8_t *)do_p->obj)[1];
  memcpy (((uint8_t *)&kd)+4, ((uint8_t *)do_p->obj)+5, KEY_MAGIC_LEN+4+4);
  memcpy (kd.data, kd.key_addr, KEY_CONTENT_LEN);

  decrypt (keystring_pw1, ((uint8_t *)&kd) + 4);
  if (memcmp (kd.magic, GNUK_MAGIC, KEY_MAGIC_LEN) != 0)
    return -1;
  /* XXX: more sanity check */
  return 0;
}

uint32_t
get_random (void)
{
  return 0x80808080;		/* XXX: for now */
}

static uint32_t
calc_check (uint8_t *data, int len)
{
  uint32_t check = 0;
  int i;

  for (i = 0; i < len; i++)
    check += data[i];

  return check;
}

static int
gpg_write_do_keyptr (enum kind_of_key kk, uint8_t *key_data, int key_len)
{
  uint8_t *p;
  int len;
  int r;
  uint32_t key_addr;
  struct do_table_entry *do_p;

#if 0
  assert (key_len == KEY_CONTENT_LEN+4);
  /* key_data should starts with 00 01 00 01 (E) */
#endif

  put_short (key_len);

  key_data += 4;

  kd.key_addr = flash_key_alloc (kk);
  if (kd.key_addr == NULL)
    return -1;

  memcpy (kd.magic, GNUK_MAGIC, KEY_MAGIC_LEN);
  kd.check = calc_check (key_data, KEY_CONTENT_LEN);
  kd.random = get_random ();
  memcpy (kd.data, key_data, KEY_CONTENT_LEN);

  put_string ("enc...");

  encrypt (keystring_pw1, ((uint8_t *)&kd) + 4);

  put_string ("done\r\n");

  if ((r = flash_key_write (kd.key_addr, kd.data)) < 0)
    return r;

  switch (kk)
    {
    case GPG_KEY_FOR_SIGNATURE:
      p = flash_do_write (GNUK_DO_KEYPTR_SIG, &kd,
			  sizeof (kd) - KEY_CONTENT_LEN);
      do_p = get_do_entry (GNUK_DO_KEYPTR_SIG);
      do_p->obj = p;
      break;
    case GPG_KEY_FOR_DECRYPT:
      p = flash_do_write (GNUK_DO_KEYPTR_DEC, &kd,
			  sizeof (kd) - KEY_CONTENT_LEN);
      do_p = get_do_entry (GNUK_DO_KEYPTR_DEC);
      do_p->obj = p;
      break;
    case GPG_KEY_FOR_AUTHENTICATION:
      p = flash_do_write (GNUK_DO_KEYPTR_AUT, &kd,
			  sizeof (kd) - KEY_CONTENT_LEN);
      do_p = get_do_entry (GNUK_DO_KEYPTR_AUT);
      do_p->obj = p;
      break;
    }

  if (p == NULL)
    return -1;

  return 0;
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
proc_key_import (uint16_t tag, uint8_t *data, int len)
{
  int i, r;
  enum kind_of_key kk;

  for (i = 0; i < len; i++)
    {
      put_byte_with_no_nl (data[i]);
      if ((i & 0x0f) == 0x0f)
	_write ("\r\n", 2);
    }
  _write ("\r\n", 2);

  if (data[4] == 0xa4)
    kk = GPG_KEY_FOR_AUTHENTICATION;
  else if (data[4] == 0xb8)
    kk = GPG_KEY_FOR_DECRYPT;
  else				/* 0xb6 */
    kk = GPG_KEY_FOR_SIGNATURE;

  r = gpg_write_do_keyptr (kk, &data[22], len - 22);
  if (r < 0)
    write_res_apdu (NULL, 0, 0x65, 0x81); /* memory failure */
  else
    write_res_apdu (NULL, 0, 0x90, 0x00); /* success */
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
  /* Pseudo DO (private): not user accessible */
  { GNUK_DO_KEYPTR_SIG, DO_KEYPTR, AC_NEVER, AC_NEVER, NULL },
  { GNUK_DO_KEYPTR_DEC, DO_KEYPTR, AC_NEVER, AC_NEVER, NULL },
  { GNUK_DO_KEYPTR_AUT, DO_KEYPTR, AC_NEVER, AC_NEVER, NULL },
  { GNUK_DO_HASH_PW3, DO_HASH, AC_NEVER, AC_NEVER, NULL },
  { GNUK_DO_HASH_RC, DO_HASH, AC_NEVER, AC_NEVER, NULL },
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
extern const uint8_t const do_c4[];
extern const uint8_t const do_c7[];
extern const uint8_t const do_ca[];
extern const uint8_t const do_ce[];
extern const uint8_t const do_5b[];
extern const uint8_t const do_5f2d[];
extern const uint8_t const do_5f35[];
extern const uint8_t const do_93[];
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
  do_p->obj = do_c4;
#if 0
  do_p = get_do_entry (GPG_DO_FP_SIG);
  do_p->obj = do_c7;
  do_p = get_do_entry (GPG_DO_CAFP_1);
  do_p->obj = do_ca;
  do_p = get_do_entry (GPG_DO_KGTIME_SIG);
  do_p->obj = do_ce;
#endif
  do_p = get_do_entry (GPG_DO_NAME);
  do_p->obj = do_5b;
  do_p = get_do_entry (GPG_DO_LANGUAGE);
  do_p->obj = do_5f2d;
  do_p = get_do_entry (GPG_DO_SEX);
  do_p->obj = do_5f35;
  do_p = get_do_entry (GPG_DO_DS_COUNT);
  do_p->obj = do_93;
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

  if (ac_check_status (do_p->ac_read) == 0)
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
	void (*do_func)(uint16_t) = (void (*)(uint16_t))do_p->obj;

	do_func (do_p->tag);
	break;
      }
    case DO_PROC_READWRITE:
      {
	void (*rw_func)(uint16_t, uint8_t *, int, int)
	  = (void (*)(uint16_t, uint8_t *, int, int))do_p->obj;

	rw_func (do_p->tag, NULL, 0, 0);
	break;
      }
    case DO_PROC_WRITE:
    case DO_HASH:
    case DO_KEYPTR:
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

#ifdef DEBUG
  put_string ("   ");
  put_short (tag);
#endif

  if (do_p)
    {
      if (copy_do (do_p) < 0)
	/* Overwrite partially written result  */
	write_res_apdu (NULL, 0, 0x69, 0x82);
      else
	{
	  *res_p++ = 0x90;
	  *res_p++ = 0x00;
	  res_APDU_size = res_p - res_APDU;
	}
    }
  else
    /* No record */
      write_res_apdu (NULL, 0, 0x6a, 0x88);
}

void
gpg_do_put_data (uint16_t tag, uint8_t *data, int len)
{
  struct do_table_entry *do_p = get_do_entry (tag);

#ifdef DEBUG
  put_string ("   ");
  put_short (tag);
#endif

  if (do_p)
    {
      if (ac_check_status (do_p->ac_write) == 0)
	{
	  write_res_apdu (NULL, 0, 0x69, 0x82);
	  return;
	}

      switch (do_p->do_type)
	{
	case DO_FIXED:
	case DO_CN_READ:
	case DO_PROC_READ:
	case DO_HASH:
	case DO_KEYPTR:
	  write_res_apdu (NULL, 0, 0x69, 0x82);
	  break;
	case DO_VAR:
	  {
#if 0
	    const uint8_t *do_data = (const uint8_t *)do_p->obj;

	    flash_do_release (do_data);
#endif
	    if (len == 0)
	      /* make DO empty */
	      do_p->obj = NULL;
	    else
	      {
		do_p->obj = flash_do_write (tag, data, len);
		if (do_p->obj)
		  write_res_apdu (NULL, 0, 0x90, 0x00); /* success */
		else
		  write_res_apdu (NULL, 0, 0x65, 0x81); /* memory failure */
	      }
	    break;
	  }
	case DO_PROC_READWRITE:
	  {
	    void (*rw_func)(uint16_t, uint8_t *, int, int)
	      = (void (*)(uint16_t, uint8_t *, int, int))do_p->obj;

	    rw_func (tag, data, len, 1);
	    break;
	  }
	case DO_PROC_WRITE:
	  {
	    void (*proc_func)(uint16_t, uint8_t *, int)
	      = (void (*)(uint16_t, uint8_t *, int))do_p->obj;

	    proc_func (tag, data, len);
	    break;
	  }
	}
    }
  else
    /* No record */
    write_res_apdu (NULL, 0, 0x6a, 0x88);
}
