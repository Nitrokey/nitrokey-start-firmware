"""
test_personalize_reset_card.py - test resetting personalization of card

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

def test_login_put(card):
    r = card.cmd_put_data(0x00, 0x5e, b"")
    assert r

def test_name_put(card):
    r = card.cmd_put_data(0x00, 0x5b, b"")
    assert r

def test_lang_put(card):
    r = card.cmd_put_data(0x5f, 0x2d, b"")
    assert r

def test_sex_put(card):
    r = card.cmd_put_data(0x5f, 0x35, b"9")
    # r = card.cmd_put_data(0x5f, 0x35, b"")
    assert r

def test_url_put(card):
    r = card.cmd_put_data(0x5f, 0x50, b"")
    assert r

def test_pw1_status_put(card):
    r = card.cmd_put_data(0x00, 0xc4, b"\x00")
    assert r

def test_setup_pw3_0(card):
    r = card.cmd_change_reference_data(3, PW3_TEST0 + FACTORY_PASSPHRASE_PW3)
    assert r

def test_verify_pw3_0(card):
    v = card.cmd_verify(3, FACTORY_PASSPHRASE_PW3)
    assert v

def test_setup_pw1_0(card):
    r = card.cmd_change_reference_data(1, PW1_TEST4 + FACTORY_PASSPHRASE_PW1)
    assert r

def test_verify_pw1_0(card):
    v = card.cmd_verify(1, FACTORY_PASSPHRASE_PW1)
    assert v

def test_verify_pw1_0_2(card):
    v = card.cmd_verify(2, FACTORY_PASSPHRASE_PW1)
    assert v

def test_setup_reset_code(card):
    r = card.cmd_put_data(0x00, 0xd3, b"")
    assert r
