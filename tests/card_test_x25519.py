"""
card_test_x25519.py - test x25519 support

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

from func_pso_auth import assert_ec_pso
from card_const import *

class Test_Card_X25519(object):
    def test_reference_vectors(self, card):
        assert card.verify(3, FACTORY_PASSPHRASE_PW3)
        assert card.verify(2, FACTORY_PASSPHRASE_PW1)
        # https://tools.ietf.org/html/rfc7748#section-6.1
        assert_ec_pso(
            card=card,
            key_index=1,
            key_attributes=KEY_ATTRIBUTES_X25519,
            key_attribute_caption='x25519',
            private_key=(
                b'\x77\x07\x6d\x0a\x73\x18\xa5\x7d\x3c\x16\xc1\x72\x51\xb2\x66\x45'
                b'\xdf\x4c\x2f\x87\xeb\xc0\x99\x2a\xb1\x77\xfb\xa5\x1d\xb9\x2c\x2a'
            ),
            expected_public_key=(
                b'\x85\x20\xf0\x09\x89\x30\xa7\x54\x74\x8b\x7d\xdc\xb4\x3e\xf7\x5a'
                b'\x0d\xbf\x3a\x0d\x26\x38\x1a\xf4\xeb\xa4\xa9\x8e\xaa\x9b\x4e\x6a'
            ),
            pso_input=(
                b'\xa6\x25\x7f\x49\x22\x86\x20'
                b'\xde\x9e\xdb\x7d\x7b\x7d\xc1\xb4\xd3\x5b\x61\xc2\xec\xe4\x35\x37'
                b'\x3f\x83\x43\xc8\x5b\x78\x67\x4d\xad\xfc\x7e\x14\x6f\x88\x2b\x4f'
            ),
            expected_pso_output=(
                b'\x4a\x5d\x9d\x5b\xa4\xce\x2d\xe1\x72\x8e\x3b\xf4\x80\x35\x0f\x25'
                b'\xe0\x7e\x21\xc9\x47\xd1\x9e\x33\x76\xf0\x9b\x3c\x1e\x16\x17\x42'
            ),
        )
