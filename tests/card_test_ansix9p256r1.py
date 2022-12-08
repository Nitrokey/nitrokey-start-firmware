"""
card_test_ansix9p256r1.py - test ansix9p256r1 support

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

class Test_Card_AnsiX9P256R1(object):
    def test_ECDH_reference_vectors(self, card):
        assert card.verify(3, FACTORY_PASSPHRASE_PW3)
        assert card.verify(2, FACTORY_PASSPHRASE_PW1)
        # https://tools.ietf.org/html/rfc5903#section-8.1
        assert_ec_pso(
            card=card,
            key_index=1,
            key_attributes=KEY_ATTRIBUTES_ECDH_ANSIX9P256R1,
            key_attribute_caption='ECDH ansix9p256r1',
            private_key=(
                b'\xC8\x8F\x01\xF5\x10\xD9\xAC\x3F\x70\xA2\x92\xDA\xA2\x31\x6D\xE5'
                b'\x44\xE9\xAA\xB8\xAF\xE8\x40\x49\xC6\x2A\x9C\x57\x86\x2D\x14\x33'
            ),
            expected_public_key=(
                b'\x04'
                b'\xDA\xD0\xB6\x53\x94\x22\x1C\xF9\xB0\x51\xE1\xFE\xCA\x57\x87\xD0'
                b'\x98\xDF\xE6\x37\xFC\x90\xB9\xEF\x94\x5D\x0C\x37\x72\x58\x11\x80'
                b'\x52\x71\xA0\x46\x1C\xDB\x82\x52\xD6\x1F\x1C\x45\x6F\xA3\xE5\x9A'
                b'\xB1\xF4\x5B\x33\xAC\xCF\x5F\x58\x38\x9E\x05\x77\xB8\x99\x0B\xB3'
            ),
            pso_input=(
                b'\xa6\x46\x7f\x49\x43\x86\x41'
                b'\x04'
                b'\xD1\x2D\xFB\x52\x89\xC8\xD4\xF8\x12\x08\xB7\x02\x70\x39\x8C\x34'
                b'\x22\x96\x97\x0A\x0B\xCC\xB7\x4C\x73\x6F\xC7\x55\x44\x94\xBF\x63'
                b'\x56\xFB\xF3\xCA\x36\x6C\xC2\x3E\x81\x57\x85\x4C\x13\xC5\x8D\x6A'
                b'\xAC\x23\xF0\x46\xAD\xA3\x0F\x83\x53\xE7\x4F\x33\x03\x98\x72\xAB'
            ),
            expected_pso_output=(
                b'\xD6\x84\x0F\x6B\x42\xF6\xED\xAF\xD1\x31\x16\xE0\xE1\x25\x65\x20'
                b'\x2F\xEF\x8E\x9E\xCE\x7D\xCE\x03\x81\x24\x64\xD0\x4B\x94\x42\xDE'
            ),
        )
