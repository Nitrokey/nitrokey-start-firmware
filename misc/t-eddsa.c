/*
 * t-eddsa.c - testing EdDSA
 * Copyright (C) 2014 Free Software Initiative of Japan
 * Author: NIIBE Yutaka <gniibe@fsij.org>
 *
 * Run following commands.  The file t-ed25519.inp is available in GNU
 * libgcrypt source code under 'tests' directory.

  gcc -Wall -c ecc-edwards.c
  gcc -Wall -c -DBN256_NO_RANDOM -DBN256_C_IMPLEMENTATION bn.c
  gcc -Wall -c mod.c
  gcc -Wall -c -DBN256_C_IMPLEMENTATION mod25638.c
  gcc -Wall -c sha512.c
  gcc -Wall -c t-eddsa.c
  gcc -o t-eddsa t-eddsa.o ecc-edwards.o bn.o mod.o mod25638.o sha512.o
  ./t-eddsa < ./t-ed25519.inp

 *
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "bn.h"
#include "affine.h"
#include "sha512.h"

static void
print_le_bn256 (const bn256 *X)
{
  int i;
  const uint8_t *p = (const uint8_t *)X;

  for (i = 0; i < 32; i++)
    printf ("%02x", p[i]);
  puts ("");
}

static void
print_be_bn256 (const bn256 *X)
{
  int i;

  for (i = 7; i >= 0; i--)
    printf ("%08x", X->word[i]);
  puts ("");
}

static void
print_point (const ac *X)
{
  int i;

  puts ("--");
  for (i = 7; i >= 0; i--)
    printf ("%08x", X->x->word[i]);
  puts ("");
  for (i = 7; i >= 0; i--)
    printf ("%08x", X->y->word[i]);
  puts ("");
  puts ("--");
}

extern void eddsa_25519 (bn256 *r, bn256 *s,
			 const uint8_t *input, size_t ilen, const bn256 *d);
#define MAXLINE 4096

static int lineno;
static int test_no;
static bn256 sk[1];
static int pk_is_compressed;
static ac pk[1];
static unsigned char msg[MAXLINE];
static size_t msglen;
static bn512 sig[1];

static const char *
skip_white_space (const char *l)
{
  while (*l != '\n' && isspace (*l))
    l++;

  return l;
}


static int
read_hex_4bit (char c)
{
  int r;

  if (c >= '0' && c <= '9')
    r = c - '0';
  else if (c >= 'a' && c <= 'f')
    r = c - 'a' + 10;
  else if (c >= 'A' && c <= 'F')
    r = c - 'A' + 10;
  else
    r = -1;
  return r;
}

static int
read_hex_8bit (const char **l_p)
{
  const char *l = *l_p;
  int r, v;

  r = read_hex_4bit (*l++);
  if (r < 0)
    return -1;
  v = r*16;
  r = read_hex_4bit (*l++);
  if (r < 0)
    return -1;
  v += r;

  *l_p = l;
  return v;
}

static int
read_msg (unsigned char *msg, const char *l, int len)
{
  int i, r;

  for (i = 0; i < len; i++)
    {
      r = read_hex_8bit (&l);
      if (r < 0)
	return -1;
      msg[i] = r;
    }

  return 0;
}


static int
read_le_bn256 (bn256 *sk, const char *l)
{
  int i;
  uint8_t *p = (uint8_t *)sk;

  for (i = 0; i < sizeof (bn256); i++)
    {
      int r;

      if (*l == '\n')
	{
	  /* should support small input??? */
	  return -1;
	}

      r = read_hex_8bit (&l);
      if (r < 0)
	return -1;

      p[i] = r;
    }

  return 0;
}

static int
read_be_bn256 (bn256 *sk, const char *l)
{
  int i;
  uint8_t *p = (uint8_t *)sk;

  for (i = 0; i < sizeof (bn256); i++)
    {
      int r;

      if (*l == '\n')
	{
	  /* should support small input??? */
	  return -1;
	}

      r = read_hex_8bit (&l);
      if (r < 0)
	return -1;

      p[31 - i] = r;
    }

  return 0;
}


static int
read_pk (ac *pk, const char *l, int len)
{
  int r;

  if (len == 64)		/* 64 chars == 32-byte */
    {				/* compressed form */
      r = read_le_bn256 (pk->y, l);
      if (r < 0)
	return -1;
      return 1;
    }
  else
    {
      r = read_hex_8bit (&l);
      if (r < 0)
	return -1;
      if (r != 4)
	return -1;

      r = read_be_bn256 (pk->x, l);
      if (r < 0)
	return -1;
      r = read_be_bn256 (pk->y, l+64);
      if (r < 0)
	return -1;

      return 0;
    }
}

static int
read_le_bn512 (bn512 *sig, const char *l)
{
  int i;
  uint8_t *p = (uint8_t *)sig;

  for (i = 0; i < sizeof (bn512); i++)
    {
      int r;

      if (*l == '\n')
	{
	  /* should support small input??? */
	  return -1;
	}

      r = read_hex_8bit (&l);
      if (r < 0)
	return -1;

      p[i] = r;
    }

  return 0;
}

static int
read_testcase (void)
{
  ssize_t r;
  size_t len = 0;
  char *line = NULL;
  int start = 0;
  int err = 0;

  test_no = 0;
  memset (sk, 0, sizeof (bn256));
  pk_is_compressed = 0;
  memset (pk, 0, sizeof (ac));
  msglen = 0;
  memset (sig, 0, sizeof (bn512));

  while (1)
    {
      lineno++;
      r = getline (&line, &len, stdin);
      if (r < 0)
	{
	  /* EOF */
	  if (!start)
	    err = -1;
	  break;
	}
      len = r;	       /* We don't need allocated size, but length.  */
      if (len >= MAXLINE)
	{
	  fprintf (stderr, "Line too long: %d: >= %d\n", lineno, MAXLINE);
	  err = -1;
	  break;
	}

      if (r == 1 && *line == '\n')
	{
	  if (start)
	    break;		/* Done. */
	  else
	    continue; /* Ignore blank line before start.  */
	}

      if (r > 0 && *line == '#') /* Ignore comment line.  */
	continue;

      start = 1;
      if (r > 4 && strncmp (line, "TST:", 4) == 0)
	test_no = strtol (line+4, NULL, 10);
      else if (r > 3 && strncmp (line, "SK:", 3) == 0)
	{
	  const char *l = skip_white_space (line+3);
	  if (read_le_bn256 (sk, l) < 0)
	    {
	      fprintf (stderr, "read_le_bn256: %d\n", lineno);
	      err = -1;
	      break;
	    }
	}
      else if (r > 3 && strncmp (line, "PK:", 3) == 0)
	{
	  const char *l = skip_white_space (line+3);
	  pk_is_compressed = read_pk (pk, l, line+len-1-l);
	  if (pk_is_compressed < 0)
	    {
	      fprintf (stderr, "read_pk: %d\n", lineno);
	      err = -1;
	      break;
	    }
	}
      else if (r > 4 && strncmp (line, "MSG:", 4) == 0)
	{
	  const char *l = skip_white_space (line+4);
	  msglen = (line+len-1-l)/2;
	  if (read_msg (msg, l, msglen) < 0)
	    {
	      fprintf (stderr, "read_msg: %d\n", lineno);
	      err = -1;
	      break;
	    }
	}
      else if (r > 4 && strncmp (line, "SIG:", 4) == 0)
	{
	  const char *l = skip_white_space (line+4);
	  if (read_le_bn512 (sig, l) < 0)
	    {
	      fprintf (stderr, "read_le_bn512: %d\n", lineno);
	      err = -1;
	      break;
	    }
	}
      else
	{
	  fprintf (stderr, "Garbage line: %d", lineno);
	  err = -1;
	  break;
	}
    }

  free (line);
  return err;
}


int
main (int argc, char *argv[])
{
  int r;
  ac pk_calculated[1];
  uint8_t hash[64];
  bn256 a[1];
  extern int compute_kG_25519 (ac *X, const bn256 *K);
  extern int mod25519_is_neg (const bn256 *a);
  extern void eddsa_25519 (bn256 *r, bn256 *s, const uint8_t *input,
			   size_t ilen, const bn256 *d);

  bn256 R[1], S[1];

  while (1)
    {
      r = read_testcase ();
      if (r < 0)
	break;

      sha512 ((uint8_t *)sk, sizeof (bn256), hash);
      hash[0] &= 248;
      hash[31] &= 127;
      hash[31] |= 64;
      memcpy (a, hash, sizeof (bn256)); /* Lower half of hash */

      compute_kG_25519 (pk_calculated, a);
      if (pk_is_compressed)
	{
	  /* EdDSA encoding.  */
	  pk_calculated->y->word[7] ^= 
	    mod25519_is_neg (pk_calculated->x) * 0x80000000;
	  r = memcmp (pk->y, pk_calculated->y, sizeof (bn256));
	}
      else
	{
	  r = memcmp (pk, pk_calculated, sizeof (ac));
	}
      if (r != 0)
	{
	  printf ("ERR PK: %d\n", test_no);
	  print_be_bn256 (sk);
	  print_point (pk);
	  print_point (pk_calculated);
	  continue;
	}

      eddsa_25519 (R, S, msg, msglen, sk);
      if (memcmp (sig, R, sizeof (bn256)) != 0
	  || memcmp (((const uint8_t *)sig)+32, S, sizeof (bn256)) != 0)
	{
	  printf ("ERR SIG: %d\n", test_no);
	  print_le_bn256 (R);
	  print_le_bn256 (S);
	  print_le_bn256 ((const bn256 *)sig);
	  print_le_bn256 ((const bn256 *)(((const uint8_t *)sig)+32));
	  continue;
	}

      printf ("%d\n", test_no);
    }
  return 0;
}
