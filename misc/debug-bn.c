/*
 * debug-bn.c - Debug Bignum
 * Copyright (C) 2014 Free Software Initiative of Japan
 * Author: NIIBE Yutaka <gniibe@fsij.org>
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "bn.h"

void
print_le_bn256 (const bn256 *X)
{
  int i;
  const uint8_t *p = (const uint8_t *)X;

  for (i = 0; i < 32; i++)
    printf ("%02x", p[i]);
  puts ("");
}

void
print_be_bn256 (const bn256 *X)
{
  int i;

  for (i = 7; i >= 0; i--)
    printf ("%08x", X->word[i]);
  puts ("");
}

#define MAXLINE 4096

static int lineno;
static int test_no;
static bn256 sk[1];
static bn256 pk[1];
static unsigned char msg[MAXLINE];
static size_t msglen;
static bn512 sig[1];

const char *
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
read_pk (bn256 *pk, const char *l, int len)
{
  int r;

  if (len == 64)		/* 64 chars == 32-byte */
    {				/* compressed form */
      r = read_le_bn256 (pk, l);
      if (r < 0)
	return -1;
      return 0;
    }
  else
    {
      bn256 x[1];

      r = read_hex_8bit (&l);
      if (r < 0)
	return -1;
      if (r != 4)
	return -1;

      r = read_be_bn256 (x, l);
      if (r < 0)
	return -1;
      r = read_be_bn256 (pk, l+64);
      if (r < 0)
	return -1;

      pk->word[7] ^= (x->word[0] & 1) * 0x80000000;
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
