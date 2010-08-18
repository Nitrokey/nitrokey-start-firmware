/*
 * gpg.c -- 
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

#define RSA_SIGNATURE_LENGTH 128 /* 128 byte == 1024-bit */
extern unsigned char *rsa_sign (unsigned char *);

#define INS_PUT_DATA      0xDA
#define INS_VERIFY        0x20
#define INS_GET_DATA      0xCA
#define INS_GET_RESPONSE  0xC0
#define INS_SELECT_FILE   0xA4
#define INS_READ_BINARY   0xB0
#define INS_PGP_GENERATE_ASYMMETRIC_KEY_PAIR 0x47
#define INS_PSO		  0x2A

static const char const select_file_TOP_result[] =
  { 0x00, 0x00, 0x0b, 0x10, 0x3f, 0x00, 0x38, 0xff, 0xff, 0x44,
    0x44, 0x01, 0x05, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00 };

static const char const get_data_64_result[] =
  {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };

static const char const get_data_5e_result[] =
  {				/* Login Data */
    'g', 'n', 'i', 'i', 'b', 'e'
  };

/***** do_65 is compound object of { do_47, do_4f, do_c0,..,c6,cd }*/
const unsigned char const do_6e_head[] =
  {
    0x6e, 0x81, 2*10+3+16+1+1+1+1+7+60+60+12 /* (> 128) */
  };

const char const do_47[] = /* Card Capabilities */
  {
    0x47, 3,
    0x00 /*???*/, 0x00 /*???*/, 0x00 /*???*/
    /* XXX: See ISO 7816-4 for first byte and second byte */
  };

const char const do_4f[] = /* AID */
  {
    0x4f, 16,
    0xD2, 0x76, 0x00, 0x01, 0x24, 0x01,
    0x01, 0x01,			/* Version 1.1 */
    0xF5, 0x17,			/* Manufacturer (FSIJ) */
    0x00, 0x00, 0x00, 0x02,	/* Serial */
    0x00, 0x00
  };

const char const do_c0[] =
  {				/* Extended capability */
    0xc0, 1,
    0x00
  };

const char const do_c1[] =
  {				/* Algorithm Attributes Signature ??? */
    0xc1, 1,
    0x01, /* RSA */ /*??? should have length modulus, length exponent ??? */
  };

const char const do_c2[] =
  {				/* Algorithm Attributes Decryption ??? */
    0xc2, 1,
    0x00
  };

const char const do_c3[] =
  {				/* Algorithm Attributes Authentication ??? */
    0xc3, 1,
    0x00
  };

const char const do_c4[] =
  {				/* CHV status bytes */
    0xc4, 7,
    0x01, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01
  };

const char const do_c5[] =
  {
    0xc5, 60,
    /* sign */
    0x5b, 0x85, 0x67, 0x3c, 0x08, 0x4f, 0x80, 0x0d,
    0x54, 0xac, 0x95, 0x1c, 0x35, 0x15, 0x97, 0xcc,
    0xe5, 0x02, 0xbf, 0xcd,
    /* enc */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    /* auth */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
  };

const char const do_c6[] = /* CA Fingerprints */
  {
    0xc6, 60,
    /* c6 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    /* c7 */
    0x5b, 0x85, 0x67, 0x3c, 0x08, 0x4f, 0x80, 0x0d,
    0x54, 0xac, 0x95, 0x1c, 0x35, 0x15, 0x97, 0xcc,
    0xe5, 0x02, 0xbf, 0xcd,
    /* c8 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
  };

const char const do_cd[] =
  {				/* Generation time */
    0xcd, 12,
    0x00, 0x00, 0x00, 0x00,
    0x49, 0x8a, 0x50, 0x7a, /* 0xce */
    0x00, 0x00, 0x00, 0x00,
  };
/*************************/

/***** do_65 is compound object of { do_5b, do_5f2d, do_5f35 }*/
const char const do_65_head[] =
  {
    0x65, 2*1+3*2+12+2+1
  };

const char const do_5b[] = 
  {
    0x5b, 12,
    'N', 'I', 'I', 'B', 'E', ' ', 'Y', 'u', 't', 'a', 'k', 'a'
  };

const char const do_5f2d[] =
  {
    0x5f, 0x2d, 2,
    'j', 'a'
  };

const char const do_5f35[] =
  {
    0x5f, 0x35, 1,
    '1'
  };
/****************************/

/* do_7a is compound object of { do_93 } */
const char const do_7a_head[] =
  {
    0x7a, 2+3
  };

/* Digital Signature Counter (3-bytes) */
const char const do_93[] =
  {
    0x93, 3,
    0, 0, 0
  };
/****************************/


const char const do_5f50[] =
  {
    0x5f, 0x50, 20,
    'h', 't', 't', 'p', ':', '/', '/', 'w', 'w', 'w',
    '.', 'f', 's', 'i', 'j', '.', 'o', 'r', 'g', '/'
  };


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

static byte
process_command_adpu (void)
{
  if (icc_read_buf[1] == INS_GET_RESPONSE)
    {
      stx_put_string (" - GET Response\r\n");

      if ((icc_result_flag & ICC_RESULT_BUF))
	return 0;
      else
	{
	  stx_put_string ("Wrong GET Response\r\n");
	  return 1;
	}
    }

  icc_result_flag = 0;

  /*
INS_VERIFY

 00 20 00 81 06 - ???
          CHV1
 00 20 00 82 06 - ???
          CHV2
 00 20 00 83 08 - ???
          CHV3
  */

  if (icc_read_buf[1] == INS_PUT_DATA)
    {
      stx_put_string (" - PUT DATA\r\n");
      icc_result_value = 0x9000; /* 6a88: No record */
      icc_result_len = 0;
      icc_result_flag = 0;
      return 0;
    }

  if (icc_read_buf[1] == INS_PGP_GENERATE_ASYMMETRIC_KEY_PAIR)
    {
      stx_put_string (" - Generate Asymmetric Key Pair\r\n");

      if (icc_read_buf[2] == 0x81)
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

	  if (icc_read_buf[6] == 0x00 && icc_read_buf[5] == 0xb6)
	    {			/* Key for Sign */
	      static const char const get_data_sigkey_result[] =
		{
		  0x7f, 0x49, 0x81, 0x88,
		  0x81, 0x81, 0x80,
		  /* 128-byte data */
		  /* modulus */
		  0xdb, 0xca, 0x58, 0x74, 0x44, 0x8e, 0x1a, 0x2c,
		  0xa0, 0x91, 0xac, 0xc4, 0xe2, 0x77, 0x2b, 0x90,
		  0xcf, 0x3c, 0x7e, 0x81, 0xdc, 0x8d, 0xb0, 0xe2,
		  0xf1, 0xfe, 0x56, 0x7e, 0x54, 0x57, 0xf0, 0xd8,
		  0xb1, 0xb1, 0xaa, 0x9d, 0x8f, 0xb0, 0x56, 0x01,
		  0xaa, 0x6b, 0xa7, 0x2e, 0xce, 0x01, 0x20, 0xd2,
		  0xf8, 0xf5, 0x85, 0x3a, 0xc2, 0x73, 0xf9, 0x66,
		  0x30, 0x28, 0x65, 0x5e, 0x3f, 0x91, 0xaf, 0x3f,
		  0xf6, 0x1c, 0x31, 0x2f, 0xa2, 0x91, 0xbb, 0x41,
		  0x91, 0x41, 0x08, 0x0a, 0xc5, 0x3e, 0x39, 0xda,
		  0x2f, 0x6f, 0x58, 0x51, 0xe2, 0xd2, 0xe9, 0x42,
		  0x8a, 0x7b, 0x72, 0x7b, 0x15, 0xf6, 0xf6, 0x6a,
		  0x12, 0x6e, 0x0c, 0x15, 0x24, 0x13, 0x16, 0x55,
		  0x3a, 0xf1, 0xa7, 0x16, 0x3e, 0xe9, 0xc8, 0x3d,
		  0x2c, 0x3d, 0xae, 0x51, 0x2d, 0x7f, 0xef, 0x92,
		  0x25, 0x6a, 0xbb, 0x02, 0x03, 0x70, 0x45, 0x3d,
		  /* public exponent */
		  0x82, 3, 0x01, 0x00, 0x01
		};

	      icc_result_value = get_data_sigkey_result;
	      icc_result_len = sizeof (get_data_sigkey_result);
	      icc_result_flag = ICC_RESULT_BUF | ICC_RESULT_BUF_ROM;
	    }
	  else if (icc_read_buf[6] == 0x00 && icc_read_buf[5] == 0xb8)
	    {			/* Key for Encryption */
	      static const char const get_data_enckey_result[] =
		{
		  0x7f, 0x49, 0x81, 0x88,
		  0x81, 0x81, 0x80,
		  /* 128-byte data */
		  0xB2, 0x19, 0x91, 0x42, 0x27, 0xC7, 0x97, 0xFE, 
		  0x92, 0x64, 0x42, 0xCA, 0xE3, 0x66, 0x4D, 0xD0, 
		  0x31, 0xE4, 0x10, 0x31, 0x0F, 0xC7, 0x07, 0x4A, 
		  0xAA, 0x6D, 0x31, 0xA2, 0x88, 0x68, 0xAF, 0x45, 
		  0x8E, 0x42, 0x12, 0xFF, 0xB6, 0xEF, 0x6E, 0x54, 
		  0x7E, 0x51, 0x8E, 0xBC, 0xE8, 0x18, 0x79, 0xA7, 
		  0xBC, 0xA8, 0x14, 0x8B, 0xE7, 0x91, 0x57, 0x38, 
		  0xCE, 0x4F, 0x6E, 0x16, 0x48, 0xCB, 0xD6, 0x0B, 
		  0x3A, 0x53, 0x70, 0xF3, 0xFC, 0xFA, 0xC3, 0x58, 
		  0x3D, 0xE7, 0x2A, 0x5E, 0xDD, 0xE1, 0x38, 0x82, 
		  0x57, 0x87, 0x3A, 0xDC, 0x34, 0xDE, 0xCD, 0x5D, 
		  0x33, 0x1C, 0xAB, 0xB0, 0x1B, 0xEE, 0x82, 0x43, 
		  0x7B, 0xAC, 0xF8, 0xF0, 0xB2, 0x62, 0xB2, 0x6D, 
		  0x09, 0xED, 0x2E, 0xD1, 0xBA, 0xB8, 0xC6, 0x96, 
		  0xFA, 0x3E, 0xB4, 0xE3, 0xFE, 0x68, 0xF9, 0x51, 
		  0x9A, 0x8C, 0x8B, 0x20, 0x93, 0xD0, 0x2E, 0x0F,
		  0x82, 3, 0x01, 0x00, 0x01
		};

	      icc_result_value = get_data_enckey_result;
	      icc_result_len = sizeof (get_data_enckey_result);
	      icc_result_flag = ICC_RESULT_BUF | ICC_RESULT_BUF_ROM;
	    }
	  /*  icc_read_buf[5] == 0xa4 */
	  else
	    {
	      icc_result_value = 0x6a88; /* No record */
	      icc_result_len = 0;
	      icc_result_flag = 0;
	    }
	}
      else
	{
	  icc_result_value = 0x6a88; /* No record */
	  icc_result_len = 0;
	  icc_result_flag = 0;
	}
    }
  else if (icc_read_buf[1] == INS_READ_BINARY)
    {				/* it must be for DF 0x2f02 */
      stx_put_string (" - Read binary\r\n");

      if (icc_read_buf[3] >= 6)
	{
	  icc_result_value = 0x6b00; /* BAD_P0_P1 */
	  icc_result_len = 0;
	  icc_result_flag = 0;
	}
      else
	{			/* Tag 5a, serial number */
	  static const char const get_data_rb_result[] = { 0x5a, 0x4, 0x01, 0x02, 0x03, 0x04 };

	  icc_result_value = (word)get_data_rb_result;
	  icc_result_len = sizeof (get_data_rb_result);
	  icc_result_flag = ICC_RESULT_BUF | ICC_RESULT_BUF_ROM;

	  /* XXX: Next get response returns 6282??? eof_reached ??? */
	}
    }
  else if (icc_read_buf[1] == INS_SELECT_FILE)
    {
      if (icc_read_buf[2] == 4)	/* Selection by DF name */
	{
	  stx_put_string (" - select DF by name\r\n");
	  /*
	   * XXX: Should return contents.
	   */

	  if (1)
	    {
	      icc_result_value = 0x9000;
	      icc_result_len = 0;
	      icc_result_flag = 0;
	    }
	}
      else if (icc_read_buf[4] == 2
	      && icc_read_buf[5] == 0x2f
	      && icc_read_buf[6] == 02)
	{
	  stx_put_string (" - select 0x2f02 EF\r\n");
	  /*
	   * MF.EF-GDO -- Serial number of the card and name of the owner
	   */
	  icc_result_value = 0x9000;
	  icc_result_len = 0;
	  icc_result_flag = 0;
	}
      else
	if (icc_read_buf[4] == 2
	    && icc_read_buf[5] == 0x3f
	    && icc_read_buf[6] == 0)
	  {
	    stx_put_string (" - select ROOT MF\r\n");
	    if (icc_read_buf[3] == 0x0c)
	      {
		icc_result_value = 0x9000;
		icc_result_len = 0;
		icc_result_flag = 0;
	      }
	    else
	      {
		icc_result_value = select_file_TOP_result;
		icc_result_len = sizeof (select_file_TOP_result);
		icc_result_flag = ICC_RESULT_BUF | ICC_RESULT_BUF_ROM;
	      }
	  }
	else
	  {
	    stx_put_string (" - select ?? \r\n");

	    icc_result_value = 0x6a82; /* File missing */
	    icc_result_len = 0;
	    icc_result_flag = 0;
	  }
    }
  else if (icc_read_buf[1] == INS_GET_DATA)
    {
      stx_put_string (" - Get Data\r\n");

      switch (((icc_read_buf[2]<<8) | icc_read_buf[3]))
	{
	case 0x4f:		/* AID */
	    {
	      icc_result_value = (word)&do_4f[2];
	      icc_result_len = sizeof (do_4f) - 2;
	      icc_result_flag = ICC_RESULT_BUF | ICC_RESULT_BUF_ROM;
	      break;
	    }
	case 0x5e:		/* Login data */
	    {
	      icc_result_value = (word)get_data_5e_result;
	      icc_result_len = sizeof (get_data_5e_result);
	      icc_result_flag = ICC_RESULT_BUF | ICC_RESULT_BUF_ROM;
	      break;
	    }
	case 0x64:
	    {
	      icc_result_value = (word)get_data_64_result;
	      icc_result_len = sizeof (get_data_64_result);
	      icc_result_flag = ICC_RESULT_BUF | ICC_RESULT_BUF_ROM;
	      break;
	    }
	case 0xc0:
	    {
	      icc_result_value = (word)&do_c0[2];
	      icc_result_len = sizeof (do_c0) - 2;
	      icc_result_flag = ICC_RESULT_BUF | ICC_RESULT_BUF_ROM;
	      break;
	    }
	case 0xc4:
	    {
	      icc_result_value = (word)&do_c4[2];
	      icc_result_len = sizeof (do_c4) - 2;
	      icc_result_flag = ICC_RESULT_BUF | ICC_RESULT_BUF_ROM;
	      break;
	    }
	case 0x5b:		/* Name */
	    {
	      icc_result_value = (word)&do_5b[2];
	      icc_result_len = sizeof (do_5b) - 2;
	      icc_result_flag = ICC_RESULT_BUF | ICC_RESULT_BUF_ROM;
	      break;
	    }
	case 0x93:		/* Digital Signature Counter (3-bytes) */
	    {
	      icc_result_value = (word)&do_93[2];
	      icc_result_len = sizeof (do_93) - 2;
	      icc_result_flag = ICC_RESULT_BUF | ICC_RESULT_BUF_ROM;
	      break;
	    }
	case 0xc5:		/* Fingerprints */
	    {
	      icc_result_value = &do_c5[2];
	      icc_result_len = sizeof (do_c5) - 2;
	      icc_result_flag = ICC_RESULT_BUF | ICC_RESULT_BUF_ROM;
	      break;
	    }
	case 0x5f2d:		/* Language preference */
	    {
	      icc_result_value = (word)&do_5f2d[3];
	      icc_result_len = sizeof (do_5f2d) - 3;
	      icc_result_flag = ICC_RESULT_BUF | ICC_RESULT_BUF_ROM;
	      break;
	    }
	case 0x5f35:		/* Sex */
	    {
	      icc_result_value = (word)&do_5f35[3];
	      icc_result_len = sizeof (do_5f35) - 3;
	      icc_result_flag = ICC_RESULT_BUF | ICC_RESULT_BUF_ROM;
	      break;
	    }
	case 0x5f50:		/* URL */
	    {
	      icc_result_value = (word)&do_5f50[3];
	      icc_result_len = sizeof (do_5f50) - 3;
	      icc_result_flag = ICC_RESULT_BUF | ICC_RESULT_BUF_ROM;
	      break;
	    }
	case 0x65:		/* Card Holder Related Data (Tag) */
	    {
	      icc_result_value = (word)do_65_head;
	      icc_result_len = do_65_head[1] + 2;
	      icc_result_flag = ICC_RESULT_BUF | ICC_RESULT_BUF_ROM;
	      break;
	    }
	case 0x6e:		/* Application Related Data (Tag) */
	    {
	      icc_result_value = (word)do_6e_head;
	      icc_result_len = do_6e_head[2] + 3;
	      icc_result_flag = ICC_RESULT_BUF | ICC_RESULT_BUF_ROM;
	      break;
	    }
	case 0x7a:		/* Security Support Template (Tag) */
	    {
	      icc_result_value = (word)do_7a_head;
	      icc_result_len = do_7a_head[1] + 2;
	      icc_result_flag = ICC_RESULT_BUF | ICC_RESULT_BUF_ROM;
	      break;
	    }
	case 0xc6:		/* List of CA fingerprints */
	case 0xcd:		/* List of generation dates/times public-key pairs */
	default:
	  icc_result_value = 0x9000;
	  icc_result_len = 0;
	  icc_result_flag = 0;
	}
    }
  else if (icc_read_buf[1] == INS_PSO)
    {
      stx_put_string (" - PSO\r\n");

      if (icc_read_buf[2] == 0x9E && icc_read_buf[3] == 0x9A)
	{
	  if (icc_read_len != 5 + 35 && icc_read_len != 5 + 35 + 1)
	    stx_put_string (" wrong length\r\n");
	  else
	    {
	      icc_result_value = rsa_sign (&icc_read_buf[5]);
	      icc_result_len = RSA_SIGNATURE_LENGTH;
	      icc_result_flag = ICC_RESULT_BUF | ICC_RESULT_BUF_RAM;
	    }

	  stx_put_string ("done.\r\n");
	}
      else
	{
	  stx_put_string (" - ???\r\n");
	  icc_result_value = 0x9000;
	  icc_result_len = 0;
	  icc_result_flag = 0;
	}
    }
  else
    {
      stx_put_string (" - ???\r\n");
      icc_result_value = 0x9000;
      icc_result_len = 0;
      icc_result_flag = 0;
    }

  return 0;
}
