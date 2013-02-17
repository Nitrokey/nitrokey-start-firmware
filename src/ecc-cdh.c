/*
 * ecc-cdh.c - One-Pass Diffie-Hellman method implementation
 *             C(1, 1, ECC CDH) for EC DH of OpenPGP ECC
 *
 * Copyright (C) 2013 Free Software Initiative of Japan
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

/*
 * References:
 *
 * [1] A. Jivsov, Elliptic Curve Cryptography (ECC) in OpenPGP, RFC 6637,
 *     June 2012.
 *
 * [2] Suite B Implementer's Guide to NIST SP 800-56A, July 28, 2009.
 *
 */

static const char param[] = {
  /**/
  curve_OID_len,
  curve_OID,
  public-key_alg_ID, /*ecdh*/
  0x03,
  0x01,
  KDF_hash_ID, /*sha256*/
  KEK_alg_ID, /*aes128*/
  "Anonymous Sender    ",
  my_finger_print /*20-byte*/
};

/*
 *
 */
int
ecdh (unsigned char *key,
      const unsigned char *key_encrypted, const ac *P,
      const naf4_257 *naf_d, const unsigned char *fp)
{
  ac S[1];
  sha256_context ctx;
  unsigned char kek[32];

  compute_kP (S, naf_d, P);	/* Get shared key.  */

  /* kdf (kek, S, parameter) */
  sha256_start (&ctx);
  sha256_update (&ctx, "\x00\x00\x00\x01", 4);
  sha256_update (&ctx, (const char *)S, size of S); /* XXX 04, X, Y bigendian!! */
  sha256_update (&ctx, (const char *)param, size of param);
  sha256_finish (&ctx, kek);
}
