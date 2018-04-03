"""
test_remove_keys_card.py - test removing keys on card

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

# Remove a key material on card by changing algorithm attributes of the key

KEY_ATTRIBUTES_RSA4K=b"\x01\x10\x00\x00\x20\x00"
KEY_ATTRIBUTES_RSA2K=b"\x01\x08\x00\x00\x20\x00"

def test_rsa_import_key_1(card):
    r = card.cmd_put_data(0x00, 0xc1, KEY_ATTRIBUTES_RSA4K)
    if r:
        r = card.cmd_put_data(0x00, 0xc1, KEY_ATTRIBUTES_RSA2K)
    assert r

def test_rsa_import_key_2(card):
    r = card.cmd_put_data(0x00, 0xc2, KEY_ATTRIBUTES_RSA4K)
    if r:
        r = card.cmd_put_data(0x00, 0xc2, KEY_ATTRIBUTES_RSA2K)
    assert r

def test_rsa_import_key_3(card):
    r = card.cmd_put_data(0x00, 0xc3, KEY_ATTRIBUTES_RSA4K)
    if r:
        r = card.cmd_put_data(0x00, 0xc3, KEY_ATTRIBUTES_RSA2K)
    assert r
