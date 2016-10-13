"""
test_empty_card.py - test empty card

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

from binascii import hexlify
from re import match, DOTALL
from struct import pack
from util import *

EMPTY_60=bytes(60)

FACTORY_PASSPHRASE_PW1=b"123456"
FACTORY_PASSPHRASE_PW3=b"12345678"

def test_login(card):
    login = get_data_object(card, 0x5e)
    assert check_null(login)

"""
def test_name(card):
    name = get_data_object(card, 0x5b)
    assert check_null(name)

def test_lang(card):
    lang = get_data_object(card, 0x5f2d)
    assert check_null(lang)

def test_sex(card):
    sex = get_data_object(card, 0x5f35)
    assert check_null(sex)
"""

def test_name_lang_sex(card):
    name = b""
    lang = b""
    sex = b"9"
    expected = b'\x5b' + pack('B', len(name)) + name \
               +  b'\x5f\x2d' + pack('B', len(lang)) + lang \
               + b'\x5f\x35' + pack('B', len(sex)) + sex
    name_lang_sex = get_data_object(card, 0x65)
    assert name_lang_sex == b'' or name_lang_sex == expected

def test_url(card):
    url = get_data_object(card, 0x5f50)
    assert check_null(url)

def test_ds_counter(card):
    c = get_data_object(card, 0x93)
    assert c == None or c == b'\x00\x00\x00'

def test_pw1_status(card):
    s = get_data_object(card, 0xc4)
    assert match(b'\x00...\x03[\x00\x03]\x03', s, DOTALL)

def test_fingerprint_0(card):
    fprlist = get_data_object(card, 0xC5)
    assert fprlist == None or fprlist == EMPTY_60

def test_fingerprint_1(card):
    fpr = get_data_object(card, 0xC7)
    assert check_null(fpr)

def test_fingerprint_2(card):
    fpr = get_data_object(card, 0xC8)
    assert check_null(fpr)

def test_fingerprint_3(card):
    fpr = get_data_object(card, 0xC9)
    assert check_null(fpr)

def test_ca_fingerprint_0(card):
    cafprlist = get_data_object(card, 0xC6)
    assert cafprlist == None or cafprlist == EMPTY_60

def test_ca_fingerprint_1(card):
    cafp = get_data_object(card, 0xCA)
    assert check_null(cafp)

def test_ca_fingerprint_2(card):
    cafp = get_data_object(card, 0xCB)
    assert check_null(cafp)

def test_ca_fingerprint_3(card):
    cafp = get_data_object(card, 0xCC)
    assert check_null(cafp)

def test_timestamp_0(card):
    t = get_data_object(card, 0xCD)
    assert t == None or t == b'\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'

def test_timestamp_1(card):
    t = get_data_object(card, 0xCE)
    assert check_null(t)

def test_timestamp_2(card):
    t = get_data_object(card, 0xCF)
    assert check_null(t)

def test_timestamp_3(card):
    t = get_data_object(card, 0xD0)
    assert check_null(t)

def test_verify_pw1_1(card):
    v = card.cmd_verify(1, FACTORY_PASSPHRASE_PW1)
    assert v

def test_verify_pw1_2(card):
    v = card.cmd_verify(2, FACTORY_PASSPHRASE_PW1)
    assert v

def test_verify_pw3(card):
    v = card.cmd_verify(3, FACTORY_PASSPHRASE_PW3)
    assert v

def test_historical_bytes(card):
    h = get_data_object(card, 0x5f52)
    assert h == b'\x001\xc5s\xc0\x01@\x05\x90\x00' or \
           h == b'\x00\x31\x84\x73\x80\x01\x80\x00\x90\x00' or \
           h == b'\x00\x31\x84\x73\x80\x01\x80\x05\x90\x00'

def test_extended_capabilities(card):
    a = get_data_object(card, 0xc0)
    assert a == None or match(b'[\x70\x74]\x00\x00\x20[\x00\x08]\x00\x00\xff\x01\x00', a)

def test_algorithm_attributes_1(card):
    a = get_data_object(card, 0xc1)
    assert a == None or a == b'\x01\x08\x00\x00\x20\x00'

def test_algorithm_attributes_2(card):
    a = get_data_object(card, 0xc2)
    assert a == None or a == b'\x01\x08\x00\x00\x20\x00'

def test_algorithm_attributes_3(card):
    a = get_data_object(card, 0xc3)
    assert a == None or a == b'\x01\x08\x00\x00\x20\x00'

def test_AID(card):
    a = get_data_object(card, 0x4f)
    print()
    print("OpenPGP card version: %d.%d" % (a[6], a[7]))
    print("Card Manufacturer:  ", hexlify(a[8:10]).decode("UTF-8"))
    print("Card serial:    ", hexlify(a[10:14]).decode("UTF-8"))
    assert match(b'\xd2\x76\x00\x01\\$\x01........\x00\x00', a, DOTALL)
