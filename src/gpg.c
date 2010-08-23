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

static void
put_hex (uint8_t nibble)
{
  uint8_t c;

  if (nibble < 0x0a)
    c = '0' + nibble;
  else
    c = 'a' + nibble - 0x0a;

  _write (&c, 1);
}

void
put_byte (uint8_t b)
{
  put_hex (b >> 4);
  put_hex (b &0x0f);
  _write ("\r\n", 2);
}

void
put_string (const char *s)
{
  _write (s, strlen (s));
}


#define RSA_SIGNATURE_LENGTH 128 /* 256 byte == 2048-bit */
extern unsigned char *rsa_sign (unsigned char *);

#define INS_PUT_DATA      0xDA
#define INS_VERIFY        0x20
#define INS_GET_DATA      0xCA
#define INS_GET_RESPONSE  0xC0
#define INS_SELECT_FILE   0xA4
#define INS_READ_BINARY   0xB0
#define INS_PGP_GENERATE_ASYMMETRIC_KEY_PAIR 0x47
#define INS_PSO		  0x2A

extern const char const select_file_TOP_result[20];
extern const char const get_data_64_result[7];
extern const char const get_data_5e_result[6];
extern const char const do_6e_head[3];
extern const char const do_47[2+3];
extern const char const do_4f[2+16];
extern const char const do_c0[2+1];
extern const char const do_c1[2+1];
extern const char const do_c2[2+1];
extern const char const do_c3[2+1];
extern const char const do_c4[2+7];
extern const char const do_c5[2+60];
extern const char const do_c6[2+60];
extern const char const do_cd[2+12];
extern const char const do_65_head[2];
extern const char const do_5b[2+12];
extern const char const do_5f2d[3+2];
extern const char const do_5f35[3+1];
extern const char const do_7a_head[2];
extern const char const do_93[2+3];
extern const char const do_5f50[3+20];
extern const char const do_5f52[3+10];
extern const char const get_data_rb_result[6];
extern const char const get_data_sigkey_result[7+128];
extern const char const get_data_enckey_result[7+128];


/*
 * 73
 * 101
 * 102
 * 103
 * 104
 *
 * 65 - 5b, 5f2d, 5f35
 * 6e - 47, 4f, c0, c1, c2, c3, c4, c5, c6, cd
 * 7a - 93
 *
 *
 * 65 L-65 [5b L-5b .... ] [5f2d 2 'j' 'a'] [5f35 1 '1']
 * 6e L-6e [47 3 x x x ] [4f L-4f ...] [c0 L-c0 ...] ...
 * 7a L-7a [93 L-93 ... ]
 */

static void
write_res_apdu (const uint8_t *p, int len, uint8_t sw1, uint8_t sw2)
{
  res_APDU_size = 2 + len;
  if (len)
    memcpy (res_APDU, p, len);
  res_APDU[len] = sw1;
  res_APDU[len+1] = sw2;
}

static int
process_command_apdu (void)
{
  /*
INS_VERIFY

 00 20 00 81 06 - ???
          CHV1
 00 20 00 82 06 - ???
          CHV2
 00 20 00 83 08 - ???
          CHV3
  */

  if (cmd_APDU[1] == INS_PUT_DATA)
    {
      put_string (" - PUT DATA\r\n");
      write_res_apdu (NULL, 0, 0x90, 0x00); /* 0x6a, 0x88: No record */
      return 0;
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
    {				/* it must be for DF 0x2f02 */
      put_string (" - Read binary\r\n");

      if (cmd_APDU[3] >= 6)
	{
	  write_res_apdu (NULL, 0, 0x6b, 0x00); /* BAD_P0_P1 */
	}
      else
	{			/* Tag 5a, serial number */
	  write_res_apdu (get_data_rb_result,
			  sizeof (get_data_rb_result), 0x90, 0x00);
	}
    }
  else if (cmd_APDU[1] == INS_SELECT_FILE)
    {
      if (cmd_APDU[2] == 4)	/* Selection by DF name */
	{
	  put_string (" - select DF by name\r\n");
	  /*
	   * XXX: Should return contents.
	   */

	  if (1)
	    {
	      write_res_apdu (NULL, 0, 0x90, 0x00);
	    }
	}
      else if (cmd_APDU[4] == 2
	      && cmd_APDU[5] == 0x2f
	      && cmd_APDU[6] == 02)
	{
	  put_string (" - select 0x2f02 EF\r\n");
	  /*
	   * MF.EF-GDO -- Serial number of the card and name of the owner
	   */
	  write_res_apdu (NULL, 0, 0x90, 0x00);
	}
      else
	if (cmd_APDU[4] == 2
	    && cmd_APDU[5] == 0x3f
	    && cmd_APDU[6] == 0)
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
	  }
	else
	  {
	    put_string (" - select ?? \r\n");

	    write_res_apdu (NULL, 0, 0x6a, 0x82); /* File missing */
	  }
    }
  else if (cmd_APDU[1] == INS_GET_DATA)
    {
      put_string (" - Get Data\r\n");

      switch (((cmd_APDU[2]<<8) | cmd_APDU[3]))
	{
	case 0x4f:		/* AID */
	    {
	      put_string ("   AID\r\n");
	      write_res_apdu (&do_4f[2],
			      sizeof (do_4f) - 2, 0x90, 0x00);
	      break;
	    }
	case 0x5e:		/* Login data */
	    {
	      put_string ("   Login data\r\n");
	      write_res_apdu (get_data_5e_result,
			      sizeof (get_data_5e_result), 0x90, 0x00);
	      break;
	    }
	case 0x64:
	    {
	      put_string ("   64\r\n");
	      write_res_apdu (get_data_64_result,
			      sizeof (get_data_64_result), 0x90, 0x00);
	      break;
	    }
	case 0xc0:
	    {
	      put_string ("   c0\r\n");
	      write_res_apdu (&do_c0[2],
			      sizeof (do_c0) - 2, 0x90, 0x00);
	      break;
	    }
	case 0xc1:
	    {
	      put_string ("   c1\r\n");
	      write_res_apdu (&do_c1[2],
			      sizeof (do_c1) - 2, 0x90, 0x00);
	      break;
	    }
	case 0xc2:
	    {
	      put_string ("   c2\r\n");
	      write_res_apdu (&do_c2[2],
			      sizeof (do_c2) - 2, 0x90, 0x00);
	      break;
	    }
	case 0xc3:
	    {
	      put_string ("   c3\r\n");
	      write_res_apdu (&do_c3[2],
			      sizeof (do_c3) - 2, 0x90, 0x00);
	      break;
	    }
	case 0xc4:
	    {
	      put_string ("   c4\r\n");
	      write_res_apdu (&do_c4[2],
			      sizeof (do_c4) - 2, 0x90, 0x00);
	      break;
	    }
	case 0x5b:		/* Name */
	    {
	      put_string ("   5b\r\n");
	      write_res_apdu (&do_5b[2],
			      sizeof (do_5b) - 2, 0x90, 0x00);
	      break;
	    }
	case 0x93:		/* Digital Signature Counter (3-bytes) */
	    {
	      put_string ("   93\r\n");
	      write_res_apdu (&do_93[2],
			      sizeof (do_93) - 2, 0x90, 0x00);
	      break;
	    }
	case 0xc5:		/* Fingerprints */
	    {
	      put_string ("   c5\r\n");
	      write_res_apdu (&do_c5[2],
			      sizeof (do_c5) - 2, 0x90, 0x00);
	      break;
	    }
	case 0x5f2d:		/* Language preference */
	    {
	      put_string ("   5f2d\r\n");
	      write_res_apdu (&do_5f2d[3],
			      sizeof (do_5f2d) - 3, 0x90, 0x00);
	      break;
	    }
	case 0x5f35:		/* Sex */
	    {
	      put_string ("   5f35\r\n");
	      write_res_apdu (&do_5f35[3],
			      sizeof (do_5f35) - 3, 0x90, 0x00);
	      break;
	    }
	case 0x5f50:		/* URL */
	    {
	      put_string ("   5f50\r\n");
	      write_res_apdu (&do_5f50[3],
			      sizeof (do_5f50) - 3, 0x90, 0x00);
	      break;
	    }
	case 0x5f52:		/* Historycal bytes */
	    {
	      put_string ("   5f52\r\n");
	      write_res_apdu (&do_5f52[3],
			      sizeof (do_5f52) - 3, 0x90, 0x00);
	      break;
	    }
	case 0x65:		/* Card Holder Related Data (Tag) */
	    {
	      put_string ("   65\r\n");
	      write_res_apdu (do_65_head,
			      do_65_head[1] + 2, 0x90, 0x00);
	      break;
	    }
	case 0x6e:		/* Application Related Data (Tag) */
	    {
	      put_string ("   6e\r\n");
	      write_res_apdu (do_6e_head,
			      do_6e_head[2] + 3, 0x90, 0x00);
	      break;
	    }
	case 0x7a:		/* Security Support Template (Tag) */
	    {
	      put_string ("   7a\r\n");
	      write_res_apdu (do_7a_head,
			      do_7a_head[1] + 2, 0x90, 0x00);
	      break;
	    }
	case 0xc6:		/* List of CA fingerprints */
	    {
	      put_string ("   c6\r\n");
	      write_res_apdu (&do_c6[2],
			      sizeof (do_c6) - 2, 0x90, 0x00);
	      break;
	    }
	case 0xcd:		/* List of generation dates/times public-key pairs */
	    {
	      put_string ("   cd\r\n");
	      write_res_apdu (&do_cd[2],
			      sizeof (do_cd) - 2, 0x90, 0x00);
	      break;
	    }
	default:
	  put_string ("   ?");
	  put_byte (((cmd_APDU[2]<<8) | cmd_APDU[3]));
	  write_res_apdu (NULL, 0, 0x90, 0x00);
	  break;
	}
    }
  else if (cmd_APDU[1] == INS_PSO)
    {
      put_string (" - PSO\r\n");

      if (cmd_APDU[2] == 0x9E && cmd_APDU[3] == 0x9A)
	{
	  if (cmd_APDU_size != 5 + 35 && cmd_APDU_size != 5 + 35 + 1)
	    put_string (" wrong length\r\n");
	  else
	    {
	      unsigned char * r = rsa_sign (&cmd_APDU[5]);
	      write_res_apdu (r, RSA_SIGNATURE_LENGTH, 0x90, 0x00);
	    }

	  put_string ("done.\r\n");
	}
      else
	{
	  put_string (" - ???\r\n");
	  write_res_apdu (NULL, 0, 0x90, 0x00);
	}
    }
  else
    {
      put_string (" - ???\r\n");
      write_res_apdu (NULL, 0, 0x90, 0x00);
    }

  return 0;
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
