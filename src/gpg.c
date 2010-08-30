/*
 * gpg.c -- OpenPGP card protocol support 
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

#include "ch.h"
#include "hal.h"
#include "gnuk.h"

#define RSA_SIGNATURE_LENGTH 256 /* 256 byte == 2048-bit */
extern unsigned char *rsa_sign (unsigned char *);

#define INS_PUT_DATA      0xDA
#define INS_PUT_DATA_ODD  0xDB	/* For key import */
#define INS_VERIFY        0x20
#define INS_GET_DATA      0xCA
#define INS_GET_RESPONSE  0xC0
#define INS_SELECT_FILE   0xA4
#define INS_READ_BINARY   0xB0
#define INS_PGP_GENERATE_ASYMMETRIC_KEY_PAIR 0x47
#define INS_PSO		  0x2A

extern const char const select_file_TOP_result[20];
extern const char const get_data_rb_result[6];
extern const char const get_data_sigkey_result[7+128];
extern const char const get_data_enckey_result[7+128];


void
write_res_apdu (const uint8_t *p, int len, uint8_t sw1, uint8_t sw2)
{
  res_APDU_size = 2 + len;
  if (len)
    memcpy (res_APDU, p, len);
  res_APDU[len] = sw1;
  res_APDU[len+1] = sw2;
}

#define FILE_NONE	-1
#define FILE_DF_OPENPGP	0
#define FILE_MF		1
#define FILE_EF_DIR	2
#define FILE_EF_SERIAL	3

static int file_selection = FILE_NONE;

static void
process_command_apdu (void)
{
  uint16_t tag;
  uint8_t *data;
  int len;

  if (cmd_APDU[1] == INS_VERIFY)
    {
      uint8_t p2 = cmd_APDU[3];
      int r;

      put_string (" - VERIFY\r\n");

      len = cmd_APDU[4];
      if (p2 == 0x81)
	r = verify_pso_cds (&cmd_APDU[5], len);
      else if (p2 == 0x82)
	r = verify_pso_other (&cmd_APDU[5], len);
      else
	r = verify_pso_admin (&cmd_APDU[5], len);

      if (r < 0)
	write_res_apdu (NULL, 0, 0x69, 0x82);
      else
	write_res_apdu (NULL, 0, 0x90, 0x00);
      return;
    }

  if (cmd_APDU[1] == INS_PUT_DATA || cmd_APDU[1] == INS_PUT_DATA_ODD)
    {
      put_string (" - PUT DATA\r\n");

      if (file_selection != FILE_DF_OPENPGP)
	write_res_apdu (NULL, 0, 0x6a, 0x88); /* No record */

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
      return;
    }

  if (cmd_APDU[1] == INS_PGP_GENERATE_ASYMMETRIC_KEY_PAIR)
    {
      put_string (" - Generate Asymmetric Key Pair\r\n");

      if (cmd_APDU[2] == 0x81)
	{
	  /*
	   * tag: 0x7f49 public key data
	   * tag: 0x0081 RSA modulus
	   * tag: 0x0082 RSA exponent
	   *
	   *  TAG
	   * [0x7f 0x49][LEN][DATA]
	   *          _______/    \_________________
	   *         /				    \
	   *         [0x81][128][DATA][0x82][3][DATA]
	   *               __/ \__               0x01, 0x00, 0x01
	   *              /	  \
	   *              0x81 0x80
	   */

	  if (cmd_APDU[6] == 0x00 && cmd_APDU[5] == 0xb6)
	    {			/* Key for Sign */
	      write_res_apdu (get_data_sigkey_result,
			      sizeof (get_data_sigkey_result), 0x90, 0x00);
	    }
	  else if (cmd_APDU[6] == 0x00 && cmd_APDU[5] == 0xb8)
	    {			/* Key for Encryption */
	      write_res_apdu (get_data_enckey_result,
			      sizeof (get_data_enckey_result), 0x90, 0x00);
	    }
	  /*  cmd_APDU[5] == 0xa4 */
	  else
	    {
	      write_res_apdu (NULL, 0, 0x6a, 0x88); /* No record */
	    }
	}
      else
	{
	  write_res_apdu (NULL, 0, 0x6a, 0x88); /* No record */
	}
    }
  else if (cmd_APDU[1] == INS_READ_BINARY)
    {
      put_string (" - Read binary\r\n");

      if (file_selection == FILE_EF_SERIAL)
	{
	  if (cmd_APDU[3] >= 6)
	    write_res_apdu (NULL, 0, 0x6b, 0x00); /* BAD_P0_P1 */
	  else
	    /* Tag 5a, serial number */
	    write_res_apdu (get_data_rb_result,
			    sizeof (get_data_rb_result), 0x90, 0x00);
	}
      else
	write_res_apdu (NULL, 0, 0x6a, 0x88); /* No record */
    }
  else if (cmd_APDU[1] == INS_SELECT_FILE)
    {
      if (cmd_APDU[2] == 4)	/* Selection by DF name */
	{
	  put_string (" - select DF by name\r\n");

	  /*
	   * P2 == 0, LC=6, name = D2 76 00 01 24 01
	   */

	  file_selection = FILE_DF_OPENPGP;

	  /* XXX: Should return contents??? */
	  write_res_apdu (NULL, 0, 0x90, 0x00);
	}
      else if (cmd_APDU[4] == 2
	      && cmd_APDU[5] == 0x2f
	      && cmd_APDU[6] == 0x02)
	{
	  put_string (" - select 0x2f02 EF\r\n");
	  /*
	   * MF.EF-GDO -- Serial number of the card and name of the owner
	   */
	  write_res_apdu (NULL, 0, 0x90, 0x00);
	  file_selection = FILE_EF_SERIAL;
	}
      else if (cmd_APDU[4] == 2
	       && cmd_APDU[5] == 0x3f
	       && cmd_APDU[6] == 0x00)
	{
	  put_string (" - select ROOT MF\r\n");
	  if (cmd_APDU[3] == 0x0c)
	    {
	      write_res_apdu (NULL, 0, 0x90, 0x00);
	    }
	  else
	    {
	      write_res_apdu (select_file_TOP_result,
			      sizeof (select_file_TOP_result), 0x90, 0x00);
	    }

	  file_selection = FILE_MF;
	}
      else
	{
	  put_string (" - select ?? \r\n");

	  write_res_apdu (NULL, 0, 0x6a, 0x82); /* File missing */
	  file_selection = FILE_NONE;
	}
    }
  else if (cmd_APDU[1] == INS_GET_DATA)
    {
      tag = ((cmd_APDU[2]<<8) | cmd_APDU[3]);

      put_string (" - Get Data\r\n");

      if (file_selection != FILE_DF_OPENPGP)
	write_res_apdu (NULL, 0, 0x6a, 0x88); /* No record */

      gpg_do_get_data (tag);
    }
  else if (cmd_APDU[1] == INS_PSO)
    {
      put_string (" - PSO\r\n");

      if (cmd_APDU[2] == 0x9E && cmd_APDU[3] == 0x9A)
	{
	  if (cmd_APDU_size != 8 + 35 && cmd_APDU_size != 8 + 35 + 1)
	    /* Extended Lc: 3-byte */
	    {
	      put_string (" wrong length: ");
	      put_short (cmd_APDU_size);
	    }
	  else
	    {
	      unsigned char * r = rsa_sign (&cmd_APDU[7]);
	      write_res_apdu (r, RSA_SIGNATURE_LENGTH, 0x90, 0x00);
	    }

	  put_string ("done.\r\n");
	}
      else
	{
	  put_string (" - ??");
	  put_byte (cmd_APDU[2]);
	  put_string (" - ??");
	  put_byte (cmd_APDU[3]);
	  write_res_apdu (NULL, 0, 0x90, 0x00);
	}
    }
  else
    {
      put_string (" - ??");
      put_byte (cmd_APDU[1]);
      write_res_apdu (NULL, 0, 0x6D, 0x00); /* INS not supported. */
    }
}

Thread *gpg_thread;

msg_t
GPGthread (void *arg)
{
  (void)arg;

  gpg_thread = chThdSelf ();
  chEvtClear (ALL_EVENTS);

  while (1)
    {
      eventmask_t m;

      m = chEvtWaitOne (ALL_EVENTS);

      _write ("GPG!\r\n", 6);

      process_command_apdu ();

      chEvtSignal (icc_thread, EV_EXEC_FINISHED);
    }

  return 0;
}
