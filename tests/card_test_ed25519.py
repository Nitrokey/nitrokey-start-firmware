"""
card_test_ed25519.py - test ed25519 support

Copyright (C) 2021  Vincent Pelletier <plr.vincent@gmail.com>

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

import hashlib
from func_pso_auth import assert_ec_pso
from card_const import *

class Test_Card_ED25519(object):
    def test_reference_vectors(self, card):
        assert card.verify(3, FACTORY_PASSPHRASE_PW3)
        assert card.verify(1, FACTORY_PASSPHRASE_PW1)
        # https://tools.ietf.org/html/rfc8032#section-7.3
        assert_ec_pso(
            card=card,
            key_index=0,
            key_attributes=KEY_ATTRIBUTES_ED25519,
            key_attribute_caption='ed25519',
            private_key=(
                b'\x83\x3f\xe6\x24\x09\x23\x7b\x9d\x62\xec\x77\x58\x75\x20\x91\x1e'
                b'\x9a\x75\x9c\xec\x1d\x19\x75\x5b\x7d\xa9\x01\xb9\x6d\xca\x3d\x42'
            ),
            expected_public_key=(
                b'\xec\x17\x2b\x93\xad\x5e\x56\x3b\xf4\x93\x2c\x70\xe1\x24\x50\x34'
                b'\xc3\x54\x67\xef\x2e\xfd\x4d\x64\xeb\xf8\x19\x68\x34\x67\xe2\xbf'
            ),
            pso_input=hashlib.sha512(b'\x61\x62\x63').digest(),
            expected_pso_output=(
                b'\x98\xa7\x02\x22\xf0\xb8\x12\x1a\xa9\xd3\x0f\x81\x3d\x68\x3f\x80'
                b'\x9e\x46\x2b\x46\x9c\x7f\xf8\x76\x39\x49\x9b\xb9\x4e\x6d\xae\x41'
                b'\x31\xf8\x50\x42\x46\x3c\x2a\x35\x5a\x20\x03\xd0\x62\xad\xf5\xaa'
                b'\xa1\x0b\x8c\x61\xe6\x36\x06\x2a\xaa\xd1\x1c\x2a\x26\x08\x34\x06'
            ),
        )
