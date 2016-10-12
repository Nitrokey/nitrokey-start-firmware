"""
test_personalize_card.py - test personalizing card

Copyright (C) 2016  g10 Code GmbH
Author: NIIBE Yutaka <gniibe@fsij.org>

This file is a part of Gnuk, a GnuPG USB Token implementation.

Gnuk is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Gnuk is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
"""

from struct import pack
from re import match, DOTALL
from util import *
import rsa_keys

FACTORY_PASSPHRASE_PW1=b"123456"
FACTORY_PASSPHRASE_PW3=b"12345678"
PW1_TEST0=b"another user pass phrase"
PW1_TEST1=b"PASSPHRASE SHOULD BE LONG"
PW1_TEST2=b"new user pass phrase"
PW1_TEST3=b"next user pass phrase"
PW1_TEST4=b"another user pass phrase"
PW3_TEST0=b"admin pass phrase"
PW3_TEST1=b"another admin pass phrase"

RESETCODE_TEST=b"example reset code 000"

def test_setup_pw3_0(card):
    r = card.cmd_change_reference_data(3, FACTORY_PASSPHRASE_PW3 + PW3_TEST0)
    assert r

def test_verify_pw3_0(card):
    v = card.cmd_verify(3, PW3_TEST0)
    assert v

def test_login_put(card):
    r = card.cmd_put_data(0x00, 0x5e, b"gpg_user")
    assert r

def test_name_put(card):
    r = card.cmd_put_data(0x00, 0x5b, b"GnuPG User")
    assert r

def test_lang_put(card):
    r = card.cmd_put_data(0x5f, 0x2d, b"ja")
    assert r

def test_sex_put(card):
    r = card.cmd_put_data(0x5f, 0x35, b"1")
    assert r

def test_url_put(card):
    r = card.cmd_put_data(0x5f, 0x50, b"https://www.fsij.org/gnuk/")
    assert r

def test_pw1_status_put(card):
    r = card.cmd_put_data(0x00, 0xc4, b"\x01")
    assert r

def test_login(card):
    login = get_data_object(card, 0x5e)
    assert login == b"gpg_user"

def test_name_lang_sex(card):
    name = b"GnuPG User"
    lang = b"ja"
    sex = b"1"
    expected = b'\x5b' + pack('B', len(name)) + name \
               +  b'\x5f\x2d' + pack('B', len(lang)) + lang \
               + b'\x5f\x35' + pack('B', len(sex)) + sex
    name_lang_sex = get_data_object(card, 0x65)
    assert name_lang_sex == expected

def test_url(card):
    url = get_data_object(card, 0x5f50)
    assert url == b"https://www.fsij.org/gnuk/"

def test_pw1_status(card):
    s = get_data_object(card, 0xc4)
    assert match(b'\x01...\x03[\x00\x03]\x03', s, DOTALL)

def test_rsa_import_key_1(card):
    t = rsa_keys.build_privkey_template(1, 0)
    r = card.cmd_put_data_odd(0x3f, 0xff, t)
    assert r

def test_rsa_import_key_2(card):
    t = rsa_keys.build_privkey_template(2, 1)
    r = card.cmd_put_data_odd(0x3f, 0xff, t)
    assert r

def test_rsa_import_key_3(card):
    t = rsa_keys.build_privkey_template(3, 2)
    r = card.cmd_put_data_odd(0x3f, 0xff, t)
    assert r

def test_fingerprint_1_put(card):
    fpr1 = rsa_keys.fpr[0]
    r = card.cmd_put_data(0x00, 0xc7, fpr1)
    assert r

def test_fingerprint_2_put(card):
    fpr2 = rsa_keys.fpr[1]
    r = card.cmd_put_data(0x00, 0xc8, fpr2)
    assert r

def test_fingerprint_3_put(card):
    fpr3 = rsa_keys.fpr[2]
    r = card.cmd_put_data(0x00, 0xc9, fpr3)
    assert r

def test_timestamp_1(card):
    timestamp1 = rsa_keys.timestamp[0]
    r = card.cmd_put_data(0x00, 0xce, timestamp1)
    assert r

def test_timestamp_2(card):
    timestamp2 = rsa_keys.timestamp[1]
    r = card.cmd_put_data(0x00, 0xcf, timestamp2)
    assert r

def test_timestamp_3(card):
    timestamp3 = rsa_keys.timestamp[2]
    r = card.cmd_put_data(0x00, 0xd0, timestamp3)
    assert r

def test_setup_pw1_0(card):
    r = card.cmd_change_reference_data(1, FACTORY_PASSPHRASE_PW1 + PW1_TEST0)
    assert r

def test_verify_pw1_0(card):
    v = card.cmd_verify(1, PW1_TEST0)
    assert v

def test_verify_pw1_0_2(card):
    v = card.cmd_verify(2, PW1_TEST0)
    assert v

def test_setup_pw1_1(card):
    r = card.cmd_change_reference_data(1, PW1_TEST0 + PW1_TEST1)
    assert r

def test_verify_pw1_1(card):
    v = card.cmd_verify(1, PW1_TEST1)
    assert v

def test_verify_pw1_1_2(card):
    v = card.cmd_verify(2, PW1_TEST1)
    assert v

def test_setup_reset_code(card):
    r = card.cmd_put_data(0x00, 0xd3, RESETCODE_TEST)
    assert r

def test_reset_code(card):
    r = card.cmd_reset_retry_counter(0, 0x81, RESETCODE_TEST + PW1_TEST2)
    assert r

def test_verify_pw1_2(card):
    v = card.cmd_verify(1, PW1_TEST2)
    assert v

def test_verify_pw1_2_2(card):
    v = card.cmd_verify(2, PW1_TEST2)
    assert v

def test_setup_pw3_1(card):
    r = card.cmd_change_reference_data(3, PW3_TEST0 + PW3_TEST1)
    assert r

def test_verify_pw3_1(card):
    v = card.cmd_verify(3, PW3_TEST1)
    assert v

def test_reset_userpass_admin(card):
    r = card.cmd_reset_retry_counter(2, 0x81, PW1_TEST3)
    assert r

def test_verify_pw1_3(card):
    v = card.cmd_verify(1, PW1_TEST3)
    assert v

def test_verify_pw1_3_2(card):
    v = card.cmd_verify(2, PW1_TEST3)
    assert v

def test_setup_pw1_4(card):
    r = card.cmd_change_reference_data(1, PW1_TEST3 + PW1_TEST4)
    assert r

def test_verify_pw1_4(card):
    v = card.cmd_verify(1, PW1_TEST4)
    assert v

def test_verify_pw1_4_2(card):
    v = card.cmd_verify(2, PW1_TEST4)
    assert v

def test_setup_pw3_2(card):
    r = card.cmd_change_reference_data(3, PW3_TEST1 + PW3_TEST0)
    assert r

def test_verify_pw3_2(card):
    v = card.cmd_verify(3, PW3_TEST0)
    assert v

PLAIN_TEXT0=b"This is a test message."
PLAIN_TEXT1=b"RSA decryption is as easy as pie."
PLAIN_TEXT2=b"This is another test message.\nMultiple lines.\n"

def test_sign_0(card):
    digestinfo = rsa_keys.compute_digestinfo(PLAIN_TEXT0)
    r = card.cmd_pso_longdata(0x9e, 0x9a, digestinfo)
    sig = rsa_keys.compute_signature(0, digestinfo)
    sig_bytes = sig.to_bytes(int((sig.bit_length()+7)/8), byteorder='big')
    assert r == sig_bytes

def test_sign_1(card):
    digestinfo = rsa_keys.compute_digestinfo(PLAIN_TEXT1)
    r = card.cmd_pso_longdata(0x9e, 0x9a, digestinfo)
    sig = rsa_keys.compute_signature(0, digestinfo)
    sig_bytes = sig.to_bytes(int((sig.bit_length()+7)/8), byteorder='big')
    assert r == sig_bytes

def test_sign_auth_0(card):
    digestinfo = rsa_keys.compute_digestinfo(PLAIN_TEXT0)
    r = card.cmd_internal_authenticate(digestinfo)
    sig = rsa_keys.compute_signature(2, digestinfo)
    sig_bytes = sig.to_bytes(int((sig.bit_length()+7)/8), byteorder='big')
    assert r == sig_bytes

def test_sign_auth_1(card):
    digestinfo = rsa_keys.compute_digestinfo(PLAIN_TEXT1)
    r = card.cmd_internal_authenticate(digestinfo)
    sig = rsa_keys.compute_signature(2, digestinfo)
    sig_bytes = sig.to_bytes(int((sig.bit_length()+7)/8), byteorder='big')
    assert r == sig_bytes

def test_decrypt_0(card):
    ciphertext = rsa_keys.encrypt(1, PLAIN_TEXT0)
    r = card.cmd_pso_longdata(0x80, 0x86, ciphertext)
    assert r == PLAIN_TEXT0

def test_decrypt_1(card):
    ciphertext = rsa_keys.encrypt(1, PLAIN_TEXT1)
    r = card.cmd_pso_longdata(0x80, 0x86, ciphertext)
    assert r == PLAIN_TEXT1
